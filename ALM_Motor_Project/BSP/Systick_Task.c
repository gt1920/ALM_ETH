
#include "Systick_Task.h"

#include "CAN_Comm.h"
#include "adjuster_flash.h"

__IO uint16_t Heartbeat_Count, LED_OFF_Count;

volatile uint8_t motor_status_tick_1ms = 0;

CAN_Message_t heartbeat_msg;

/* Request flag: set in SysTick, consumed in main loop */
static volatile uint8_t s_param_flash_write_req = 0u;

void Adjuster_FlashDeferred_Tick1ms(void);

void Systick_Task(void)
{
	/* 不要在 SysTick 中写 Flash，只做计时与置位 */
	Adjuster_FlashDeferred_Tick1ms();
		
	if(Heartbeat_Count<UINT16_MAX)
	{
		Heartbeat_Count++;
	}	

	if(LED_OFF_Count<UINT16_MAX)
	{
		LED_OFF_Count++;
	}

    /* ����λ�������ػ� */
    motor_status_tick_1ms = 1;	
	
}

void Adjuster_FlashDeferred_Tick1ms(void)
{
    if (!g_param_dirty)
        return;

    /* 累计时间（1ms tick） */
    if (g_param_dirty_tick < 0xFFFFFFFFu)
    {
        g_param_dirty_tick++;
    }

    /* 达到延时，置位请求，交给主循环处理 */
    if (g_param_dirty_tick >= PARAM_FLASH_DELAY_MS)
    {
        s_param_flash_write_req = 1u;
    }
}

void Adjuster_FlashDeferred_Task(void)
{
    /* 在主循环中调用（线程上下文） */
    if (!s_param_flash_write_req)
        return;

    /* 达到时间，执行写 Flash */
    if (Adjuster_Flash_Write(&g_adj_params))
    {
        g_param_dirty = 0u;
        g_param_dirty_tick = 0u;
        s_param_flash_write_req = 0u;
    }
    else
    {
        /* дʧ�ܲ��ԣ�
         * - ά�� dirty = 1
         * - tick ������
         * - ��һ�μ�������
         */
    }
}

