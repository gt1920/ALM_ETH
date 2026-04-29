
#include "LED.h"

/* Diagnostic mode: LED is ON whenever the App is running.
   The argument is ignored so existing call sites continue to compile. */
void LED_Ctrl(uint8_t Comm)
{
	(void)Comm;
	LED_ON;
}

