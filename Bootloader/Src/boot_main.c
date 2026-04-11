/**
 * boot_main.c - Bootloader for STM32H563VGTx IAP system
 *
 * Flow:
 *   1. Init HAL, SystemClock, OCTOSPI1
 *   2. Read IAP metadata from external flash (0x0F0000)
 *   3. If magic != IAP_META_MAGIC -> jump to App
 *   4. Read firmware from ext flash, compute CRC32
 *   5. If CRC mismatch -> clear metadata -> jump to App (keep old firmware)
 *   6. Erase internal flash App area
 *   7. Copy firmware from ext flash to internal flash
 *   8. Read-back verify
 *   9. Clear metadata (upgrade flag)
 *  10. Jump to App
 */

#include "main.h"
#include "octospi.h"
#include "boot_config.h"
#include "boot_ext_flash.h"
#include <string.h>

/* Forward declarations */
void SystemClock_Config(void);

/* ---- LED indicator (optional, PE0 if available) ---- */
#define BOOT_LED_PORT   GPIOE
#define BOOT_LED_PIN    GPIO_PIN_0

static void Boot_LED_Toggle(void)
{
    HAL_GPIO_TogglePin(BOOT_LED_PORT, BOOT_LED_PIN);
}

/* ---- CRC32 (same as BSP_CRC32_Calc) ---- */
uint32_t Boot_CRC32_Calc(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++)
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
    return ~crc;
}

/* ---- Jump to Application ---- */
void Boot_JumpToApp(uint32_t addr)
{
    uint32_t sp = *(volatile uint32_t *)addr;
    uint32_t pc = *(volatile uint32_t *)(addr + 4);

    /* Basic sanity: SP must be in SRAM range */
    if ((sp & 0xFFF00000) != 0x20000000)
        return;

    __disable_irq();

    /* Reset SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* Set vector table */
    SCB->VTOR = addr;

    /* Set MSP and jump */
    __set_MSP(sp);
    ((void (*)(void))pc)();
}

/* ---- Clear upgrade metadata in external flash ---- */
static HAL_StatusTypeDef clear_metadata(void)
{
    /* Erase the 4KB sector containing metadata, which zeros the magic */
    return BootFlash_SectorErase4K(IAP_META_ADDR);
}

/* ---- Erase internal flash App area ---- */
static HAL_StatusTypeDef erase_app_area(uint32_t fw_size)
{
    uint32_t num_sectors = (fw_size + INTERNAL_FLASH_SECTOR_SIZE - 1) /
                           INTERNAL_FLASH_SECTOR_SIZE;

    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error = 0;

    HAL_FLASH_Unlock();

    /* Determine which bank the App sectors are in.
       0x08010000 is in Bank 1 (sectors 8+) for H563 with 1MB.
       Bank 1: 0x08000000 - 0x0807FFFF (sectors 0-63)
       Bank 2: 0x08080000 - 0x080FFFFF (sectors 0-63)
       App at 0x08010000 = Bank 1, sector 8.
       If App extends beyond 0x08080000, we also need Bank 2. */

    /* First: erase Bank 1 sectors (from sector 8) */
    uint32_t bank1_max_sectors = 64 - APP_START_SECTOR; /* sectors 8..63 in Bank1 */
    uint32_t bank1_sectors = (num_sectors > bank1_max_sectors) ?
                              bank1_max_sectors : num_sectors;

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks     = FLASH_BANK_1;
    erase.Sector    = APP_START_SECTOR;
    erase.NbSectors = bank1_sectors;

    HAL_StatusTypeDef rc = HAL_FLASHEx_Erase(&erase, &sector_error);
    if (rc != HAL_OK)
    {
        HAL_FLASH_Lock();
        return rc;
    }

    /* If firmware exceeds Bank 1 capacity, erase Bank 2 sectors */
    if (num_sectors > bank1_max_sectors)
    {
        uint32_t bank2_sectors = num_sectors - bank1_max_sectors;
        /* Don't erase the last sector of Bank 2 (device config at 0x080FE000) */
        if (bank2_sectors > 63)
            bank2_sectors = 63;

        erase.Banks     = FLASH_BANK_2;
        erase.Sector    = 0;
        erase.NbSectors = bank2_sectors;

        rc = HAL_FLASHEx_Erase(&erase, &sector_error);
        if (rc != HAL_OK)
        {
            HAL_FLASH_Lock();
            return rc;
        }
    }

    HAL_FLASH_Lock();
    return HAL_OK;
}

