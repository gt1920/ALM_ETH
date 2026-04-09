#include "adjuster_can.h"

#include "fdcan.h"
#include "CAN_Motor.h"
#include "Motor.h"
#include "adjuster_can_report.h"
#include "CAN_comm.h"
#include "adjuster_flash.h"
#include "adjuster_motor_task.h"

void Motor_StartFromCommand(const CAN_MotionCmd_t *cmd);

static void Stage_Heartbeat_Init(void);

void CAN_Init(void)
{
    /* === 生成本机 NodeID（关键） === */
    //FDCAN_NodeID = Build_NodeID_From_UID();	
		FDCAN_NodeID = g_adj_params.node_id;
		
		HAL_FDCAN_Start(&hfdcan1);

    HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
	
		Stage_Heartbeat_Init();	
}

// CAN → Motor 的唯一入口

//CAN_MotionCmd_t cmd;

/* Motor.c 里有实现，但 Motor.h 可能没声明：先 extern 兜底 */
extern void Motor_SetStepFreq(uint16_t step_hz);

static void CAN_Parse_ParamFrame(const uint8_t *data, CAN_ParamCmd_t *out)
{
    out->NodeID =
        ((uint32_t)data[0])       |
        ((uint32_t)data[1] << 8)  |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);

    out->Axis    = data[4];
    out->ParamID = data[5];

    out->Value =
        ((uint16_t)data[6]) |
        ((uint16_t)data[7] << 8);
}


static void CAN_Send_ParamFeedback(uint32_t node_id,
                                   uint8_t axis,
                                   uint8_t param_id,
                                   uint8_t status,
                                   uint32_t value)
{
    uint8_t tx_buf[12] = {0};

    tx_buf[0] = (uint8_t)(node_id & 0xFF);
    tx_buf[1] = (uint8_t)((node_id >> 8) & 0xFF);
    tx_buf[2] = (uint8_t)((node_id >> 16) & 0xFF);
    tx_buf[3] = (uint8_t)((node_id >> 24) & 0xFF);

    tx_buf[4] = axis;
    tx_buf[5] = param_id;
    tx_buf[6] = status;   // ACK status
    tx_buf[7] = 0x00;

    tx_buf[8]  = (uint8_t)(value & 0xFF);
    tx_buf[9]  = (uint8_t)((value >> 8) & 0xFF);

    CAN_Send_FD(CAN_ID_PARAM_FEEDBACK, tx_buf, 12);
}



