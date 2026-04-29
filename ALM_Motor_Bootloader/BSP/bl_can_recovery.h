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
 *   - The App's saved params (incl. node_id) live at MOT_PARAMS_BASE
 *     (0x0801F800), which sits OUTSIDE the staging-erase region by design
 *     so it survives every OTA. On entry the listener reads { magic, node_id }
 *     from that page:
 *       - magic == MOT_PARAMS_MAGIC ("APJA"): filter incoming START / DATA /
 *         END frames by data[0..3] == node_id, and reply with that node_id
 *         in the RESP. Multiple bricked modules on the same bus can be
 *         recovered without isolation.
 *       - magic missing (virgin device, or page corrupted): fall back to
 *         accept-any mode and reply with node_id = 0xFFFFFFFF. Single-
 *         bricked-module recovery only in this fallback.
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
