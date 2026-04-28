/* ============================================================================
 *  cpu_id.h — derive a stable 32-bit device SN from STM32 96-bit UID.
 *
 *  ⚠️  MUST stay byte-identical to ALM_CIC_Bootloader/BSP/cpu_id.h (and .c).
 *
 *  Both bootloader and app run the same UID→SN algorithm so a single MCU
 *  reports the same SN in both modes; PC-side Copy_Bin reads the very same
 *  value from the firmware's FW_HW_ID block. Any change here MUST be
 *  mirrored on the bootloader side, otherwise OTA bind/unbind breaks.
 * ========================================================================== */
#ifndef __CPU_ID_H__
#define __CPU_ID_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef struct {
    uint32_t Uid0;                /* HAL_GetUIDw0 — wafer X/Y           */
    uint32_t Uid1;                /* HAL_GetUIDw1 — WAF_NUM + LOT low   */
    uint32_t Uid2;                /* HAL_GetUIDw2 — LOT_NUM high 32 b   */

    uint8_t  SPC_8bit_UID_0;      /* high  byte of g_device_sn          */
    uint8_t  SPC_8bit_UID_1;      /*       :                            */
    uint8_t  SPC_8bit_UID_2;      /*       :                            */
    uint8_t  SPC_8bit_UID_3;      /* low   byte of g_device_sn          */
} cpu_id_t;

extern cpu_id_t cpu_id;
extern uint32_t g_device_sn;      /* (SPC_0 << 24) | ... | SPC_3        */

/* Read 96-bit UID, derive 4 dispersed SPC bytes, pack into g_device_sn. */
void get_cpu_id(void);

#ifdef __cplusplus
}
#endif

#endif /* __CPU_ID_H__ */