void AdjusterCAN_OnRx(uint32_t id,
                      uint32_t idType,
                      const uint8_t *data,
                      uint8_t len)
{
    if (idType != FDCAN_STANDARD_ID)
        return;

    if (len < 4u)
        return;

    uint32_t node_id =
        ((uint32_t)data[0])       |
        ((uint32_t)data[1] << 8)  |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);

    if (!Adjuster_IsMyNode(node_id))
        return;

    switch (id)
    {
        /* ================= Motion ================= */
        case CAN_ID_MOTION_CONTROL:
        {
            if (len < 12u)
                return;

            CAN_MotionCmd_t frame;
            CAN_Parse_ControlFrame(data, &frame);
            Motor_StartFromCommand(&frame);
            break;
        }

        /* ================= Device Name ================= */
        case CAN_ID_DEVICE_NAME:
        {
            if (len < (4u + 8u))
                return;

            (void)Adjuster_SetDeviceName(&data[4]);
            break;
        }

        /* ================= Parameters ================= */
        case CAN_ID_PARAM_SET:
        {
            if (len < 12u)
                return;

            CAN_ParamCmd_t p;
            CAN_Parse_ParamFrame(data, &p);

            switch ((Adjuster_ParamId_t)p.ParamID)
            {
                /* -------- X Run Current -------- */
                case ADJ_PARAM_X_CURRENT:
                {
                    g_adj_params.x_run_current = (uint16_t)p.Value;
                    __HAL_TIM_SET_COMPARE(&htim1,
                                          TIM_CHANNEL_2,
                                          g_adj_params.x_run_current);
									
										Adjuster_MarkParamDirty();  // Write flash flag

										CAN_Send_ParamFeedback(node_id,1,ADJ_PARAM_X_CURRENT,PARAM_ACK_OK,g_adj_params.x_run_current);
                    break;
                }

                /* -------- Y Run Current -------- */
                case ADJ_PARAM_Y_CURRENT:
                {
                    g_adj_params.y_run_current = (uint16_t)p.Value;
                    __HAL_TIM_SET_COMPARE(&htim3,
                                          TIM_CHANNEL_1,
                                          g_adj_params.y_run_current);
									
										Adjuster_MarkParamDirty();  // Write flash flag

                    CAN_Send_ParamFeedback(node_id,
                                            2,
                                            ADJ_PARAM_Y_CURRENT,
                                            PARAM_ACK_OK,
                                            g_adj_params.y_run_current);
                    break;
                }

                /* -------- Hold Current (%) -------- */
                case ADJ_PARAM_HOLD_CURRENT:
                {
                    uint16_t v = p.Value;
                    if (v > ADJ_HOLD_CURRENT_MAX_PCT)
                        v = ADJ_HOLD_CURRENT_MAX_PCT;

                    if (p.Axis == 0u)
                    {
                        g_adj_params.x_hold_current_pct = (uint8_t)v;
											
												Adjuster_MarkParamDirty();  // Write flash flag

                        CAN_Send_ParamFeedback(node_id,
                                                0,
                                                ADJ_PARAM_HOLD_CURRENT,
                                                PARAM_ACK_OK,
                                                g_adj_params.x_hold_current_pct);
                    }
                    else if (p.Axis == 1u)
                    {
                        g_adj_params.y_hold_current_pct = (uint8_t)v;
											
												Adjuster_MarkParamDirty();  // Write flash flag

                        CAN_Send_ParamFeedback(node_id,
                                                1,
                                                ADJ_PARAM_HOLD_CURRENT,
                                                PARAM_ACK_OK,
                                                g_adj_params.y_hold_current_pct);
                    }
                    else if (p.Axis == 0xFF)
                    {
                        g_adj_params.x_hold_current_pct = (uint8_t)v;
                        g_adj_params.y_hold_current_pct = (uint8_t)v;
											
												Adjuster_MarkParamDirty();  // Write flash flag

                        CAN_Send_ParamFeedback(node_id,
                                                0xFF,
                                                ADJ_PARAM_HOLD_CURRENT,
                                                PARAM_ACK_OK,
                                                v);
                    }
                    else
                    {											
												//Adjuster_MarkParamDirty();  // Write flash flag
											
                        CAN_Send_ParamFeedback(node_id,
                                                p.Axis,
                                                ADJ_PARAM_HOLD_CURRENT,
                                                PARAM_ACK_INVALID_AXIS,
                                                0u);
                    }
                    break;
                }

                /* -------- Speed (step/s, global) -------- */
                case ADJ_PARAM_SPEED:
                {
                    uint16_t v = p.Value;

                    if (v < ADJ_SPEED_MIN_STEP_S)
                        v = ADJ_SPEED_MIN_STEP_S;
                    if (v > ADJ_SPEED_MAX_STEP_S)
                        v = ADJ_SPEED_MAX_STEP_S;

                    g_adj_params.x_step_freq_hz = (uint16_t)v;
                    g_adj_params.y_step_freq_hz = (uint16_t)v;

                    Motor_SetStepFreq(v);
										
										Adjuster_MarkParamDirty();  // Write flash flag

                    CAN_Send_ParamFeedback(node_id,
                                            0xFF,
                                            ADJ_PARAM_SPEED,
                                            PARAM_ACK_OK,
                                            v);
                    break;
                }

                /* -------- Save Parameters -------- */
								case ADJ_PARAM_SAVE:
								{
										uint8_t status = PARAM_ACK_OK;

										if (Adjuster_Flash_Write(&g_adj_params))
										{
												g_param_dirty = 0u;
												g_param_dirty_tick = 0u;
										}
										else
										{
												status = PARAM_ACK_INTERNAL_ERR;
										}

										CAN_Send_ParamFeedback(node_id,
																						0xFF,
																						ADJ_PARAM_SAVE,
																						status,
																						0u);
										break;
								}


                /* -------- Read All Parameters -------- */
                case ADJ_PARAM_READ_ALL:
                {
                    CAN_Send_ParamFeedback(node_id,
                                            0,
                                            ADJ_PARAM_X_CURRENT,
                                            PARAM_ACK_OK,
                                            g_adj_params.x_run_current);

                    CAN_Send_ParamFeedback(node_id,
                                            1,
                                            ADJ_PARAM_Y_CURRENT,
                                            PARAM_ACK_OK,
                                            g_adj_params.y_run_current);

                    CAN_Send_ParamFeedback(node_id,
                                            0,
                                            ADJ_PARAM_HOLD_CURRENT,
                                            PARAM_ACK_OK,
                                            g_adj_params.x_hold_current_pct);

                    CAN_Send_ParamFeedback(node_id,
                                            1,
                                            ADJ_PARAM_HOLD_CURRENT,
                                            PARAM_ACK_OK,
                                            g_adj_params.y_hold_current_pct);

                    CAN_Send_ParamFeedback(node_id,
                                            0xFF,
                                            ADJ_PARAM_SPEED,
                                            PARAM_ACK_OK,
                                            g_adj_params.x_step_freq_hz);
                    break;
                }

                default:
                    /* Unknown ParamID */
                    CAN_Send_ParamFeedback(node_id,
                                            p.Axis,
                                            p.ParamID,
                                            PARAM_ACK_INVALID_PARAM,
                                            0u);
                    break;
            }
            break;
        }

        default:
            break;
    }
}



