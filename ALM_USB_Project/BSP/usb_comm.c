#include "usb_comm.h"
#include "usbd_customhid.h"
#include "USB_Send_Queue.h"


/* ---------- 固件与生产信息 ---------- */
#define FW_VER_0   1
#define FW_VER_1   1
#define FW_VER_2   12
#define FW_VER_3   24

#define MFG_YEAR   25
#define MFG_MONTH  12
#define MFG_DAY    15

uint8_t g_HID_RX_Buffer[64];
uint8_t Send_buf[64];
uint16_t g_motion_report_seq = 0;

/* ===== Debug: 最近一次发给 PC 的 ModuleInfo 帧（Keil Watch 用） ===== */
uint8_t dbg_last_module_tx[64];

static void USB_Handle_MotionCommand(const uint8_t *rx);
void USB_Reply_OK(uint16_t seq);

/* =========================
 * HID 接收（保持你原来能工作的方式）
 * ========================= */
void Process_HID_Command(uint8_t *buf)
{
    (void)buf;

    USBD_CUSTOM_HID_HandleTypeDef *hhid =
        (USBD_CUSTOM_HID_HandleTypeDef *)hUsbDeviceFS.pClassData;

    memcpy(g_HID_RX_Buffer,
           hhid->Report_buf,
           USBD_CUSTOMHID_OUTREPORT_BUF_SIZE);

    /* Frame header */
    if (g_HID_RX_Buffer[0] != 0xA5)
        return;

    uint8_t  cmd = g_HID_RX_Buffer[1];
    uint16_t seq =
        (uint16_t)g_HID_RX_Buffer[3] |
        ((uint16_t)g_HID_RX_Buffer[4] << 8);

    /* ===============================
     * INFO command
     * =============================== */
    if (cmd == CMD_INFO)
    {
        uint8_t subcmd = g_HID_RX_Buffer[6];

        switch (subcmd)
        {
            case SUBCMD_GET_VERSION_INFO:
                Send_VersionInfo(seq);
                break;

            case SUBCMD_GET_MODULE_LIST:
                USB_Handle_GetModuleList(seq, Send_buf);
                break;

            case SUBCMD_GET_MODULE_INFO:
                USB_Handle_GetModuleInfoReply(seq, g_HID_RX_Buffer[7]);
                break;

            /* ===============================
             * NEW: Set Module Name
             * =============================== */
            case SUBCMD_SET_MODULE_NAME:
            {
                uint8_t  module_index = g_HID_RX_Buffer[7];

                uint32_t node_id =
                    ((uint32_t)g_HID_RX_Buffer[8])       |
                    ((uint32_t)g_HID_RX_Buffer[9]  << 8) |
                    ((uint32_t)g_HID_RX_Buffer[10] << 16)|
                    ((uint32_t)g_HID_RX_Buffer[11] << 24);

                const uint8_t *name_ptr = &g_HID_RX_Buffer[12];

                /* 下发到 CAN → Module */
                CAN_Send_SetModuleName(node_id, name_ptr);

                /* 可选：立即 ACK PC（成功只表示已转发） */
                //USB_Reply_OK(seq);
                break;
            }						

            default:
                break;
        }
    }
    /* ===============================
     * PARAM_SET command (PC -> Device)
     * Frame (64B):
     *   [0]  0xA5
     *   [1]  CMD_PARAM_SET (0x20)
     *   [2]  DIR_PC_TO_DEV (0x02)
     *   [3]  seq_L
     *   [4]  seq_H
     *   [5]  payload_len (=7)
     *   [6..9]  NodeID (LE)
     *   [10]    param_id
     *   [11..12] value (uint16, LE)  (uint8 uses value_L, value_H=0)
     */
    else if (cmd == CMD_PARAM_SET)
    {
        /* Basic sanity: payload must at least contain NodeID + param_id + value16 */
        if (g_HID_RX_Buffer[5] < 7u)
            return;

        uint32_t node_id =
            ((uint32_t)g_HID_RX_Buffer[6])       |
            ((uint32_t)g_HID_RX_Buffer[7]  << 8) |
            ((uint32_t)g_HID_RX_Buffer[8]  << 16)|
            ((uint32_t)g_HID_RX_Buffer[9]  << 24);

        uint8_t param_id = g_HID_RX_Buffer[10];

        uint16_t value16 =
            ((uint16_t)g_HID_RX_Buffer[11]) |
            ((uint16_t)g_HID_RX_Buffer[12] << 8);

        /* Axis is not carried in USB CMD_PARAM_SET; param_id already encodes X/Y.
         * Use 0xFF as Global/Unused axis when forwarding to CAN.
         */
        CAN_Send_ParamSet(node_id, 0xFFu, param_id, value16);

        /* Optional: ACK PC (means "forwarded") */
        /* USB_Reply_OK(seq); */
    }
    /* ===============================
     * Motion command
     * =============================== */
    else if (cmd == CMD_MOTION || cmd == CMD_MOTION_LEGACY)
    {
        USB_Handle_MotionCommand(g_HID_RX_Buffer);
    }
}


