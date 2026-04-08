/**
  * lwip_app.c - LwIP initialisation with DHCP (bare-metal, no RTOS)
  *
  * Call LWIP_APP_Init() once after HAL & PHY init,
  * then call LWIP_APP_Poll() repeatedly in the main while(1) loop.
  *
  * g_lwip_ipinfo is a global struct you can watch in Keil to see the
  * assigned IP address, netmask, gateway and DHCP state.
  */
#include "lwip_app.h"
#include "ethernetif.h"
#include "main.h"

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/ip4_addr.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"

/* ---- Global IP info (watch in Keil) ---- */
LWIP_IPInfo_t g_lwip_ipinfo;

/* ---- LwIP netif ---- */
static struct netif gnetif;

/* ---- helper: copy ip4_addr to uint8_t[4] ---- */
static void ip4_to_array(const ip4_addr_t *addr, uint8_t out[4])
{
  uint32_t val = ip4_addr_get_u32(addr);
  out[0] = (uint8_t)(val & 0xFFU);
  out[1] = (uint8_t)((val >> 8) & 0xFFU);
  out[2] = (uint8_t)((val >> 16) & 0xFFU);
  out[3] = (uint8_t)((val >> 24) & 0xFFU);
}

/* ---- netif status callback ---- */
static void netif_status_cb(struct netif *nif)
{
  if (dhcp_supplied_address(nif))
  {
    ip4_to_array(netif_ip4_addr(nif),  g_lwip_ipinfo.ip);
    ip4_to_array(netif_ip4_netmask(nif), g_lwip_ipinfo.netmask);
    ip4_to_array(netif_ip4_gw(nif),    g_lwip_ipinfo.gateway);
    g_lwip_ipinfo.dhcp_state = 2;   /* addr assigned */
  }
}

/* ---- init ---- */
void LWIP_APP_Init(void)
{
  ip4_addr_t ipaddr, netmask, gw;

  /* zero out the global struct */
  g_lwip_ipinfo.ip[0] = g_lwip_ipinfo.ip[1] = g_lwip_ipinfo.ip[2] = g_lwip_ipinfo.ip[3] = 0;
  g_lwip_ipinfo.netmask[0] = g_lwip_ipinfo.netmask[1] = g_lwip_ipinfo.netmask[2] = g_lwip_ipinfo.netmask[3] = 0;
  g_lwip_ipinfo.gateway[0] = g_lwip_ipinfo.gateway[1] = g_lwip_ipinfo.gateway[2] = g_lwip_ipinfo.gateway[3] = 0;
  g_lwip_ipinfo.dhcp_state = 0;
  g_lwip_ipinfo.ip_source  = 0;
  g_lwip_ipinfo.link_up = 0;

  /* Initialize LwIP stack */
  lwip_init();

  /* Start with 0.0.0.0 because DHCP will assign the real address */
  ip4_addr_set_zero(&ipaddr);
  ip4_addr_set_zero(&netmask);
  ip4_addr_set_zero(&gw);

  /* Add the network interface */
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);
  netif_set_default(&gnetif);

  /* Register status callback */
  netif_set_status_callback(&gnetif, netif_status_cb);

  /* Bring the interface up */
  netif_set_up(&gnetif);
  netif_set_link_up(&gnetif);
  g_lwip_ipinfo.link_up = 1;

  /* Start DHCP */
  dhcp_start(&gnetif);
  g_lwip_ipinfo.dhcp_state = 1;   /* started */
}

/* Gratuitous ARP interval: 30 seconds (in ms) */
#define GARP_INTERVAL_MS   30000U

/* ---- poll (call from main loop) ---- */
void LWIP_APP_Poll(void)
{
  /* 1. Read any received Ethernet frames into LwIP */
  ethernetif_input(&gnetif);

  /* 2. Handle LwIP timers (ARP, DHCP, TCP, ...) */
  sys_check_timeouts();

  /* 3. Periodic Gratuitous ARP so other devices keep our MAC in their cache */
  static uint32_t garp_timer = 0;
  uint32_t now = HAL_GetTick();
  if ((now - garp_timer) >= GARP_INTERVAL_MS)
  {
    garp_timer = now;
    if (!ip4_addr_isany(netif_ip4_addr(&gnetif)))
    {
      etharp_gratuitous(&gnetif);
    }
  }

  /* 4. Update global IP info struct from current netif state */
  ip4_to_array(netif_ip4_addr(&gnetif),    g_lwip_ipinfo.ip);
  ip4_to_array(netif_ip4_netmask(&gnetif), g_lwip_ipinfo.netmask);
  ip4_to_array(netif_ip4_gw(&gnetif),      g_lwip_ipinfo.gateway);

  if (dhcp_supplied_address(&gnetif))
  {
    /* DHCP成功 */
    g_lwip_ipinfo.dhcp_state = 2;
    g_lwip_ipinfo.ip_source  = 1;
  }
  else if (autoip_supplied_address(&gnetif))
  {
    /* 无路由器，AutoIP分配了169.254.x.x */
    g_lwip_ipinfo.dhcp_state = 4;
    g_lwip_ipinfo.ip_source  = 2;
  }
  else if (g_lwip_ipinfo.dhcp_state == 1)
  {
    /* DHCP等待中，检查是否已触发AutoIP */
    struct dhcp *d = netif_dhcp_data(&gnetif);
    if (d != NULL && d->tries >= LWIP_DHCP_AUTOIP_COOP_TRIES)
    {
      g_lwip_ipinfo.dhcp_state = 3;  /* DHCP超时，AutoIP已启动 */
    }
  }
}
