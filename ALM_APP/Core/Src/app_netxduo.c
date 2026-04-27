#include "app_netxduo.h"

#include "main.h"
#include "eth.h"
#include "tx_api.h"
#include "nx_api.h"

#define NX_APP_IP_THREAD_STACK_SIZE          2048U
#define NX_APP_PACKET_COUNT                  16U
#define NX_APP_PACKET_SIZE                   1536U
#define NX_APP_BYTE_POOL_SIZE                16384U
#define NX_APP_ARP_CACHE_SIZE                1024U

#define NX_APP_STATIC_IP                     IP_ADDRESS(192, 168, 3, 200)
#define NX_APP_STATIC_MASK                   IP_ADDRESS(255, 255, 255, 0)
#define NX_APP_STATIC_GW                     IP_ADDRESS(192, 168, 3, 1)

/* Provided by NetX STM32 Ethernet driver (Keil Pack / X-CUBE-AZRTOS-H5). */
extern VOID nx_stm32_eth_driver(NX_IP_DRIVER *driver_req_ptr);

static TX_BYTE_POOL nx_app_pool_ctrl;
static UCHAR nx_app_pool_mem[NX_APP_BYTE_POOL_SIZE];

static NX_PACKET_POOL nx_app_packet_pool;
static NX_IP nx_app_ip;

static UCHAR nx_app_ip_thread_stack[NX_APP_IP_THREAD_STACK_SIZE];
static UCHAR nx_app_arp_cache[NX_APP_ARP_CACHE_SIZE];

void APP_NETXDUO_Init(void)
{
  tx_kernel_enter();
}

VOID tx_application_define(VOID *first_unused_memory)
{
  UINT status;
  VOID *packet_pool_mem = NX_NULL;
  ULONG actual_status = 0U;

  NX_PARAMETER_NOT_USED(first_unused_memory);

  nx_system_initialize();

  status = tx_byte_pool_create(&nx_app_pool_ctrl,
                               (CHAR *)"nx_app_pool",
                               nx_app_pool_mem,
                               sizeof(nx_app_pool_mem));
  if (status != TX_SUCCESS)
  {
    Error_Handler();
  }

  status = tx_byte_allocate(&nx_app_pool_ctrl, &packet_pool_mem, NX_APP_PACKET_COUNT * NX_APP_PACKET_SIZE, TX_NO_WAIT);
  if (status != TX_SUCCESS)
  {
    Error_Handler();
  }

  status = nx_packet_pool_create(&nx_app_packet_pool,
                                 (CHAR *)"nx_packet_pool",
                                 NX_APP_PACKET_SIZE,
                                 (UCHAR *)packet_pool_mem,
                                 NX_APP_PACKET_COUNT * NX_APP_PACKET_SIZE);
  if (status != NX_SUCCESS)
  {
    Error_Handler();
  }

  status = nx_ip_create(&nx_app_ip,
                        (CHAR *)"nx_ip",
                        NX_APP_STATIC_IP,
                        NX_APP_STATIC_MASK,
                        &nx_app_packet_pool,
                        nx_stm32_eth_driver,
                        nx_app_ip_thread_stack,
                        sizeof(nx_app_ip_thread_stack),
                        2U);
  if (status != NX_SUCCESS)
  {
    Error_Handler();
  }

  status = nx_arp_enable(&nx_app_ip, nx_app_arp_cache, sizeof(nx_app_arp_cache));
  if (status != NX_SUCCESS)
  {
    Error_Handler();
  }

  status = nx_icmp_enable(&nx_app_ip);
  if (status != NX_SUCCESS)
  {
    Error_Handler();
  }

  status = nx_ip_driver_direct_command(&nx_app_ip, NX_LINK_ENABLE, &actual_status);
  if (status != NX_SUCCESS)
  {
    Error_Handler();
  }

  status = nx_ip_gateway_address_set(&nx_app_ip, NX_APP_STATIC_GW);
  if (status != NX_SUCCESS)
  {
    Error_Handler();
  }
}
