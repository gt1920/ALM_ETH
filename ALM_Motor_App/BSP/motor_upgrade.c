/**
 * motor_upgrade.c - ALM_Motor_APP OTA receiver
 *
 * State machine:
 *   IDLE      ─[START]─▶ validate hdr (magic+MOT+CRC); on fail → reject (state stays IDLE,
 *                       STAGING untouched, old firmware keeps running)
 *                       on pass → erase STAGING; write hdr[0..15] to STAGING[0..15]
 *                                  → state RECVING; next_offset=16
 *   RECVING   ─[DATA] ─▶ if offset == next_offset, write to STAGING[offset];
 *                                  next_offset += data_len
 *   RECVING   ─[END]  ─▶ if total_size == next_offset, schedule deferred reboot
 *                                  → state DONE
 *
 * We don't decrypt or verify the payload here (Bootloader does that). We only
 * decrypt the 16 B FW_ID header to enforce board=MOT before touching flash.
 * That way a wrong-board frame can't even erase STAGING, let alone the app.
 */
#include "motor_upgrade.h"
#include "motor_partition.h"
#include "aes.h"
#include "Motor.h"             /* FDCAN_NodeID */
#include "CAN_Comm.h"          /* CAN_Send_FD */
#include "LED.h"
#include "main.h"
#include <string.h>

typedef enum {
    UPG_S_IDLE    = 0,
    UPG_S_RECVING = 1,
    UPG_S_DONE    = 2,
} UpgState_t;

/* Made global (non-static) so Keil watch can inspect during OTA debug.
   Also added counters that the ISR/state machine bumps on each event;
   handy when looking at "did any frame even arrive?". */
struct UpgCtx {
    UpgState_t state;
    uint32_t   file_size;       /* total .mot bytes (header + payload) */
    uint32_t   next_offset;     /* bytes already written to STAGING    */
    uint32_t   rx_start_count;  /* CANID_UPG_START frames seen          */
    uint32_t   rx_data_count;   /* CANID_UPG_DATA frames seen           */
    uint32_t   rx_end_count;    /* CANID_UPG_END frames seen            */
    uint32_t   tx_resp_count;   /* RESP frames sent to device           */
    uint8_t    last_resp_status;/* status byte of the last RESP we sent */
};
struct UpgCtx g_upg;

/* ---- helpers ---- */
static inline uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static uint32_t crc32_calc(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    while (len--)
    {
        crc ^= *buf++;
        for (uint8_t b = 0U; b < 8U; b++)
            crc = (crc & 1U) ? (0xEDB88320UL ^ (crc >> 1)) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* Send a 12-byte RESP frame back to the device. */
static void send_resp(uint8_t status, uint32_t last_offset)
{
    uint8_t f[12];
    f[0] = (uint8_t)(FDCAN_NodeID);
    f[1] = (uint8_t)(FDCAN_NodeID >> 8);
    f[2] = (uint8_t)(FDCAN_NodeID >> 16);
    f[3] = (uint8_t)(FDCAN_NodeID >> 24);
    f[4] = status;
    f[5] = 0U;
    f[6] = 0U;
    f[7] = 0U;
    f[8]  = (uint8_t)(last_offset);
    f[9]  = (uint8_t)(last_offset >> 8);
    f[10] = (uint8_t)(last_offset >> 16);
    f[11] = (uint8_t)(last_offset >> 24);
    g_upg.last_resp_status = status;
    g_upg.tx_resp_count++;
    (void)CAN_Send_FD(CANID_UPG_RESP, f, 12U);
}

/* Erase all 32 pages of STAGING (64 KB). Blocking, ~150 ms total. */
static HAL_StatusTypeDef erase_staging(void)
{
    HAL_StatusTypeDef rc;
    FLASH_EraseInitTypeDef e = {0};
    uint32_t pe = 0xFFFFFFFFUL;

    e.TypeErase = FLASH_TYPEERASE_PAGES;
    e.Banks     = FLASH_BANK_1;
    e.Page      = (MOT_STAGE_BASE - MOT_FLASH_BASE) / MOT_FLASH_SECTOR_SIZE;
    e.NbPages   = MOT_STAGE_SIZE / MOT_FLASH_SECTOR_SIZE;

    HAL_FLASH_Unlock();
    rc = HAL_FLASHEx_Erase(&e, &pe);
    HAL_FLASH_Lock();
    return rc;
}

/* Write `len` bytes (must be multiple of 8) at MOT_STAGE_BASE+offset. */
static HAL_StatusTypeDef write_staging(uint32_t offset, const uint8_t *src, uint32_t len)
{
    HAL_StatusTypeDef rc = HAL_OK;
    uint32_t addr = MOT_STAGE_BASE + offset;
    uint32_t i;

    if ((len & 7U) != 0U) return HAL_ERROR;     /* must be doubleword aligned */

    HAL_FLASH_Unlock();
    for (i = 0U; i < len; i += 8U)
    {
        uint64_t dw;
        memcpy(&dw, &src[i], 8U);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, dw) != HAL_OK)
        {
            rc = HAL_ERROR;
            break;
        }
    }
    HAL_FLASH_Lock();
    return rc;
}

