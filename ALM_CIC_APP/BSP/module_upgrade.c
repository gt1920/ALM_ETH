/**
 * module_upgrade.c - device-side relay for ALM_Motor OTA
 *
 * TCP side (Process_ETH_Command → MUR_HandleCommand):
 *   START : file_size(4) + target_node_id(4) + cic_hdr_16(16)
 *           → erase W25Q[MUR_W25Q_BASE..+ceil(file_size, 4 KB)],
 *             stash hdr at W25Q[base+0..15], remember target_node_id.
 *   DATA  : offset(4) + data(≤128 B)
 *           → write to W25Q[base+offset]. Offset already includes the 16 B
 *             header position (PC also re-sends the header here for simplicity).
 *   END   : total_size(4)  → mark "ready to relay", reply OK to PC.
 *
 * CAN-FD relay (driven by MUR_PollRelay() in main loop):
 *   READY → SEND_START → WAIT_START_RESP → SEND_DATA loop → SEND_END → DONE.
 *   Each frame is sent, then wait up to MUR_RESP_TIMEOUT_MS for the Motor's
 *   RESP (CAN ID 0x305). On timeout the relay aborts and resets to IDLE.
 *
 * NB: device-side does NOT decrypt or validate the .mot — the Motor App
 * does that on the receiving end. We only sanity-check sizes so a broken
 * PC client can't spam the W25Q.
 */
#include "module_upgrade.h"
#include "comm_protocol.h"
#include "bsp_w25q16.h"
#include "tcp_server.h"
#include "CAN_comm.h"
#include "main.h"
#include <string.h>

#define MUR_RESP_TIMEOUT_MS     800U    /* Motor erase_staging is ~150 ms */
#define MUR_END_MAX_RETRIES     3U      /* END is the reboot trigger — retry */

/* ---- relay state machine ---- */
typedef enum {
    MUR_S_IDLE         = 0,
    MUR_S_STAGING      = 1,   /* receiving DATA from PC into W25Q       */
    MUR_S_READY        = 2,   /* TCP END received; relay pending        */
    MUR_S_WAIT_START   = 3,
    MUR_S_SEND_DATA    = 4,
    MUR_S_WAIT_DATA    = 5,
    MUR_S_SEND_END     = 6,
    MUR_S_WAIT_END     = 7,
    MUR_S_DONE         = 8,
    MUR_S_FAILED       = 9,
} MurState_t;

/* Made global (non-static) so Keil watch can inspect during OTA debug. */
struct MurCtx {
    MurState_t  state;
    uint32_t    file_size;
    uint32_t    rx_next;         /* next expected DATA offset (monotonic) */
    uint32_t    tx_offset;       /* next .mot offset to relay             */
    uint32_t    target_node_id;
    uint8_t     hdr16[16];       /* cached encrypted FW_ID hdr            */
    uint32_t    deadline;        /* HAL tick deadline for current wait    */

    /* Latest CAN RESP captured by MUR_OnCanResp (volatile: ISR context). */
    volatile uint8_t  resp_pending;
    volatile uint8_t  resp_status;
    volatile uint32_t resp_node_id;
    volatile uint32_t resp_last_offset;

    /* CAN-FD relay debug counters: increments at each side of every relay edge. */
    uint32_t tx_start_count;     /* CANID_UPG_START frames we sent       */
    uint32_t tx_data_count;      /* CANID_UPG_DATA  frames we sent       */
    uint32_t tx_end_count;       /* CANID_UPG_END   frames we sent       */
    uint32_t rx_resp_count;      /* RESP frames received from Motor      */
    uint32_t timeout_count;      /* deadline expirations (per-stage)     */
    uint8_t  end_retries;        /* current END retransmission attempt   */
};
struct MurCtx g;

/* =========================================================================
 *  helpers
 * ========================================================================= */
static inline uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static inline void wr_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* Pick the largest chunk size that maps to a valid CAN-FD DLC after the
   8 B preamble (node_id + offset). Valid DLCs ≥ 16: 16/24/32/48/64 → chunk
   sizes 8/16/24/40/56. All multiples of 8 (doubleword-aligned for Motor's
   FLASH_PROGRAM). Caller guarantees `remaining` is a multiple of 8. */
static uint8_t pick_chunk(uint32_t remaining)
{
    if (remaining >= 56U) return 56U;
    if (remaining >= 40U) return 40U;
    if (remaining >= 24U) return 24U;
    if (remaining >= 16U) return 16U;
    if (remaining >=  8U) return  8U;
    return 0U;
}

static void send_tcp_resp(uint16_t seq, uint8_t status)
{
    uint8_t r[8];
    r[0] = PROTO_FRAME_HEAD;
    r[1] = CMD_MODULE_UPGRADE;
    r[2] = DIR_MCU_TO_PC;
    r[3] = (uint8_t)(seq & 0xFFU);
    r[4] = (uint8_t)(seq >> 8);
    r[5] = 2U;                              /* LEN_BODY = subcmd(1) + status(1) */
    r[6] = SUBCMD_MUPG_RESP;
    r[7] = status;
    TCP_Server_Send(r, 8U);
}

