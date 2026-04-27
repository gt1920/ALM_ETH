#ifndef __CAN_COMM_H
#define __CAN_COMM_H

#include "main.h"

// xxxxCAN命令
#define CMD_SET_X_DIRECTION   0x10
#define CMD_SET_Y_DIRECTION   0x11
#define CMD_TOGGLE_LED        0x20
#define CMD_SET_FREQUENCY     0x15
#define CMD_SET_CENTER_POINT  0x30
#define CMD_SET_LIMIT_RANGE   0x31
#define CMD_FORCE_SET_POSITION 0x33


/* ================= Adjuster Module CAN IDs ================= */

/* Motion */
#define CAN_ID_MOTION_CONTROL     0x120  // Device -> Module : Motion Command (step)
#define CAN_ID_STATUS_FEEDBACK    0x121  // Module -> Device : Status & Remaining Step

/* Parameters */
#define CAN_ID_PARAM_SET          0x122  // Device -> Module : Speed / Current
#define CAN_ID_PARAM_FEEDBACK     0x123  // Module -> Device : Param Readback

/* Identity */
#define CAN_ID_DEVICE_NAME        0x124  // Device -> Module : Set Device Name
#define CAN_ID_HEARTBEAT          0x203  // Module -> Device : Identity & Heartbeat

/* Fault / Event */
#define CAN_ID_ERROR_REPORT       0x125  // Module -> Device : Error / Fault
#define CAN_ID_EVENT_REPORT       0x126  // Module -> Device : Motion Done / Event

// Motion Control Flags
#define MOTION_FLAG_DIRECTION_MASK  0x01  // Byte5 bit0: Direction (0=reverse, 1=forward)
#define MOTION_FLAG_IMMEDIATE_MASK  0x02  // Byte5 bit1: Immediate (1=execute immediately)

// Axis Mask
#define AXIS_X  0x01
#define AXIS_Y  0x02

// Status definitions
#define STATUS_IDLE             0x00
#define STATUS_RUNNING          0x01
#define STATUS_ERROR            0x02

// Error Code definitions
#define ERROR_NONE              0x00
#define ERROR_INVALID_AXIS      0x01
#define ERROR_SOFT_LIMIT        0x02
#define ERROR_HARDWARE          0x03

#define RENAME_HEAD_OFFSET        0u
#define RENAME_CMD_OFFSET         1u
#define RENAME_DIR_OFFSET         2u
#define RENAME_SEQ_L_OFFSET       3u
#define RENAME_SEQ_H_OFFSET       4u
#define RENAME_LEN_OFFSET         5u

#define RENAME_NAME_OFFSET        8u   // <-- 这就是你要的 OFFSET_NAME

/* ================= Adjuster Parameter IDs ================= */
/*
 * Used with CAN_ID_PARAM_SET (0x122)
 *
 * Byte layout:
 *   Byte0-3 : NodeID
 *   Byte4   : Axis (0 = X, 1 = Y, 0xFF = Global)
 *   Byte5   : Param ID (below)
 *   Byte6-7 : Reserved
 *   Byte8-11: Value (uint32_t)
 */

typedef enum
{
    ADJ_PARAM_X_CURRENT    = 0x01,
    ADJ_PARAM_Y_CURRENT    = 0x02,
    ADJ_PARAM_HOLD_CURRENT = 0x03,
    ADJ_PARAM_SPEED        = 0x04,

    ADJ_PARAM_SAVE         = 0x0F,   // << 新增：保存当前参数到 Flash
		ADJ_PARAM_READ_ALL     = 0x10   // << 新增：请求上报全部参数	
} Adjuster_ParamId_t;

/* ================= Parameter ACK Status ================= */
#define PARAM_ACK_OK              0x00
#define PARAM_ACK_INVALID_AXIS    0x01
#define PARAM_ACK_INVALID_PARAM   0x02
#define PARAM_ACK_OUT_OF_RANGE    0x03
#define PARAM_ACK_INTERNAL_ERR    0x04

