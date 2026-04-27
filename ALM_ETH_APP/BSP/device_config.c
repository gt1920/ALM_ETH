#include "device_config.h"
#include "main.h"
#include <string.h>

/* ---------- Flash layout ----------
 * STM32H563VGTx: 1 MB dual-bank flash, 8 KB per sector.
 * We use the LAST sector of Bank 2 (sector 63, address 0x080FE000)
 * to store a 32-byte config block (two quad-words).
 */
#define CONFIG_FLASH_BANK    FLASH_BANK_2
#define CONFIG_FLASH_SECTOR  63
#define CONFIG_FLASH_ADDR    0x080FE000UL
#define CONFIG_MAGIC         0x44544352UL  /* "DTCR" */

/* Config block: 32 bytes = two quad-words (flash programs 16 bytes at a time) */
typedef struct {
    uint32_t magic;                        /*  4 bytes */
    char     name[DEVICE_NAME_MAX_LEN];    /* 16 bytes */
    uint32_t reserved[3];                  /* 12 bytes  => total 32 */
} ConfigBlock_t;

/* ---------- RAM state ---------- */
char g_device_hostname[DEVICE_NAME_MAX_LEN + 1];

static char  g_custom_name[DEVICE_NAME_MAX_LEN + 1];
static uint8_t g_saved_mac[6];
static int   g_has_custom = 0;

/* ---------- Helpers ---------- */
static void rebuild_hostname(void)
{
    if (g_has_custom)
    {
        memcpy(g_device_hostname, g_custom_name, DEVICE_NAME_MAX_LEN);
        g_device_hostname[DEVICE_NAME_MAX_LEN] = '\0';
    }
    else
    {
        static const char hex[] = "0123456789ABCDEF";
        g_device_hostname[0] = 'D';
        g_device_hostname[1] = 'T';
        g_device_hostname[2] = '-';
        g_device_hostname[3] = hex[(g_saved_mac[3] >> 4) & 0x0F];
        g_device_hostname[4] = hex[ g_saved_mac[3]       & 0x0F];
        g_device_hostname[5] = hex[(g_saved_mac[4] >> 4) & 0x0F];
        g_device_hostname[6] = hex[ g_saved_mac[4]       & 0x0F];
        g_device_hostname[7] = hex[(g_saved_mac[5] >> 4) & 0x0F];
        g_device_hostname[8] = hex[ g_saved_mac[5]       & 0x0F];
        g_device_hostname[9] = '\0';
    }
}

static void write_config_to_flash(void)
{
    ConfigBlock_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    if (g_has_custom)
    {
        cfg.magic = CONFIG_MAGIC;
        memcpy(cfg.name, g_custom_name, DEVICE_NAME_MAX_LEN);
    }
    /* else magic stays 0 => "no config" */

    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error = 0;

    HAL_FLASH_Unlock();

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks     = CONFIG_FLASH_BANK;
    erase.Sector    = CONFIG_FLASH_SECTOR;
    erase.NbSectors = 1;
    HAL_FLASHEx_Erase(&erase, &sector_error);

    /* Program two quad-words (32 bytes) */
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,
                      CONFIG_FLASH_ADDR,
                      (uint32_t)&cfg);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,
                      CONFIG_FLASH_ADDR + 16U,
                      (uint32_t)((uint8_t *)&cfg + 16));

    HAL_FLASH_Lock();
}

/* ---------- Public API ---------- */

void DeviceConfig_Init(void)
{
    memset(g_custom_name, 0, sizeof(g_custom_name));
    memset(g_device_hostname, 0, sizeof(g_device_hostname));
    g_has_custom = 0;

    const volatile ConfigBlock_t *cfg =
        (const volatile ConfigBlock_t *)CONFIG_FLASH_ADDR;

    if (cfg->magic == CONFIG_MAGIC &&
        cfg->name[0] != '\0' && cfg->name[0] != (char)0xFF)
    {
        memcpy(g_custom_name, (const void *)cfg->name, DEVICE_NAME_MAX_LEN);
        g_custom_name[DEVICE_NAME_MAX_LEN] = '\0';
        g_has_custom = 1;
    }
}

void DeviceConfig_BuildHostname(const uint8_t *mac6)
{
    memcpy(g_saved_mac, mac6, 6);
    rebuild_hostname();
}

void DeviceConfig_SetName(const char *name, uint8_t len)
{
    memset(g_custom_name, 0, sizeof(g_custom_name));

    if (name != NULL && len > 0)
    {
        uint8_t n = (len > DEVICE_NAME_MAX_LEN) ? DEVICE_NAME_MAX_LEN : len;
        memcpy(g_custom_name, name, n);
        g_has_custom = 1;
    }
    else
    {
        g_has_custom = 0;
    }

    rebuild_hostname();
    write_config_to_flash();
}
