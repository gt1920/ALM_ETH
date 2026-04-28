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

#define SUBCMD_GET_VERSION_INFO 0x01
#define SUBCMD_GET_MODULE_LIST  0x02
#define SUBCMD_GET_MODULE_INFO  0x03
#define SUBCMD_SET_MODULE_NAME  0x04
#define SUBCMD_SET_PARAM        0x05
#define SUBCMD_SET_DEVICE_NAME  0x06
#define SUBCMD_PARAM_FEEDBACK   0x85

/* OTA firmware upgrade (CMD_UPGRADE = 0x30) */
#define CMD_UPGRADE             0x30
#define SUBCMD_UPG_START        0x01   /* PC→Dev: file_size(4 LE)            */
#define SUBCMD_UPG_DATA         0x02   /* PC→Dev: offset(4 LE) + data(≤128B) */
#define SUBCMD_UPG_END          0x03   /* PC→Dev: total_size(4 LE)           */
#define SUBCMD_UPG_RESP         0x81   /* Dev→PC: status(1)                  */
/* status codes */
#define UPG_STATUS_OK           0x00
#define UPG_STATUS_WRONG_BOARD  0x01   /* board_id mismatch            */
#define UPG_STATUS_SIZE_ERROR   0x02   /* file_size out of range       */
#define UPG_STATUS_WRITE_ERROR  0x03   /* W25Q erase/write failure     */
#define UPG_STATUS_VERIFY_ERROR 0x04   /* rx_bytes != expected         */
#define UPG_STATUS_BAD_FW       0x05   /* .cic header decrypt/CRC fail */
#define UPG_STATUS_WRONG_SN     0x06   /* fw_sn locked to other MCU    */

/* Module OTA upgrade — relay .mot to a sub-module via CAN FD (CMD_MODULE_UPGRADE = 0x31)
   START payload : file_size(4 LE) + target_node_id(4 LE) + cic_hdr[0..15](16) = 24 B
   DATA  payload : offset(4 LE) + data(≤128 B)
   END   payload : total_size(4 LE)
   RESP  payload : status(1)                                                            */
#define CMD_MODULE_UPGRADE      0x31
#define SUBCMD_MUPG_START       0x01
#define SUBCMD_MUPG_DATA        0x02
#define SUBCMD_MUPG_END         0x03
#define SUBCMD_MUPG_RESP        0x81
/* MUPG status codes — reuse UPG_STATUS_* and add: */
#define MUPG_STATUS_NO_TARGET   0x10   /* target_node_id not in module list   */
#define MUPG_STATUS_RELAY_FAIL  0x11   /* CAN-FD relay to module failed       */
#define MUPG_STATUS_BUSY        0x12   /* a previous relay is still running   */

void Process_ETH_Command(const uint8_t *buf, uint16_t len);
void ETH_Report_ParamFeedback(uint32_t node_id, uint8_t axis, uint8_t param_id, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* __COMM_PROTOCOL_H__ */