/* Erase W25Q sectors covering [base, base + size), 4 KB sectors. */
static W25Q_Status_t erase_w25q_range(uint32_t base, uint32_t size)
{
    uint32_t end = base + size;
    uint32_t addr;
    for (addr = base & ~0x0FFFU; addr < end; addr += 0x1000U)
    {
        W25Q_Status_t s = BSP_W25Q_EraseSector4K(addr);
        if (s != W25Q_OK) return s;
    }
    return W25Q_OK;
}

/* =========================================================================
 *  Public API
 * ========================================================================= */

void MUR_Init(void)
{
    memset(&g, 0, sizeof(g));
    g.state = MUR_S_IDLE;
}

/* ---- TCP-side handler ---- */
void MUR_HandleCommand(const uint8_t *buf, uint16_t len)
{
    if (len < 7U) return;

    uint16_t seq    = (uint16_t)buf[3] | ((uint16_t)buf[4] << 8);
    uint8_t  subcmd = buf[6];

    switch (subcmd)
    {
    case SUBCMD_MUPG_START:
    {
        /* Need: hdr(6) + subcmd(1) + file_size(4) + target_node_id(4) + cic_hdr(16) = 31 */
        if (len < 31U) { send_tcp_resp(seq, UPG_STATUS_SIZE_ERROR); return; }
        if (g.state != MUR_S_IDLE && g.state != MUR_S_DONE && g.state != MUR_S_FAILED)
        {
            send_tcp_resp(seq, MUPG_STATUS_BUSY);
            return;
        }

        uint32_t fsz   = rd_le32(&buf[7]);
        uint32_t node  = rd_le32(&buf[11]);
        const uint8_t *hdr = &buf[15];

        if (fsz == 0U || fsz > MUR_W25Q_SIZE)
        {
            send_tcp_resp(seq, UPG_STATUS_SIZE_ERROR);
            return;
        }

        /* Erase W25Q region for this upload (round up to 4 KB sectors). */
        if (erase_w25q_range(MUR_W25Q_BASE, fsz) != W25Q_OK)
        {
            send_tcp_resp(seq, UPG_STATUS_WRITE_ERROR);
            return;
        }

        /* Cache hdr in RAM for the CAN-FD START frame; the same 16 B will be
           re-sent by the PC at offset 0 in the DATA stream and land in
           W25Q[base..base+15] then. No need to write it twice. */
        memcpy(g.hdr16, hdr, 16U);
        g.file_size      = fsz;
        g.target_node_id = node;
        g.rx_next        = 0U;           /* PC streams the full .mot from 0  */
        g.tx_offset      = 0U;
        g.end_retries    = 0U;
        g.state          = MUR_S_STAGING;
        send_tcp_resp(seq, UPG_STATUS_OK);
        break;
    }

    case SUBCMD_MUPG_DATA:
    {
        if (g.state != MUR_S_STAGING) { send_tcp_resp(seq, UPG_STATUS_VERIFY_ERROR); return; }
        if (len < 12U)                { send_tcp_resp(seq, UPG_STATUS_SIZE_ERROR);   return; }

        uint32_t offset    = rd_le32(&buf[7]);
        uint32_t body_len  = (uint16_t)buf[5];        /* SUBCMD(1) + offset(4) + data */
        uint32_t data_len  = body_len - 5U;
        const uint8_t *data = &buf[11];

        if (offset != g.rx_next || offset + data_len > g.file_size)
        {
            send_tcp_resp(seq, UPG_STATUS_VERIFY_ERROR);
            return;
        }
        if (BSP_W25Q_Write(MUR_W25Q_BASE + offset, data, data_len) != W25Q_OK)
        {
            send_tcp_resp(seq, UPG_STATUS_WRITE_ERROR);
            return;
        }
        g.rx_next = offset + data_len;
        send_tcp_resp(seq, UPG_STATUS_OK);
        break;
    }

    case SUBCMD_MUPG_END:
    {
        if (g.state != MUR_S_STAGING) { send_tcp_resp(seq, UPG_STATUS_VERIFY_ERROR); return; }
        if (len < 11U)                { send_tcp_resp(seq, UPG_STATUS_SIZE_ERROR);   return; }

        uint32_t total = rd_le32(&buf[7]);
        if (total != g.file_size || total != g.rx_next)
        {
            send_tcp_resp(seq, UPG_STATUS_VERIFY_ERROR);
            return;
        }

        g.state = MUR_S_READY;
        send_tcp_resp(seq, UPG_STATUS_OK);
        /* Relay starts asynchronously from MUR_PollRelay(). */
        break;
    }

    default:
        break;
    }
}

/* ---- CAN RX side: called from HAL_FDCAN_RxFifo0Callback when ID == 0x305. */
void MUR_OnCanResp(uint32_t node_id, uint8_t status, uint32_t last_offset)
{
    g.resp_node_id     = node_id;
    g.resp_status      = status;
    g.resp_last_offset = last_offset;
    g.resp_pending     = 1U;
    g.rx_resp_count++;
}