/* =========================
 * Version Info
 * ========================= */
void Send_VersionInfo(uint16_t seq)
{
    memset(Send_buf, 0, 64);

    Send_buf[0] = 0xA5;
    Send_buf[1] = CMD_INFO;
    Send_buf[2] = DIR_MCU_TO_PC;
    Send_buf[3] = (uint8_t)(seq & 0xFF);
    Send_buf[4] = (uint8_t)(seq >> 8);
    Send_buf[5] = 8;
    Send_buf[6] = SUBCMD_GET_VERSION_INFO;

    Send_buf[7]  = FW_VER_0;
    Send_buf[8]  = FW_VER_1;
    Send_buf[9]  = FW_VER_2;
    Send_buf[10] = FW_VER_3;

    Send_buf[11] = MFG_YEAR;
    Send_buf[12] = MFG_MONTH;
    Send_buf[13] = MFG_DAY;

    //USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, Send_buf, 64);		
    if (!USB_Send_Queue(Send_buf, USB_FRAME_LEN))
    {
        usb_drop_cnt++;
    }		
}

/* =========================
 * Module List
 * ========================= */
void USB_Handle_GetModuleList(uint16_t seq, uint8_t *tx_buf)
{
    memset(tx_buf, 0, 64);

    tx_buf[0] = 0xA5;
    tx_buf[1] = CMD_INFO;
    tx_buf[2] = DIR_MCU_TO_PC;
    tx_buf[3] = (uint8_t)(seq & 0xFF);
    tx_buf[4] = (uint8_t)(seq >> 8);
    tx_buf[6] = SUBCMD_GET_MODULE_LIST;
    tx_buf[7] = g_module_count;

    uint8_t pos = 8;
    for (int i = 0; i < MAX_SUB_MODULES; i++)
    {
        if (!g_modules[i].valid)
            continue;

        tx_buf[pos++] = g_modules[i].module_type;
        tx_buf[pos++] = 1;
    }

    tx_buf[5] = pos - 6;
    //USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, tx_buf, 64);
    if (!USB_Send_Queue(tx_buf, USB_FRAME_LEN))
    {
        usb_drop_cnt++;
    }		
}

/* =========================
 * Motion Command → CAN
 * ========================= */
