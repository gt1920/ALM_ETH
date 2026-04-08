/**
  * ethernetif.c - LwIP network interface driver for STM32H5 ETH HAL (bare-metal)
  *
  * Bridges the STM32H5xx HAL Ethernet peripheral to LwIP.
  * DMA descriptors and RX buffers are placed in a non-cacheable RAM section
  * (.EthSection) via the scatter file so that DCache coherency is handled
  * automatically.
  */
#include "ethernetif.h"
#include "eth.h"
#include "main.h"
#include "string.h"

#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"
#include "lwip/ethip6.h"

/* -------- configuration -------- */
#define IFNAME0                 's'
#define IFNAME1                 't'
#define ETH_RX_BUFFER_SIZE      1536U
#define ETH_RX_BUFFER_CNT       ETH_RX_DESC_CNT

/* -------- DMA descriptors & RX buffers in non-cacheable RAM -------- */
#if defined(__ARMCC_VERSION)
  /* Keil MDK / ARM Compiler 6 */
  __attribute__((section(".EthSection")))
  static ETH_DMADescTypeDef DMARxDscrTab_lwip[ETH_RX_DESC_CNT];

  __attribute__((section(".EthSection")))
  static ETH_DMADescTypeDef DMATxDscrTab_lwip[ETH_TX_DESC_CNT];

  __attribute__((section(".EthSection"), aligned(32)))
  static uint8_t Rx_Buff[ETH_RX_BUFFER_CNT][ETH_RX_BUFFER_SIZE];
#elif defined(__GNUC__)
  static ETH_DMADescTypeDef DMARxDscrTab_lwip[ETH_RX_DESC_CNT]
    __attribute__((section(".EthSection")));
  static ETH_DMADescTypeDef DMATxDscrTab_lwip[ETH_TX_DESC_CNT]
    __attribute__((section(".EthSection")));
  static uint8_t Rx_Buff[ETH_RX_BUFFER_CNT][ETH_RX_BUFFER_SIZE]
    __attribute__((section(".EthSection"), aligned(32)));
#endif

/* HAL helpers */
extern ETH_HandleTypeDef heth;
extern ETH_TxPacketConfigTypeDef TxConfig;

/* -------------------------------------------------------------------------- */
/*  HAL ETH Rx Alloc callback                                                 */
/* -------------------------------------------------------------------------- */
void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
  /* We use static buffers; just cycle through them.
     The HAL stores the index in the descriptor internally. */
  static uint32_t rx_idx = 0;
  *buff = Rx_Buff[rx_idx];
  rx_idx = (rx_idx + 1U) % ETH_RX_BUFFER_CNT;
}

/* -------------------------------------------------------------------------- */
/*  HAL ETH Rx Link callback – called once per received frame                 */
/* -------------------------------------------------------------------------- */
void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd,
                            uint8_t *buff, uint16_t Length)
{
  struct pbuf **ppStart = (struct pbuf **)pStart;
  struct pbuf **ppEnd   = (struct pbuf **)pEnd;
  struct pbuf *p;

  /* Allocate a PBUF_RAM and copy the data from the DMA buffer.
     This frees the static DMA RX buffer immediately for reuse. */
  p = pbuf_alloc(PBUF_RAW, Length, PBUF_POOL);
  if (p == NULL)
  {
    return;
  }
  memcpy(p->payload, buff, Length);

  if (*ppStart == NULL)
  {
    *ppStart = p;
  }
  else
  {
    pbuf_cat(*ppEnd, p);
  }
  *ppEnd = p;
}

