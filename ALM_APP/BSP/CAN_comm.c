#include "CAN_comm.h"
#include "module_upgrade.h"
#include "comm_protocol.h"
#include "eth_send_queue.h"

#include <string.h>
#include <stdint.h>

#define MODULE_HEARTBEAT_TIMEOUT_MS 5000u

/* =========================================================
 * Globals
 * ========================================================= */
uint8_t CAN_rxData[64];

SubModule_t g_modules[MAX_SUB_MODULES];
uint8_t g_module_count = 0;

static CAN_Heartbeat_t hb;

static uint16_t g_motion_report_seq = 0;

/* =========================================================
 * Helpers
 * ========================================================= */
static uint32_t u32_le(const uint8_t *p)
{
    return  ((uint32_t)p[0])        |
            ((uint32_t)p[1] << 8)   |
            ((uint32_t)p[2] << 16)  |
            ((uint32_t)p[3] << 24);
}

static int find_module_by_nodeid(uint32_t node_id)
{
    for (int i = 0; i < MAX_SUB_MODULES; i++)
        if (g_modules[i].valid && g_modules[i].node_id == node_id)
            return i;
    return -1;
}

static int allocate_module_slot(void)
{
    for (int i = 0; i < MAX_SUB_MODULES; i++)
        if (!g_modules[i].valid)
            return i;
    return -1;
}

static void recompute_module_count(void)
{
    uint8_t cnt = 0;
    for (int i = 0; i < MAX_SUB_MODULES; i++)
        if (g_modules[i].valid)
            cnt++;
    g_module_count = cnt;
}

static void clear_module_slot(int idx)
{
    if (idx < 0 || idx >= MAX_SUB_MODULES)
        return;
    memset(&g_modules[idx], 0, sizeof(g_modules[idx]));
}

/* =========================================================
 * Heartbeat
 * ========================================================= */
void CAN_Parse_Heartbeat(const uint8_t *data, CAN_Heartbeat_t *hb)
{
    hb->ModuleType = data[0];
    hb->NodeID     = u32_le(&data[1]);

    memcpy(hb->DeviceName, &data[5], 8);
    hb->DeviceName[8] = '\0';

    hb->fw_hw = data[13];
    hb->fw_dd = data[14];
    hb->fw_mm = data[15];
    hb->fw_yy = data[16];

    hb->mfg_year  = data[17];
    hb->mfg_month = data[18];
    hb->mfg_day   = data[19];

    hb->status_flags = data[20];
    hb->error_flags  = data[21];

    hb->hold_current = data[22];

    hb->x_pos = (int16_t)(
        ((int16_t)data[23]) |
        ((int16_t)data[24] << 8)
    );

    hb->y_pos = (int16_t)(
        ((int16_t)data[25]) |
        ((int16_t)data[26] << 8)
    );

    hb->x_cur = data[27];
    hb->y_cur = data[28];

    hb->heartbeat_cnt = data[29];
    hb->motor_speed   = (uint16_t)data[30] | ((uint16_t)data[31] << 8);

    hb->crc8 = data[47];
}

void Module_Update_From_Heartbeat(const CAN_Heartbeat_t *hb)
{
    int idx = find_module_by_nodeid(hb->NodeID);
    if (idx < 0)
    {
        idx = allocate_module_slot();
        if (idx < 0)
            return;
    }

    SubModule_t *m = &g_modules[idx];

    m->module_index = (uint8_t)idx;
    m->module_type  = hb->ModuleType;
    m->node_id      = hb->NodeID;
    m->serial       = hb->NodeID;

    memcpy(m->device_name, hb->DeviceName, 8);

    m->fw_version =
        ((uint32_t)hb->fw_hw      ) |
        ((uint32_t)hb->fw_dd <<  8) |
        ((uint32_t)hb->fw_mm << 16) |
        ((uint32_t)hb->fw_yy << 24);

    m->mfg_year  = hb->mfg_year;
    m->mfg_month = hb->mfg_month;
    m->mfg_day   = hb->mfg_day;

    m->hold_current = hb->hold_current;
    m->x_pos = hb->x_pos;
    m->y_pos = hb->y_pos;
    m->x_current = hb->x_cur;
    m->y_current = hb->y_cur;
    m->speed = hb->motor_speed;

    m->last_seen_tick = HAL_GetTick();
    m->valid = 1;

    recompute_module_count();
}

