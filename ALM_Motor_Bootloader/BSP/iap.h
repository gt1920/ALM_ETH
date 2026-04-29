/**
 * iap.h - ALM_Motor_Bootloader IAP entry
 *
 * .mot file layout (Copy_Bin output, written to MOT_STAGE_BASE by ALM_Motor_APP):
 *
 *   [  0.. 15] Encrypted FW_ID header — independent CBC, iv = copy of global IV
 *                plaintext: magic(4,LE) | board_id(4,LE) | fw_sn(4,LE) | crc32(4,LE)
 *   [ 16.. 31] Encrypted payload block-0 — chain CBC, iv = copy of global IV
 *                plaintext: userstr_len(4) | "LEDFW012"(8) | filesize(4,LE)
 *   [ 32.. 47] Encrypted CRC block — chain CBC continues
 *                plaintext: crc_magic(4,LE,"MOTC") | fw_crc32(4,LE) | reserved(8)
 *   [ 48..end] Encrypted firmware blocks — chain continues
 *
 * Boot flow (brick-safe):
 *   1. Decrypt staging[0..15] → verify magic == "GTFW", board_id == "MOT", CRC.
 *      Any failure → jump app, leave STAGING untouched.
 *   2. Decrypt block-0 → extract filesize.
 *   3. Decrypt CRC block → verify crc_magic == "MOTC", read fw_crc32.
 *      Pre-CRC .mot (older Copy_Bin) → refuse to flash (jump app).
 *   4. PASS 1: walk encrypted payload, decrypt, compute CRC32 of plaintext;
 *      compare against fw_crc32. Mismatch → jump app, STAGING intact.
 *   5. Erase APP region; program APP from re-decrypted staging.
 *      Any flash error → NVIC_SystemReset (re-enters BL with STAGING intact).
 *   6. PASS 2: re-read APP region, recompute CRC32, compare. Mismatch →
 *      NVIC_SystemReset (retry on next boot).
 *   7. Erase STAGING (clears magic so we don't re-flash) → jump APP.
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
