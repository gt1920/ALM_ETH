
#include "LED.h"

void LED_Ctrl(uint8_t Comm)
{
	if(Comm)
	{
		LED_ON;
	}
	else
	{
		LED_OFF;
	}
}

