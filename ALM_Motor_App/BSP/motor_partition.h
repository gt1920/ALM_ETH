/* ============================================================================
 *  motor_partition.h — STM32G0B1KBUx flash partition for ALM_Motor.
 *
 *  This header MUST be byte-identical between:
 *    ALM_Motor_Bootloader/BSP/motor_partition.h
 *    ALM_Motor_App/BSP/motor_partition.h
 *  Bootloader and App share these constants — drift breaks OTA.
 *
 *  Layout (128 KB internal flash, 2 KB sector):
 *    0x08000000 + 8 KB   — Bootloader (.text, .data, vector table)
 *    0x08002000 + 56 KB  — Application
 *    0x08010000 + 64 KB  — Staging (encrypted .mot delivered via CAN FD)
 * ========================================================================== */
#ifndef __MOTOR_PARTITION_H__
#define __MOTOR_PARTITION_H__

#include <stdint.h>

#define MOT_FLASH_BASE          0x08000000UL
#define MOT_FLASH_SECTOR_SIZE   0x00000800UL    /* 2 KB page */
#define MOT_FLASH_TOTAL_SIZE    0x00020000UL    /* 128 KB    */

#define MOT_BL_BASE             0x08000000UL
#define MOT_BL_SIZE             0x00002000UL    /* 8 KB      */

#define MOT_APP_BASE            0x08002000UL
#define MOT_APP_SIZE            0x0000E000UL    /* 56 KB     */
#define MOT_APP_END             (MOT_APP_BASE + MOT_APP_SIZE)

#define MOT_STAGE_BASE          0x08010000UL
#define MOT_STAGE_SIZE          0x00010000UL    /* 64 KB     */
#define MOT_STAGE_END           (MOT_STAGE_BASE + MOT_STAGE_SIZE)

/* ---- Firmware identity (must match Copy_Bin output for Motor) ---- */
#define MOT_FW_ID_MAGIC         0x47544657UL    /* "GTFW" LE — same as ETH      */
#define MOT_BOARD_ID            0x00544F4DUL    /* "MOT\0" LE                   */
#define MOT_FW_SN_WILDCARD      0xA5C3F09EUL    /* any MCU may install          */

#endif /* __MOTOR_PARTITION_H__ */
