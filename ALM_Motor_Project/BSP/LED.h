#ifndef __LED__H
#define __LED__H

#include "main.h"

#define LED_ON		HAL_GPIO_WritePin(Stat_LED_GPIO_Port, Stat_LED_Pin, GPIO_PIN_SET)
#define LED_OFF		HAL_GPIO_WritePin(Stat_LED_GPIO_Port, Stat_LED_Pin, GPIO_PIN_RESET);
#define LED_Inv		Stat_LED_GPIO_Port->ODR = Stat_LED_Pin
			
void LED_Ctrl(uint8_t Comm);

#endif // __CAN_COMM_H
