/**
 * motor_upgrade.h - ALM_Motor_APP OTA receiver over CAN FD
 *
 * Receives the encrypted .mot upgrade package from the Device (relay),
 * validates the FW_ID header (board must be "MOT"), writes the encrypted
 * blob into STAGING (internal flash MOT_STAGE_BASE), then triggers a
 * deferred reset. On the next boot, ALM_Motor_Bootloader/iap.c picks up
 * STAGING, decrypts and flashes APP.
 *
 * Wire-format of the CAN FD sub-protocol (see motor_upgrade.c):
 *   START (0x300, 24 B):  node_id(4) | file_size(4) | cic_hdr[0..15](16)
 *   DATA  (0x301, 12..64): node_id(4) | offset(4)   | data (8 B aligned)
 *   END   (0x302,  8 B):  node_id(4) | total_size(4)
 *   RESP  (0x305, 12 B):  node_id(4) | status(1)|0|0|0 | last_offset(4)
 *
 * Failure modes leave existing app firmware untouched (we erase STAGING
 * only AFTER the encrypted header passes board+CRC validation).
 */
#ifndef __MOTOR_UPGRADE_H__
#define __MOTOR_UPGRADE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* CAN IDs for the upgrade sub-protocol (in the unused 0x300 range). */
#define CANID_UPG_START   0x300U
#define CANID_UPG_DATA    0x301U
#define CANID_UPG_END     0x302U
#define CANID_UPG_RESP    0x305U

/* Status codes returned in RESP[4]. */
#define UPG_OK              0x00U
#define UPG_WRONG_BOARD     0x01U
#define UPG_SIZE_ERROR      0x02U
#define UPG_WRITE_ERROR     0x03U
#define UPG_BAD_HEADER      0x04U   /* magic / CRC fail */
#define UPG_SEQ_ERROR       0x05U   /* offset mismatch */
#define UPG_NOT_RECEIVING   0x06U   /* DATA/END before START */
#define UPG_FRAME_TOO_SHORT 0x07U

void Upgrade_Init(void);
void Upgrade_OnCanFrame(uint32_t id, const uint8_t *data, uint8_t len);
void Upgrade_PollReboot(void);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_UPGRADE_H__ */
