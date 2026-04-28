
#include "adjuster_update_task.h"
#include "adjuster_CAN_Poll.h"

__IO uint8_t singal_led_on = 0;

void update_data(uint32_t now)
{
	if(g_can_rx_has_new_data)
	{
		singal_led_on = g_can_rx_has_new_data;  // if CAN got new data, should turn on led.			
		
		g_can_rx_has_new_data = 0;
	}
}
