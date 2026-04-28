#ifndef __UDP_DISCOVERY_H__
#define __UDP_DISCOVERY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define UDP_DISCOVERY_PORT  40001

void UDP_Discovery_Init(void);
void UDP_Discovery_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* __UDP_DISCOVERY_H__ */
