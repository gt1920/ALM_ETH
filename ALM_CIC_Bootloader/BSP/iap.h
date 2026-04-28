/**
 * iap.h - ALM_CIC_Bootloader IAP entry
 *
 * .gt file layout written by Copy_Bin (placed at W25Q[BL_W25Q_GT_ADDR]):
 *
 *   [  0.. 15] Encrypted FW_ID header — independent CBC, iv = copy of global IV
 *                plaintext: magic(4,LE) | board_id(4,LE) | fw_sn(4,LE) | crc32(4,LE)
 *   [ 16.. 31] Encrypted payload block-0 — chain CBC, iv = copy of global IV
 *                plaintext: userstr_len(4) | "LEDFW012"(8) | filesize(4,LE)
 *   [ 32..end] Encrypted firmware blocks — chain continues
 *                plaintext: firmware[0..15], firmware[16..31], ...
 *                last block: firmware tail + timestamp padding to 16B boundary
 *
 * Boot flow:
 *   1. Decrypt W25Q[0..15] → check magic, board_id, fw_sn, CRC
 *      magic != FW_ID_MAGIC  → no upgrade, jump app
 *      board/sn/crc fail     → erase .gt sector, jump app
 *   2. Decrypt W25Q[16..31] → extract filesize
 *   3. Erase + program internal flash from BL_APP_START_ADDR
 *   4. Erase W25Q[0..FFF] (clears .gt magic so we don't re-flash next boot)
 *   5. Jump to app
 */
#ifndef __IAP_H__
#define __IAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ---- board & firmware identity ---- */
#define BL_BOARD_ID                    0x00485445UL  /* "ETH\0" packed */
#define FW_ID_MAGIC                    0x47544657UL  /* "GTFW" LE       */
#define FW_SN_WILDCARD                 0xA5C3F09EUL  /* any MCU may install */

/* ---- W25Q .gt placement ---- */
#define BL_W25Q_GT_ADDR                0x00000000UL  /* .gt starts here */

/* ---- internal flash app region ---- */
#define BL_APP_START_ADDR              0x08008000UL  /* matches ALM_CIC_APP scatter */
#define BL_APP_END_ADDR                0x08100000UL
#define BL_MAX_FW_SIZE                 (BL_APP_END_ADDR - BL_APP_START_ADDR)

/* Run the bootloader. Never returns. */
void BL_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* __IAP_H__ */
