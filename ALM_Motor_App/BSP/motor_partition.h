/* ============================================================================
 *  motor_partition.h — STM32G0B1KBUx flash partition for ALM_Motor.
 *
 *  This header MUST be byte-identical between:
 *    ALM_Motor_Bootloader/BSP/motor_partition.h
 *    ALM_Motor_App/BSP/motor_partition.h
 *  Bootloader and App share these constants — drift breaks OTA.
 *
 *  Layout (128 KB internal flash, 2 KB sector):
 *    0x08000000 + 12 KB  — Bootloader (.text, .data, vector table, CAN recovery)
 *    0x08003000 + 52 KB  — Application
 *    0x08010000 + 62 KB  — Staging (encrypted .mot delivered via CAN FD)
 *    0x0801F800 + 2 KB   — Persistent params (node_id, calibration, etc.)
 *
 *  Bootloader expanded from 8 KB → 12 KB across two page moves: +2 KB for
 *  the brick-recovery CAN-FD listener (HAL_FDCAN pulls in a lot of code),
 *  +2 KB headroom for future BL features. APP shrank by the same two pages.
 *
 *  The PARAMS page is carved out of the back of STAGING so that the BL's
 *  staging-erase pass leaves it intact. That way the App's saved node_id
 *  survives every OTA, and the BL recovery listener can read its own
 *  node_id from flash instead of broadcasting. The App's adjuster_flash.c
 *  uses ADJUSTER_FLASH_ADDR == MOT_PARAMS_BASE (must stay equal).
 * ========================================================================== */
#ifndef __MOTOR_PARTITION_H__
#define __MOTOR_PARTITION_H__

#include <stdint.h>

#define MOT_FLASH_BASE          0x08000000UL
#define MOT_FLASH_SECTOR_SIZE   0x00000800UL    /* 2 KB page */
#define MOT_FLASH_TOTAL_SIZE    0x00020000UL    /* 128 KB    */

#define MOT_BL_BASE             0x08000000UL
#define MOT_BL_SIZE             0x00003000UL    /* 12 KB (6 pages)  */

#define MOT_APP_BASE            0x08003000UL
#define MOT_APP_SIZE            0x0000D000UL    /* 52 KB (26 pages) */
#define MOT_APP_END             (MOT_APP_BASE + MOT_APP_SIZE)

#define MOT_STAGE_BASE          0x08010000UL
#define MOT_STAGE_SIZE          0x0000F800UL    /* 62 KB (31 pages) */
#define MOT_STAGE_END           (MOT_STAGE_BASE + MOT_STAGE_SIZE)

#define MOT_PARAMS_BASE         (MOT_STAGE_BASE + MOT_STAGE_SIZE)   /* 0x0801F800 */
#define MOT_PARAMS_SIZE         0x00000800UL    /* 2 KB (1 page) */

/* ---- Persistent-params layout (subset; first 12 bytes used by BL) ---- */
/* Plaintext at MOT_PARAMS_BASE, written by ALM_Motor_App/BSP/adjuster_flash.c.
   BL only reads { magic[4], param_version[2], reserved0[2], node_id[4] }; the
   rest of the struct is opaque to the BL. */
#define MOT_PARAMS_MAGIC        0x41504A41UL    /* "APJA" — ADJUSTER_FLASH_MAGIC */

/* ---- Firmware identity (must match Copy_Bin output for Motor) ---- */
#define MOT_FW_ID_MAGIC         0x47544657UL    /* "GTFW" LE — same as ETH      */
#define MOT_BOARD_ID            0x00544F4DUL    /* "MOT\0" LE                   */
#define MOT_FW_SN_WILDCARD      0xA5C3F09EUL    /* any MCU may install          */

#endif /* __MOTOR_PARTITION_H__ */
