#include "comm_protocol.h"
#include "device_config.h"
#include "iap_handler.h"
#include "eth_send_queue.h"
#include <string.h>

/* ---------- Firmware & Manufacturing info ---------- */
#define FW_HW_VER  1   /* hardware version, fixed */

/* Auto-derive build date from __DATE__ ("Mmm DD YYYY") */
#define BUILD_DAY   ((__DATE__[4] == ' ' ? 0 : ((__DATE__[4] - '0') * 10)) + (__DATE__[5] - '0'))
#define BUILD_MONTH ( \
    __DATE__[0] == 'J' ? (__DATE__[1] == 'a' ? 1 : (__DATE__[2] == 'n' ? 6 : 7)) : \
    __DATE__[0] == 'F' ? 2 : \
    __DATE__[0] == 'M' ? (__DATE__[2] == 'r' ? 3 : 5) : \
    __DATE__[0] == 'A' ? (__DATE__[1] == 'p' ? 4 : 8) : \
    __DATE__[0] == 'S' ? 9 : \
    __DATE__[0] == 'O' ? 10 : \
    __DATE__[0] == 'N' ? 11 : 12)
#define BUILD_YEAR  (((__DATE__[9] - '0') * 10) + (__DATE__[10] - '0'))

/* ---------- Working buffers ---------- */
static uint8_t send_buf[PROTO_FRAME_LEN];

/* Debug: last ModuleInfo frame sent (for Keil Watch) */
uint8_t dbg_last_module_tx[PROTO_FRAME_LEN];

static uint16_t g_report_seq = 0;

/* ---------- Forward declarations ---------- */
static void ETH_Handle_MotionCommand(const uint8_t *rx);
static void ETH_Send_VersionInfo(uint16_t seq);
static void ETH_Handle_GetModuleList(uint16_t seq);
static void ETH_Handle_GetModuleInfoReply(uint16_t seq, uint8_t module_index);

/* =========================================================
 * Command dispatcher (called from TCP recv_cb)
 * ========================================================= */
void Process_ETH_Command(const uint8_t *buf, uint16_t len)
{
    if (len < 6)
        return;

    if (buf[0] != PROTO_FRAME_HEAD)
        return;

    uint8_t  cmd = buf[1];
    uint16_t seq = (uint16_t)buf[3] | ((uint16_t)buf[4] << 8);

    /* ---- INFO command ---- */
    if (cmd == CMD_INFO)
    {
        uint8_t subcmd = buf[6];

        switch (subcmd)
        {
            case SUBCMD_GET_VERSION_INFO:
                ETH_Send_VersionInfo(seq);
                break;

            case SUBCMD_GET_MODULE_LIST:
                ETH_Handle_GetModuleList(seq);
                break;

            case SUBCMD_GET_MODULE_INFO:
                ETH_Handle_GetModuleInfoReply(seq, buf[7]);
                break;

            case SUBCMD_SET_MODULE_NAME:
            {
                uint32_t node_id =
                    ((uint32_t)buf[8])        |
                    ((uint32_t)buf[9]  << 8)  |
                    ((uint32_t)buf[10] << 16) |
                    ((uint32_t)buf[11] << 24);

                CAN_Send_SetModuleName(node_id, &buf[12]);
                break;
            }

            case SUBCMD_SET_DEVICE_NAME:
            {
                /* buf[7..22] = 16-byte device name (ASCII, 0-padded).
                   len=0 clears the custom name. */
                uint8_t name_len = 0;
                for (int i = 0; i < DEVICE_NAME_MAX_LEN; i++)
                {
                    if (buf[7 + i] == 0) break;
                    name_len++;
                }
                DeviceConfig_SetName((const char *)&buf[7], name_len);
                break;
            }

            default:
                break;
        }
    }
    /* ---- PARAM_SET command ---- */
    else if (cmd == CMD_PARAM_SET)
    {
        if (buf[5] < 7u)
            return;

        uint32_t node_id =
            ((uint32_t)buf[6])        |
            ((uint32_t)buf[7]  << 8)  |
            ((uint32_t)buf[8]  << 16) |
            ((uint32_t)buf[9]  << 24);

        uint8_t  param_id = buf[10];
        uint16_t value16  = (uint16_t)buf[11] | ((uint16_t)buf[12] << 8);

        CAN_Send_ParamSet(node_id, 0xFFu, param_id, value16);
    }
    /* ---- IAP command ---- */
    else if (cmd == CMD_IAP)
    {
        IAP_Process_Command(buf, len, seq);
    }
    /* ---- Motion command ---- */
    else if (cmd == CMD_MOTION || cmd == CMD_MOTION_LEGACY)
    {
        ETH_Handle_MotionCommand(buf);
    }
}

/* =========================================================
 * Version Info reply
 * ========================================================= */
static void ETH_Send_VersionInfo(uint16_t seq)
{
    memset(send_buf, 0, PROTO_FRAME_LEN);

    send_buf[0] = PROTO_FRAME_HEAD;
    send_buf[1] = CMD_INFO;
    send_buf[2] = DIR_MCU_TO_PC;
    send_buf[3] = (uint8_t)(seq & 0xFF);
    send_buf[4] = (uint8_t)(seq >> 8);
    send_buf[5] = 8;
    send_buf[6] = SUBCMD_GET_VERSION_INFO;

    send_buf[7]  = FW_HW_VER;
    send_buf[8]  = BUILD_DAY;
    send_buf[9]  = BUILD_MONTH;
    send_buf[10] = BUILD_YEAR;

    send_buf[11] = BUILD_YEAR;
    send_buf[12] = BUILD_MONTH;
    send_buf[13] = BUILD_DAY;

    /* Device SN (UID XOR, 4 bytes LE) [14..17] */
    extern uint32_t g_device_sn;
    send_buf[14] = (uint8_t)(g_device_sn);
    send_buf[15] = (uint8_t)(g_device_sn >> 8);
    send_buf[16] = (uint8_t)(g_device_sn >> 16);
    send_buf[17] = (uint8_t)(g_device_sn >> 24);

    ETH_Send_Queue(send_buf, PROTO_FRAME_LEN);
}

