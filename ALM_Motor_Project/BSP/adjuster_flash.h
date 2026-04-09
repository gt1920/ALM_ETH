#pragma once

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* ================================
 * Flash Parameter Version
 * ================================ */
#define ADJUSTER_PARAM_VERSION   0x0001u

/* ================================
 * Flash Magic
 * ================================ */
#define ADJUSTER_FLASH_MAGIC     0x41504A41u   // 'A''P''J''A'

/* ================================
 * Module Type
 * ================================ */
#define MODULE_ADJUSTER          0x12

/* ================================
 * Adjuster Parameters Structure
 * ================================ */
typedef struct
{
    /* -------- Flash Header -------- */
    uint32_t magic;              // 魔数，用于判断 Flash 是否已初始化
    uint16_t param_version;      // 参数结构版本
    uint16_t reserved0;

    /* -------- Device Identity -------- */
    uint32_t node_id;            // NodeID

    char     device_name[8];     // 设备名（固定 8 字节，无 '\0'）

    uint8_t  pcb_revision;       // PCB 版本号（01,02,...）
    uint8_t  fw_day;             // 固件编译日
    uint8_t  fw_month;           // 固件编译月
    uint8_t  fw_year;            // 固件编译年（25=2025）

    uint8_t  module_type;        // MODULE_ADJUSTER
    uint8_t  identity_reserved[3];

    /* -------- Manufacture Info -------- */
    uint8_t  mfg_year;
    uint8_t  mfg_month;
    uint8_t  mfg_day;
    uint8_t  mfg_reserved;

    /* -------- Motor Configuration -------- */
    uint16_t x_run_current;      // X 轴运行电流
    uint16_t y_run_current;      // Y 轴运行电流

    uint8_t  x_hold_current_pct; // X 保持电流百分比（0–100）
    uint8_t  y_hold_current_pct; // Y 保持电流百分比（0–100）

    uint16_t x_step_freq_hz;     // X 轴步进频率
    uint16_t y_step_freq_hz;     // Y 轴步进频率

    uint8_t  x_dir_invert;       // X 方向反转
    uint8_t  y_dir_invert;       // Y 方向反转
    uint16_t motor_reserved;

    /* -------- Runtime Snapshot -------- */
    int32_t  x_position_step;    // X 当前 / 剩余步数
    int32_t  y_position_step;    // Y 当前 / 剩余步数

    uint8_t  last_shutdown_reason;
    uint8_t  runtime_reserved[3];

    /* -------- CRC -------- */
    uint32_t crc32;              // CRC32（不包含自身）

} Adjuster_Params_t;

extern Adjuster_Params_t g_adj_params;

extern volatile uint8_t g_param_dirty;

extern volatile uint32_t g_param_dirty_tick;

#define PARAM_FLASH_DELAY_MS   5000u

/* ================================
 * Public API
 * ================================ */

void Adjuster_MarkParamDirty(void);


/* 初始化：上电调用 */
bool Adjuster_Flash_Init(Adjuster_Params_t *params);

/* 设置默认参数（全新 MCU / 工厂写入） */
void Adjuster_Flash_SetDefault(Adjuster_Params_t *params);

/* 从 Flash 读取 */
void Adjuster_Flash_Read(Adjuster_Params_t *params);

/* 写入 Flash（自动计算 CRC） */
bool Adjuster_Flash_Write(Adjuster_Params_t *params);

/* CRC 校验 */
bool Adjuster_Flash_CheckCRC(const Adjuster_Params_t *params);

/**
 * @brief 设置模块名称（8 字节 ASCII）
 * @param name  指向 8 字节名称缓冲区（不要求 '\0'）
 * @return true 成功
 *         false 参数非法
 */
bool Adjuster_SetDeviceName(const uint8_t name[8]);

