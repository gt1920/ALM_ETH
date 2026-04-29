
#include "LED.h"

/* ACT-style indicator: caller controls ON/OFF.
   Comm != 0 → LED ON; Comm == 0 → LED OFF. */
void LED_Ctrl(uint8_t Comm)
{
	if (Comm)
		LED_ON;
	else
		LED_OFF;
}