/* =========================================================================
 *  Public API
 * ========================================================================= */

void Upgrade_Init(void)
{
    memset(&g_upg, 0, sizeof(g_upg));
    g_upg.state = UPG_S_IDLE;
}

void Upgrade_OnCanFrame(uint32_t id, const uint8_t *data, uint8_t len)
{
    /* Caller (AdjusterCAN_OnRx) has already matched data[0..3] == FDCAN_NodeID. */

    switch (id)
    {
    case CANID_UPG_START:
    {
        g_upg.rx_start_count++;
        if (len < 24U) { send_resp(UPG_FRAME_TOO_SHORT, 0); return; }

        uint32_t file_size = rd_le32(&data[4]);
        const uint8_t *enc_hdr = &data[8];     /* 16 B encrypted FW_ID header */

        /* file_size sanity: 16 hdr + 16 metadata + 16 CRC block + ≥1 payload block.
           App side does not interpret the CRC block — it just stages bytes —
           but the lower bound moves with the .mot format change. */
        if (file_size < 64U || file_size > MOT_STAGE_SIZE)
        {
            send_resp(UPG_SIZE_ERROR, 0);
            return;
        }

        /* Decrypt hdr (independent CBC, fresh IV copy). DO NOT touch flash yet. */
        uint8_t  block[16];
        uint8_t  iv[16];
        memcpy(block, enc_hdr, 16U);
        memcpy(iv,    IV,      16U);
        Aes_IV_key256bit_Decode(iv, block, Key);

        uint32_t magic   = rd_le32(&block[0]);
        uint32_t board   = rd_le32(&block[4]);
        uint32_t hdr_crc = rd_le32(&block[12]);

        if (magic != MOT_FW_ID_MAGIC || crc32_calc(block, 12U) != hdr_crc)
        {
            send_resp(UPG_BAD_HEADER, 0);
            return;
        }
        if (board != MOT_BOARD_ID)
        {
            /* Wrong PCB — never erase STAGING, never reset. Existing app keeps running. */
            send_resp(UPG_WRONG_BOARD, 0);
            return;
        }

        /* Header passed → erase STAGING, write 16 B encrypted hdr. */
        if (erase_staging() != HAL_OK)            { send_resp(UPG_WRITE_ERROR, 0); return; }
        if (write_staging(0U, enc_hdr, 16U) != HAL_OK) { send_resp(UPG_WRITE_ERROR, 0); return; }

        g_upg.state       = UPG_S_RECVING;
        g_upg.file_size   = file_size;
        g_upg.next_offset = 16U;
        send_resp(UPG_OK, 16U);
        break;
    }

    case CANID_UPG_DATA:
    {
        g_upg.rx_data_count++;
        if (g_upg.state != UPG_S_RECVING)        { send_resp(UPG_NOT_RECEIVING, 0); return; }
        if (len < 12U)                            { send_resp(UPG_FRAME_TOO_SHORT, g_upg.next_offset); return; }

        uint32_t offset    = rd_le32(&data[4]);
        uint32_t chunk_len = (uint32_t)len - 8U;     /* node_id(4) + offset(4) */

        if (offset != g_upg.next_offset)         { send_resp(UPG_SEQ_ERROR, g_upg.next_offset); return; }
        if (offset + chunk_len > g_upg.file_size){ send_resp(UPG_SIZE_ERROR, g_upg.next_offset); return; }
        if ((chunk_len & 7U) != 0U)              { send_resp(UPG_FRAME_TOO_SHORT, g_upg.next_offset); return; }

        if (write_staging(offset, &data[8], chunk_len) != HAL_OK)
        {
            /* Flash failed mid-write. Leave state in RECVING so retry can resend
               this frame; if device gives up, clearing STAGING happens next START. */
            send_resp(UPG_WRITE_ERROR, g_upg.next_offset);
            return;
        }

        g_upg.next_offset += chunk_len;
        send_resp(UPG_OK, g_upg.next_offset);

        /* All bytes staged? Reboot now — don't wait for an END frame.
           Observed in the field: rx_end_count stayed 0 even with all DATA
           accounted for (next_offset == file_size), so the device sat
           wedged in RECVING forever and the user had to power-cycle to
           land in the BL. Once the byte count is complete the staging
           image is structurally whole; the BL can verify CRC on its own. */
        if (g_upg.next_offset >= g_upg.file_size)
        {
            g_upg.state = UPG_S_DONE;
            HAL_Delay(50U);
            __disable_irq();
            NVIC_SystemReset();
            while (1) { __NOP(); }
        }
        break;
    }

    case CANID_UPG_END:
    {
        g_upg.rx_end_count++;
        /* Idempotent: if the device retransmits END (its RESP was lost and
           we've already crossed the reboot path with a prior END), there's
           nothing left to do but ack — but in practice we never reach here
           after the first valid END because that one resets the MCU below. */
        if (g_upg.state == UPG_S_DONE)
        {
            send_resp(UPG_OK, g_upg.next_offset);
            break;
        }
        if (g_upg.state != UPG_S_RECVING)        { send_resp(UPG_NOT_RECEIVING, 0); return; }
        if (len < 8U)                            { send_resp(UPG_FRAME_TOO_SHORT, g_upg.next_offset); return; }

        uint32_t total = rd_le32(&data[4]);
        if (total != g_upg.next_offset || total != g_upg.file_size)
        {
            send_resp(UPG_SIZE_ERROR, g_upg.next_offset);
            return;
        }

        g_upg.state = UPG_S_DONE;
        send_resp(UPG_OK, g_upg.next_offset);

        /* Inline NVIC_SystemReset (was deferred via main-loop polling — that
           path turned out to be flaky; some boots wrote staging successfully
           but never auto-rebooted, requiring a manual power-cycle to land in
           the BL). 50 ms is plenty for the RESP frame to leave the TX FIFO
           on a 1 Mbit/2 Mbit FDCAN bus. */
        HAL_Delay(50U);
        __disable_irq();
        NVIC_SystemReset();
        while (1) { __NOP(); }
    }

    default:
        break;
    }
}

void Upgrade_PollReboot(void)
{
    /* No-op since the END handler now resets inline. Kept as a stub so older
       callers (main loop) link without an edit. */
}

void Upgrade_LedBlinkPoll(void)
{
    /* No-op: Stat_LED is now driven by the CAN-RX ACT-light path
       (update_data → process → signal_led_off). Forcing LED_ON here
       would clobber the OFF half of the blink and pin the LED solid. */
}
