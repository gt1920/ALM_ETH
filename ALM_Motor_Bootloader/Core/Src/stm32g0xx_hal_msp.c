/**
 * stm32g0xx_hal_msp.c - ALM_Motor_Bootloader MSP
 *
 * Only HAL core is initialized; no peripheral MSP overrides needed.
 */
#include "main.h"

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}