/* ---- main-loop relay state machine ---- */
void MUR_PollRelay(void)
{
    uint32_t now = HAL_GetTick();

    switch (g.state)
    {
    case MUR_S_IDLE:
    case MUR_S_STAGING:
    case MUR_S_DONE:
    case MUR_S_FAILED:
        return;

    case MUR_S_READY:
    {
        /* Build START frame: node_id(4) + file_size(4) + hdr16(16) = 24 */
        uint8_t f[24];
        wr_le32(&f[0], g.target_node_id);
        wr_le32(&f[4], g.file_size);
        memcpy(&f[8], g.hdr16, 16U);

        g.resp_pending = 0U;
        CAN_Send_FD_Frame(MUR_CANID_START, f, 24U);
        g.tx_start_count++;

        g.deadline = now + MUR_RESP_TIMEOUT_MS;
        g.state    = MUR_S_WAIT_START;
        break;
    }

    case MUR_S_WAIT_START:
    {
        if (g.resp_pending)
        {
            uint8_t  st = g.resp_status;
            uint32_t nd = g.resp_node_id;
            g.resp_pending = 0U;
            if (nd != g.target_node_id || st != UPG_STATUS_OK)
            {
                g.state = MUR_S_FAILED;
                return;
            }
            g.tx_offset = 16U;        /* Motor wrote hdr at 0..15 already */
            g.state     = MUR_S_SEND_DATA;
            return;
        }
        if ((int32_t)(now - g.deadline) >= 0) { g.timeout_count++; g.state = MUR_S_FAILED; }
        break;
    }

    case MUR_S_SEND_DATA:
    {
        uint32_t remaining = g.file_size - g.tx_offset;
        uint8_t  chunk     = pick_chunk(remaining);
        if (chunk == 0U) { g.state = MUR_S_SEND_END; return; }

        /* Build DATA frame: node_id(4) + offset(4) + data(chunk) */
        uint8_t f[64];
        wr_le32(&f[0], g.target_node_id);
        wr_le32(&f[4], g.tx_offset);
        if (BSP_W25Q_Read(MUR_W25Q_BASE + g.tx_offset, &f[8], chunk) != W25Q_OK)
        {
            g.state = MUR_S_FAILED;
            return;
        }

        g.resp_pending = 0U;
        CAN_Send_FD_Frame(MUR_CANID_DATA, f, (uint8_t)(8U + chunk));
        g.tx_data_count++;

        g.deadline = now + MUR_RESP_TIMEOUT_MS;
        g.state    = MUR_S_WAIT_DATA;
        break;
    }

    case MUR_S_WAIT_DATA:
    {
        if (g.resp_pending)
        {
            uint8_t  st = g.resp_status;
            uint32_t nd = g.resp_node_id;
            uint32_t lo = g.resp_last_offset;
            g.resp_pending = 0U;
            if (nd != g.target_node_id || st != UPG_STATUS_OK)
            {
                g.state = MUR_S_FAILED;
                return;
            }
            /* lo is the new "next expected offset" on Motor side. Sanity-
               check: it should equal g.tx_offset + chunk_just_sent. We
               recover the chunk size from lo - tx_offset. */
            if (lo > g.file_size || lo < g.tx_offset)
            {
                g.state = MUR_S_FAILED;
                return;
            }
            g.tx_offset = lo;
            g.state     = MUR_S_SEND_DATA;
            return;
        }
        if ((int32_t)(now - g.deadline) >= 0) { g.timeout_count++; g.state = MUR_S_FAILED; }
        break;
    }

    case MUR_S_SEND_END:
    {
        uint8_t f[8];
        wr_le32(&f[0], g.target_node_id);
        wr_le32(&f[4], g.file_size);

        g.resp_pending = 0U;
        CAN_Send_FD_Frame(MUR_CANID_END, f, 8U);
        g.tx_end_count++;

        g.deadline = now + MUR_RESP_TIMEOUT_MS;
        g.state    = MUR_S_WAIT_END;
        break;
    }

    case MUR_S_WAIT_END:
    {
        if (g.resp_pending)
        {
            uint8_t  st = g.resp_status;
            uint32_t nd = g.resp_node_id;
            g.resp_pending = 0U;
            if (nd == g.target_node_id && st == UPG_STATUS_OK)
            {
                g.end_retries = 0;
                g.state = MUR_S_DONE;
            }
            else
            {
                g.state = MUR_S_FAILED;
            }
            return;
        }
        if ((int32_t)(now - g.deadline) >= 0)
        {
            /* END is the most fragile frame — Motor needs the reboot signal to
               commit the staged firmware. Retry up to MUR_END_MAX_RETRIES
               before giving up; in practice a single retransmit is plenty. */
            g.timeout_count++;
            if (g.end_retries < MUR_END_MAX_RETRIES)
            {
                g.end_retries++;
                g.state = MUR_S_SEND_END;   /* SEND_END resends and re-arms deadline */
            }
            else
            {
                g.end_retries = 0;
                g.state = MUR_S_FAILED;
            }
        }
        break;
    }
    }
}