static void USB_Handle_MotionCommand(const uint8_t *rx)
{
    uint8_t can_payload[16] = {0};

    can_payload[0] = rx[8];  // 0x75
    can_payload[1] = rx[7];  // 0x68
    can_payload[2] = rx[6];  // 0x2D
    can_payload[3] = rx[5];  // 0x16

    can_payload[4] = rx[9];  // axis, x=0, y=1
    can_payload[5] = rx[10];  // Direction

    can_payload[8]  = rx[12];
    can_payload[9]  = rx[13];
    can_payload[10] = rx[14];
    can_payload[11] = rx[15];

    FDCAN_TxHeaderTypeDef tx_header = {0};
    tx_header.Identifier = 0x120;
    tx_header.IdType     = FDCAN_STANDARD_ID;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.DataLength = FDCAN_DLC_BYTES_16;
    tx_header.BitRateSwitch = FDCAN_BRS_ON;
    tx_header.FDFormat = FDCAN_FD_CAN;

    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &tx_header, can_payload);
}

/* =========================
 * Remaining Step → PC
 * ========================= */
/*
void USB_Report_RemainingStep(const uint8_t *can_data)
{
    uint8_t tx_buf[64] = {0};
    uint16_t seq = g_motion_report_seq++;

    tx_buf[0] = USB_FRAME_HEAD;
    tx_buf[1] = CMD_MOTION;
    tx_buf[2] = DIR_MCU_TO_PC;
    tx_buf[3] = (uint8_t)(seq & 0xFF);
    tx_buf[4] = (uint8_t)(seq >> 8);

    tx_buf[5] = can_data[0];
    tx_buf[6] = can_data[1];
    tx_buf[7] = can_data[2];
    tx_buf[8] = can_data[3];

    tx_buf[9] = can_data[4];

    tx_buf[12] = can_data[8];
    tx_buf[13] = can_data[9];
    tx_buf[14] = can_data[10];
    tx_buf[15] = can_data[11];

    USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, tx_buf, 64);
}
*/

/* =========================
 * Module Info Reply
 * ✅ 唯一修改点：serial / fw_version 改为小端
 * ========================= */
void USB_Handle_GetModuleInfoReply(uint16_t seq, uint8_t module_index)
{
    uint8_t tx_buf[64] = {0};

    tx_buf[0] = 0xA5;
    tx_buf[1] = CMD_INFO;
    tx_buf[2] = DIR_MCU_TO_PC;
    tx_buf[3] = (uint8_t)(seq & 0xFF);
    tx_buf[4] = (uint8_t)(seq >> 8);
    tx_buf[5] = 22;
    tx_buf[6] = SUBCMD_GET_MODULE_INFO;
    tx_buf[7] = module_index;

    if (module_index >= MAX_SUB_MODULES ||
        !g_modules[module_index].valid)
    {
        //USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, tx_buf, USB_FRAME_LEN);
				if (!USB_Send_Queue(tx_buf, USB_FRAME_LEN))
				{
						usb_drop_cnt++;
				}					
        return;
    }

    SubModule_t *m = &g_modules[module_index];

    tx_buf[8] = m->module_type;
    memcpy(&tx_buf[9], m->device_name, 8);

    /* serial = NodeID (little-endian) */
    tx_buf[17] = (uint8_t)(m->serial & 0xFF);
    tx_buf[18] = (uint8_t)((m->serial >> 8) & 0xFF);
    tx_buf[19] = (uint8_t)((m->serial >> 16) & 0xFF);
    tx_buf[20] = (uint8_t)((m->serial >> 24) & 0xFF);

    /* fw_version (little-endian) */
    tx_buf[21] = (uint8_t)(m->fw_version & 0xFF);
    tx_buf[22] = (uint8_t)((m->fw_version >> 8) & 0xFF);
    tx_buf[23] = (uint8_t)((m->fw_version >> 16) & 0xFF);
    tx_buf[24] = (uint8_t)((m->fw_version >> 24) & 0xFF);

    tx_buf[25] = m->mfg_year;
    tx_buf[26] = m->mfg_month;
    tx_buf[27] = m->mfg_day;

    /* ======== Module Info V2 扩展区 ======== */

    /* Byte[28-29] : X current (0.1A) */
    tx_buf[28] = (uint8_t)(m->x_current & 0xFF);
    tx_buf[29] = (uint8_t)(m->x_current >> 8);

    /* Byte[30-31] : Y current (0.1A) */
    tx_buf[30] = (uint8_t)(m->y_current & 0xFF);
    tx_buf[31] = (uint8_t)(m->y_current >> 8);

    /* Byte[32] : Hold current (%) */
    tx_buf[32] = m->hold_current;

    /* Byte[33] : Reserved */
    tx_buf[33] = 0;

    /* Byte[34-35] : Speed (step/s) */
    tx_buf[34] = (uint8_t)(m->speed & 0xFF);
    tx_buf[35] = (uint8_t)(m->speed >> 8);

    /* Byte[36-39] : X position (int32) */
    tx_buf[36] = (uint8_t)(m->x_pos & 0xFF);
    tx_buf[37] = (uint8_t)(m->x_pos >> 8);
    tx_buf[38] = (uint8_t)(m->x_pos >> 16);
    tx_buf[39] = (uint8_t)(m->x_pos >> 24);

    /* Byte[40-43] : Y position (int32) */
    tx_buf[40] = (uint8_t)(m->y_pos & 0xFF);
    tx_buf[41] = (uint8_t)(m->y_pos >> 8);
    tx_buf[42] = (uint8_t)(m->y_pos >> 16);
    tx_buf[43] = (uint8_t)(m->y_pos >> 24);

    //USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, tx_buf, 64);
    memcpy(dbg_last_module_tx, tx_buf, USB_FRAME_LEN);  /* Debug: 保存副本供 Keil Watch */
    if (!USB_Send_Queue(tx_buf, USB_FRAME_LEN))
    {
        usb_drop_cnt++;
    }
}

