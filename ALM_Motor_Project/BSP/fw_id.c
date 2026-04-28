/* ============================================================================
 *  fw_id.c — embed the FW_HW_ID descriptor into the Motor App firmware image.
 *
 *  Copy_Bin_Motor scans the linked .bin for FW_ID_MAGIC ("GTFW", LE) and uses
 *  the 12-byte block right after it as the plaintext FW_ID header (which it
 *  AES-encrypts and prepends to the .mot upgrade package). The Motor's own
 *  Bootloader/iap.c decrypts that header on boot to validate magic / board_id
 *  / fw_sn / CRC32 before flashing.
 *
 *  Layout (12 bytes, little-endian, NO padding — must stay packed for
 *  byte-perfect match with Copy_Bin's BitConverter calls):
 *     u32 magic     = 'G' 'T' 'F' 'W'      = 0x47544657
 *     u32 board_id  = 'M' 'O' 'T' '\0'     = 0x00544F4D
 *     u32 fw_sn     = 0xA5C3F09E (wildcard) by default;
 *                     replace with a per-MCU SN to bind this build to one MCU.
 * ========================================================================== */
#include <stdint.h>
#include "motor_partition.h"

#define FW_SN_WILDCARD   MOT_FW_SN_WILDCARD

__attribute__((used))
const struct {
    uint32_t magic;
    uint32_t board_id;
    uint32_t fw_sn;
} FW_HW_ID = {
    MOT_FW_ID_MAGIC,
    MOT_BOARD_ID,
    FW_SN_WILDCARD
};