//void AdjusterCAN_OnRx(uint32_t id,
//                      uint32_t idType,
//                      const uint8_t *data,
//                      uint8_t len)
//{
//    /* Only handle standard ID */
//    if (idType != FDCAN_STANDARD_ID)
//        return;

//    switch (id)
//    {

//				case CAN_ID_MOTION_CONTROL:   /* Motion command */
//        {
//						/*
//						Byte 0–3   : node_id
//						Byte 4      : axis
//						Byte 5      : dir
//						Byte 6–7   : padding / reserved
//						Byte 8–11  : step
//						Byte 12–15 : padding / reserved
//						*/
//					
//            CAN_MotionCmd_t frame;

//            if (len < 12u)
//                return;

//						CAN_Parse_ControlFrame(data, &frame);

//            /* NodeID / target check（如果你协议里有） */
//            if (!Adjuster_IsMyNode(frame.node_id))
//                return;

//            Motor_StartFromCommand(&frame);
//            break;
//        }

//        /* =========================
//         * 0x123: Device name / info
//         * ========================= */
//        
//				case CAN_ID_DEVICE_NAME:
//        {
//					if (Adjuster_SetDeviceName(&rx_buf[RENAME_NAME_OFFSET]))
//					{
//							// ACK OK
//					}
//					else
//					{
//							// ACK ERROR
//					}
//					break;
//        }
//								

//				/* =========================
//         * 0x122: Parameter config
//         * ========================= */
//        /*
//				case CAN_ID_PARAM_CONFIG:
//        {
//            CAN_ParamCmd_t param;

//            if (len < sizeof(CAN_ParamCmd_t))
//                return;

//            memcpy(&param, data, sizeof(CAN_ParamCmd_t));

//            if (!Adjuster_IsMyNode(param.node_id))
//                return;

//            Adjuster_ApplyParam(&param);
//            break;
//        }
//				*/



//        /* =========================
//         * Unknown / unsupported
//         * ========================= */
//        default:
//            /* Ignore silently */
//            break;
//    }
//}

void AdjusterCAN_TxStatus(void){}
void AdjusterCAN_TxHeartbeat(void){}
void AdjusterCAN_TxParamReport(void){}

void CAN_Report_MotorStatus(void)
{
    uint8_t tx_buf[12];

    uint32_t x_rem = Motor_GetRemaining(MOTOR_AXIS_X);
    uint32_t y_rem = Motor_GetRemaining(MOTOR_AXIS_Y);

    /* NodeID */
    tx_buf[0]  = (uint8_t)(FDCAN_NodeID & 0xFFu);
    tx_buf[1]  = (uint8_t)((FDCAN_NodeID >> 8) & 0xFFu);
    tx_buf[2]  = (uint8_t)((FDCAN_NodeID >> 16) & 0xFFu);
    tx_buf[3]  = (uint8_t)((FDCAN_NodeID >> 24) & 0xFFu);

    /* X remaining */
    tx_buf[4]  = (uint8_t)(x_rem & 0xFFu);
    tx_buf[5]  = (uint8_t)((x_rem >> 8) & 0xFFu);
    tx_buf[6]  = (uint8_t)((x_rem >> 16) & 0xFFu);
    tx_buf[7]  = (uint8_t)((x_rem >> 24) & 0xFFu);

    /* Y remaining */
    tx_buf[8]  = (uint8_t)(y_rem & 0xFFu);
    tx_buf[9]  = (uint8_t)((y_rem >> 8) & 0xFFu);
    tx_buf[10] = (uint8_t)((y_rem >> 16) & 0xFFu);
    tx_buf[11] = (uint8_t)((y_rem >> 24) & 0xFFu);

    FDCAN_TxHeaderTypeDef txHeader = {0};

    txHeader.Identifier          = CAN_ID_STATUS_FEEDBACK;   // 例如 0x121
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = FDCAN_DLC_BYTES_12;
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_ON;
    txHeader.FDFormat            = FDCAN_FD_CAN;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;

    (void)HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, tx_buf);
}


