/**
 * upgrade_handler.c - OTA firmware upgrade over TCP (CMD_UPGRADE = 0x30)
 *
 * START payload: file_size(4 LE) + cic_hdr[0..15](16 B)
 *   Device decrypts the .cic header block, checks magic / board_id / fw_sn / CRC32
 *   before erasing W25Q. Rejects immediately if any check fails.
 *
 * DATA payload:  offset(4 LE) + data(≤128 B)
 * END  payload:  total_size(4 LE)
 */
#include "upgrade_handler.h"
#include "comm_protocol.h"
#include "aes.h"
#include "bsp_w25q16.h"
#include "tcp_server.h"
#include "cpu_id.h"
#include "main.h"
#include <string.h>

/* .cic is stored at W25Q address 0 — bootloader reads it from there */
#define UPG_W25Q_ADDR       0x00000000UL

/* This firmware's board identity ("ETH\0" packed LE) */
#define THIS_BOARD_ID       0x00485445UL
#define FW_ID_MAGIC         0x47544657UL
#define FW_SN_WILDCARD      0xA5C3F09EUL

/* Status bytes (aliases kept local; canonical defines are in comm_protocol.h) */
#define UPG_OK              UPG_STATUS_OK
#define UPG_WRONG_BOARD     UPG_STATUS_WRONG_BOARD
#define UPG_SIZE_ERROR      UPG_STATUS_SIZE_ERROR
#define UPG_WRITE_ERROR     UPG_STATUS_WRITE_ERROR
#define UPG_VERIFY_ERROR    UPG_STATUS_VERIFY_ERROR
#define UPG_BAD_FW          UPG_STATUS_BAD_FW
#define UPG_WRONG_SN        UPG_STATUS_WRONG_SN

/* Response frame is 8 bytes (A5 + CMD + DIR + SEQ_L + SEQ_H + LEN_BODY + SUBCMD + STATUS) */
#define RESP_LEN            8U

/* ---- state ---- */
typedef enum { UPG_IDLE = 0, UPG_ACTIVE } UpgState_t;
static UpgState_t s_state        = UPG_IDLE;
static uint32_t   s_file_size    = 0U;
static uint32_t   s_rx_bytes     = 0U;
static uint8_t    s_reboot_pending = 0U;

/* ---- helpers ---- */
static inline uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* CRC32/ISO-HDLC (matches Copy_Bin and bootloader) */
static uint32_t crc32(const uint8_t *buf, uint32_t len)
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

static void send_resp(uint16_t seq, uint8_t status)
{
    uint8_t r[RESP_LEN];
    r[0] = PROTO_FRAME_HEAD;
    r[1] = CMD_UPGRADE;
    r[2] = DIR_MCU_TO_PC;
    r[3] = (uint8_t)(seq & 0xFFU);
    r[4] = (uint8_t)(seq >> 8);
    r[5] = 2U;            /* LEN_BODY = SUBCMD_UPG_RESP(1) + status(1) */
    r[6] = SUBCMD_UPG_RESP;
    r[7] = status;
    TCP_Server_Send(r, RESP_LEN);
}

/* =========================================================
 * UPG_HandleCommand
 * Called from Process_ETH_Command when buf[1] == CMD_UPGRADE.
 * buf[0..5] = standard frame header; buf[6] = subcmd; buf[7..] = payload.
 * ========================================================= */
