#ifndef LWIP_APP_H
#define LWIP_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ---- Global IP info struct for Keil Watch window ---- */
typedef struct
{
  uint8_t  ip[4];
  uint8_t  netmask[4];
  uint8_t  gateway[4];
  uint8_t  dhcp_state;     /* 0=off, 1=started, 2=addr_assigned, 3=timeout */
  uint8_t  link_up;        /* 1=link up */
} LWIP_IPInfo_t;

extern LWIP_IPInfo_t g_lwip_ipinfo;   /* watch this in Keil debugger */

void LWIP_APP_Init(void);
void LWIP_APP_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* LWIP_APP_H */
