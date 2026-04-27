#include "systick_task.h"
#include "eth_send_queue.h"
#include "CAN_comm.h"

static volatile uint32_t tick_1ms = 0;

void SysTick_Task(void)
{
    tick_1ms++;

    /* Every 10ms: drain one TX queue frame */
    if ((tick_1ms % 10) == 0)
    {
        ETH_Tx_Task_Poll();
    }

    /* Every 1000ms: remove stale modules */
    if ((tick_1ms % 1000) == 0)
    {
        Module_Heartbeat_Timeout_Check();
    }
}