void UPG_HandleCommand(const uint8_t *buf, uint16_t len)
{
    if (len < 7U)
        return;

    uint16_t seq    = (uint16_t)buf[3] | ((uint16_t)buf[4] << 8);
    uint8_t  subcmd = buf[6];

    switch (subcmd)
    {
        /* ---- START: validate .cic header, erase W25Q, ACK ----
         * Payload: file_size(4 LE) + cic_hdr[0..15](16 B)
         * The 16-byte block is the encrypted FW_ID header from the .cic file.
         * We decrypt it and verify magic / board_id / fw_sn / CRC32
         * before touching the flash.
         * ---- */
        case SUBCMD_UPG_START:
        {
            /* Board identity check (always ETH for this firmware) */
            if (THIS_BOARD_ID != 0x00485445UL)
            {
                send_resp(seq, UPG_WRONG_BOARD);
                return;
            }

            /* Need: subcmd(1) + file_size(4) + cic_hdr(16) = 21 bytes after frame hdr */
            if (len < 27U)
            {
                send_resp(seq, UPG_SIZE_ERROR);
                return;
            }

            uint32_t fsz = rd_le32(&buf[7]);
            if (fsz == 0U || fsz > W25Q_FLASH_SIZE)
            {
                send_resp(seq, UPG_SIZE_ERROR);
                return;
            }

            /* --- Decrypt .cic header block (independent CBC, fresh IV copy) --- */
            uint8_t block[16];
            uint8_t iv_copy[16];
            memcpy(block,   &buf[11], 16U);   /* encrypted header from .cic[0..15] */
            memcpy(iv_copy, IV,       16U);   /* fresh copy of global IV */
            Aes_IV_key256bit_Decode(iv_copy, block, Key);
            /* plaintext: magic(4) | board_id(4) | fw_sn(4) | crc32(4) */

            uint32_t fw_magic = rd_le32(&block[0]);
            uint32_t fw_board = rd_le32(&block[4]);
            uint32_t fw_sn    = rd_le32(&block[8]);
            uint32_t fw_crc   = rd_le32(&block[12]);

            if (fw_magic != FW_ID_MAGIC)
            {
                send_resp(seq, UPG_BAD_FW);
                return;
            }

            if (fw_board != THIS_BOARD_ID)
            {
                send_resp(seq, UPG_WRONG_BOARD);
                return;
            }

            if (fw_sn != FW_SN_WILDCARD && fw_sn != g_device_sn)
            {
                send_resp(seq, UPG_WRONG_SN);
                return;
            }

            if (crc32(block, 12U) != fw_crc)
            {
                send_resp(seq, UPG_BAD_FW);
                return;
            }

            /* --- All checks passed: erase required sectors (~300ms each) --- */
            uint32_t sectors = (fsz + W25Q_SECTOR_SIZE - 1U) / W25Q_SECTOR_SIZE;
            for (uint32_t i = 0U; i < sectors; i++)
            {
                if (BSP_W25Q_EraseSector4K(UPG_W25Q_ADDR + i * W25Q_SECTOR_SIZE) != W25Q_OK)
                {
                    s_state = UPG_IDLE;
                    send_resp(seq, UPG_WRITE_ERROR);
                    return;
                }
            }

            s_file_size = fsz;
            s_rx_bytes  = 0U;
            s_state     = UPG_ACTIVE;
            send_resp(seq, UPG_OK);
            break;
        }

        /* ---- DATA: write chunk to W25Q then ACK ---- */
        case SUBCMD_UPG_DATA:
        {
            if (s_state != UPG_ACTIVE)
            {
                send_resp(seq, UPG_VERIFY_ERROR);
                return;
            }

            /* payload = offset(4) + data(N); minimum 1 byte of data */
            if (len < 12U)
            {
                send_resp(seq, UPG_SIZE_ERROR);
                return;
            }

            uint32_t       offset   = rd_le32(&buf[7]);
            uint16_t       data_len = (uint16_t)(len - 11U);
            const uint8_t *data     = &buf[11];

            if (data_len == 0U || offset + data_len > s_file_size)
            {
                send_resp(seq, UPG_SIZE_ERROR);
                return;
            }

            if (BSP_W25Q_Write(UPG_W25Q_ADDR + offset, data, data_len) != W25Q_OK)
            {
                s_state = UPG_IDLE;
                send_resp(seq, UPG_WRITE_ERROR);
                return;
            }

            s_rx_bytes += data_len;
            send_resp(seq, UPG_OK);
            break;
        }

        /* ---- END: verify total then ACK + reboot ---- */
        case SUBCMD_UPG_END:
        {
            if (s_state != UPG_ACTIVE)
            {
                send_resp(seq, UPG_VERIFY_ERROR);
                return;
            }

            if (len < 11U)
            {
                send_resp(seq, UPG_SIZE_ERROR);
                return;
            }

            uint32_t reported = rd_le32(&buf[7]);

            if (s_rx_bytes != reported || s_rx_bytes != s_file_size)
            {
                s_state = UPG_IDLE;
                send_resp(seq, UPG_VERIFY_ERROR);
                return;
            }

            s_state = UPG_IDLE;
            send_resp(seq, UPG_OK);
            /* Deferred reboot: let the main loop run LWIP_APP_Poll() once more
               so the ACK frame is actually flushed before the MCU resets. */
            s_reboot_pending = 1U;
            break;
        }

        default:
            break;
    }
}

/* =========================================================
 * UPG_PollReboot — call from main while(1) after LWIP_APP_Poll().
 * Waits until the TCP ACK frame has been flushed, then resets.
 * ========================================================= */
void UPG_PollReboot(void)
{
    if (!s_reboot_pending)
        return;

    /* One more lwIP poll cycle has already run (caller is in the main loop).
       A short delay lets the ETH DMA finish transmitting the ACK segment. */
    HAL_Delay(200U);
    HAL_NVIC_SystemReset();
}
