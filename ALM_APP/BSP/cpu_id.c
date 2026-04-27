/* ============================================================================
 *  cpu_id.c — derive a stable 32-bit device SN from STM32 96-bit UID.
 *
 *  ⚠️  MUST stay byte-identical to ALM_Bootloader/BSP/cpu_id.c.
 *      (Bootloader and App must produce the same SN for a given MCU.)
 * ========================================================================== */
#include "cpu_id.h"

cpu_id_t cpu_id;
uint32_t g_device_sn = 0U;

/* Fold LOT_NUM (7 ASCII bytes spread across Uid1[31:8] and Uid2) into 1 byte
   for cross-batch dispersion. */
static uint8_t foldLotNum(void)
{
    return (uint8_t)( ((cpu_id.Uid1 >>  8) & 0xFFU)
                    ^ ((cpu_id.Uid1 >> 16) & 0xFFU)
                    ^ ((cpu_id.Uid1 >> 24) & 0xFFU)
                    ^ ( cpu_id.Uid2        & 0xFFU)
                    ^ ((cpu_id.Uid2 >>  8) & 0xFFU)
                    ^ ((cpu_id.Uid2 >> 16) & 0xFFU)
                    ^ ((cpu_id.Uid2 >> 24) & 0xFFU) );
}

/* Split UID into 4 dispersed bytes:
     SPC_0 = WAF_NUM_low  XOR foldLotNum()      (cross-batch dispersion)
     SPC_1 = wafer X high byte
     SPC_2 = wafer X low / Y mid
     SPC_3 = wafer Y low byte
*/
static void splitUid(uint8_t *u0, uint8_t *u1, uint8_t *u2, uint8_t *u3)
{
    *u0 = (uint8_t)( cpu_id.Uid1        & 0xFFU) ^ foldLotNum();
    *u1 = (uint8_t)((cpu_id.Uid0 >> 16) & 0xFFU);
    *u2 = (uint8_t)((cpu_id.Uid0 >>  8) & 0xFFU);
    *u3 = (uint8_t)( cpu_id.Uid0        & 0xFFU);
}

void get_cpu_id(void)
{
    cpu_id.Uid0 = HAL_GetUIDw0();
    cpu_id.Uid1 = HAL_GetUIDw1();
    cpu_id.Uid2 = HAL_GetUIDw2();

    splitUid(&cpu_id.SPC_8bit_UID_0,
             &cpu_id.SPC_8bit_UID_1,
             &cpu_id.SPC_8bit_UID_2,
             &cpu_id.SPC_8bit_UID_3);

    g_device_sn = ((uint32_t)cpu_id.SPC_8bit_UID_0 << 24)
                | ((uint32_t)cpu_id.SPC_8bit_UID_1 << 16)
                | ((uint32_t)cpu_id.SPC_8bit_UID_2 <<  8)
                |  (uint32_t)cpu_id.SPC_8bit_UID_3;
}
