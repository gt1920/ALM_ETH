#include "CAN_comm.h"
#include "fdcan.h"
#include "usb_comm.h"
#include "usbd_customhid.h"
#include "USB_Send_Queue.h"

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

/* motion report seq�����ڱ��Ѷ��� extern����ɾ�����У� */
static uint16_t g_motion_report_seq = 0;

/* =========================================================
 * Helpers
 * ========================================================= */
/*
 * @brief  按小端序读取 32bit 无符号整数。
 * @param  p 指向 4 字节数据的指针（LE）。
 * @return 组合后的 uint32_t。
 */
static uint32_t u32_le(const uint8_t *p)
{
    return  ((uint32_t)p[0])        |
            ((uint32_t)p[1] << 8)   |
            ((uint32_t)p[2] << 16)  |
            ((uint32_t)p[3] << 24);
}

/*
 * @brief  按 NodeID 在模块表中查找已登记模块。
 * @param  node_id 模块 NodeID。
 * @return 找到返回索引；未找到返回 -1。
 */
static int find_module_by_nodeid(uint32_t node_id)
{
    for (int i = 0; i < MAX_SUB_MODULES; i++)
        if (g_modules[i].valid && g_modules[i].node_id == node_id)
            return i;
    return -1;
}

/*
 * @brief  分配一个空闲模块槽位。
 * @return 找到返回索引；无空闲返回 -1。
 */
static int allocate_module_slot(void)
{
    for (int i = 0; i < MAX_SUB_MODULES; i++)
        if (!g_modules[i].valid)
            return i;
    return -1;
}

/*
 * @brief  重新统计当前有效模块数量。
 */
static void recompute_module_count(void)
{
    uint8_t cnt = 0;
    for (int i = 0; i < MAX_SUB_MODULES; i++)
        if (g_modules[i].valid)
            cnt++;
    g_module_count = cnt;
}

/*
 * @brief  清空指定模块槽位（等价于从 index 中剔除）。
 * @param  idx 槽位索引。
 */
static void clear_module_slot(int idx)
{
    if (idx < 0 || idx >= MAX_SUB_MODULES)
        return;
    memset(&g_modules[idx], 0, sizeof(g_modules[idx]));
}

/* =========================================================
 * Heartbeat
 * ========================================================= */
/*
 * @brief  解析 CAN 心跳帧载荷到心跳结构体。
 * @param  data   CAN 载荷（至少 20 字节）。
 * @param  hb_out 输出心跳信息。
 */
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



/*
 * @brief  收到心跳后更新/登记模块表，并刷新 last_seen_tick。
 * @param  hb_in 心跳解析结果。
 */
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

    /* ---------- Identity ---------- */
    m->module_index = (uint8_t)idx;
    m->module_type  = hb->ModuleType;

    m->node_id    = hb->NodeID;
    m->serial     = hb->NodeID;

    /* ---------- Name ---------- */
    memcpy(m->device_name, hb->DeviceName, 8);

    /* ---------- FW version（关键修复） ---------- */
    m->fw_version =
        ((uint32_t)hb->fw_hw      ) |
        ((uint32_t)hb->fw_dd <<  8) |
        ((uint32_t)hb->fw_mm << 16) |
        ((uint32_t)hb->fw_yy << 24);
		
    /* 如果你 fw_version 是另一路来的，可以不动 */
    m->mfg_year  = hb->mfg_year;
    m->mfg_month = hb->mfg_month;
    m->mfg_day   = hb->mfg_day;

    /* ---------- V2: Motor & Runtime ---------- */
    m->hold_current = hb->hold_current;

    m->x_pos = hb->x_pos;
    m->y_pos = hb->y_pos;

    /* 注意：这里是“运行电流百分比”，不是 mA */
    m->x_current = hb->x_cur;
    m->y_current = hb->y_cur;

    /* motor_speed 单位已升级为 step/s (uint16) */
    m->speed = hb->motor_speed;

    m->last_seen_tick = HAL_GetTick();
    m->valid = 1;

    recompute_module_count();
}


/*
 * @brief  扫描模块表：超过阈值未收到心跳的模块将被剔除。
 *         当前阈值为 5000ms（心跳 1Hz，连续 5 次未收到）。
 */
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
 * USB: XY Remaining Step Report (ONE FRAME)
USB Remaining Step Report��Device �� PC��
Byte 0    0xA5
Byte 1    CMD_MOTION (0x10)
Byte 2    DIR_MCU_TO_PC (0x01)
Byte 3    seq_L
Byte 4    seq_H
Byte 5..8 NodeID (uint32, LE)
Byte 9    Reserved
Byte 10   StatusFlags (bit0 = running, bit1 = done) // Ԥ��
Byte 11   Reserved
Byte 12..15 X_RemainingStep (uint32, LE)
Byte 16..19 Y_RemainingStep (uint32, LE)
Byte 20..63 Reserved = 0
 * ========================================================= */
