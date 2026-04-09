
#include "Systick_Task.h"

#include "USB_Send_Queue.h"

#include "CAN_comm.h"

/* 1ms tick */
static uint8_t usb_tx_tick = 0;
static uint16_t module_timeout_tick = 0;

/*
 * @brief  1ms 周期任务：驱动 USB 发送队列（10ms）并触发模块心跳超时检查（1s）。
 */
void SysTick_Task(void)
{
    usb_tx_tick++;
    if (usb_tx_tick >= 10)
    {
        usb_tx_tick = 0;
        USB_Tx_Task_10ms();
    }

    module_timeout_tick++;
    if (module_timeout_tick >= 1000)
    {
        module_timeout_tick = 0;
        Module_Heartbeat_Timeout_Check();
    }
}
