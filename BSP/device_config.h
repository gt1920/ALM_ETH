#ifndef __DEVICE_CONFIG_H__
#define __DEVICE_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DEVICE_NAME_MAX_LEN  16

/* Live hostname buffer (custom name or default "DT-XXYYZZ") */
extern char g_device_hostname[DEVICE_NAME_MAX_LEN + 1];

/* Load custom name from flash (call early, before LWIP init) */
void DeviceConfig_Init(void);

/* Build hostname from MAC: uses custom name if stored, else "DT-XXYYZZ".
   Call from ethernetif_init after MAC is available. */
void DeviceConfig_BuildHostname(const uint8_t *mac6);

/* Save custom name to flash and update g_device_hostname.
   name=NULL or len=0 clears the custom name (reverts to default). */
void DeviceConfig_SetName(const char *name, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __DEVICE_CONFIG_H__ */