/* ---- Copy firmware from external to internal flash ---- */
static HAL_StatusTypeDef copy_firmware(uint32_t fw_size)
{
    uint8_t buf[256]; /* read buffer */
    uint32_t remaining = fw_size;
    uint32_t ext_addr  = IAP_FW_BASE_ADDR;
    uint32_t int_addr  = APP_BASE_ADDR;

    HAL_FLASH_Unlock();

    while (remaining > 0)
    {
        uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;

        /* Pad to 16-byte alignment for quad-word programming */
        uint32_t padded = (chunk + 15U) & ~15U;

        /* Read from external flash */
        memset(buf, 0xFF, sizeof(buf));
        if (BootFlash_Read(ext_addr, buf, chunk) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }

        /* Program internal flash in 16-byte quad-words */
        for (uint32_t i = 0; i < padded; i += 16U)
        {
            HAL_StatusTypeDef rc = HAL_FLASH_Program(
                FLASH_TYPEPROGRAM_QUADWORD,
                int_addr + i,
                (uint32_t)&buf[i]);

            if (rc != HAL_OK)
            {
                HAL_FLASH_Lock();
                return rc;
            }
        }

        ext_addr  += chunk;
        int_addr  += padded;
        remaining -= chunk;

        Boot_LED_Toggle();
    }

    HAL_FLASH_Lock();
    return HAL_OK;
}

/* ---- Verify: read back internal flash and compare CRC32 ---- */
static int verify_internal_flash(uint32_t fw_size, uint32_t expected_crc)
{
    uint32_t crc = Boot_CRC32_Calc((const uint8_t *)APP_BASE_ADDR, fw_size);
    return (crc == expected_crc) ? 1 : 0;
}

/* ---- Main ---- */
int main(void)
{
    IAP_Meta_t meta;

    HAL_Init();

    /* Use default SystemClock_Config (CSI-based PLL, same as App) */
    SystemClock_Config();

    /* Init GPIO for LED */
    __HAL_RCC_GPIOE_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = BOOT_LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BOOT_LED_PORT, &gpio);

    /* Init OCTOSPI1 for external flash access */
    MX_OCTOSPI1_Init();

    /* Init external flash */
    if (BootFlash_Init() != HAL_OK)
    {
        /* Can't read ext flash — just jump to App */
        Boot_JumpToApp(APP_BASE_ADDR);
    }

    /* Read IAP metadata */
    if (BootFlash_Read(IAP_META_ADDR, (uint8_t *)&meta, sizeof(meta)) != HAL_OK)
    {
        Boot_JumpToApp(APP_BASE_ADDR);
    }

    /* Check upgrade flag */
    if (meta.magic != IAP_META_MAGIC)
    {
        /* No pending upgrade */
        Boot_JumpToApp(APP_BASE_ADDR);
    }

    /* Validate size */
    if (meta.fw_size == 0 || meta.fw_size > IAP_FW_MAX_SIZE ||
        meta.fw_size > APP_MAX_SIZE)
    {
        clear_metadata();
        Boot_JumpToApp(APP_BASE_ADDR);
    }

    /* Verify CRC32 of firmware in external flash */
    {
        uint32_t crc = 0xFFFFFFFF;
        uint32_t remaining = meta.fw_size;
        uint32_t addr = IAP_FW_BASE_ADDR;
        uint8_t buf[256];

        while (remaining > 0)
        {
            uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
            if (BootFlash_Read(addr, buf, chunk) != HAL_OK)
            {
                clear_metadata();
                Boot_JumpToApp(APP_BASE_ADDR);
            }

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
            Boot_LED_Toggle();
        }
        crc = ~crc;

        if (crc != meta.fw_crc32)
        {
            /* CRC mismatch — keep old firmware */
            clear_metadata();
            Boot_JumpToApp(APP_BASE_ADDR);
        }
    }

    /* ---- Firmware is valid, proceed with upgrade ---- */

    /* Erase internal flash App area */
    if (erase_app_area(meta.fw_size) != HAL_OK)
    {
        clear_metadata();
        Boot_JumpToApp(APP_BASE_ADDR);
    }

    /* Copy firmware */
    if (copy_firmware(meta.fw_size) != HAL_OK)
    {
        /* Copy failed — App area is corrupt, but we still clear the flag
           to avoid infinite retry loop. Old App is lost. */
        clear_metadata();
        Boot_JumpToApp(APP_BASE_ADDR);
    }

    /* Read-back verify */
    if (!verify_internal_flash(meta.fw_size, meta.fw_crc32))
    {
        clear_metadata();
        Boot_JumpToApp(APP_BASE_ADDR);
    }

    /* Success — clear upgrade flag */
    clear_metadata();

    /* Jump to new App */
    Boot_JumpToApp(APP_BASE_ADDR);

    /* Should never reach here */
    while (1) {}
}

/* SystemClock_Config — same as App (CSI + PLL = 250MHz) */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_CSI;
    RCC_OscInitStruct.CSIState = RCC_CSI_ON;
    RCC_OscInitStruct.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState  = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_CSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 125;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE    = RCC_PLL1_VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInitStruct.PLL.PLLFRACN  = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        while (1) {}
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        while (1) {}
    }

    __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
