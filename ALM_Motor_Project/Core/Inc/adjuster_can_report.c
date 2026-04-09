
#include "adjuster_can_report.h"

#include "Systick_Task.h"
#include "adjuster_motor_task.h"
#include "Motor.h"
#include "adjuster_can.h"
#include "adjuster_flash.h"
#include "stage_status.h"

Stage_Heartbeat_t g_stage_hb;

static void Stage_Heartbeat_Task(void);
static void Stage_Heartbeat_Update(void);
static void Stage_Heartbeat_Send(void);
static void CAN_Report_MotorDone(void);

static bool motor_stop_report = false;

void Stage_Heartbeat_TriggerNow(void)
{
    /* Ensure we are in the heartbeat branch (not the 250ms one-shot stop report)
     * and force the next main-loop tick to pass the 1s threshold immediately.
     */
    motor_stop_report = true;
    Heartbeat_Count = 1000u;
}

void Status_Report_Task(void)
{
	uint8_t running = Motor_IsRunning();
	
	/* -------- Motion done event -------- */
	if (Motor_IsMotionDone())
	{
			/* ����ֻ�����ϱ����������κ�״̬ */
			CAN_Report_MotorDone();     // �� CAN_Motor_Status_Task()
	}
		
	/* ---------- 2. State: remaining steps ---------- */
	if ( g_motor_axis[MOTOR_AXIS_X].steps_remaining!=0 || g_motor_axis[MOTOR_AXIS_Y].steps_remaining!=0)
	{
		if (Heartbeat_Count >= 250u)
		{
			CAN_Report_MotorStatus();   // �ϱ� remaining step
			Heartbeat_Count = 0u;
		}
		motor_stop_report = false;
	}
	
	/* -------- Heartbeat (periodic) -------- */
	
	else		
	{
		if(motor_stop_report==false)
		{
			if (Heartbeat_Count >= 250u)
			{
				/* ����һ�� remaining = 0 */
				CAN_Report_MotorStatus();
				Heartbeat_Count = 0u;
				motor_stop_report = true;
			}		
		}
		else
		{
			if (Heartbeat_Count >= 1000u)
			{
					Heartbeat_Count = 0u;
					Stage_Heartbeat_Task();
			}			
		}
	}
}

static void CAN_Report_MotorDone(void)
{
    //uint8_t tx_buf[12] = {0};

    /* Payload layout (12 bytes):
     * [0..3]  NodeID (LE)
     * [4..7]  X remaining steps (LE)
     * [8..11] Y remaining steps (LE)
     *
     * Note: For "Done" event, remaining should be 0.
     * Keeping it explicit helps PC side sanity-check.
     */

		/*
    uint32_t x_rem = Motor_GetRemaining(MOTOR_AXIS_X);
    uint32_t y_rem = Motor_GetRemaining(MOTOR_AXIS_Y);

    tx_buf[0]  = (uint8_t)(FDCAN_NodeID & 0xFFu);
    tx_buf[1]  = (uint8_t)((FDCAN_NodeID >> 8) & 0xFFu);
    tx_buf[2]  = (uint8_t)((FDCAN_NodeID >> 16) & 0xFFu);
    tx_buf[3]  = (uint8_t)((FDCAN_NodeID >> 24) & 0xFFu);

    tx_buf[4]  = (uint8_t)(x_rem & 0xFFu);
    tx_buf[5]  = (uint8_t)((x_rem >> 8) & 0xFFu);
    tx_buf[6]  = (uint8_t)((x_rem >> 16) & 0xFFu);
    tx_buf[7]  = (uint8_t)((x_rem >> 24) & 0xFFu);

    tx_buf[8]  = (uint8_t)(y_rem & 0xFFu);
    tx_buf[9]  = (uint8_t)((y_rem >> 8) & 0xFFu);
    tx_buf[10] = (uint8_t)((y_rem >> 16) & 0xFFu);
    tx_buf[11] = (uint8_t)((y_rem >> 24) & 0xFFu);

    
		FDCAN_TxHeaderTypeDef txHeader = {0};
    txHeader.Identifier          = CANID_MOTION_STATUS;
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = FDCAN_DLC_BYTES_12;
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_ON;
    txHeader.FDFormat            = FDCAN_FD_CAN;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;

    (void)HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, tx_buf);
		*/
}


static void Stage_Heartbeat_Task(void)
{
    Stage_Heartbeat_Update();
    Stage_Heartbeat_Send();
}

static void Stage_Heartbeat_Update(void)
{
    /* ---------- Heartbeat Counter ---------- */
    g_stage_hb.heartbeat_cnt++;

    /* ---------- Identity ---------- */
    g_stage_hb.module_type = g_adj_params.module_type;
    g_stage_hb.node_id     = g_adj_params.node_id;

    memcpy(g_stage_hb.name,
           g_adj_params.device_name,
           sizeof(g_stage_hb.name));   // �̶� 8 byte

    /* ---------- Firmware Version ---------- */
    g_stage_hb.fw_hw = g_adj_params.pcb_revision;
    g_stage_hb.fw_dd = g_adj_params.fw_day;
    g_stage_hb.fw_mm = g_adj_params.fw_month;
    g_stage_hb.fw_yy = g_adj_params.fw_year;

    /* ---------- Manufacture Date ---------- */
    g_stage_hb.mfg_year  = g_adj_params.mfg_year;
    g_stage_hb.mfg_month = g_adj_params.mfg_month;
    g_stage_hb.mfg_day   = g_adj_params.mfg_day;

    /* ---------- Status & Error Flags ---------- */
    g_stage_hb.status_flags = Stage_GetStatusFlags();
    g_stage_hb.error_flags  = Stage_GetErrorFlags();

    /* ---------- Motor Parameters ---------- */
    g_stage_hb.motor_hold_current = g_adj_params.x_hold_current_pct;
    /* ˵����
     * - �㵱ǰ�� XY һ��ģ��
     * - ������ֻ��һ�� motor_hold_current
     * - ����ѡ X ��Ϊ��������������ɸ��? max(X,Y)��
     */

    /* ---------- Position ---------- */
    g_stage_hb.x_pos = (int16_t)g_adj_params.x_position_step;
    g_stage_hb.y_pos = (int16_t)g_adj_params.y_position_step;

    /* ---------- Motor Runtime State ---------- */
    g_stage_hb.x_cur = (int16_t)g_adj_params.x_run_current;
    g_stage_hb.y_cur = (int16_t)g_adj_params.y_run_current;

    /* ---------- Runtime Info ---------- */
    g_stage_hb.motor_speed = (uint16_t)(g_adj_params.x_step_freq_hz);    
    /* 200 step/s = 20
     * 500 step/s = 50
     */

    /* ---------- Integrity ---------- */
    g_stage_hb.crc8 = 0;   // V1.2 Ԥ������ǰ�� 0
}


extern FDCAN_HandleTypeDef hfdcan1;

static void Stage_Heartbeat_Send(void)
{
    FDCAN_TxHeaderTypeDef txHeader;

    txHeader.Identifier          = STAGE_HEARTBEAT_CAN_ID;
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = FDCAN_DLC_BYTES_48;
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_ON;
    txHeader.FDFormat            = FDCAN_FD_CAN;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;

    if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) > 0)
    {
        HAL_FDCAN_AddMessageToTxFifoQ(
            &hfdcan1,
            &txHeader,
            (uint8_t *)&g_stage_hb
        );
    }
}


