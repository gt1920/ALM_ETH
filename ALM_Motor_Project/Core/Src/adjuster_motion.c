#include "adjuster_motion.h"

void AdjusterMotion_Init(void){}
bool AdjusterMotion_IsReady(void){ return true; }

void AdjusterMotion_Start(AxisMask_t axis, MotionMode_t mode,
                          int32_t x_step, int32_t y_step)
{
    (void)axis; (void)mode; (void)x_step; (void)y_step;
}

void AdjusterMotion_Stop(void){}
void AdjusterMotion_Tick1ms(void){}