/* -------------------------------------------------------------------------- */
/*  low_level_init – called from ethernetif_init                              */
/* -------------------------------------------------------------------------- */
static void low_level_init(struct netif *netif)
{

  /* set MAC hardware address length */
  netif->hwaddr_len = ETH_HWADDR_LEN;

  /* copy MAC from HAL handle */
  netif->hwaddr[0] = heth.Init.MACAddr[0];
  netif->hwaddr[1] = heth.Init.MACAddr[1];
  netif->hwaddr[2] = heth.Init.MACAddr[2];
  netif->hwaddr[3] = heth.Init.MACAddr[3];
  netif->hwaddr[4] = heth.Init.MACAddr[4];
  netif->hwaddr[5] = heth.Init.MACAddr[5];

  /* maximum transfer unit */
  netif->mtu = 1500;

  /* device capabilities */
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

  /* Override HAL descriptors with our non-cacheable ones */
  heth.Init.TxDesc = DMATxDscrTab_lwip;
  heth.Init.RxDesc = DMARxDscrTab_lwip;
  heth.Init.RxBuffLen = ETH_RX_BUFFER_SIZE;

  /* Re-init ETH with our descriptors */
  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    Error_Handler();
  }

  /* TxConfig kept from CubeMX-generated eth.c */

  /* Start ETH with interrupts */
  if (HAL_ETH_Start_IT(&heth) != HAL_OK)
  {
    Error_Handler();
  }
}

/* -------------------------------------------------------------------------- */
/*  low_level_output – called by LwIP when it wants to send a frame           */
/* -------------------------------------------------------------------------- */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  uint32_t framelen = 0;
  ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT];
  struct pbuf *q;
  uint32_t i = 0;

  (void)netif;

  memset(Txbuffer, 0, sizeof(Txbuffer));

  for (q = p; q != NULL; q = q->next)
  {
    if (i >= ETH_TX_DESC_CNT)
    {
      return ERR_IF;
    }

    Txbuffer[i].buffer = q->payload;
    Txbuffer[i].len    = q->len;
    framelen          += q->len;

    if (q->next != NULL)
    {
      Txbuffer[i].next = &Txbuffer[i + 1U];
    }
    else
    {
      Txbuffer[i].next = NULL;
    }
    i++;
  }

  TxConfig.Length   = framelen;
  TxConfig.TxBuffer = Txbuffer;
  TxConfig.pData    = p;

  /* DCache clean for TX payload is handled by HAL if needed */
  HAL_ETH_Transmit_IT(&heth, &TxConfig);

  return ERR_OK;
}

/* -------------------------------------------------------------------------- */
/*  ethernetif_input – polls one frame from ETH DMA and feeds it to LwIP      */
/* -------------------------------------------------------------------------- */
void ethernetif_input(struct netif *netif)
{
  struct pbuf *p = NULL;

  /* Drain all pending frames from the DMA ring */
  while (HAL_ETH_ReadData(&heth, (void **)&p) == HAL_OK)
  {
    if (p != NULL)
    {
      if (netif->input(p, netif) != ERR_OK)
      {
        pbuf_free(p);
      }
      p = NULL;
    }
  }
}

/* -------------------------------------------------------------------------- */
/*  ethernetif_init – LwIP netif init callback                                */
/* -------------------------------------------------------------------------- */
err_t ethernetif_init(struct netif *netif)
{
#if LWIP_NETIF_HOSTNAME
  netif->hostname = "STM32H563";
#endif

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;

  netif->output     = etharp_output;
  netif->linkoutput = low_level_output;

  low_level_init(netif);

  return ERR_OK;
}

/* -------------------------------------------------------------------------- */
/*  HAL ETH callbacks                                                          */
/* -------------------------------------------------------------------------- */
void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth_handle)
{
  (void)heth_handle;
}

void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth_handle)
{
  (void)heth_handle;
  /* Input will be polled from main loop via ethernetif_input() */
}

void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth_handle)
{
  /* On DMA error, restart ETH */
  if ((HAL_ETH_GetDMAError(heth_handle) & ETH_DMA_FATAL_BUS_ERROR_FLAG) != 0U)
  {
    HAL_ETH_Stop_IT(heth_handle);
    HAL_ETH_Start_IT(heth_handle);
  }
}
