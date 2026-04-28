#include "CAN_Motor.h"
#include "Motor.h"
#include "Systick_Task.h"
#include "LED.h"
#include "fdcan.h"
#include <stdbool.h>

/* =========================
 * 接收缓存（原有）
 * ========================= */
uint16_t rx_id;
uint8_t  rx_buf[64];

CAN_MotionCmd_t frame;

/* =========================
 * Motion flags（原有）
 * ========================= */
#define MOTION_FLAG_IMMEDIATE   (1u << 1)


/* =========================
 * 状态上报相关（新增）
 * ========================= */

/* 500ms 计数（假设 Systick_Task 是 1ms） */
static uint16_t status_tick_500ms = 0;

/* 是否处于“需要周期上报”的阶段 */
static uint8_t  status_reporting_active = 0;

/* 上一次是否 still running（用于保证结束后再上报一次） */
static uint8_t  last_running_state = 0;

/* =========================
 * 内部工具函数
 * ========================= */


/* 任意轴是否还有剩余 step */
static uint8_t Motor_IsAnyAxisRunning(void)
{
    if (g_motor_axis[MOTOR_AXIS_X].steps_remaining > 0)
        return 1;

    if (g_motor_axis[MOTOR_AXIS_Y].steps_remaining > 0)
        return 1;

    return 0;
}

/* =========================
 * CAN 任务（原有，不改）
 * ========================= */
void CAN_Task(void)
{
    if (CAN_Receive_Arb_Frame(&rx_id, rx_buf))
    {
        CAN_Parse_ControlFrame(rx_buf, &frame);

        if (frame.node_id == FDCAN_NodeID)
        {
            LED_OFF_Count = 0;
            LED_Ctrl(1);
            Process_Command(&frame);
        }
    }
}

/* =========================
 * 接收仲裁帧（原有，不改）
 * ========================= */
uint8_t CAN_Receive_Arb_Frame(uint16_t *rx_id, uint8_t *rx_buf)
{
    FDCAN_RxHeaderTypeDef rxHeader;

    if (HAL_FDCAN_GetRxMessage(&hfdcan1,
                              FDCAN_RX_FIFO0,
                              &rxHeader,
                              rx_buf) == HAL_OK)
    {
        *rx_id = rxHeader.Identifier;
        return 1;
    }

    return 0;
}

/* =========================
 * 控制命令处理（原有，不改）
 * ========================= */
void Process_Command(const CAN_MotionCmd_t *cmd)
{
		/* Axis 合法性 */
    if (cmd->axis > MOTOR_AXIS_Y)
        return;

    if (cmd->step == 0u)
        return;

    /* 是否忙 */
    bool motor_busy =
        (Motor_GetRemaining(MOTOR_AXIS_X) != 0u) ||
        (Motor_GetRemaining(MOTOR_AXIS_Y) != 0u);

    /* 忙时暂不接受新命令（你也可以以后加 Immediate） */
    if (motor_busy)
        return;

    uint8_t  axis  = cmd->axis;
    uint8_t  dir   = (cmd->dir != 0u) ? 1u : 0u;
    uint32_t steps = cmd->step;

    g_motor_axis[axis].direction       = dir;
    g_motor_axis[axis].steps_remaining = steps;
    g_motor_axis[axis].running         = 1u;

    /* 设置方向 */
    if (axis == MOTOR_AXIS_X)
        Motor_Set_X_Direction(dir ? GPIO_PIN_SET : GPIO_PIN_RESET);
    else
        Motor_Set_Y_Direction(dir ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* 启动步进定时器 */
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    HAL_TIM_Base_Start_IT(&htim2);    
}

/* =========================
 * 控制帧解析（原有，不改）
 * ========================= */
void CAN_Parse_ControlFrame(const uint8_t *data, CAN_MotionCmd_t *out)
{
    out->node_id =
        ((uint32_t)data[0]      ) |
        ((uint32_t)data[1] <<  8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);

    out->axis  = data[4];
    out->dir   = data[5];

    out->step  =
        ((int32_t)data[8]       ) |
        ((int32_t)data[9]  <<  8) |
        ((int32_t)data[10] << 16) |
        ((int32_t)data[11] << 24);
}

/* =========================================================
 * 【新增】0x121 状态上报帧（完整可用）
 * ========================================================= */

/*
 * CAN ID : 0x121
 * DLC    : 12 Bytes
 *
 * Byte0-3  : NodeID (uint32, little-endian)
 * Byte4-7  : X 剩余 step (uint32, little-endian)
 * Byte8-11 : Y 剩余 step (uint32, little-endian)
 */
void CAN_Report_Motor_Status(uint32_t x_remain, uint32_t y_remain)
{
    uint8_t tx_buf[12];
    FDCAN_TxHeaderTypeDef txHeader = {0};

    /* Payload */
    tx_buf[0]  = (uint8_t)(FDCAN_NodeID & 0xFF);
    tx_buf[1]  = (uint8_t)((FDCAN_NodeID >> 8) & 0xFF);
    tx_buf[2]  = (uint8_t)((FDCAN_NodeID >> 16) & 0xFF);
    tx_buf[3]  = (uint8_t)((FDCAN_NodeID >> 24) & 0xFF);

    tx_buf[4]  = (uint8_t)(x_remain & 0xFF);
    tx_buf[5]  = (uint8_t)((x_remain >> 8) & 0xFF);
    tx_buf[6]  = (uint8_t)((x_remain >> 16) & 0xFF);
    tx_buf[7]  = (uint8_t)((x_remain >> 24) & 0xFF);

    tx_buf[8]  = (uint8_t)(y_remain & 0xFF);
    tx_buf[9]  = (uint8_t)((y_remain >> 8) & 0xFF);
    tx_buf[10] = (uint8_t)((y_remain >> 16) & 0xFF);
    tx_buf[11] = (uint8_t)((y_remain >> 24) & 0xFF);

    /* CAN FD + BRS Header */
    txHeader.Identifier = 0x121;
    txHeader.IdType     = FDCAN_STANDARD_ID;
    txHeader.TxFrameType= FDCAN_DATA_FRAME;

    txHeader.DataLength = FDCAN_DLC_BYTES_12;
    txHeader.FDFormat   = FDCAN_FD_CAN;
    txHeader.BitRateSwitch = FDCAN_BRS_ON;   // ★ 和心跳包一致

    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;

    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, tx_buf);
}



/* =========================================================
 * 【新增】500ms 状态上报调度（核心逻辑）
 * ========================================================= */

/*
 * 调用周期：1ms（推荐在 main loop 或 Systick_Task 后）
 */
void CAN_Motor_Status_Task(void)
{
    uint8_t running_now = Motor_IsAnyAxisRunning();

    /* 有轴开始跑 → 打开周期上报 */
    if (running_now && !status_reporting_active)
    {
        status_reporting_active = 1;
        status_tick_500ms = 0;
    }

    if (running_now)
        last_running_state = 1;

    if (status_reporting_active)
    {
        status_tick_500ms++;

        if (status_tick_500ms >= 500)   // 500ms
        {
            status_tick_500ms = 0;

            CAN_Report_Motor_Status(
                g_motor_axis[MOTOR_AXIS_X].steps_remaining,
                g_motor_axis[MOTOR_AXIS_Y].steps_remaining
            );

            /* 跑完后的“最后一次上报” */
            if (!running_now && last_running_state)
            {
                status_reporting_active = 0;
                last_running_state = 0;
            }
        }
    }
}