uint8_t Adjuster_IsMyNode(uint32_t target_node_id)
{
    /* 0 means broadcast */
    if (target_node_id == 0u)
        return 1u;

    if (target_node_id == FDCAN_NodeID)
        return 1u;

    return 0u;
}

void Motor_StartFromCommand(const CAN_MotionCmd_t *cmd)
{
    /* Axis validity */
    if (cmd->axis > MOTOR_AXIS_Y)
        return;

    if (cmd->step == 0u)
        return;

    /* Check busy state */
    bool motor_busy =
        (Motor_GetRemaining(MOTOR_AXIS_X) != 0u) ||
        (Motor_GetRemaining(MOTOR_AXIS_Y) != 0u);

    /* Reject new command while busy (no Immediate support yet) */
    if (motor_busy)
        return;

    uint8_t  axis  = cmd->axis;
    uint8_t  dir   = (cmd->dir != 0u) ? 1u : 0u;
    uint32_t steps = cmd->step;

    g_motor_axis[axis].direction       = dir;
    g_motor_axis[axis].steps_remaining = steps << 1;
    g_motor_axis[axis].running         = 1u;

    /* Set direction GPIO */
    if (axis == MOTOR_AXIS_X)
        Motor_Set_X_Direction(dir ? GPIO_PIN_SET : GPIO_PIN_RESET);
    else
        Motor_Set_Y_Direction(dir ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* Start step timer */
    Motor_RuntimeBegin();
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    HAL_TIM_Base_Start_IT(&htim2);
}

/* =========================
 * NodeID 生成（STM32G0 UID）
 * ========================= */

/**
 * @brief  Build a 32-bit NodeID from STM32 unique 96-bit UID
 *
 * This function derives a stable 32-bit NodeID from the STM32 internal
 * 96-bit unique device identifier (UID).
 *
 * Design rationale:
 * - Use all 96 bits of the UID (UID[0..2]) and mix them into a single
 *   32-bit value using XOR to reduce collision probability.
 * - Avoid using only part of the UID (e.g. last 32 bits), which may
 *   have insufficient entropy across devices from the same batch.
 * - The resulting NodeID is deterministic and remains constant for
 *   the lifetime of the MCU.
 *
 * Usage notes:
 * - NodeID is intended to be unique within the same system / CAN bus,
 *   not guaranteed to be globally unique.
 * - NodeID value 0 is reserved (e.g. for broadcast or invalid ID),
 *   so this function guarantees a non-zero result.
 *
 * @return 32-bit NodeID derived from MCU UID (non-zero)
 */

#define STM32G0_UID_BASE    0x1FFF7590UL

#define STM32_UID_BASE      STM32G0_UID_BASE

uint32_t Build_NodeID_From_UID(void)
{
    const uint32_t *uid = (const uint32_t *)STM32_UID_BASE;

    /* Mix full 96-bit STM32 UID into a 32-bit NodeID to reduce collision risk */
    uint32_t id = uid[0] ^ uid[1] ^ uid[2];

    /* Ensure NodeID is non-zero (zero reserved for broadcast / invalid) */
    if (id == 0)
        id = uid[0] | 0x01000000UL;

    return id;
}

static void Stage_Heartbeat_Init(void)
{
		g_stage_hb.module_type = MODULE_ADJUSTER;
	
		g_stage_hb.node_id = FDCAN_NodeID;

		memset(g_stage_hb.name, 0, 8);
    memcpy(g_stage_hb.name, "R/GB", 6);	
	
		g_stage_hb.fw_hw = 0x01;  // 硬件版本
		g_stage_hb.fw_dd = 17;  // 日
		g_stage_hb.fw_mm = 12;  // 月
		g_stage_hb.fw_yy = 24;  // 年
	
		g_stage_hb.mfg_year = 25;  // 2025
		g_stage_hb.mfg_month = 12;
		g_stage_hb.mfg_day = 17;
}

static void CAN_TxLed_On(void)
{
    HAL_GPIO_WritePin(Stat_LED_GPIO_Port, Stat_LED_Pin, GPIO_PIN_SET);
}

static void CAN_TxLed_Off(void)
{
    HAL_GPIO_WritePin(Stat_LED_GPIO_Port, Stat_LED_Pin, GPIO_PIN_RESET);
}


void CAN_TxService(void)
{
    /* TX FIFO free level */
    uint32_t free = HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1);

    if (free == 0u)
    {
        /* TX FIFO full */
        CAN_TxLed_On();
    }
    else
    {
        CAN_TxLed_Off();
    }
}


