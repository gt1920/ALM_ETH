
#ifndef __Usart_Func__H
#define __Usart_Func__H
			   
#include "main.h"


#define UART1_RX_LEN     1024+50
#define UART1_TX_LEN     256

extern uint8_t Uart1_Rx[UART1_RX_LEN];
extern uint8_t Uart1_Tx[UART1_TX_LEN];

extern uint16_t Uart1_Rx_length;
extern uint16_t Uart1_Tx_length;

void Uart1_Start_DMA_Tx(uint16_t size);

void Check_Usart_Data(void);

#endif