/*
 * @brief  通过 USB 上报 Adjuster 的 XY 剩余步数（单帧）。
 * @param  node_id   模块 NodeID。
 * @param  x_remain  X 轴剩余步数。
 * @param  y_remain  Y 轴剩余步数。
 */
static void USB_Report_RemainingStep_XY(uint32_t node_id,
                                        uint32_t x_remain,
                                        uint32_t y_remain)
{
    uint8_t  tx_buf[USB_FRAME_LEN] = {0};
    uint16_t seq = g_motion_report_seq++;

    tx_buf[0] = USB_FRAME_HEAD;    // 0xA5
    tx_buf[1] = CMD_MOTION;        // 0x10
    tx_buf[2] = DIR_MCU_TO_PC;     // 0x01
    tx_buf[3] = (uint8_t)(seq & 0xFF);
    tx_buf[4] = (uint8_t)(seq >> 8);

    tx_buf[5] = (uint8_t)(node_id & 0xFF);
    tx_buf[6] = (uint8_t)((node_id >> 8) & 0xFF);
    tx_buf[7] = (uint8_t)((node_id >> 16) & 0xFF);
    tx_buf[8] = (uint8_t)((node_id >> 24) & 0xFF);

    tx_buf[9]  = 0x00;   // Reserved
    tx_buf[10] = 0x00;   // StatusFlags��Ԥ����
    tx_buf[11] = 0x00;

    /* X remain */
    tx_buf[12] = (uint8_t)(x_remain & 0xFF);
    tx_buf[13] = (uint8_t)((x_remain >> 8) & 0xFF);
    tx_buf[14] = (uint8_t)((x_remain >> 16) & 0xFF);
    tx_buf[15] = (uint8_t)((x_remain >> 24) & 0xFF);

    /* Y remain */
    tx_buf[16] = (uint8_t)(y_remain & 0xFF);
    tx_buf[17] = (uint8_t)((y_remain >> 8) & 0xFF);
    tx_buf[18] = (uint8_t)((y_remain >> 16) & 0xFF);
    tx_buf[19] = (uint8_t)((y_remain >> 24) & 0xFF);

    //USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, tx_buf, USB_FRAME_LEN);

    if (!USB_Send_Queue(tx_buf, USB_FRAME_LEN))
    {
        usb_drop_cnt++;
    }
}

/* =========================================================
 * FDCAN Rx Callback
 * ========================================================= */
/*
 * @brief  FDCAN2 RX FIFO0 新消息中断回调：解析心跳/状态上报。
 * @param  hfdcan FDCAN 句柄。
 * @param  it     中断标志。
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t it)
{
    if (hfdcan->Instance != FDCAN2)
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

        USB_Report_RemainingStep_XY(node_id, x_remain, y_remain);
        return;
    }

    /* ---------- Param feedback 0x123 ---------- */
    if (rxHeader.Identifier == CAN_ID_PARAM_FEEDBACK &&
        rxHeader.DataLength >= FDCAN_DLC_BYTES_12)
    {
        uint32_t node_id = u32_le(&CAN_rxData[0]);
        uint8_t  axis    = CAN_rxData[4];   /* 0=X, 1=Y, 0xFF=Global */
        uint8_t  pid     = CAN_rxData[5];
        uint32_t value   = u32_le(&CAN_rxData[8]);

        /* ======== 新增：更新模块参数缓存 ======== */
        int idx = find_module_by_nodeid(node_id);
        if (idx >= 0)
        {
            SubModule_t *m = &g_modules[idx];

            switch ((Adjuster_ParamId_t)pid)
            {
                case ADJ_PARAM_X_CURRENT:
                    if (axis == 0)   /* X axis */
                        m->x_current = (uint16_t)value;
                    break;

                case ADJ_PARAM_Y_CURRENT:
                    if (axis == 1)   /* Y axis */
                        m->y_current = (uint16_t)value;
                    break;

                case ADJ_PARAM_HOLD_CURRENT:
                    /* 全局参数，axis 通常为 0xFF */
                    m->hold_current = (uint8_t)value;
                    break;

                case ADJ_PARAM_SPEED:
                    /* 全局参数，axis 通常为 0xFF */
                    m->speed = (uint16_t)value;
                    break;

                default:
                    /* 未关心的参数，忽略 */
                    break;
            }
        }

        /* ======== 保留原有逻辑：透传给 PC ======== */
        USB_Report_ParamFeedback(node_id, axis, pid, value);
        return;
    }

		
}

