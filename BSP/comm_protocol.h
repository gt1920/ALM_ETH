#ifndef __COMM_PROTOCOL_H__
#define __COMM_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "CAN_comm.h"
#include <stdint.h>

#define PROTO_FRAME_LEN         64
#define PROTO_FRAME_HEAD        0xA5

#define CMD_INFO                0x01
#define DIR_MCU_TO_PC           0x01
#define DIR_PC_TO_DEV           0x02
#define CMD_MOTION              0x10
#define CMD_MOTION_LEGACY       0x02
#define CMD_PARAM_SET           0x20
#define CMD_IAP                 0x30

#define SUBCMD_GET_VERSION_INFO 0x01
#define SUBCMD_GET_MODULE_LIST  0x02
#define SUBCMD_GET_MODULE_INFO  0x03
#define SUBCMD_SET_MODULE_NAME  0x04
#define SUBCMD_SET_PARAM        0x05
#define SUBCMD_SET_DEVICE_NAME  0x06
#define SUBCMD_PARAM_FEEDBACK   0x85

void Process_ETH_Command(const uint8_t *buf, uint16_t len);
void ETH_Report_ParamFeedback(uint32_t node_id, uint8_t axis, uint8_t param_id, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* __COMM_PROTOCOL_H__ */
