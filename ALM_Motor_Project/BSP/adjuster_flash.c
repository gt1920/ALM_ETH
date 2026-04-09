#include "adjuster_flash.h"
#include <string.h>

#include "bsp_flash.h"
#include "adjuster_can.h"
#include "Motor.h"

extern void Stage_Heartbeat_TriggerNow(void);

/* ================================
 * Flash Address Configuration
 * ================================ */

/*
 * 注：存储的地址可以在后续统一规划
 * 当前使用最后一页 Flash
 * STM32G0的页面大小通常为 2KB
 */
#define ADJUSTER_FLASH_ADDR   (0x0801F800u)

/* ================================
 * External Low-Level Interfaces
 * ================================ */

/*
 * 这几个函数在具体项目中会由 BSP / Flash 层来统一实现
 * 这里当作已经存在的接口。
 */
extern void     BSP_Flash_Erase(uint32_t addr);
extern void     BSP_Flash_Program(uint32_t addr, const uint8_t *buf, uint32_t len);
extern uint32_t BSP_CRC32_Calc(const uint8_t *buf, uint32_t len);

/* Debug: last HAL status from BSP_Flash_Erase() (0=HAL_OK, 1=HAL_ERROR, 2=HAL_BUSY, 3=HAL_TIMEOUT) */
volatile uint32_t g_bsp_flash_erase_status = 0xFFFFFFFFu;
/* Debug: last bank/page selected for erase */
volatile uint32_t g_bsp_flash_erase_bank = 0xFFFFFFFFu;
volatile uint32_t g_bsp_flash_erase_page = 0xFFFFFFFFu;
/* Debug: page_error output from HAL_FLASHEx_Erase() */
volatile uint32_t g_bsp_flash_erase_page_error = 0xFFFFFFFFu;
/* Debug: HAL_FLASH_GetError() after erase/program */
volatile uint32_t g_bsp_flash_last_error = 0xFFFFFFFFu;

/* Debug: last HAL status from BSP_Flash_Program() */
volatile uint32_t g_bsp_flash_program_status = 0xFFFFFFFFu;
/* Debug: first address where HAL_FLASH_Program() failed (if any) */
volatile uint32_t g_bsp_flash_program_fail_addr = 0xFFFFFFFFu;

/* STM32G0 Flash page size is 2KB; this module stores params in the last page. */
#define ADJUSTER_FLASH_PAGE_SIZE   (2048u)

Adjuster_Params_t g_adj_params;

/* ================= Flash Deferred Save Control ================= */

/* 标记是否有参数改变但未写入 Flash */
volatile uint8_t g_param_dirty = 0;

/* 最后一次参数改变的计时器（单位：ms） */
volatile uint32_t g_param_dirty_tick = 0;

/* 延迟写 Flash 时间（ms） */



static void NodeID_ToAscii8(uint32_t node_id, char out[8]);

/* ================================
 * Internal Helpers
 * ================================ */

void Adjuster_MarkParamDirty(void)
{
    g_param_dirty = 1u;
    g_param_dirty_tick = 0u;   // 重置计时器，重新计时
}


static bool is_valid_name_char(uint8_t c)
{
    return (c >= 0x20 && c <= 0x7E);
}

bool Adjuster_SetDeviceName(const uint8_t name[8])
{
    bool all_zero  = true;
    bool all_space = true;

    /* ---------- 字符校验 + 空串判断 ---------- */
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t c = name[i];

        if (c != 0x00) all_zero  = false;
        if (c != 0x20) all_space = false;

        /* 允许 0x00 作为末尾填充；非 0 则必须是可见 ASCII */
        if (c != 0x00 && !is_valid_name_char(c))
        {
            return false;   // 非法 ASCII
        }
    }

    /* ---------- 更新 RAM ---------- */
    if (all_zero || all_space)
    {
        /* PC 传入空或纯空格，则填充为 NodeID 默认名 */
        NodeID_ToAscii8(g_adj_params.node_id, g_adj_params.device_name);
    }
    else
    {
        /* 使用 PC 下发的新名称（可包含末尾的 0x00 作为填充） */
        memcpy(g_adj_params.device_name, name, 8);
    }

    /* ---------- 持久化 ---------- */
    Adjuster_Flash_Write(&g_adj_params);

    /* Send a heartbeat immediately with the updated name (no need to wait 1Hz). */
    Stage_Heartbeat_TriggerNow();

    return true;
}


static uint8_t month_str_to_num(const char *m)
{
    if (m[0] == 'J' && m[1] == 'a') return 1;   // Jan
    if (m[0] == 'F')               return 2;   // Feb
    if (m[0] == 'M' && m[2] == 'r') return 3;   // Mar
    if (m[0] == 'A' && m[1] == 'p') return 4;   // Apr
    if (m[0] == 'M' && m[2] == 'y') return 5;   // May
    if (m[0] == 'J' && m[2] == 'n') return 6;   // Jun
    if (m[0] == 'J' && m[2] == 'l') return 7;   // Jul
    if (m[0] == 'A' && m[1] == 'u') return 8;   // Aug
    if (m[0] == 'S')               return 9;   // Sep
    if (m[0] == 'O')               return 10;  // Oct
    if (m[0] == 'N')               return 11;  // Nov
    if (m[0] == 'D')               return 12;  // Dec
    return 0;
}

