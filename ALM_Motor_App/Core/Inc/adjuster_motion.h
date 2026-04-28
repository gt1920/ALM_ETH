#pragma once
#include "main.h"
#include "adjuster_types.h"
#include <stdint.h>
#include <stdbool.h>

void AdjusterMotion_Init(void);
bool AdjusterMotion_IsReady(void);
void AdjusterMotion_Start(AxisMask_t axis, MotionMode_t mode,
                          int32_t x_step, int32_t y_step);
void AdjusterMotion_Tick1ms(void);
void AdjusterMotion_Stop(void);
