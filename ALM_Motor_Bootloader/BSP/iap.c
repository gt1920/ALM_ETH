/**
 * iap.c - ALM_Motor_Bootloader IAP logic
 *
 * Reads encrypted .mot from internal flash STAGING region, validates header
 * (magic + "MOT" board_id + CRC32) and a payload CRC32 metadata block,
 * decrypts CBC-chain payload and programs it into APP region. Mirrors
 * ALM_CIC_Bootloader/BSP/iap.c with G0 flash semantics (page=2 KB,
 * program=DOUBLEWORD=8 B).
 *
 * Brick-safety design (see commit fixing OTA dead-zones):
 *   - Failure paths never erase STAGING. Only a fully-verified flash succeeds
 *     in clearing the staging slot. A transient program/erase fault, a power
 *     loss mid-program, or a payload CRC mismatch all leave STAGING intact so
 *     the next BL boot re-tries from the same .mot.
 *   - PASS 1 verifies the entire decrypted payload's CRC32 BEFORE touching
 *     APP flash. A bad .mot can never erase a working APP.
 *   - PASS 2 re-reads APP flash after program and re-checks CRC32. Detects
 *     silent flash corruption / partial program.
 *   - Any post-erase failure triggers NVIC_SystemReset rather than jumping to
 *     a half-written APP (which would deadlock the BL's MSP-sanity guard).
 */
#include "iap.h"
#include "aes.h"
#include "motor_partition.h"
#include "bl_can_recovery.h"
#include <string.h>

/* Plaintext magic at the head of the CRC metadata block (chain CBC, third
   16B block of the .mot, immediately after the hdr-CRC and the metadata-0
   block). Stamped by Copy_Bin_Motor_Project. Mismatch → refuse to flash. */
#define MOT_CRC_BLOCK_MAGIC     0x4D4F5443UL    /* "MOTC" LE */

/* Layout offsets inside STAGING. */
#define STAGE_OFF_FW_ID_HDR     0U      /* [ 0..15] encrypted FW_ID hdr        */
#define STAGE_OFF_META_BLOCK    16U     /* [16..31] encrypted metadata block-0 */
#define STAGE_OFF_CRC_BLOCK     32U     /* [32..47] encrypted CRC block        */
#define STAGE_OFF_PAYLOAD       48U     /* [48..  ] encrypted firmware payload */

static void              bl_jump_to_app          (uint32_t app_base);
static HAL_StatusTypeDef bl_erase_app_region     (uint32_t fw_size);
static HAL_StatusTypeDef bl_erase_staging        (void);
static HAL_StatusTypeDef bl_decrypt_and_program  (uint32_t total_blocks, uint8_t *iv_chain);
static HAL_StatusTypeDef bl_verify_staging_crc   (uint32_t fw_size, uint8_t *iv_chain,
                                                  uint32_t expected_crc);
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
    uint8_t  hdr[16], blk[16], iv_hdr[16], iv_pay[16], iv_pay_saved[16];
    uint32_t fw_magic, fw_board, fw_sn, hdr_crc;
    uint32_t fw_size, total_blocks;
    uint32_t crc_magic, fw_crc32;

    /* Staging is internal flash — directly memcpy from MOT_STAGE_BASE */
    memcpy(hdr, (const void *)(MOT_STAGE_BASE + STAGE_OFF_FW_ID_HDR), 16U);

    memcpy(iv_hdr, IV, 16U);
    Aes_IV_key256bit_Decode(iv_hdr, hdr, Key);

    fw_magic = rd_le32(&hdr[0]);
    fw_board = rd_le32(&hdr[4]);
    fw_sn    = rd_le32(&hdr[8]);
    hdr_crc  = rd_le32(&hdr[12]);
    (void)fw_sn;

    /* No magic → no pending upgrade. Run existing app. */
    if (fw_magic != MOT_FW_ID_MAGIC)
    {
        bl_jump_to_app(MOT_APP_BASE);
        return;
    }

    /* Wrong board / bad header CRC → leave APP and STAGING untouched.
       Existing app keeps running; user can overwrite STAGING with a correct
       .mot. (Old code erased STAGING here — removed: flash-read transients
       on the hdr block could destroy an otherwise-valid staging slot.) */
    if (fw_board != MOT_BOARD_ID || bl_crc32(hdr, 12U) != hdr_crc)
    {
        bl_jump_to_app(MOT_APP_BASE);
        return;
    }

    /* Decrypt metadata block-0 → filesize. Chain CBC starts here. */
    memcpy(blk, (const void *)(MOT_STAGE_BASE + STAGE_OFF_META_BLOCK), 16U);
    memcpy(iv_pay, IV, 16U);
    Aes_IV_key256bit_Decode(iv_pay, blk, Key);
    fw_size = rd_le32(&blk[12]);

    if (fw_size == 0U || fw_size > MOT_APP_SIZE)
    {
        bl_jump_to_app(MOT_APP_BASE);
        return;
    }

    /* Decrypt CRC metadata block → payload CRC32. Chain continues. */
    memcpy(blk, (const void *)(MOT_STAGE_BASE + STAGE_OFF_CRC_BLOCK), 16U);
    Aes_IV_key256bit_Decode(iv_pay, blk, Key);
    crc_magic = rd_le32(&blk[0]);
    fw_crc32  = rd_le32(&blk[4]);

    /* Pre-CRC .mot (built by an old Copy_Bin) → refuse: we cannot prove the
       payload is intact, so flashing it would risk a brick. User must rebuild
       the .mot with the new tool. */
    if (crc_magic != MOT_CRC_BLOCK_MAGIC)
    {
        bl_jump_to_app(MOT_APP_BASE);
        return;
    }

    /* Save chain IV — we'll walk the payload twice (verify, then program). */
    memcpy(iv_pay_saved, iv_pay, 16U);

    /* PASS 1 — verify payload CRC BEFORE touching APP flash.
       If this fails, APP is intact and STAGING is intact; user can re-stage. */
    if (bl_verify_staging_crc(fw_size, iv_pay, fw_crc32) != HAL_OK)
    {
        bl_jump_to_app(MOT_APP_BASE);
        return;
    }

    /* PASS 1 OK — payload is good. Restore IV for the program pass. */
    memcpy(iv_pay, iv_pay_saved, 16U);

    /* From here on, any failure means APP is partially modified. Do NOT erase
       STAGING and do NOT jump to a half-written APP — reset so the next boot
       re-walks BL_Run with the same staging slot. */
    if (bl_erase_app_region(fw_size) != HAL_OK)
    {
        NVIC_SystemReset();
    }

    total_blocks = (fw_size + 15U) >> 4;
    if (bl_decrypt_and_program(total_blocks, iv_pay) != HAL_OK)
    {
        NVIC_SystemReset();
    }

    /* PASS 2 — re-read programmed APP and re-check CRC32. Catches silent
       flash corruption (cell-stuck-at, partial program, etc.). */
    if (bl_crc32((const uint8_t *)MOT_APP_BASE, fw_size) != fw_crc32)
    {
        NVIC_SystemReset();
    }

    /* All checks passed → safe to clear staging and jump. */
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
    uint32_t stage_addr = MOT_STAGE_BASE + STAGE_OFF_PAYLOAD;
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

