/**
 * iap.c - ALM_Motor_Bootloader IAP logic
 *
 * Reads encrypted .mot from internal flash STAGING region (no W25Q on Motor),
 * validates header (magic + "MOT" board_id + CRC32), decrypts CBC-chain
 * payload and programs it into APP region. Mirrors ALM_Bootloader/BSP/iap.c
 * with G0 flash semantics (page=2 KB, program=DOUBLEWORD=8 B).
 */
#include "iap.h"
#include "aes.h"
#include "motor_partition.h"
#include <string.h>

static void              bl_jump_to_app          (uint32_t app_base);
static HAL_StatusTypeDef bl_erase_app_region     (uint32_t fw_size);
static HAL_StatusTypeDef bl_erase_staging        (void);
static HAL_StatusTypeDef bl_decrypt_and_program  (uint32_t total_blocks, uint8_t *iv_chain);
static uint32_t          bl_crc32                (const uint8_t *buf, uint32_t len);

static inline uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

void BL_Run(void)
{
    uint8_t  hdr[16], blk[16], iv_hdr[16], iv_pay[16];
    uint32_t fw_magic, fw_board, fw_sn, hdr_crc;
    uint32_t fw_size, total_blocks;

    /* Staging is internal flash — directly memcpy from MOT_STAGE_BASE */
    memcpy(hdr, (const void *)MOT_STAGE_BASE, 16U);

    memcpy(iv_hdr, IV, 16U);
    Aes_IV_key256bit_Decode(iv_hdr, hdr, Key);

    fw_magic = rd_le32(&hdr[0]);
    fw_board = rd_le32(&hdr[4]);
    fw_sn    = rd_le32(&hdr[8]);
    hdr_crc  = rd_le32(&hdr[12]);

    /* No magic → no pending upgrade. Run existing app. */
    if (fw_magic != MOT_FW_ID_MAGIC)
    {
        bl_jump_to_app(MOT_APP_BASE);
        return;
    }

    /* Board ID must be "MOT". CRC32 over magic|board|sn must match. */
    {
        uint8_t ok = 1U;
        if (fw_board != MOT_BOARD_ID)                          ok = 0U;
        if (ok && fw_sn != MOT_FW_SN_WILDCARD)                 ok = 0U;
        /* Optional per-MCU lock — not used by Motor for now;
           accept only wildcard, otherwise reject. */
        if (ok && bl_crc32(hdr, 12U) != hdr_crc)               ok = 0U;

        if (!ok)
        {
            (void)bl_erase_staging();
            bl_jump_to_app(MOT_APP_BASE);
            return;
        }
    }

    /* Decrypt block-0 to recover filesize */
    memcpy(blk, (const void *)(MOT_STAGE_BASE + 16U), 16U);
    memcpy(iv_pay, IV, 16U);
    Aes_IV_key256bit_Decode(iv_pay, blk, Key);
    fw_size = rd_le32(&blk[12]);

    if (fw_size == 0U || fw_size > MOT_APP_SIZE)
    {
        (void)bl_erase_staging();
        bl_jump_to_app(MOT_APP_BASE);
        return;
    }

    if (bl_erase_app_region(fw_size) != HAL_OK)
    {
        (void)bl_erase_staging();
        bl_jump_to_app(MOT_APP_BASE);
        return;
    }

    total_blocks = (fw_size + 15U) >> 4;
    if (bl_decrypt_and_program(total_blocks, iv_pay) != HAL_OK)
    {
        (void)bl_erase_staging();
        bl_jump_to_app(MOT_APP_BASE);
        return;
    }

    (void)bl_erase_staging();
    bl_jump_to_app(MOT_APP_BASE);
}

