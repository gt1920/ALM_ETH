#ifndef __BOOT_CONFIG_H__
#define __BOOT_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ---- Memory layout ---- */
#define APP_BASE_ADDR           0x08010000UL   /* Application start (64KB offset) */
#define APP_MAX_SIZE            (952U * 1024U)  /* 952KB */

/* ---- External flash layout (must match iap_handler.h) ---- */
#define IAP_FW_BASE_ADDR        0x000000UL
#define IAP_FW_MAX_SIZE         (960U * 1024U)
#define IAP_META_ADDR           0x0F0000UL
#define IAP_META_MAGIC          0x49415055UL    /* "IAPU" */

/* ---- IAP metadata structure ---- */
typedef struct {
    uint32_t magic;
    uint32_t fw_size;
    uint32_t fw_crc32;
    uint32_t fw_version;
    uint32_t reserved[4];
} IAP_Meta_t;

/* ---- Internal flash parameters ---- */
#define INTERNAL_FLASH_SECTOR_SIZE  8192U       /* 8KB per sector */
#define APP_START_SECTOR            8           /* sector 8 = 0x08010000 */

/* ---- Functions ---- */
void Boot_JumpToApp(uint32_t addr);
uint32_t Boot_CRC32_Calc(const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_CONFIG_H__ */
