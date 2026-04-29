/**
 * bl_can_recovery.h - Brick-recovery CAN-FD listener for ALM_Motor_Bootloader
 *
 * Last-resort upgrade path: when the BL boots and finds the APP region's
 * vector table corrupted (bad MSP / Reset_Handler), there's nothing valid
 * to jump to and the App-side OTA receiver can't run. This listener brings
 * up FDCAN1 directly from the bootloader, accepts the standard MUPG
 * protocol (CANID_UPG_START / DATA / END / RESP) used by ALM_Motor_App's
 * motor_upgrade.c, writes the encrypted .mot to STAGING, then NVIC_SystemReset.
 *
 * On the next boot the normal BL_Run() flow picks up STAGING, verifies and
 * programs APP, and the device is recovered.
 *
 * Identity:
 *   - Without a healthy APP we cannot read the App's saved node_id (it lives
 *     in a flash page at 0x0801F800 that may itself have been wiped). The
 *     listener therefore accepts upgrade frames addressed to ANY node_id
 *     during recovery. Single-bricked-module recovery only — power-down or
 *     bus-isolate other healthy modules before triggering OTA in this mode.
 *
 * This module never returns through normal control flow: either it triggers
 * NVIC_SystemReset on a complete .mot, or it spin-waits forever for one.
 */
#ifndef __BL_CAN_RECOVERY_H__
#define __BL_CAN_RECOVERY_H__

#ifdef __cplusplus
extern "C" {
#endif

void BL_CanRecovery_RunForever(void);

#ifdef __cplusplus
}
#endif

#endif /* __BL_CAN_RECOVERY_H__ */