/* ===== STM32G0B1 flash erase: page = 2 KB, single bank. ===== */
static HAL_StatusTypeDef bl_erase_app_region(uint32_t fw_size)
{
    HAL_StatusTypeDef rc = HAL_OK;
    uint32_t aligned_size = (fw_size + (MOT_FLASH_SECTOR_SIZE - 1U))
                          & ~(MOT_FLASH_SECTOR_SIZE - 1U);
    uint32_t pages = aligned_size / MOT_FLASH_SECTOR_SIZE;
    uint32_t first_page = (MOT_APP_BASE - MOT_FLASH_BASE) / MOT_FLASH_SECTOR_SIZE;

    HAL_FLASH_Unlock();
    {
        FLASH_EraseInitTypeDef e = {0};
        uint32_t pe = 0xFFFFFFFFUL;
        e.TypeErase = FLASH_TYPEERASE_PAGES;
        e.Banks     = FLASH_BANK_1;
        e.Page      = first_page;
        e.NbPages   = pages;
        if (HAL_FLASHEx_Erase(&e, &pe) != HAL_OK) rc = HAL_ERROR;
    }
    HAL_FLASH_Lock();
    return rc;
}

static HAL_StatusTypeDef bl_erase_staging(void)
{
    HAL_StatusTypeDef rc = HAL_OK;
    uint32_t pages = MOT_STAGE_SIZE / MOT_FLASH_SECTOR_SIZE;
    uint32_t first_page = (MOT_STAGE_BASE - MOT_FLASH_BASE) / MOT_FLASH_SECTOR_SIZE;

    HAL_FLASH_Unlock();
    {
        FLASH_EraseInitTypeDef e = {0};
        uint32_t pe = 0xFFFFFFFFUL;
        e.TypeErase = FLASH_TYPEERASE_PAGES;
        e.Banks     = FLASH_BANK_1;
        e.Page      = first_page;
        e.NbPages   = pages;
        if (HAL_FLASHEx_Erase(&e, &pe) != HAL_OK) rc = HAL_ERROR;
    }
    HAL_FLASH_Lock();
    return rc;
}

/* Decrypt AES blocks from STAGING and write to APP. G0 program unit is
 * DOUBLEWORD (64-bit), so each 16-B AES block is 2 doubleword writes. */
static HAL_StatusTypeDef bl_decrypt_and_program(uint32_t total_blocks,
                                                uint8_t *iv_chain)
{
    HAL_StatusTypeDef rc = HAL_OK;
    uint8_t  block[16];
    uint32_t flash_addr = MOT_APP_BASE;
    uint32_t stage_addr = MOT_STAGE_BASE + 32U;   /* skip 16B hdr + 16B blk-0 */
    uint32_t i;

    HAL_FLASH_Unlock();

    for (i = 0U; i < total_blocks; i++)
    {
        uint64_t dw0, dw1;

        memcpy(block, (const void *)stage_addr, 16U);
        Aes_IV_key256bit_Decode(iv_chain, block, Key);

        memcpy(&dw0, &block[0], 8U);
        memcpy(&dw1, &block[8], 8U);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                              flash_addr,      dw0) != HAL_OK ||
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                              flash_addr + 8U, dw1) != HAL_OK)
        {
            rc = HAL_ERROR;
            break;
        }

        flash_addr += 16U;
        stage_addr += 16U;
    }

    HAL_FLASH_Lock();
    return rc;
}

static uint32_t bl_crc32(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    while (len--)
    {
        crc ^= *buf++;
        for (uint8_t bit = 0U; bit < 8U; bit++)
            crc = (crc & 1U) ? (0xEDB88320UL ^ (crc >> 1)) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* Cortex-M0+: VTOR exists, MSP set via __set_MSP. */
static void bl_jump_to_app(uint32_t app_base)
{
    uint32_t app_msp   = *(volatile uint32_t *)(app_base);
    uint32_t app_reset = *(volatile uint32_t *)(app_base + 4U);

    /* MSP must be in SRAM (0x20000000..0x20023FFF on G0B1) */
    if ((app_msp & 0xFFF00000UL) != 0x20000000UL)
        while (1) { __NOP(); }

    /* Reset handler must be inside the app flash region */
    if ((app_reset & ~1UL) <  MOT_APP_BASE ||
        (app_reset & ~1UL) >= MOT_APP_END)
        while (1) { __NOP(); }

    __disable_irq();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* G0 has 1 NVIC ICER/ICPR pair (32 IRQs total) */
    NVIC->ICER[0] = 0xFFFFFFFFUL;
    NVIC->ICPR[0] = 0xFFFFFFFFUL;

    SCB->VTOR = app_base;
    __DSB();
    __ISB();

    __set_MSP(app_msp);
    __enable_irq();

    ((void (*)(void))app_reset)();

    while (1) { __NOP(); }
}
