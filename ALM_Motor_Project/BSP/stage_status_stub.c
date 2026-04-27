#include <stdbool.h>
#include <stdint.h>

/* ================= Motor ================= */

__weak bool Motor_IsRunning_X(void) { return false; }
__weak bool Motor_IsRunning_Y(void) { return false; }
__weak bool Motor_IsEnabled(void)   { return false; }

/* ================= Motion ================= */

__weak volatile uint8_t motor_motion_done = 0;

__weak bool Stage_IsBusy(void)  { return false; }
__weak bool Stage_IsHomed(void) { return false; }
__weak bool Stage_IsEStop(void) { return false; }

/* ================= Errors ================= */

__weak bool Error_OverCurrent(void) { return false; }
__weak bool Error_OverTemp(void)    { return false; }
__weak bool Error_LimitHit(void)    { return false; }
__weak bool Error_Comm(void)        { return false; }
__weak bool Error_MotorFault(void)  { return false; }

/* ================= Motor Runtime ================= */

__weak uint8_t Motor_GetCurrentPct_X(void) { return 0; }
__weak uint8_t Motor_GetCurrentPct_Y(void) { return 0; }