void USB_Reply_OK(uint16_t seq)
{
    memset(Send_buf, 0, 64);

    Send_buf[0] = 0xA5;
    Send_buf[1] = CMD_INFO;
    Send_buf[2] = 0x01;     // MCU → PC
    Send_buf[3] = (uint8_t)(seq & 0xFF);
    Send_buf[4] = (uint8_t)((seq >> 8) & 0xFF);
    Send_buf[5] = 0x01;     // length
    Send_buf[6] = 0x00;     // status OK

    USB_Send_Queue(Send_buf, 64);
}

void USB_Report_ParamFeedback(uint32_t node_id,
                              uint8_t axis,
                              uint8_t param_id,
                              uint32_t value)
{
    uint8_t  tx_buf[USB_FRAME_LEN] = {0};
    uint16_t seq = g_motion_report_seq++;

    tx_buf[0] = USB_FRAME_HEAD;
    tx_buf[1] = CMD_INFO;
    tx_buf[2] = DIR_MCU_TO_PC;
    tx_buf[3] = (uint8_t)(seq & 0xFF);
    tx_buf[4] = (uint8_t)(seq >> 8);
    tx_buf[5] = 0x0E;
    tx_buf[6] = SUBCMD_PARAM_FEEDBACK;
    tx_buf[7] = 0x00;

    tx_buf[8]  = (uint8_t)(node_id & 0xFF);
    tx_buf[9]  = (uint8_t)((node_id >> 8) & 0xFF);
    tx_buf[10] = (uint8_t)((node_id >> 16) & 0xFF);
    tx_buf[11] = (uint8_t)((node_id >> 24) & 0xFF);

    tx_buf[12] = axis;
    tx_buf[13] = param_id;
    tx_buf[14] = 0x00;
    tx_buf[15] = 0x00;

    tx_buf[16] = (uint8_t)(value & 0xFF);
    tx_buf[17] = (uint8_t)((value >> 8) & 0xFF);
    tx_buf[18] = (uint8_t)((value >> 16) & 0xFF);
    tx_buf[19] = (uint8_t)((value >> 24) & 0xFF);

    if (!USB_Send_Queue(tx_buf, USB_FRAME_LEN))
    {
        usb_drop_cnt++;
    }
}

