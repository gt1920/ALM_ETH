#ifndef __ETH_SEND_QUEUE_H__
#define __ETH_SEND_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

extern uint16_t eth_drop_cnt;

bool ETH_Send_Queue(const uint8_t *data, uint8_t len);
void ETH_Tx_Task_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* __ETH_SEND_QUEUE_H__ */
