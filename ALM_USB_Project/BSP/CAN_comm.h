#pragma once



#include "main.h"

#include "fdcan.h"

#include <stdint.h>



#define CAN_ID_HEARTBEAT       0x203
#define CAN_HEARTBEAT_LEN      48
#define CAN_ID_ADJ_STATUS_RPT  0x121   // Module -> Device 状态上报
#define CAN_ID_DEVICE_NAME		 0x124

/* Parameters (Device -> Module / Module -> Device) */
#define CAN_ID_PARAM_SET        0x122  /* Device -> Module : Set Speed / Current / Hold */
#define CAN_ID_PARAM_FEEDBACK   0x123  /* Module -> Device : Param Readback / Ack */

/* Parameter IDs used with CAN_ID_PARAM_SET / CAN_ID_PARAM_FEEDBACK
 * Payload (CAN FD, 12 bytes):
 *   Byte0-3  : NodeID (LE)
 *   Byte4    : Axis (0=X, 1=Y, 0xFF=Global)
 *   Byte5    : ParamID
 *   Byte6-7  : Reserved
 *   Byte8-11 : Value (uint32, LE)
 */
typedef enum
{
    ADJ_PARAM_X_CURRENT    = 0x01,
    ADJ_PARAM_Y_CURRENT    = 0x02,
    ADJ_PARAM_HOLD_CURRENT = 0x03,   /* 0-100 (%) */
    ADJ_PARAM_SPEED        = 0x04    /* step/s */
} Adjuster_ParamId_t;

typedef struct
{
    uint32_t NodeID;       // Byte0-3
    uint8_t  Axis;         // Byte4 (0=X, 1=Y, 0xFF=Global)
    uint8_t  ParamID;      // Byte5
    uint16_t Value;        // Byte6-7 (uint16, LE)
} CAN_ParamCmd_t;

void CAN_Send_ParamSet(uint32_t node_id, uint8_t axis, uint8_t param_id, uint16_t value);

#define CAN_STATUS_FLAG_BUSY   (1u << 0)
#define CAN_STATUS_FLAG_ERROR  (1u << 1)

#define MAX_SUB_MODULES     16


typedef enum

{

    MODULE_LASER_DRIVER = 0x10,

    MODULE_GALVO        = 0x11,

    MODULE_ADJUSTER     = 0x12,



    MODULE_FAN          = 0x20,

    MODULE_DEFOGGER     = 0x21,

    MODULE_DEHUMIDIFIER = 0x22,

    MODULE_HEATER       = 0x23

} ModuleType_t;



/* ---------- CAN 心跳解析结构（瞬时） ---------- */
typedef struct
{
    /* ---------- Identity ---------- */
    uint8_t  ModuleType;
    uint32_t NodeID;
    char     DeviceName[9];   // 8 + '\0'

    /* ---------- Firmware ---------- */
    uint8_t  fw_hw;
    uint8_t  fw_dd;
    uint8_t  fw_mm;
    uint8_t  fw_yy;

    /* ---------- Manufacture ---------- */
    uint8_t  mfg_year;
    uint8_t  mfg_month;
    uint8_t  mfg_day;

    /* ---------- Status ---------- */
    uint8_t  status_flags;
    uint8_t  error_flags;

    /* ---------- Motor Parameters ---------- */
    uint8_t  hold_current;    // 0–100 %

    /* ---------- Position ---------- */
    int16_t  x_pos;
    int16_t  y_pos;

    /* ---------- Runtime ---------- */
    uint8_t  x_cur;           // 0–100 %
    uint8_t  y_cur;           // 0–100 %
    uint8_t  heartbeat_cnt;
    uint16_t motor_speed;     // step/s (uint16, LE)

    /* ---------- Integrity ---------- */
    uint8_t  crc8;            // CRC8 (V1.2 可填 0，预留)

} CAN_Heartbeat_t;

/* ---------- 系统内模块表（长期有效） ---------- */

typedef struct
{
    uint8_t  valid;

    uint8_t  module_index;
    uint8_t  module_type;

    uint32_t node_id;
    uint32_t serial;
    uint32_t fw_version;

    char     device_name[8];

    uint8_t  mfg_year;
    uint8_t  mfg_month;
    uint8_t  mfg_day;

    uint32_t last_seen_tick;

    /* ======== V2 新增参数缓存 ======== */

    uint16_t x_current;      // 单位：0.1A（放大10×）
    uint16_t y_current;      // 单位：0.1A（放大10×）
    uint8_t  hold_current;   // 0–100 (%)
    uint16_t speed;          // step/s

    int32_t  x_pos;          // step
    int32_t  y_pos;          // step

} SubModule_t;


extern SubModule_t g_modules[MAX_SUB_MODULES];

extern uint8_t     g_module_count;



void CAN_Parse_Heartbeat(const uint8_t *data, CAN_Heartbeat_t *hb);

void Module_Update_From_Heartbeat(const CAN_Heartbeat_t *hb);

void Module_Heartbeat_Timeout_Check(void);

void CAN_Send_SetModuleName(uint32_t node_id, const uint8_t name[8]);

void CAN_Send_FD_Frame(uint16_t std_id, const uint8_t *data, uint8_t len);



