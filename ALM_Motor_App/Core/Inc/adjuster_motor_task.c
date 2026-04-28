
#include "adjuster_motor_task.h"

#include "Motor.h"

static volatile uint8_t motor_motion_done = 0u;
static uint8_t last_running[2] = {0u, 0u};
static uint8_t hold_pending[2] = {0u, 0u};
static uint32_t hold_deadline_ms[2] = {0u, 0u};
static uint32_t motor_runtime_start_ms = 0u;
static volatile uint32_t motor_last_runtime_ms = 0u;
static uint8_t motor_runtime_active = 0u;

uint8_t Motor_IsMotionDone(void)
{
    if (motor_motion_done)
    {
        motor_motion_done = 0u;   // consume once
        return 1u;
    }
    return 0u;
}

uint8_t Motor_IsRunning(void)
{
    return (g_motor_axis[MOTOR_AXIS_X].running != 0u) ||
           (g_motor_axis[MOTOR_AXIS_Y].running != 0u);
}

void Motor_RuntimeBegin(void)
{
    motor_last_runtime_ms = 0u;
    motor_runtime_start_ms = HAL_GetTick();
    motor_runtime_active = 1u;
}

uint32_t Motor_GetLastRuntimeMs(void)
{
    return motor_last_runtime_ms;
}


void Motor_Task(void)
{
    static uint8_t last_running_any = 0u;

    uint8_t now_running_any = 0u;

    for (uint8_t axis = MOTOR_AXIS_X; axis <= MOTOR_AXIS_Y; ++axis)
    {
        uint8_t now_running = (g_motor_axis[axis].running != 0u);

        /* running -> idle: start 250 ms hold timer for this axis */
        if ((last_running[axis] != 0u) && (now_running == 0u))
        {
            hold_pending[axis] = 1u;
            hold_deadline_ms[axis] = HAL_GetTick() + 250u;
        }

        /* idle -> running: cancel pending hold for this axis */
        if ((last_running[axis] == 0u) && (now_running != 0u))
        {
            hold_pending[axis] = 0u;
        }

        last_running[axis] = now_running;
        now_running_any |= now_running;
    }

    /* any axis running -> all idle : motion just finished (one-shot) */
    if ((last_running_any != 0u) && (now_running_any == 0u))
    {
        if (motor_runtime_active != 0u)
        {
            motor_last_runtime_ms = HAL_GetTick() - motor_runtime_start_ms;
            motor_runtime_active = 0u;
        }
        motor_motion_done = 1u;
    }

    last_running_any = now_running_any;
}

void Motor_HoldService(void)
{
    uint32_t now = HAL_GetTick();

    for (uint8_t axis = MOTOR_AXIS_X; axis <= MOTOR_AXIS_Y; ++axis)
    {
        if (!hold_pending[axis])
            continue;

        if ((int32_t)(now - hold_deadline_ms[axis]) >= 0)
        {
            /* Debounce: only drop current if still idle */
            if (Motor_IsAxisIdle(axis))
            {
                Motor_SetHoldCurrent(axis);
            }

            hold_pending[axis] = 0u;
        }
    }
}
