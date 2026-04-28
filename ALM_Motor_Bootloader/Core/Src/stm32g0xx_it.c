/**
 * stm32g0xx_it.c - ALM_Motor_Bootloader interrupt handlers
 *
 * Bootloader uses no peripherals beyond SysTick (HAL tick) and FLASH.
 * Faults trap forever; SysTick ticks the HAL.
 */
#include "main.h"

void NMI_Handler(void)         { while (1) { __NOP(); } }
void HardFault_Handler(void)   { while (1) { __NOP(); } }
void SVC_Handler(void)         { }
void PendSV_Handler(void)      { }
void SysTick_Handler(void)     { HAL_IncTick(); }
