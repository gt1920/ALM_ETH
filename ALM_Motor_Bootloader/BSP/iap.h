/**
 * iap.h - ALM_Motor_Bootloader IAP entry
 *
 * .mot file layout (Copy_Bin output, written to MOT_STAGE_BASE by ALM_Motor_APP):
 *
 *   [  0.. 15] Encrypted FW_ID header — independent CBC, iv = copy of global IV
 *                plaintext: magic(4,LE) | board_id(4,LE) | fw_sn(4,LE) | crc32(4,LE)
 *   [ 16.. 31] Encrypted payload block-0 — chain CBC, iv = copy of global IV
 *                plaintext: userstr_len(4) | "LEDFW012"(8) | filesize(4,LE)
 *   [ 32..end] Encrypted firmware blocks — chain continues
 *
 * Boot flow:
 *   1. Decrypt internal flash[STAGE..STAGE+15] → check magic, board_id, fw_sn, CRC
 *      magic != FW_ID_MAGIC      → no upgrade, jump app
 *      board != "MOT" / crc fail → erase staging, jump app
 *   2. Decrypt block-0 → extract filesize
 *   3. Erase + program app region from MOT_APP_BASE
 *   4. Erase staging region (clears magic so we don't re-flash next boot)
 *   5. Jump to app
 */
#ifndef __IAP_H__
#define __IAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "motor_partition.h"
#include <stdint.h>

/* Run the bootloader. Never returns. */
void BL_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* __IAP_H__ */
