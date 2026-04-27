/* ============================================================================
 * ⚠️  MUST stay byte-identical to App/BSP/CPU_ID.h
 * Bootloader 与 App 必须用完全相同的 UID→SN 算法，否则同一颗 MCU 在
 * 两端算出的 SN 不一致，会直接把本机锁死（无法 OTA）。
 * 任何算法变动都要两侧同步修改。
 * ========================================================================== */

#ifndef __CPU_ID__H
#define __CPU_ID__H

#include "main.h"

typedef struct {
  uint32_t Uid0;
  uint32_t Uid1;
  uint32_t Uid2;

	uint8_t SPC_8bit_UID_0;
	uint8_t SPC_8bit_UID_1;
	uint8_t SPC_8bit_UID_2;
	uint8_t SPC_8bit_UID_3;

} cpu_id_t;

extern cpu_id_t cpu_id;

void get_cpu_id(void);

#endif
