#ifndef __USB_COMM_H__
#define __USB_COMM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "CAN_comm.h"

#define USB_FRAME_LEN           64
#define USB_FRAME_HEAD          0xA5

#define USB_TX_LEN              USB_FRAME_LEN

#define CMD_INFO                0x01
#define DIR_MCU_TO_PC           0x01
/* Direction in USB frames */
#define DIR_PC_TO_DEV           0x02
#define CMD_MOTION              0x10
#define CMD_MOTION_LEGACY       0x02   // 兼容PC端使用的旧 motion 命令码

/* PC -> Device: Parameter set (forward to CAN 0x122) */
#define CMD_PARAM_SET           0x20

#define SUBCMD_GET_VERSION_INFO 0x01
#define SUBCMD_GET_MODULE_LIST  0x02
#define SUBCMD_GET_MODULE_INFO  0x03
#define SUBCMD_SET_MODULE_NAME  0x04

#define SUBCMD_SET_PARAM        0x05  // PC->Device: Set Adjuster Param (forward to CAN 0x122)
#define SUBCMD_PARAM_FEEDBACK   0x85  // Device->PC: Param feedback (from CAN 0x123)


extern uint16_t g_motion_report_seq;

void Process_HID_Command(uint8_t *buf);

void Send_VersionInfo(uint16_t seq);

void USB_Handle_GetModuleList(uint16_t seq, uint8_t *tx_buf);

void USB_Handle_GetModuleInfoReply(uint16_t seq, uint8_t module_index);

void USB_Report_RemainingStep(const uint8_t *can_data);

void USB_Report_ParamFeedback(uint32_t node_id, uint8_t axis, uint8_t param_id, uint32_t value);


#ifdef __cplusplus
}
#endif

#endif /* __USB_COMM_H__ */
