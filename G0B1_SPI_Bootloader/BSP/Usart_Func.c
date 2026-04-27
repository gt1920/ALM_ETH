
#include "Usart_Func.h"
#include "iap.h"

uint8_t Uart1_Rx[UART1_RX_LEN]={0};
uint8_t Uart1_Tx[UART1_TX_LEN]={0};

uint16_t Uart1_Rx_length;
uint16_t Uart1_Tx_length;

void Uart1_Start_DMA_Tx(uint16_t size)
{
	HAL_UART_Transmit_DMA(&huart1, IGK_IAP.TxBuff, size);	
}

void Check_Usart_Data(void) //串口1中断服务程序
{
    
	if ((USART1->ISR & USART_ISR_FE) != 0)
	{
		USART1->RDR;
		USART1->ICR|=1<<1;
	}
	
	if ((USART1->ISR & USART_ISR_IDLE) != 0)
	{
		HAL_UART_DMAStop(&huart1);
		
		USART1->ICR|= (1<<4);  //Clear Flag For IDLE
		
		USART1->RDR;
		
		Uart1_Rx_length = UART1_RX_LEN - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_1);
		
		if(Uart1_Rx_length<UART1_RX_LEN)
		{
				HAL_GPIO_TogglePin(StateLED_GPIO_Port, StateLED_Pin);
			
				IAP_BootLoad_UpData();
											
		}               
	
		HAL_UART_Receive_DMA(&huart1, Uart1_Rx, UART1_RX_LEN);	
		
	}
		
}