typedef struct
{
    uint32_t NodeID;       // Byte0-3
    uint8_t  Axis;         // Byte4 (0=X, 1=Y, 0xFF=Global)
    uint8_t  ParamID;      // Byte5
    uint16_t Value;        // Byte6-7 (uint16, LE)
} CAN_ParamCmd_t;

/* ================= Parameter Limits ================= */
#define ADJ_SPEED_MIN_STEP_S     100
#define ADJ_SPEED_MAX_STEP_S     20000

#define ADJ_RUN_CURRENT_MIN_MA   100u
#define ADJ_RUN_CURRENT_MAX_MA   1000u

#define ADJ_HOLD_CURRENT_MIN_PCT 0u
#define ADJ_HOLD_CURRENT_MAX_PCT 100u

// CAN消息结构体
typedef struct {
    uint16_t cmd;           // 命令为 uint16_t型
    uint8_t data[8];        // 数据缓冲
} CAN_Message_t;

// Motion Control Command (0x120) - 8 Bytes Classic CAN
typedef struct {
    uint32_t TargetNodeID;      // Byte0-3: Target Node ID
    uint8_t AxisMask;           // Byte4: Axis Mask (bit0=X, bit1=Y)
    uint8_t Flags;              // Byte5: Flags (bit0=Direction, bit1=Immediate)
    uint16_t Reserved;          // Byte6-7: Reserved
} MotionControlCommand_Classic_t;

// Motion Control Command - Extended Payload (12 Bytes CAN FD)
typedef struct {
    uint32_t TargetNodeID;      // Byte0-3: Target Node ID
    uint8_t AxisMask;           // Byte4: Axis Mask
    uint8_t Flags;              // Byte5: Flags
    uint16_t Reserved;          // Byte6-7: Reserved
    int32_t StepCount;          // Byte8-11: Step Count (positive=forward, negative=reverse)
} MotionControlCommand_Extended_t;

// Status Feedback (0x121) - 16 Bytes CAN FD
typedef struct {
    uint32_t SourceNodeID;      // Byte0-3: Source Node ID
    uint8_t AxisMask;           // Byte4: Axis Mask
    uint8_t Status;             // Byte5: Status (0=idle, 1=running, 2=error)
    uint8_t ErrorCode;          // Byte6: Error Code
    uint8_t Reserved;           // Byte7: Reserved
    int32_t RemainingSteps;     // Byte8-11: Remaining Steps
    int32_t CurrentPosition;    // Byte12-15: Current Position
} StatusFeedback_t;

uint8_t CAN_Receive_Frame(uint16_t *rx_id, uint8_t *rx_buf, uint8_t *rx_len);
uint8_t CANFD_Send(uint16_t std_id, uint8_t *data, uint8_t len);

// CAN初始化函数
void CAN_Init(void);

// CAN接收中断回调
void CAN_Receive_Callback(void);

// 处理CAN数据和执行相应功能
void CAN_Process_Message(CAN_Message_t *msg);

// 各命令的处理函数
void Set_Device_Name(CAN_Message_t *msg);
void Motor_Control(CAN_Message_t *msg);
void Set_Frequency(CAN_Message_t *msg);
void Set_Center_Point(CAN_Message_t *msg);
void Set_Limit_Range(CAN_Message_t *msg);
void Force_Set_Position(CAN_Message_t *msg);
void Report_Status(CAN_Message_t *msg);
void Handle_ACK_ERR(CAN_Message_t *msg);
void Heartbeat(void);

// Motion Control and Status Feedback handlers
void Send_Status_Feedback(uint32_t node_id, uint8_t axis_mask, uint8_t status, uint8_t error_code, int32_t remaining_steps, int32_t current_position);

uint8_t CAN_Send_FD(uint16_t std_id, uint8_t *data, uint8_t len);

#endif // __CAN_COMM_H
