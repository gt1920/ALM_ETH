
#ifndef __Systick_Task_H
#define __Systick_Task_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern __IO uint16_t Count_ms, Heartbeat_Count, LED_OFF_Count;

extern volatile uint8_t motor_status_tick_1ms;

void Systick_Task(void);

/* 1ms tick hook (called from SysTick) */
void Adjuster_FlashDeferred_Tick1ms(void);

void Adjuster_FlashDeferred_Task(void);


#ifdef __cplusplus
}
#endif

#endif
