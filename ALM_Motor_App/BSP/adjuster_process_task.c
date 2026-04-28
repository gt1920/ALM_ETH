
#include "adjuster_process_task.h"
#include "LED.h"
#include "adjuster_CAN_Poll.h"
#include "adjuster_update_task.h"

#define Signal_LED_Hold	200

__IO uint32_t signal_led_last_tick;

void process(uint32_t now)
{
		if(singal_led_on)
		{
			LED_Ctrl(1);
			
			signal_led_last_tick = now;
			
			singal_led_on = 0;
		}
		
		signal_led_off(now);
}

// Turn OFF singal LED
void signal_led_off(uint32_t now)
{
    //static uint32_t last_tick = 0;

    if (now - signal_led_last_tick < Signal_LED_Hold)
        return;

    signal_led_last_tick = now;
    
		LED_Ctrl(0);  // Turn OFF singal LED	
}

