/**
 * iap_handler.c - In-Application Programming protocol handler
 *
 * Receives firmware data from PC via TCP (CMD_IAP frames),
 * writes it to external SPI Flash, verifies CRC32, writes
 * upgrade metadata, and triggers system reset.
 *
 * State machine:
 *   IDLE -> [BEGIN] -> RECEIVING -> [DATA x N] -> [END] -> VERIFYING
 *        -> VERIFIED -> [REBOOT] -> system reset
 */

#include "iap_handler.h"
#include "bsp_ext_flash.h"
#include "bsp_crc.h"
#include "eth_send_queue.h"
#include "comm_protocol.h"
#include <string.h>

/* ---- State machine ---- */
typedef enum {
    IAP_STATE_IDLE = 0,
    IAP_STATE_RECEIVING,
    IAP_STATE_VERIFYING,
    IAP_STATE_VERIFIED,
} IAP_State_t;

static IAP_State_t g_iap_state = IAP_STATE_IDLE;

/* ---- Session data ---- */
static uint32_t g_fw_size     = 0;
static uint32_t g_fw_crc32    = 0;
static uint32_t g_fw_version  = 0;
static uint32_t g_next_offset = 0;
static uint32_t g_last_erased_block = 0xFFFFFFFF;

/* ---- ACK frame builder ---- */
static void send_ack(uint16_t seq, uint8_t status, uint32_t next_offset)
{
    uint8_t tx[PROTO_FRAME_LEN] = {0};

    tx[0] = PROTO_FRAME_HEAD;
    tx[1] = CMD_IAP;
    tx[2] = DIR_MCU_TO_PC;
    tx[3] = (uint8_t)(seq & 0xFF);
    tx[4] = (uint8_t)(seq >> 8);
    tx[5] = 8;                          /* payload length */
    tx[6] = IAP_SUBCMD_ACK;
    tx[7] = status;
    tx[8] = (uint8_t)(next_offset);
    tx[9] = (uint8_t)(next_offset >> 8);
    tx[10] = (uint8_t)(next_offset >> 16);
    tx[11] = (uint8_t)(next_offset >> 24);

    ETH_Send_Queue(tx, PROTO_FRAME_LEN);
}

/* ---- Erase external flash sectors covering [addr, addr+len) ---- */
static HAL_StatusTypeDef erase_if_needed(uint32_t addr, uint32_t len)
{
    /* Erase in 4KB sectors as data arrives */
    uint32_t end = addr + len;
    uint32_t sector_start = addr & ~(EXTFLASH_SECTOR_SIZE_4K - 1U);

    while (sector_start < end)
    {
        if (sector_start != g_last_erased_block)
        {
            HAL_StatusTypeDef rc = ExtFlash_SectorErase4K(sector_start);
            if (rc != HAL_OK)
                return rc;
            g_last_erased_block = sector_start;
        }
        sector_start += EXTFLASH_SECTOR_SIZE_4K;
    }
    return HAL_OK;
}

/* ---- Write metadata to external flash ---- */
static HAL_StatusTypeDef write_metadata(void)
{
    IAP_Meta_t meta;
    memset(&meta, 0, sizeof(meta));

    meta.magic      = IAP_META_MAGIC;
    meta.fw_size    = g_fw_size;
    meta.fw_crc32   = g_fw_crc32;
    meta.fw_version = g_fw_version;

    /* Erase the metadata sector */
    HAL_StatusTypeDef rc = ExtFlash_SectorErase4K(IAP_META_ADDR);
    if (rc != HAL_OK)
        return rc;

    /* Write metadata (32 bytes fits in one page) */
    return ExtFlash_PageProgram(IAP_META_ADDR, (const uint8_t *)&meta,
                                sizeof(meta));
}

/* ---- Verify CRC32 of the entire firmware in ext flash ---- */
static uint8_t verify_firmware(void)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t remaining = g_fw_size;
    uint32_t addr = IAP_FW_BASE_ADDR;
    uint8_t buf[256];

    while (remaining > 0)
    {
        uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;

        if (ExtFlash_Read(addr, buf, chunk) != HAL_OK)
            return IAP_STATUS_FLASH_ERR;

        /* Inline CRC32 update (same polynomial as BSP_CRC32_Calc) */
        for (uint32_t i = 0; i < chunk; i++)
        {
            crc ^= buf[i];
            for (uint8_t j = 0; j < 8; j++)
            {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320UL;
                else
                    crc >>= 1;
            }
        }

        addr      += chunk;
        remaining -= chunk;
    }

    crc = ~crc;

    if (crc != g_fw_crc32)
        return IAP_STATUS_CRC_ERR;

    return IAP_STATUS_OK;
}

/* ---- Command handler ---- */

/**
 * IAP_Process_Command - handle IAP sub-commands from PC
 *
 * Frame format (64 bytes):
 *   [0] = 0xA5 head
 *   [1] = CMD_IAP (0x30)
 *   [2] = direction
 *   [3..4] = seq (LE)
 *   [5] = payload length
 *   [6] = sub-command
 *   [7+] = sub-command data
 */
