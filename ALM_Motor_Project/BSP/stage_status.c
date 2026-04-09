#include "stage_status.h"
#include "motor.h"

/* =========================================================
 * External State Sources
 * =========================================================
 * 这些函数 / 变量应由你系统中其它模块提供
 * （Motor / Limit / Comm / Safety）
 */

/* Motion control */
extern volatile uint8_t motor_motion_done;
extern bool Stage_IsBusy(void);
extern bool Stage_IsHomed(void);

/* Safety */
extern bool Stage_IsEStop(void);

/* Error sources */
extern bool Error_OverCurrent(void);
extern bool Error_OverTemp(void);
extern bool Error_LimitHit(void);
extern bool Error_Comm(void);
extern bool Error_MotorFault(void);

/* =========================================================
 * Internal Sticky Error Register
 * ========================================================= */
static volatile uint8_t g_stage_error_flags = STAGE_ERROR_NONE;

/* =========================================================
 * Status Flags
 * ========================================================= */
uint8_t Stage_GetStatusFlags(void)
{
    uint8_t flags = 0;

    /* ---------- RUNNING ---------- */
    if (Motor_IsRunning_X() || Motor_IsRunning_Y())
    {
        flags |= STAGE_STATUS_RUNNING;
    }

    /* ---------- DONE (event) ---------- */
    if (motor_motion_done)
    {
        flags |= STAGE_STATUS_DONE;
    }

    /* ---------- HOMED ---------- */
    if (Stage_IsHomed())
    {
        flags |= STAGE_STATUS_HOMED;
    }

    /* ---------- BUSY ---------- */
    if (Stage_IsBusy())
    {
        flags |= STAGE_STATUS_BUSY;
    }

    /* ---------- ESTOP ---------- */
    if (Stage_IsEStop())
    {
        flags |= STAGE_STATUS_ESTOP;
    }

    /* ---------- ENABLED ---------- */
    if (Motor_IsEnabled())
    {
        flags |= STAGE_STATUS_ENABLED;
    }

    return flags;
}

/* =========================================================
 * Error Flags (sticky)
 * ========================================================= */
uint8_t Stage_GetErrorFlags(void)
{
    uint8_t flags = g_stage_error_flags;

    /* 动态检测的错误也可在此叠加 */
    if (Error_OverCurrent())
        flags |= STAGE_ERROR_OVERCURRENT;

    if (Error_OverTemp())
        flags |= STAGE_ERROR_OVERTEMP;

    if (Error_LimitHit())
        flags |= STAGE_ERROR_LIMIT;

    if (Error_Comm())
        flags |= STAGE_ERROR_COMM;

    if (Error_MotorFault())
        flags |= STAGE_ERROR_MOTOR_FAULT;

    g_stage_error_flags = flags;
    return flags;
}

/* =========================================================
 * Error Control
 * ========================================================= */

/* 在中断 / 驱动中调用 */
void Stage_SetError(uint8_t error_bit)
{
    g_stage_error_flags |= error_bit;
}

/* 由 PC / CAN 命令显式清除 */
void Stage_ClearErrorFlags(uint8_t mask)
{
    g_stage_error_flags &= ~mask;
}