static void Get_Build_Date(uint8_t *day, uint8_t *month, uint8_t *year)
{
    /* __DATE__ format: "Mmm dd yyyy" */
    const char *date = __DATE__;

    /* Day */
    if (date[4] == ' ')
        *day = (uint8_t)(date[5] - '0');
    else
        *day = (uint8_t)((date[4] - '0') * 10 + (date[5] - '0'));

    /* Month */
    *month = month_str_to_num(&date[0]);

    /* Year (take last two digits) */
    *year = (uint8_t)((date[9] - '0') * 10 + (date[10] - '0'));
}



static uint32_t Adjuster_Flash_CalcCRC(const Adjuster_Params_t *p)
{
    return BSP_CRC32_Calc(
        (const uint8_t *)p,
        offsetof(Adjuster_Params_t, crc32)
    );
}

static void NodeID_ToAscii8(uint32_t node_id, char out[8])
{
    static const char hex_table[] = "0123456789ABCDEF";

    /* 注意这里使用的顺序：MSB byte 在 LSB byte */
    for (int i = 0; i < 4; i++)
    {
        uint8_t byte = (node_id >> (i * 8)) & 0xFF;

        /* 高 nibble */
        out[i * 2]     = hex_table[(byte >> 4) & 0x0F];
        /* 低 nibble */
        out[i * 2 + 1] = hex_table[byte & 0x0F];
    }
}



/* ================================
 * Default Parameters
 * ================================ */

void Adjuster_Flash_SetDefault(Adjuster_Params_t *p)
{
    memset(p, 0, sizeof(Adjuster_Params_t));

    /* Header */
    p->magic         = ADJUSTER_FLASH_MAGIC;
    p->param_version = ADJUSTER_PARAM_VERSION;

    /* Identity */
    p->node_id = Build_NodeID_From_UID();   // Litt
    NodeID_ToAscii8(p->node_id, p->device_name);

    p->pcb_revision = 0x01;
		/* Firmware build date (auto) */
		Get_Build_Date(&p->fw_day,
									 &p->fw_month,
									 &p->fw_year);

    p->module_type  = MODULE_ADJUSTER;

    /* Manufacture Date（出厂后需写入） */
    p->mfg_year  = 26;
    p->mfg_month = 12;
    p->mfg_day   = 1;

    /* Motor Defaults */
    p->x_run_current      = 20;
    p->y_run_current      = 20;
    p->x_hold_current_pct = 50;
    p->y_hold_current_pct = 50;

    p->x_step_freq_hz     = 200;
    p->y_step_freq_hz     = 200;

    p->x_dir_invert = 0;
    p->y_dir_invert = 0;

    /* Position */
    p->x_position_step = 0;
    p->y_position_step = 0;

    p->last_shutdown_reason = 0;
}

/* ================================
 * Flash Read / Write
 * ================================ */

void Adjuster_Flash_Read(Adjuster_Params_t *p)
{
    memcpy(p,
           (const void *)ADJUSTER_FLASH_ADDR,
           sizeof(Adjuster_Params_t));
}

bool Adjuster_Flash_Write(Adjuster_Params_t *p)
{
    /* Guard: parameters must fit within a single last-page slot. */
    if (sizeof(Adjuster_Params_t) > ADJUSTER_FLASH_PAGE_SIZE)
    {
        return false;
    }

    p->crc32 = Adjuster_Flash_CalcCRC(p);

    BSP_Flash_Erase(ADJUSTER_FLASH_ADDR);
    if (g_bsp_flash_erase_status != 0u)
    {
        return false;
    }

    BSP_Flash_Program(ADJUSTER_FLASH_ADDR,
                      (const uint8_t *)p,
                      sizeof(Adjuster_Params_t));

    if (g_bsp_flash_program_status != 0u)
    {
        return false;
    }

    if (g_bsp_flash_last_error != 0u)
    {
        return false;
    }

    /* Read-back verify to ensure Flash is actually updated. */
    if (memcmp((const void *)ADJUSTER_FLASH_ADDR, (const void *)p, sizeof(Adjuster_Params_t)) != 0)
    {
        return false;
    }
											
		FDCAN_NodeID = p->node_id;

    return true;
}

/* ================================
 * CRC Check
 * ================================ */

bool Adjuster_Flash_CheckCRC(const Adjuster_Params_t *p)
{
    uint32_t crc = Adjuster_Flash_CalcCRC(p);
    return (crc == p->crc32);
}

/* ================================
 * Init Entry
 * ================================ */

bool Adjuster_Flash_Init(Adjuster_Params_t *p)
{
    Adjuster_Flash_Read(p);

    if (p->magic != ADJUSTER_FLASH_MAGIC)
        goto load_default;

    if (p->param_version != ADJUSTER_PARAM_VERSION)
        goto load_default;

    if (!Adjuster_Flash_CheckCRC(p))
        goto load_default;

    return true;

load_default:
    Adjuster_Flash_SetDefault(p);
    Adjuster_Flash_Write(p);
    return false;
}
