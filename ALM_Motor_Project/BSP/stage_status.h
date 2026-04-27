#pragma once

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================================
 * Status Flags (runtime, non-sticky)
 * ========================================================= */
#define STAGE_STATUS_RUNNING     (1u << 0)   // 任一轴正在运动
#define STAGE_STATUS_DONE        (1u << 1)   // 一次运动完成（事件态）
#define STAGE_STATUS_HOMED       (1u << 2)   // 已完成回零
#define STAGE_STATUS_BUSY        (1u << 3)   // 忙（不可接新指令）
#define STAGE_STATUS_ESTOP       (1u << 4)   // 急停触发
#define STAGE_STATUS_ENABLED     (1u << 5)   // 电机使能
/* bit6~7 预留 */


/* =========================================================
 * Error Flags (sticky, need clear)
 * ========================================================= */
#define STAGE_ERROR_NONE         0x00

#define STAGE_ERROR_OVERCURRENT  (1u << 0)   // 过流
#define STAGE_ERROR_OVERTEMP     (1u << 1)   // 过温
#define STAGE_ERROR_LIMIT        (1u << 2)   // 限位触发
#define STAGE_ERROR_COMM         (1u << 3)   // 通信错误
#define STAGE_ERROR_MOTOR_FAULT  (1u << 4)   // 驱动器故障
/* bit5~7 预留 */


/* =========================================================
 * Public API
 * ========================================================= */

/* -------- Status -------- */
uint8_t Stage_GetStatusFlags(void);

/* -------- Error -------- */
uint8_t Stage_GetErrorFlags(void);
void    Stage_ClearErrorFlags(uint8_t mask);

/* -------- Error Set (called by ISR / driver) -------- */
void Stage_SetError(uint8_t error_bit);