/* Decrypt staging payload block-by-block (without touching APP flash) and
 * compute CRC32 over the first `fw_size` plaintext bytes (excluding tail
 * padding inside the final 16-B block). Compare against expected_crc. */
static HAL_StatusTypeDef bl_verify_staging_crc(uint32_t fw_size,
                                                uint8_t *iv_chain,
                                                uint32_t expected_crc)
{
    uint32_t total_blocks = (fw_size + 15U) >> 4;
    uint32_t stage_addr   = MOT_STAGE_BASE + STAGE_OFF_PAYLOAD;
    uint32_t crc          = 0xFFFFFFFFUL;
    uint32_t bytes_left   = fw_size;
    uint8_t  block[16];
    uint32_t i;
    uint32_t b;

    for (i = 0U; i < total_blocks; i++)
    {
        uint32_t chunk;

        memcpy(block, (const void *)stage_addr, 16U);
        Aes_IV_key256bit_Decode(iv_chain, block, Key);

        chunk = (bytes_left >= 16U) ? 16U : bytes_left;
        for (b = 0U; b < chunk; b++)
        {
            uint8_t bit;
            crc ^= block[b];
            for (bit = 0U; bit < 8U; bit++)
                crc = (crc & 1U) ? (0xEDB88320UL ^ (crc >> 1)) : (crc >> 1);
        }
        bytes_left -= chunk;
        stage_addr += 16U;
    }

    crc ^= 0xFFFFFFFFUL;
    return (crc == expected_crc) ? HAL_OK : HAL_ERROR;
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

    /* MSP must be in SRAM (0x20000000..0x20023FFF on G0B1) and the Reset
       handler must point inside APP. If either is bad, APP is absent or
       corrupted and we have no valid image to jump to. Enter the brick-
       recovery CAN-FD listener and wait for the host to re-stage a .mot.
       BL_CanRecovery_RunForever never returns through normal control flow —
       it triggers NVIC_SystemReset on a complete .mot. */
    if ((app_msp & 0xFFF00000UL) != 0x20000000UL ||
        (app_reset & ~1UL) <  MOT_APP_BASE ||
        (app_reset & ~1UL) >= MOT_APP_END)
    {
        BL_CanRecovery_RunForever();
        /* Unreachable: recovery either system-resets or spins forever. */
    }

    /* Hold in BL for a fixed window before jumping to APP. Gives an operator
       time to attach SWD / J-Link before APP code starts running, regardless
       of whether this boot performed an upgrade. SysTick is already up
       (HAL_Init called from main), so HAL_Delay polls it correctly. */
    HAL_Delay(5000U);

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
