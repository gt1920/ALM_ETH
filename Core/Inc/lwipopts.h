#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

/* --------------- System / Platform --------------- */
#define NO_SYS                          1       /* bare-metal, no OS */
#define SYS_LIGHTWEIGHT_PROT            0
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0

/* --------------- Memory --------------- */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (16 * 1024)   /* heap size */

#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                6
#define MEMP_NUM_TCP_PCB                10
#define MEMP_NUM_TCP_PCB_LISTEN         6
#define MEMP_NUM_TCP_SEG                12
#define MEMP_NUM_SYS_TIMEOUT            10

#define PBUF_POOL_SIZE                  16
#define PBUF_POOL_BUFSIZE               1536

/* --------------- IP --------------- */
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0

/* --------------- ICMP --------------- */
#define LWIP_ICMP                       1

/* --------------- DHCP --------------- */
#define LWIP_DHCP                       1

/* --------------- UDP --------------- */
#define LWIP_UDP                        1

/* --------------- TCP --------------- */
#define LWIP_TCP                        1
#define TCP_MSS                         1460
#define TCP_WND                         (4 * TCP_MSS)
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define TCP_SND_QUEUELEN                (2 * TCP_SND_BUF / TCP_MSS)

/* --------------- ARP --------------- */
#define LWIP_ARP                        1
#define ARP_TABLE_SIZE                  10
#define ARP_QUEUEING                    1
#define ETHARP_SUPPORT_STATIC_ENTRIES   1

/* --------------- DNS (optional) --------------- */
#define LWIP_DNS                        0

/* --------------- IGMP --------------- */
#define LWIP_IGMP                       0

/* --------------- Checksum --------------- */
/* STM32H5 ETH hardware computes IP/TCP/UDP checksums */
#define CHECKSUM_BY_HARDWARE            1

#if CHECKSUM_BY_HARDWARE
  #define CHECKSUM_GEN_IP               0
  #define CHECKSUM_GEN_UDP              0
  #define CHECKSUM_GEN_TCP              0
  #define CHECKSUM_GEN_ICMP             0
  #define CHECKSUM_CHECK_IP             0
  #define CHECKSUM_CHECK_UDP            0
  #define CHECKSUM_CHECK_TCP            0
  #define CHECKSUM_CHECK_ICMP           0
#endif

/* --------------- Stats / Debug --------------- */
#define LWIP_STATS                      0
#define LWIP_DEBUG                      0

/* --------------- Netif --------------- */
#define LWIP_NETIF_HOSTNAME             1
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1

/* --------------- Ethernet --------------- */
#define LWIP_ETHERNET                   1
#define LWIP_ARP                        1

#endif /* __LWIPOPTS_H__ */
