/**
 * iap.c - ALM_CIC_Bootloader IAP logic
 *
 * Consumes a .gt file placed at W25Q[BL_W25Q_GT_ADDR] by the ALM_CIC_APP
 * network receiver. The .gt format is exactly what Copy_Bin produces:
 *
 *   [  0.. 15]  16B encrypted FW_ID header  (independent CBC, iv = IV[])
 *   [ 16.. 31]  16B encrypted userstr block  (chain CBC, iv = IV[] fresh copy)
 *   [ 32..end]  encrypted firmware blocks    (chain continues)
 *
 * Detects "upgrade pending" by decrypting the header and checking the magic.
 * After a successful or rejected upgrade, erases the .gt's first sector so
 * the next boot goes straight to the app.
 */
#include "iap.h"
#include "aes.h"
#include "cpu_id.h"
#include "bsp_w25q16.h"
#include <string.h>

/* ---- helpers: forward declarations ---- */
static void     bl_jump_to_app          (uint32_t app_base);
static HAL_StatusTypeDef bl_erase_app_region (uint32_t fw_size);
static HAL_StatusTypeDef bl_decrypt_and_program(uint32_t total_blocks, uint8_t *iv_chain);
static uint32_t bl_crc32               (const uint8_t *buf, uint32_t len);

/* ---- helper: read 4 bytes little-endian from a byte array ---- */
static inline uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* ===== BL_Run ===== */

void BL_Run(void)
{
    uint8_t  hdr[16];       /* encrypted FW_ID header          */
    uint8_t  blk[16];       /* encrypted payload block 0       */
    uint8_t  iv_hdr[16];    /* independent IV for header       */
    uint8_t  iv_pay[16];    /* IV for payload CBC chain        */
    uint32_t fw_magic, fw_board, fw_sn, hdr_crc;
    uint32_t fw_size, total_blocks;

    /* ---- 1. read encrypted FW_ID header ---- */
    if (BSP_W25Q_Read(BL_W25Q_GT_ADDR, hdr, 16U) != W25Q_OK)
    {
        bl_jump_to_app(BL_APP_START_ADDR);
        return;
    }

    /* ---- 2. decrypt header (independent CBC — own IV copy) ---- */
    memcpy(iv_hdr, IV, 16U);
    Aes_IV_key256bit_Decode(iv_hdr, hdr, Key);

    fw_magic = rd_le32(&hdr[0]);
    fw_board = rd_le32(&hdr[4]);
    fw_sn    = rd_le32(&hdr[8]);
    hdr_crc  = rd_le32(&hdr[12]);

    /* ---- 3. if magic wrong → no .gt on W25Q → jump to existing app ---- */
    if (fw_magic != FW_ID_MAGIC)
    {
        bl_jump_to_app(BL_APP_START_ADDR);
        return;
    }

    /* ---- 4. FW_ID checks: board, SN, CRC ---- */
    {
        uint8_t ok = 1U;

        if (fw_board != BL_BOARD_ID)
            ok = 0U;

        if (ok)
        {
            if (fw_sn != FW_SN_WILDCARD && fw_sn != g_device_sn)
                ok = 0U;
        }

        if (ok)
        {
            if (bl_crc32(hdr, 12U) != hdr_crc)
                ok = 0U;
        }

        if (!ok)
        {
            /* Erase the .gt so we don't repeatedly reject on every boot */
            (void)BSP_W25Q_EraseSector4K(BL_W25Q_GT_ADDR);
            bl_jump_to_app(BL_APP_START_ADDR);
            return;
        }
    }

    /* ---- 5. decrypt payload block-0 → extract filesize ---- */
    if (BSP_W25Q_Read(BL_W25Q_GT_ADDR + 16U, blk, 16U) != W25Q_OK)
    {
        (void)BSP_W25Q_EraseSector4K(BL_W25Q_GT_ADDR);
        bl_jump_to_app(BL_APP_START_ADDR);
        return;
    }

    /* Payload chain starts from a fresh copy of global IV (independent of
       header's iv_hdr).  Calling Decode() updates iv_pay for next block. */
    memcpy(iv_pay, IV, 16U);
    Aes_IV_key256bit_Decode(iv_pay, blk, Key);

    /* plaintext block-0: userstr_len(4) | "LEDFW012"(8) | filesize(4) */
    fw_size = rd_le32(&blk[12]);

    if (fw_size == 0U || fw_size > BL_MAX_FW_SIZE)
    {
        (void)BSP_W25Q_EraseSector4K(BL_W25Q_GT_ADDR);
        bl_jump_to_app(BL_APP_START_ADDR);
        return;
    }

    /* ---- 6. erase internal flash app region ---- */
    if (bl_erase_app_region(fw_size) != HAL_OK)
    {
        (void)BSP_W25Q_EraseSector4K(BL_W25Q_GT_ADDR);
        bl_jump_to_app(BL_APP_START_ADDR);
        return;
    }

    /* ---- 7. decrypt + program firmware (W25Q offset 32 onward) ---- */
    total_blocks = (fw_size + 15U) >> 4;   /* ceil(fw_size / 16) */

    if (bl_decrypt_and_program(total_blocks, iv_pay) != HAL_OK)
    {
        /* App region may be partially written — erasing won't help much,
           but clear the .gt so user can retry with a fresh upload. */
        (void)BSP_W25Q_EraseSector4K(BL_W25Q_GT_ADDR);
        bl_jump_to_app(BL_APP_START_ADDR);
        return;
    }

    /* ---- 8. clear .gt (erase first sector) — no re-flash on next boot ---- */
    (void)BSP_W25Q_EraseSector4K(BL_W25Q_GT_ADDR);

    /* ---- 9. jump to freshly programmed app ---- */
    bl_jump_to_app(BL_APP_START_ADDR);
}