// Device->Module must change 32bit NodeID to MSB
/*
 * @brief  将 32bit NodeID 转换为大端序 4 字节数组（MSB first）。
 * @param  node_id NodeID。
 * @param  out     输出 4 字节数组。
 */
void Pack_USB_NodeID_MSB_To_CAN(uint32_t node_id, uint8_t out[4])
{
    out[0] = (uint8_t)((node_id >> 24) & 0xFF);  // MSB
    out[1] = (uint8_t)((node_id >> 16) & 0xFF);
    out[2] = (uint8_t)((node_id >> 8)  & 0xFF);
    out[3] = (uint8_t)(node_id & 0xFF);          // LSB
}


/*
 * @brief  通过 CAN 下发设置模块名称命令。
 * @param  node_id 目标模块 NodeID。
 * @param  name    8 字节名称（固定长度）。
 */
void CAN_Send_SetModuleName(uint32_t node_id, const uint8_t name[8])
{
    uint8_t tx_buf[12] = {0};

    /* NodeID */
		Pack_USB_NodeID_MSB_To_CAN(node_id, tx_buf);

    /* Name */
    memcpy(&tx_buf[4], name, 8);

    CAN_Send_FD_Frame(CAN_ID_DEVICE_NAME, tx_buf,
                      16);   /* NodeID(4) + Name(8) */
}

/*
 * @brief  Forward Adjuster Param Set to Module (CAN_ID_PARAM_SET 0x122).
 * @note   NodeID is sent in LE byte order (same as Module expects).

		Byte 0..3   NodeID        uint32  (LSB first)
		Byte 4      Axis          uint8   (0xFF = global / auto)
		Byte 5      ParamID       uint8
		Byte 6..7   Value         uint16  (LSB first)

 */
void CAN_Send_ParamSet(uint32_t node_id, uint8_t axis, uint8_t param_id, uint16_t value)
{
    uint8_t tx_buf[12] = {0};

    /* NodeID LE */
    tx_buf[0] = (uint8_t)(node_id & 0xFF);
    tx_buf[1] = (uint8_t)((node_id >> 8) & 0xFF);
    tx_buf[2] = (uint8_t)((node_id >> 16) & 0xFF);
    tx_buf[3] = (uint8_t)((node_id >> 24) & 0xFF);

    tx_buf[4] = axis; // 0xFF
    tx_buf[5] = param_id;
		
		/* Value LE */
		tx_buf[6] = (uint8_t)(value & 0xFF);        // LSB
		tx_buf[7] = (uint8_t)((value >> 8) & 0xFF); // MSB

    CAN_Send_FD_Frame(CAN_ID_PARAM_SET, tx_buf, 12);
}


/*
 * @brief  将 payload 长度映射为 CAN-FD DLC 字段。
 * @param  len payload 字节数（0~64）。
 * @return DLC 字段（用于 FDCAN_TxHeaderTypeDef.DataLength）。
 */
static uint32_t CAN_Encode_DLC(uint8_t len)
{
    /* CAN FD DLC ӳ�� */
    if (len <= 8)   return (uint32_t)len << 16;
    if (len <= 12)  return FDCAN_DLC_BYTES_12;
    if (len <= 16)  return FDCAN_DLC_BYTES_16;
    if (len <= 20)  return FDCAN_DLC_BYTES_20;
    if (len <= 24)  return FDCAN_DLC_BYTES_24;
    if (len <= 32)  return FDCAN_DLC_BYTES_32;
    if (len <= 48)  return FDCAN_DLC_BYTES_48;
    return FDCAN_DLC_BYTES_64;
}

/*
 * @brief  发送一帧 CAN-FD 标准帧。
 * @param  std_id 标准 11bit ID。
 * @param  data   payload 数据指针。
 * @param  len    payload 长度（最大 64）。
 */
void CAN_Send_FD_Frame(uint16_t std_id,
                       const uint8_t *data,
                       uint8_t len)
{
    FDCAN_TxHeaderTypeDef txHeader;
    uint8_t tx_buf[64];

    if (len > 64)
        len = 64;

    /* Payload copy */
    memset(tx_buf, 0, sizeof(tx_buf));
    memcpy(tx_buf, data, len);

    /* Tx header config */
    txHeader.Identifier          = std_id;
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = CAN_Encode_DLC(len);
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_ON;      // FDBRS
    txHeader.FDFormat            = FDCAN_FD_CAN;      // CAN-FD
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;

    /* �ȴ� Tx FIFO �п�λ��������ʽ��device ���㹻�� */
    while (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan2) == 0)
    {
        /* ��ѡ���� timeout */
    }

    /* Send */
    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2,
                                  &txHeader,
                                  tx_buf);
}