void Module_Heartbeat_Timeout_Check(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t removed_any = 0;

    for (int i = 0; i < MAX_SUB_MODULES; i++)
    {
        if (!g_modules[i].valid)
            continue;

        uint32_t elapsed = now - g_modules[i].last_seen_tick;
        if (elapsed > MODULE_HEARTBEAT_TIMEOUT_MS)
        {
            clear_module_slot(i);
            removed_any = 1;
        }
    }

    if (removed_any)
        recompute_module_count();
}

/* =========================================================
 * ETH: XY Remaining Step Report
 * ========================================================= */
static void ETH_Report_RemainingStep_XY(uint32_t node_id,
                                        uint32_t x_remain,
                                        uint32_t y_remain)
{
    uint8_t  tx_buf[PROTO_FRAME_LEN] = {0};
    uint16_t seq = g_motion_report_seq++;

    tx_buf[0] = PROTO_FRAME_HEAD;
    tx_buf[1] = CMD_MOTION;
    tx_buf[2] = DIR_MCU_TO_PC;
    tx_buf[3] = (uint8_t)(seq & 0xFF);
    tx_buf[4] = (uint8_t)(seq >> 8);

    tx_buf[5] = (uint8_t)(node_id & 0xFF);
    tx_buf[6] = (uint8_t)((node_id >> 8) & 0xFF);
    tx_buf[7] = (uint8_t)((node_id >> 16) & 0xFF);
    tx_buf[8] = (uint8_t)((node_id >> 24) & 0xFF);

    tx_buf[9]  = 0x00;
    tx_buf[10] = 0x00;
    tx_buf[11] = 0x00;

    tx_buf[12] = (uint8_t)(x_remain & 0xFF);
    tx_buf[13] = (uint8_t)((x_remain >> 8) & 0xFF);
    tx_buf[14] = (uint8_t)((x_remain >> 16) & 0xFF);
    tx_buf[15] = (uint8_t)((x_remain >> 24) & 0xFF);

    tx_buf[16] = (uint8_t)(y_remain & 0xFF);
    tx_buf[17] = (uint8_t)((y_remain >> 8) & 0xFF);
    tx_buf[18] = (uint8_t)((y_remain >> 16) & 0xFF);
    tx_buf[19] = (uint8_t)((y_remain >> 24) & 0xFF);

    ETH_Send_Queue(tx_buf, PROTO_FRAME_LEN);
}

