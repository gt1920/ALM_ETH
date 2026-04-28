#ifndef __CAN_COMM_H__
#define __CAN_COMM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "fdcan.h"
#include <stdint.h>

/* ---- CAN ID definitions ---- */
#define CAN_ID_HEARTBEAT       0x203
#define CAN_HEARTBEAT_LEN      48
#define CAN_ID_ADJ_STATUS_RPT  0x121   /* Module -> Device */
#define CAN_ID_DEVICE_NAME     0x124

#define CAN_ID_PARAM_SET       0x122   /* Device -> Module */
#define CAN_ID_PARAM_FEEDBACK  0x123   /* Module -> Device */

/* ---- Parameter IDs ---- */
typedef enum
{
    ADJ_PARAM_X_CURRENT    = 0x01,
    ADJ_PARAM_Y_CURRENT    = 0x02,
    ADJ_PARAM_HOLD_CURRENT = 0x03,
    ADJ_PARAM_SPEED        = 0x04
} Adjuster_ParamId_t;

typedef struct
{
    uint32_t NodeID;
    uint8_t  Axis;
    uint8_t  ParamID;
    uint16_t Value;
} CAN_ParamCmd_t;

/* ---- Status flags ---- */
#define CAN_STATUS_FLAG_BUSY   (1u << 0)
#define CAN_STATUS_FLAG_ERROR  (1u << 1)

/* ---- Module table ---- */
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

/* CAN heartbeat parsed struct */
typedef struct
{
    uint8_t  ModuleType;
    uint32_t NodeID;
    char     DeviceName[9];

    uint8_t  fw_hw;
    uint8_t  fw_dd;
    uint8_t  fw_mm;
    uint8_t  fw_yy;

    uint8_t  mfg_year;
    uint8_t  mfg_month;
    uint8_t  mfg_day;

    uint8_t  status_flags;
    uint8_t  error_flags;

    uint8_t  hold_current;

    int16_t  x_pos;
    int16_t  y_pos;

    uint8_t  x_cur;
    uint8_t  y_cur;
    uint8_t  heartbeat_cnt;
    uint16_t motor_speed;

    uint8_t  crc8;
} CAN_Heartbeat_t;

/* Persistent module table entry */
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

    uint16_t x_current;
    uint16_t y_current;
    uint8_t  hold_current;
    uint16_t speed;

    int32_t  x_pos;
    int32_t  y_pos;
} SubModule_t;

extern SubModule_t g_modules[MAX_SUB_MODULES];
extern uint8_t     g_module_count;

/* ---- API ---- */
void CAN_Parse_Heartbeat(const uint8_t *data, CAN_Heartbeat_t *hb);
void Module_Update_From_Heartbeat(const CAN_Heartbeat_t *hb);
void Module_Heartbeat_Timeout_Check(void);
void CAN_Send_SetModuleName(uint32_t node_id, const uint8_t name[8]);
void CAN_Send_ParamSet(uint32_t node_id, uint8_t axis, uint8_t param_id, uint16_t value);
void CAN_Send_FD_Frame(uint16_t std_id, const uint8_t *data, uint8_t len);
void Pack_USB_NodeID_MSB_To_CAN(uint32_t node_id, uint8_t out[4]);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_COMM_H__ */
