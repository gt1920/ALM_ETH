
#pragma once
#include "main.h"

/* ---------------- Module Type ---------------- */
#define MODULE_LASER_DRIVER   0x10
#define MODULE_GALVO          0x11
#define MODULE_ADJUSTER       0x12

#define MODULE_FAN            0x20
#define MODULE_DEFOGGER       0x21
#define MODULE_DEHUMIDIFIER   0x22
#define MODULE_HEATER         0x23

/* ---------------- CAN ---------------- */
#define STAGE_HEARTBEAT_CAN_ID    0x203
#define STAGE_HEARTBEAT_DLC       48

#pragma pack(push, 1)
typedef struct
{
    /* ---------- Identity ---------- */

    uint8_t  module_type; // 模块类型，例如 MODULE_ADJUSTER (0x12)

    uint32_t node_id;     // 节点唯一 ID（由 STM32 UID 派生，LSB first）

    char     name[8];     // 模块名称，ASCII 编码，'\0' 填充（如 "ADJ_XY"）

    /* ---------- Firmware Version ---------- */

    uint8_t  fw_hw; // 硬件版本号

    uint8_t  fw_dd; // 固件日

    uint8_t  fw_mm; // 固件月

    uint8_t  fw_yy; // 固件年

    /* ---------- Manufacture Date ---------- */

    uint8_t  mfg_year;   // 出厂年份（25 = 2025）

    uint8_t  mfg_month;  // 出厂月份（1–12）

    uint8_t  mfg_day;    // 出厂日期（1–31）

    /* ---------- Status & Error ---------- */

    uint8_t  status_flags;  // 运行状态位图（RUNNING / DONE / HOMED / BUSY / ESTOP 等）

    uint8_t  error_flags;   // 错误状态位图（粘性错误，需要复位或清除）

    /* ---------- Motor Parameters ---------- */

    uint8_t  motor_hold_current;   	// 电机保持电流百分比（0–100）
																		// 实际保持电流 = 设定运行电流 × (value / 100)

    /* ---------- Position ---------- */

    int16_t  x_pos; // X 轴当前位置（单位：step 或内部坐标）

    int16_t  y_pos; // Y 轴当前位置（单位：step 或内部坐标）

    /* ---------- Motor Runtime State ---------- */

    uint8_t  x_cur; // X 轴当前运行电流百分比（0–100，相对于设定电流）

    uint8_t  y_cur; // Y 轴当前运行电流百分比（0–100，相对于设定电流）

    /* ---------- Runtime Info ---------- */

    uint8_t  heartbeat_cnt; // 心跳计数器（递增，用于 PC 检测丢包）

    uint16_t motor_speed;   // 电机速度（单位：step/s，升级为 16-bit）

    /* ---------- Reserved ---------- */

    uint8_t  reserved_1;
    uint8_t  reserved_2;
    uint8_t  reserved_3;
    uint8_t  reserved_4;
    uint8_t  reserved_5;
    uint8_t  reserved_6;
    uint8_t  reserved_7;
    uint8_t  reserved_8;
    uint8_t  reserved_9;
    uint8_t  reserved_10;
    uint8_t  reserved_11;
    uint8_t  reserved_12;
    uint8_t  reserved_13;
    uint8_t  reserved_14;
    uint8_t  reserved_15;

    /* ---------- Integrity (Byte 48) ---------- */

    uint8_t  crc8;               
    // CRC8 校验（V1.2 可填 0，预留）
    
} Stage_Heartbeat_t;


#pragma pack(pop)

typedef char _assert_hb_size[(sizeof(Stage_Heartbeat_t) == 48) ? 1 : -1];
extern Stage_Heartbeat_t g_stage_hb;

void Stage_Heartbeat_Init(void);

void Status_Report_Task(void);
