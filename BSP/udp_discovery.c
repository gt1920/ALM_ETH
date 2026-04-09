/**
 * udp_discovery.c - UDP device announcement broadcaster
 *
 * MCU periodically broadcasts a 32-byte announcement to
 * 255.255.255.255:40001 so PC apps on the same L2 segment
 * can discover the device regardless of IP subnet.
 *
 * Packet format:
 *   [0..3]   "DTDR"          magic
 *   [4..9]   MAC address     6 bytes
 *   [10..25] hostname         16 bytes (null-padded)
 *   [26..29] FW version      4 bytes (major.minor.patch.build)
 *   [30..31] TCP port        2 bytes LE
 */

#include "udp_discovery.h"
#include "tcp_server.h"
#include "main.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include <string.h>

#define MAGIC_RESP  "DTDR"
#define RESP_LEN    32
#define ANNOUNCE_INTERVAL_MS  3000U

/* FW version (keep in sync with comm_protocol.c) */
#define FW_VER_0  1
#define FW_VER_1  0
#define FW_VER_2  0
#define FW_VER_3  1

static struct udp_pcb *disc_pcb = NULL;

void UDP_Discovery_Init(void)
{
    disc_pcb = udp_new();
    if (disc_pcb != NULL)
    {
        /* Allow sending to 255.255.255.255 */
        ip_set_option(disc_pcb, SOF_BROADCAST);
        udp_bind(disc_pcb, IP_ADDR_ANY, 0);
    }
}

void UDP_Discovery_Poll(void)
{
    static uint32_t last_tick = 0;
    uint32_t now;
    struct pbuf *p;
    uint8_t *buf;
    struct netif *nif;
    ip4_addr_t dst4;

    if (disc_pcb == NULL)
        return;

    now = HAL_GetTick();
    if ((now - last_tick) < ANNOUNCE_INTERVAL_MS)
        return;
    last_tick = now;

    /* Only announce when we have an IP address */
    nif = netif_default;
    if (nif == NULL || ip4_addr_isany(netif_ip4_addr(nif)))
        return;

    p = pbuf_alloc(PBUF_TRANSPORT, RESP_LEN, PBUF_RAM);
    if (p == NULL)
        return;

    buf = (uint8_t *)p->payload;
    memset(buf, 0, RESP_LEN);

    /* Magic */
    memcpy(buf, MAGIC_RESP, 4);

    /* MAC address */
    memcpy(buf + 4, nif->hwaddr, 6);

    /* Hostname */
    if (nif->hostname != NULL)
        strncpy((char *)(buf + 10), nif->hostname, 15);

    /* FW version */
    buf[26] = FW_VER_0;
    buf[27] = FW_VER_1;
    buf[28] = FW_VER_2;
    buf[29] = FW_VER_3;

    /* TCP port (LE) */
    buf[30] = (uint8_t)(TCP_SERVER_PORT & 0xFF);
    buf[31] = (uint8_t)((TCP_SERVER_PORT >> 8) & 0xFF);

    /* Broadcast to 255.255.255.255 */
    IP4_ADDR(&dst4, 255, 255, 255, 255);

    udp_sendto(disc_pcb, p, (const ip_addr_t *)&dst4, UDP_DISCOVERY_PORT);
    pbuf_free(p);
}
