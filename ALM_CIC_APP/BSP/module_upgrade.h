/**
 * module_upgrade.h - device-side relay for ALM_Motor OTA
 *
 *   PC ───TCP CMD_MODULE_UPGRADE───▶ Device (this code)
 *                                         │ stages .mot in W25Q[MUR_W25Q_BASE]
 *                                         ▼
 *                                  CAN FD ID 0x300/0x301/0x302 ───▶ Motor
 *                                         ▲
 *                                  CAN FD ID 0x305 ◀───── Motor RESP
 *
 * The Motor staging region in W25Q is separate from the device's own .cic
 * staging region (0x00000000), so an in-flight Motor upgrade never collides
 * with the device's own pending self-upgrade.
 */
#ifndef __MODULE_UPGRADE_H__
#define __MODULE_UPGRADE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* W25Q region for the Motor .mot package (1 MB offset; ETH .cic owns 0..1MB). */
#define MUR_W25Q_BASE     0x00100000UL
#define MUR_W25Q_SIZE     0x00020000UL  /* 128 KB ample for any .mot           */

/* CAN-FD IDs — must mirror ALM_Motor_App/BSP/motor_upgrade.h */
#define MUR_CANID_START   0x300U
#define MUR_CANID_DATA    0x301U
#define MUR_CANID_END     0x302U
#define MUR_CANID_RESP    0x305U

/* Called from Process_ETH_Command when buf[1] == CMD_MODULE_UPGRADE */
void MUR_HandleCommand(const uint8_t *buf, uint16_t len);

/* Called from main while(1); drives the CAN-FD relay state machine. */
void MUR_PollRelay(void);

/* Hook called from HAL_FDCAN_RxFifo0Callback when CAN ID 0x305 (Motor RESP)
   arrives. node_id and status are extracted from the frame payload by the
   caller; here we just stash them for the polling state machine.            */
void MUR_OnCanResp(uint32_t node_id, uint8_t status, uint32_t last_offset);

void MUR_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __MODULE_UPGRADE_H__ */
