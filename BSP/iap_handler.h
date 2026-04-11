#ifndef __IAP_HANDLER_H__
#define __IAP_HANDLER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ---- IAP command/sub-command definitions ---- */
#define CMD_IAP                 0x30

#define IAP_SUBCMD_BEGIN        0x01
#define IAP_SUBCMD_DATA         0x02
#define IAP_SUBCMD_END          0x03
#define IAP_SUBCMD_REBOOT       0x04
#define IAP_SUBCMD_ACK          0x81

/* IAP ACK status codes */
#define IAP_STATUS_OK           0x00
#define IAP_STATUS_FLASH_ERR    0x01
#define IAP_STATUS_CRC_ERR      0x02
#define IAP_STATUS_SIZE_ERR     0x03
#define IAP_STATUS_SEQ_ERR      0x04
#define IAP_STATUS_BUSY         0x05

/* ---- External flash layout for IAP ---- */
#define IAP_FW_BASE_ADDR        0x000000UL   /* firmware staging area */
#define IAP_FW_MAX_SIZE         (960U * 1024U)
#define IAP_META_ADDR           0x0F0000UL   /* metadata sector */
#define IAP_META_MAGIC          0x49415055UL /* "IAPU" */

/* ---- IAP metadata structure (32 bytes, stored in ext flash) ---- */
typedef struct {
    uint32_t magic;         /* == IAP_META_MAGIC when upgrade pending */
    uint32_t fw_size;       /* firmware size in bytes */
    uint32_t fw_crc32;      /* CRC32 of entire firmware image */
    uint32_t fw_version;    /* version (optional) */
    uint32_t reserved[4];   /* pad to 32 bytes */
} IAP_Meta_t;

/* ---- API ---- */
void IAP_Process_Command(const uint8_t *buf, uint16_t len, uint16_t seq);

#ifdef __cplusplus
}
#endif

#endif /* __IAP_HANDLER_H__ */
