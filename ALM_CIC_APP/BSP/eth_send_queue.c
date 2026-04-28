#include "eth_send_queue.h"
#include "tcp_server.h"
#include <string.h>

#define ETH_TX_QUEUE_DEPTH   8
#define ETH_FRAME_LEN        64

uint16_t eth_drop_cnt = 0;

typedef struct
{
    uint8_t buf[ETH_FRAME_LEN];
    uint8_t len;
} ETH_TxFrame_t;

static ETH_TxFrame_t eth_tx_queue[ETH_TX_QUEUE_DEPTH];

static volatile uint8_t eth_tx_head  = 0;
static volatile uint8_t eth_tx_tail  = 0;
static volatile uint8_t eth_tx_count = 0;

/* Enqueue (called from ISR context) */
bool ETH_Send_Queue(const uint8_t *data, uint8_t len)
{
    if (len > ETH_FRAME_LEN)
        return false;

    __disable_irq();

    if (eth_tx_count >= ETH_TX_QUEUE_DEPTH)
    {
        __enable_irq();
        eth_drop_cnt++;
        return false;
    }

    memcpy(eth_tx_queue[eth_tx_head].buf, data, len);
    eth_tx_queue[eth_tx_head].len = len;

    eth_tx_head = (eth_tx_head + 1) % ETH_TX_QUEUE_DEPTH;
    eth_tx_count++;

    __enable_irq();

    return true;
}

/* Drain all queued frames per call (called from SysTick every 10ms) */
void ETH_Tx_Task_Poll(void)
{
    if (!TCP_Server_IsConnected())
        return;

    while (eth_tx_count > 0)
    {
        uint8_t tail = eth_tx_tail;

        if (!TCP_Server_Send(eth_tx_queue[tail].buf, eth_tx_queue[tail].len))
            break;   /* TCP send buffer full, retry next cycle */

        __disable_irq();

        eth_tx_tail = (eth_tx_tail + 1) % ETH_TX_QUEUE_DEPTH;
        eth_tx_count--;

        __enable_irq();
    }
}