/* ===== erase internal flash sectors [BL_APP_START_ADDR, +fw_size) =====
 *
 * STM32H563: bank 1 = 0x08000000..0x0807FFFF, bank 2 = 0x08080000..0x080FFFFF
 * Each bank has 64 sectors of 8 KB.
 */
static HAL_StatusTypeDef bl_erase_app_region(uint32_t fw_size)
{
    HAL_StatusTypeDef rc = HAL_OK;
    uint32_t addr;
    uint32_t end_addr = BL_APP_START_ADDR + ((fw_size + 15U) & ~15U);

    HAL_FLASH_Unlock();

    for (addr = BL_APP_START_ADDR; addr < end_addr; addr += 0x2000UL)
    {
        FLASH_EraseInitTypeDef e = {0};
        uint32_t pe = 0xFFFFFFFFUL;

        if (addr < 0x08080000UL)
        {
            e.Banks  = FLASH_BANK_1;
            e.Sector = (addr - 0x08000000UL) >> 13;
        }
        else
        {
            e.Banks  = FLASH_BANK_2;
            e.Sector = (addr - 0x08080000UL) >> 13;
        }
        e.TypeErase = FLASH_TYPEERASE_SECTORS;
        e.NbSectors = 1;

        if (HAL_FLASHEx_Erase(&e, &pe) != HAL_OK)
        {
            rc = HAL_ERROR;
            break;
        }
    }

    HAL_FLASH_Lock();
    return rc;
}

/* ===== decrypt + program loop =====
 *
 * Firmware starts at W25Q offset 32.  iv_chain carries the CBC state that
 * was left after decoding the userstr block-0 (so the chain is continuous).
 * Each 16B block: read from W25Q, Decode (updates iv_chain), write QUADWORD.
 */
static HAL_StatusTypeDef bl_decrypt_and_program(uint32_t total_blocks,
                                                 uint8_t *iv_chain)
{
    HAL_StatusTypeDef rc = HAL_OK;
    uint8_t  block[16] __attribute__((aligned(8)));
    uint32_t flash_addr = BL_APP_START_ADDR;
    uint32_t w25q_addr  = BL_W25Q_GT_ADDR + 32U;   /* skip 16B hdr + 16B blk-0 */
    uint32_t i;

    HAL_FLASH_Unlock();

    for (i = 0U; i < total_blocks; i++)
    {
        if (BSP_W25Q_Read(w25q_addr, block, 16U) != W25Q_OK)
        {
            rc = HAL_ERROR;
            break;
        }

        /* Decode in place; iv_chain advances for the next iteration. */
        Aes_IV_key256bit_Decode(iv_chain, block, Key);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,
                              flash_addr,
                              (uint32_t)block) != HAL_OK)
        {
            rc = HAL_ERROR;
            break;
        }

        flash_addr += 16U;
        w25q_addr  += 16U;
    }

    HAL_FLASH_Lock();
    return rc;
}

/* ===== CRC32/ISO-HDLC (poly 0xEDB88320, init 0xFFFFFFFF, final ^0xFFFFFFFF)
 *
 * Bit-by-bit — same result as the 256-entry table approach used in
 * Copy_Bin (C#) and G0B1's CRC32Calculate().  Only called once over 12 B.
 */
static uint32_t bl_crc32(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    while (len--)
    {
        crc ^= *buf++;
        uint8_t bit;
        for (bit = 0U; bit < 8U; bit++)
            crc = (crc & 1U) ? (0xEDB88320UL ^ (crc >> 1)) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* ===== jump to app (Cortex-M33 standard sequence) ===== */
static void bl_jump_to_app(uint32_t app_base)
{
    uint32_t app_msp   = *(volatile uint32_t *)(app_base);
    uint32_t app_reset = *(volatile uint32_t *)(app_base + 4U);
    uint32_t i;

    /* MSP must be in SRAM (0x20000000..0x200FFFFF on H5xx) */
    if ((app_msp & 0xFFF00000UL) != 0x20000000UL)
    {
        while (1) { __NOP(); }
    }

    /* Reset handler must be inside the app flash region (bit 0 = Thumb flag,
       strip it before the range check). Catches blank/partially-erased flash. */
    if ((app_reset & ~1UL) < BL_APP_START_ADDR ||
        (app_reset & ~1UL) >= BL_APP_END_ADDR)
    {
        while (1) { __NOP(); }
    }

    __disable_irq();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }

    HAL_ICACHE_Disable();

    SCB->VTOR = app_base;
    __DSB();
    __ISB();

    __set_MSP(app_msp);

    /* Re-enable interrupts: the app's HAL_Init() will configure SysTick and
       expects PRIMASK = 0 so SysTick_Handler can fire. Without this, HAL
       tick stays 0 forever and all HAL_Delay / timeouts break. */
    __enable_irq();

    ((void (*)(void))app_reset)();

    while (1) { __NOP(); }
}
