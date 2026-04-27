/* ============================================================================
 * ⚠️  MUST stay byte-identical to App/BSP/CPU_ID.c
 * Bootloader 与 App 必须用完全相同的 UID→SN 算法，否则同一颗 MCU 在
 * 两端算出的 SN 不一致，会直接把本机锁死（无法 OTA）。
 * 任何算法变动都要两侧同步修改。
 * ========================================================================== */

#include "CPU_ID.h"

#include "stdint.h"
#include "stdlib.h"

cpu_id_t cpu_id;

// 将 LOT_NUM 的 7 个 ASCII 字节折叠为 1 字节，用于跨批次离散
// LOT_NUM 分布：Uid1[31:8] (3 字节) + Uid2[31:0] (4 字节)
static uint8_t foldLotNum(void) {
    return (uint8_t)( ((cpu_id.Uid1 >>  8) & 0xFF)
                    ^ ((cpu_id.Uid1 >> 16) & 0xFF)
                    ^ ((cpu_id.Uid1 >> 24) & 0xFF)
                    ^ ( cpu_id.Uid2        & 0xFF)
                    ^ ((cpu_id.Uid2 >>  8) & 0xFF)
                    ^ ((cpu_id.Uid2 >> 16) & 0xFF)
                    ^ ((cpu_id.Uid2 >> 24) & 0xFF) );
}

static void splitUid(uint8_t *SPC_8bit_UID_0, uint8_t *SPC_8bit_UID_1, uint8_t *SPC_8bit_UID_2, uint8_t *SPC_8bit_UID_3) {
    // 方案 B：WAF_NUM ⊕ LOT_fold，引入跨批次离散度
    *SPC_8bit_UID_0 = (uint8_t)(cpu_id.Uid1 & 0xFF) ^ foldLotNum();
    *SPC_8bit_UID_1 = (cpu_id.Uid0 >> 16) & 0xFF;
    *SPC_8bit_UID_2 = (cpu_id.Uid0 >>  8) & 0xFF;
    *SPC_8bit_UID_3 =  cpu_id.Uid0        & 0xFF;
}

void get_cpu_id(void) {

  // 读取 STM32 96-bit UID
  cpu_id.Uid0 = HAL_GetUIDw0();   // X/Y 坐标
  cpu_id.Uid1 = HAL_GetUIDw1();   // WAF_NUM + LOT_NUM 低 24 位
  cpu_id.Uid2 = HAL_GetUIDw2();   // LOT_NUM 高 32 位

  splitUid(&cpu_id.SPC_8bit_UID_0, &cpu_id.SPC_8bit_UID_1, &cpu_id.SPC_8bit_UID_2, &cpu_id.SPC_8bit_UID_3);

}
