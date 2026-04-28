#include "bsp_flash.h"
#include "stm32g0xx_hal.h"

/* Defined in adjuster_flash.c for live-debug monitoring. */
extern volatile uint32_t g_bsp_flash_erase_status;
extern volatile uint32_t g_bsp_flash_erase_bank;
extern volatile uint32_t g_bsp_flash_erase_page;
extern volatile uint32_t g_bsp_flash_erase_page_error;
extern volatile uint32_t g_bsp_flash_last_error;
extern volatile uint32_t g_bsp_flash_program_status;
extern volatile uint32_t g_bsp_flash_program_fail_addr;

/* =========================================================
 *  Flash Erase
 * =========================================================
 * STM32G0:
 * - Flash ҳ��С��2KB
 * - ������λ��Page
 */
void BSP_Flash_Erase(uint32_t addr)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0;

    uint32_t bank = FLASH_BANK_1;
    uint32_t page = 0;

#if defined(FLASH_DBANK_SUPPORT)
    /* In dual-bank mode, HAL expects page number within the selected bank. */
    if (addr >= (FLASH_BASE + FLASH_BANK_SIZE))
    {
        bank = FLASH_BANK_2;
        page = (uint32_t)((addr - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE);
    }
    else
    {
        bank = FLASH_BANK_1;
        page = (uint32_t)((addr - FLASH_BASE) / FLASH_PAGE_SIZE);
    }
#else
    bank = FLASH_BANK_1;
    page = (uint32_t)((addr - FLASH_BASE) / FLASH_PAGE_SIZE);
#endif

    g_bsp_flash_erase_bank = bank;
    g_bsp_flash_erase_page = page;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks     = bank;
    erase.Page      = page;
    erase.NbPages   = 1;

    /* Clear EOP + all error flags (write-1-to-clear). */
    WRITE_REG(FLASH->SR, FLASH_SR_CLEAR);

    HAL_FLASH_Unlock();
    g_bsp_flash_erase_status = (uint32_t)HAL_FLASHEx_Erase(&erase, &page_error);
    g_bsp_flash_erase_page_error = page_error;
    g_bsp_flash_last_error = (uint32_t)(FLASH->SR & FLASH_SR_ERRORS);
    HAL_FLASH_Lock();
}

/* =========================================================
 *  Flash Program
 * =========================================================
 * STM32G0:
 * - ��Сд�뵥λ��double word (64bit)
 * - ��ַ���� 8 �ֽڶ���
 */
void BSP_Flash_Program(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint64_t data;
    uint32_t i = 0;
    uint32_t tickstart;

    /* Reset debug markers for this call */
    g_bsp_flash_program_status = 0xFFFFFFFFu;
    g_bsp_flash_program_fail_addr = 0xFFFFFFFFu;

    /* Clear EOP + all error flags (write-1-to-clear). */
    WRITE_REG(FLASH->SR, FLASH_SR_CLEAR);

    HAL_FLASH_Unlock();

    while (i < len)
    {
        data = 0xFFFFFFFFFFFFFFFFULL;

        for (uint32_t b = 0; b < 8 && i < len; b++, i++)
        {
            data &= ~((uint64_t)0xFF << (b * 8));
            data |= ((uint64_t)buf[i] << (b * 8));
        }

        /* Wait for any previous operation to complete */
        tickstart = HAL_GetTick();
#if defined(FLASH_DBANK_SUPPORT)
        while ((FLASH->SR & (FLASH_SR_BSY1 | FLASH_SR_BSY2)) != 0x00U)
#else
        while ((FLASH->SR & FLASH_SR_BSY1) != 0x00U)
#endif
        {
            if ((HAL_GetTick() - tickstart) > FLASH_TIMEOUT_VALUE)
            {
                g_bsp_flash_program_status = (uint32_t)HAL_TIMEOUT;
                if (g_bsp_flash_program_fail_addr == 0xFFFFFFFFu)
                {
                    g_bsp_flash_program_fail_addr = addr;
                }
                g_bsp_flash_last_error = (uint32_t)(FLASH->SR & FLASH_SR_ERRORS);
                goto program_done;
            }
        }

        /* Clear EOP + error flags before programming this double-word */
        WRITE_REG(FLASH->SR, FLASH_SR_CLEAR);

        /* Double-word program: on STM32G0 this is two 32-bit stores.
         * Protect the sequence from interrupts to avoid FLASH_SR_PROGERR.
         */
        {
            uint32_t primask_bit;
            SET_BIT(FLASH->CR, FLASH_CR_PG);

            primask_bit = __get_PRIMASK();
            __disable_irq();
            *(uint32_t *)addr = (uint32_t)data;
            __ISB();
            *(uint32_t *)(addr + 4u) = (uint32_t)(data >> 32u);
            __set_PRIMASK(primask_bit);

            /* Wait for program to complete */
            tickstart = HAL_GetTick();
#if defined(FLASH_DBANK_SUPPORT)
            while ((FLASH->SR & (FLASH_SR_BSY1 | FLASH_SR_BSY2)) != 0x00U)
#else
            while ((FLASH->SR & FLASH_SR_BSY1) != 0x00U)
#endif
            {
                if ((HAL_GetTick() - tickstart) > FLASH_TIMEOUT_VALUE)
                {
                    g_bsp_flash_program_status = (uint32_t)HAL_TIMEOUT;
                    if (g_bsp_flash_program_fail_addr == 0xFFFFFFFFu)
                    {
                        g_bsp_flash_program_fail_addr = addr;
                    }
                    g_bsp_flash_last_error = (uint32_t)(FLASH->SR & FLASH_SR_ERRORS);
                    CLEAR_BIT(FLASH->CR, FLASH_CR_PG);
                    goto program_done;
                }
            }

            /* Clear PG bit */
            CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

            /* Check errors */
            g_bsp_flash_last_error = (uint32_t)(FLASH->SR & FLASH_SR_ERRORS);
            if (g_bsp_flash_last_error != 0u)
            {
                g_bsp_flash_program_status = (uint32_t)HAL_ERROR;
                if (g_bsp_flash_program_fail_addr == 0xFFFFFFFFu)
                {
                    g_bsp_flash_program_fail_addr = addr;
                }
                goto program_done;
            }

            g_bsp_flash_program_status = (uint32_t)HAL_OK;
        }

        addr += 8;
    }

program_done:
    /* Capture final SR error state as well */
    g_bsp_flash_last_error = (uint32_t)(FLASH->SR & FLASH_SR_ERRORS);
    HAL_FLASH_Lock();
}

/* =========================================================
 *  Software CRC32 (IEEE 802.3)
 * =========================================================
 * Polynomial: 0x04C11DB7 (reflected 0xEDB88320)
 */
uint32_t BSP_CRC32_Calc(const uint8_t *buf, uint32_t len)
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