void IAP_Process_Command(const uint8_t *buf, uint16_t len, uint16_t seq)
{
    uint8_t subcmd = buf[6];

    switch (subcmd)
    {
    /* ---- IAP_BEGIN: start new firmware transfer ---- */
    case IAP_SUBCMD_BEGIN:
    {
        if (g_iap_state != IAP_STATE_IDLE && g_iap_state != IAP_STATE_VERIFIED)
        {
            send_ack(seq, IAP_STATUS_BUSY, 0);
            return;
        }

        /* Parse: [7..10] fw_size, [11..14] fw_crc32, [15..18] fw_version */
        g_fw_size = (uint32_t)buf[7]  | ((uint32_t)buf[8] << 8) |
                    ((uint32_t)buf[9] << 16) | ((uint32_t)buf[10] << 24);
        g_fw_crc32 = (uint32_t)buf[11] | ((uint32_t)buf[12] << 8) |
                     ((uint32_t)buf[13] << 16) | ((uint32_t)buf[14] << 24);
        g_fw_version = (uint32_t)buf[15] | ((uint32_t)buf[16] << 8) |
                       ((uint32_t)buf[17] << 16) | ((uint32_t)buf[18] << 24);

        if (g_fw_size == 0 || g_fw_size > IAP_FW_MAX_SIZE)
        {
            send_ack(seq, IAP_STATUS_SIZE_ERR, 0);
            return;
        }

        /* Init external flash driver */
        if (ExtFlash_Init() != HAL_OK)
        {
            send_ack(seq, IAP_STATUS_FLASH_ERR, 0);
            return;
        }

        g_next_offset = 0;
        g_last_erased_block = 0xFFFFFFFF;
        g_iap_state = IAP_STATE_RECEIVING;

        send_ack(seq, IAP_STATUS_OK, 0);
        break;
    }

    /* ---- IAP_DATA: receive a firmware data chunk ---- */
    case IAP_SUBCMD_DATA:
    {
        if (g_iap_state != IAP_STATE_RECEIVING)
        {
            send_ack(seq, IAP_STATUS_SEQ_ERR, g_next_offset);
            return;
        }

        /* Parse: [7..10] offset, [11] chunk_len, [12..13] crc16, [14+] data */
        uint32_t offset = (uint32_t)buf[7]  | ((uint32_t)buf[8] << 8) |
                          ((uint32_t)buf[9] << 16) | ((uint32_t)buf[10] << 24);
        uint8_t  chunk_len = buf[11];
        uint16_t crc16_recv = (uint16_t)buf[12] | ((uint16_t)buf[13] << 8);

        /* Validate offset matches expected */
        if (offset != g_next_offset)
        {
            send_ack(seq, IAP_STATUS_SEQ_ERR, g_next_offset);
            return;
        }

        /* Validate chunk size */
        if (chunk_len == 0 || chunk_len > 48 ||
            (offset + chunk_len) > g_fw_size)
        {
            send_ack(seq, IAP_STATUS_SIZE_ERR, g_next_offset);
            return;
        }

        /* Verify per-chunk CRC16 */
        uint16_t crc16_calc = BSP_CRC16_CCITT(&buf[14], chunk_len);
        if (crc16_calc != crc16_recv)
        {
            send_ack(seq, IAP_STATUS_CRC_ERR, g_next_offset);
            return;
        }

        /* Erase sector if needed, then program */
        uint32_t flash_addr = IAP_FW_BASE_ADDR + offset;

        if (erase_if_needed(flash_addr, chunk_len) != HAL_OK)
        {
            g_iap_state = IAP_STATE_IDLE;
            send_ack(seq, IAP_STATUS_FLASH_ERR, g_next_offset);
            return;
        }

        /* Write data — page-aligned writes within a single page */
        uint32_t written = 0;
        while (written < chunk_len)
        {
            uint32_t wa = flash_addr + written;
            uint32_t page_remain = EXTFLASH_PAGE_SIZE -
                                   (wa % EXTFLASH_PAGE_SIZE);
            uint32_t to_write = chunk_len - written;
            if (to_write > page_remain)
                to_write = page_remain;

            if (ExtFlash_PageProgram(wa, &buf[14 + written],
                                     to_write) != HAL_OK)
            {
                g_iap_state = IAP_STATE_IDLE;
                send_ack(seq, IAP_STATUS_FLASH_ERR, g_next_offset);
                return;
            }
            written += to_write;
        }

        g_next_offset = offset + chunk_len;
        send_ack(seq, IAP_STATUS_OK, g_next_offset);
        break;
    }

    /* ---- IAP_END: data transfer complete, verify full image ---- */
    case IAP_SUBCMD_END:
    {
        if (g_iap_state != IAP_STATE_RECEIVING)
        {
            send_ack(seq, IAP_STATUS_SEQ_ERR, g_next_offset);
            return;
        }

        if (g_next_offset != g_fw_size)
        {
            send_ack(seq, IAP_STATUS_SIZE_ERR, g_next_offset);
            return;
        }

        g_iap_state = IAP_STATE_VERIFYING;

        uint8_t result = verify_firmware();

        if (result != IAP_STATUS_OK)
        {
            g_iap_state = IAP_STATE_IDLE;
            send_ack(seq, result, g_next_offset);
            return;
        }

        /* Write upgrade metadata to ext flash */
        if (write_metadata() != HAL_OK)
        {
            g_iap_state = IAP_STATE_IDLE;
            send_ack(seq, IAP_STATUS_FLASH_ERR, g_next_offset);
            return;
        }

        g_iap_state = IAP_STATE_VERIFIED;
        send_ack(seq, IAP_STATUS_OK, g_next_offset);
        break;
    }

    /* ---- IAP_REBOOT: reset MCU to enter bootloader ---- */
    case IAP_SUBCMD_REBOOT:
    {
        if (g_iap_state != IAP_STATE_VERIFIED)
        {
            send_ack(seq, IAP_STATUS_SEQ_ERR, g_next_offset);
            return;
        }

        /* Send ACK before reset */
        send_ack(seq, IAP_STATUS_OK, g_next_offset);

        /* Short delay to let the ACK frame be transmitted */
        HAL_Delay(100);

        /* System reset */
        NVIC_SystemReset();
        break;
    }

    default:
        break;
    }
}