/* =========================================================
 * FDCAN Rx Callback
 * ========================================================= */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t it)
{
    if (hfdcan->Instance != FDCAN1)
        return;

    if ((it & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0u)
        return;

    FDCAN_RxHeaderTypeDef rxHeader;
    if (HAL_FDCAN_GetRxMessage(hfdcan,
                              FDCAN_RX_FIFO0,
                              &rxHeader,
                              CAN_rxData) != HAL_OK)
        return;

    if (rxHeader.IdType != FDCAN_STANDARD_ID)
        return;

    /* ---------- Heartbeat ---------- */
    if (rxHeader.Identifier == CAN_ID_HEARTBEAT &&
        rxHeader.DataLength >= FDCAN_DLC_BYTES_48)
    {
        CAN_Parse_Heartbeat(CAN_rxData, &hb);
        Module_Update_From_Heartbeat(&hb);
        return;
    }

    /* ---------- Adjuster status 0x121 ---------- */
    if (rxHeader.Identifier == CAN_ID_ADJ_STATUS_RPT &&
        rxHeader.DataLength >= FDCAN_DLC_BYTES_12)
    {
        uint32_t node_id  = u32_le(&CAN_rxData[0]);
        uint32_t x_remain = u32_le(&CAN_rxData[4]);
        uint32_t y_remain = u32_le(&CAN_rxData[8]);

        int idx = find_module_by_nodeid(node_id);
        if (idx >= 0)
        {
            g_modules[idx].x_pos = (int32_t)x_remain;
            g_modules[idx].y_pos = (int32_t)y_remain;
        }

        ETH_Report_RemainingStep_XY(node_id, x_remain, y_remain);
        return;
    }

    /* ---------- Param feedback 0x123 ---------- */
    if (rxHeader.Identifier == CAN_ID_PARAM_FEEDBACK &&
        rxHeader.DataLength >= FDCAN_DLC_BYTES_12)
    {
        uint32_t node_id = u32_le(&CAN_rxData[0]);
        uint8_t  axis    = CAN_rxData[4];
        uint8_t  pid     = CAN_rxData[5];
        uint32_t value   = u32_le(&CAN_rxData[8]);

        int idx = find_module_by_nodeid(node_id);
        if (idx >= 0)
        {
            SubModule_t *m = &g_modules[idx];

            switch ((Adjuster_ParamId_t)pid)
            {
                case ADJ_PARAM_X_CURRENT:
                    if (axis == 0)
                        m->x_current = (uint16_t)value;
                    break;
                case ADJ_PARAM_Y_CURRENT:
                    if (axis == 1)
                        m->y_current = (uint16_t)value;
                    break;
                case ADJ_PARAM_HOLD_CURRENT:
                    m->hold_current = (uint8_t)value;
                    break;
                case ADJ_PARAM_SPEED:
                    m->speed = (uint16_t)value;
                    break;
                default:
                    break;
            }
        }

        ETH_Report_ParamFeedback(node_id, axis, pid, value);
        return;
    }

    /* ---------- Module upgrade RESP 0x305 ---------- */
    if (rxHeader.Identifier == 0x305U &&
        rxHeader.DataLength >= FDCAN_DLC_BYTES_12)
    {
        uint32_t node_id    = u32_le(&CAN_rxData[0]);
        uint8_t  status     = CAN_rxData[4];
        uint32_t last_off   = u32_le(&CAN_rxData[8]);
        MUR_OnCanResp(node_id, status, last_off);
        return;
    }
}

/* =========================================================
 * CAN Send helpers
 * ========================================================= */
void Pack_USB_NodeID_MSB_To_CAN(uint32_t node_id, uint8_t out[4])
{
    out[0] = (uint8_t)((node_id >> 24) & 0xFF);
    out[1] = (uint8_t)((node_id >> 16) & 0xFF);
    out[2] = (uint8_t)((node_id >> 8)  & 0xFF);
    out[3] = (uint8_t)(node_id & 0xFF);
}

void CAN_Send_SetModuleName(uint32_t node_id, const uint8_t name[8])
{
    uint8_t tx_buf[16] = {0};
    Pack_USB_NodeID_MSB_To_CAN(node_id, tx_buf);
    memcpy(&tx_buf[4], name, 8);
    CAN_Send_FD_Frame(CAN_ID_DEVICE_NAME, tx_buf, 16);
}

void CAN_Send_ParamSet(uint32_t node_id, uint8_t axis, uint8_t param_id, uint16_t value)
{
    uint8_t tx_buf[12] = {0};

    tx_buf[0] = (uint8_t)(node_id & 0xFF);
    tx_buf[1] = (uint8_t)((node_id >> 8) & 0xFF);
    tx_buf[2] = (uint8_t)((node_id >> 16) & 0xFF);
    tx_buf[3] = (uint8_t)((node_id >> 24) & 0xFF);

    tx_buf[4] = axis;
    tx_buf[5] = param_id;
    tx_buf[6] = (uint8_t)(value & 0xFF);
    tx_buf[7] = (uint8_t)((value >> 8) & 0xFF);

    CAN_Send_FD_Frame(CAN_ID_PARAM_SET, tx_buf, 12);
}

static uint32_t CAN_Encode_DLC(uint8_t len)
{
    if (len <= 8)   return (uint32_t)len << 16;
    if (len <= 12)  return FDCAN_DLC_BYTES_12;
    if (len <= 16)  return FDCAN_DLC_BYTES_16;
    if (len <= 20)  return FDCAN_DLC_BYTES_20;
    if (len <= 24)  return FDCAN_DLC_BYTES_24;
    if (len <= 32)  return FDCAN_DLC_BYTES_32;
    if (len <= 48)  return FDCAN_DLC_BYTES_48;
    return FDCAN_DLC_BYTES_64;
}

void CAN_Send_FD_Frame(uint16_t std_id,
                       const uint8_t *data,
                       uint8_t len)
{
    FDCAN_TxHeaderTypeDef txHeader;
    uint8_t tx_buf[64];

    if (len > 64)
        len = 64;

    memset(tx_buf, 0, sizeof(tx_buf));
    memcpy(tx_buf, data, len);

    txHeader.Identifier          = std_id;
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = CAN_Encode_DLC(len);
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_ON;
    txHeader.FDFormat            = FDCAN_FD_CAN;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;

    while (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) == 0)
    {
        /* wait for free TX slot */
    }

    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, tx_buf);
}