/* =========================================================
 * Module List reply
 * ========================================================= */
static void ETH_Handle_GetModuleList(uint16_t seq)
{
    memset(send_buf, 0, PROTO_FRAME_LEN);

    send_buf[0] = PROTO_FRAME_HEAD;
    send_buf[1] = CMD_INFO;
    send_buf[2] = DIR_MCU_TO_PC;
    send_buf[3] = (uint8_t)(seq & 0xFF);
    send_buf[4] = (uint8_t)(seq >> 8);
    send_buf[6] = SUBCMD_GET_MODULE_LIST;
    send_buf[7] = g_module_count;

    uint8_t pos = 8;
    for (int i = 0; i < MAX_SUB_MODULES; i++)
    {
        if (!g_modules[i].valid)
            continue;

        send_buf[pos++] = g_modules[i].module_type;
        send_buf[pos++] = 1;
    }

    send_buf[5] = pos - 6;

    ETH_Send_Queue(send_buf, PROTO_FRAME_LEN);
}

/* =========================================================
 * Motion Command -> CAN 0x120
 * ========================================================= */
static void ETH_Handle_MotionCommand(const uint8_t *rx)
{
    uint8_t can_payload[16] = {0};

    /* NodeID: reverse byte order from USB frame (BE -> CAN) */
    can_payload[0] = rx[8];
    can_payload[1] = rx[7];
    can_payload[2] = rx[6];
    can_payload[3] = rx[5];

    can_payload[4] = rx[9];   /* axis */
    can_payload[5] = rx[10];  /* direction */

    can_payload[8]  = rx[12]; /* step count LE */
    can_payload[9]  = rx[13];
    can_payload[10] = rx[14];
    can_payload[11] = rx[15];

    CAN_Send_FD_Frame(0x120, can_payload, 16);
}

/* =========================================================
 * Module Info Reply (V2)
 * ========================================================= */
static void ETH_Handle_GetModuleInfoReply(uint16_t seq, uint8_t module_index)
{
    uint8_t tx_buf[PROTO_FRAME_LEN] = {0};

    tx_buf[0] = PROTO_FRAME_HEAD;
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
        ETH_Send_Queue(tx_buf, PROTO_FRAME_LEN);
        return;
    }

    SubModule_t *m = &g_modules[module_index];

    tx_buf[8] = m->module_type;
    memcpy(&tx_buf[9], m->device_name, 8);

    /* serial = NodeID (LE) */
    tx_buf[17] = (uint8_t)(m->serial & 0xFF);
    tx_buf[18] = (uint8_t)((m->serial >> 8) & 0xFF);
    tx_buf[19] = (uint8_t)((m->serial >> 16) & 0xFF);
    tx_buf[20] = (uint8_t)((m->serial >> 24) & 0xFF);

    /* fw_version (LE) */
    tx_buf[21] = (uint8_t)(m->fw_version & 0xFF);
    tx_buf[22] = (uint8_t)((m->fw_version >> 8) & 0xFF);
    tx_buf[23] = (uint8_t)((m->fw_version >> 16) & 0xFF);
    tx_buf[24] = (uint8_t)((m->fw_version >> 24) & 0xFF);

    tx_buf[25] = m->mfg_year;
    tx_buf[26] = m->mfg_month;
    tx_buf[27] = m->mfg_day;

    /* V2 extension */
    tx_buf[28] = (uint8_t)(m->x_current & 0xFF);
    tx_buf[29] = (uint8_t)(m->x_current >> 8);
    tx_buf[30] = (uint8_t)(m->y_current & 0xFF);
    tx_buf[31] = (uint8_t)(m->y_current >> 8);
    tx_buf[32] = m->hold_current;
    tx_buf[33] = 0;
    tx_buf[34] = (uint8_t)(m->speed & 0xFF);
    tx_buf[35] = (uint8_t)(m->speed >> 8);

    tx_buf[36] = (uint8_t)(m->x_pos & 0xFF);
    tx_buf[37] = (uint8_t)(m->x_pos >> 8);
    tx_buf[38] = (uint8_t)(m->x_pos >> 16);
    tx_buf[39] = (uint8_t)(m->x_pos >> 24);

    tx_buf[40] = (uint8_t)(m->y_pos & 0xFF);
    tx_buf[41] = (uint8_t)(m->y_pos >> 8);
    tx_buf[42] = (uint8_t)(m->y_pos >> 16);
    tx_buf[43] = (uint8_t)(m->y_pos >> 24);

    memcpy(dbg_last_module_tx, tx_buf, PROTO_FRAME_LEN);

    ETH_Send_Queue(tx_buf, PROTO_FRAME_LEN);
}

/* =========================================================
 * Param Feedback -> PC
 * ========================================================= */
void ETH_Report_ParamFeedback(uint32_t node_id,
                              uint8_t axis,
                              uint8_t param_id,
                              uint32_t value)
{
    uint8_t tx_buf[PROTO_FRAME_LEN] = {0};
    uint16_t seq = g_report_seq++;

    tx_buf[0] = PROTO_FRAME_HEAD;
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

    ETH_Send_Queue(tx_buf, PROTO_FRAME_LEN);
}
