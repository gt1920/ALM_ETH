/**
 * main.h - ALM_Motor_Bootloader (STM32G0B1KBUx, Cortex-M0+)
 *
 * Minimal boot environment: HAL + clocks + iap.c. No app peripherals.
 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g0xx_hal.h"
#include <stdint.h>
#include <string.h>

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
