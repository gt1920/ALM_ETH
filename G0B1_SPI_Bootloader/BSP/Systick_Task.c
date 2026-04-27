
#include "Systick_Task.h"

uint8_t Step;
uint16_t State_LED_Count;

void Systick_Task(void)
{

	if(State_LED_Count<0xFFFF)
	{
		State_LED_Count++;
	}	
		
	if(Step<6)
	{
		if(State_LED_Count>200)
		{
			StateLED_GPIO_Port->ODR ^= StateLED_Pin;
			State_LED_Count = 0;
			Step++;
		}
	}
	else
	{
		if(State_LED_Count>1000)
		{
			StateLED_GPIO_Port->BSRR= StateLED_Pin;
			State_LED_Count = 0;
			Step=0;
		}
	}

}

