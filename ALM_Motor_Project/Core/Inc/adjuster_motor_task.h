
#pragma once
#include "main.h"
#include <stdint.h>

uint8_t Motor_IsMotionDone(void);
uint8_t Motor_IsRunning(void);
void Motor_RuntimeBegin(void);
uint32_t Motor_GetLastRuntimeMs(void);

void Motor_Task(void);
void Motor_HoldService(void);
