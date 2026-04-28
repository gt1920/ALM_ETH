/* ============================================================================
 *  fw_id.c — embed the FW_HW_ID descriptor into the firmware image.
 *
 *  The PC-side Copy_Bin tool searches the .bin for FW_ID_MAGIC ("GTFW", LE)
 *  and reads the 12-byte block right after it; the encrypted .gt header is
 *  built from this. The ALM_CIC_Bootloader can do the same on the W25Q-staged
 *  ciphertext header to reject mismatched firmware before flashing.
 *
 *  Layout (12 bytes, little-endian, NO padding — must stay packed for
 *  byte-perfect match with Copy_Bin's BitConverter calls):
 *     u32 magic     = 'G' 'T' 'F' 'W'      = 0x47544657
 *     u32 board_id  = 'E' 'T' 'H' '\0'     = 0x00485445
 *     u32 fw_sn     = 0xA5C3F09E (wildcard) by default;
 *                     replace with a per-MCU SN (computed by cpu_id.c
 *                     on the target MCU) to bind this build to that MCU.
 * ========================================================================== */
#include <stdint.h>

#define FW_ID_MAGIC      0x47544657U   /* "GTFW" little-endian              */
#define FW_ID_BOARD_ETH  0x00485445U   /* "ETH\0" little-endian             */
#define FW_SN_WILDCARD   0xA5C3F09EU   /* any MCU may flash this build      */

/* `__attribute__((used))` keeps the symbol even if no code references it
   (link-time DCE would otherwise drop a const that nobody reads). */
__attribute__((used))
const struct {
    uint32_t magic;
    uint32_t board_id;
    uint32_t fw_sn;
} FW_HW_ID = {
    FW_ID_MAGIC,
    FW_ID_BOARD_ETH,
    FW_SN_WILDCARD
};
