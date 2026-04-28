#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h"
#include <stdbool.h>

#include "CAN_Comm.h"

/*
 * XY Motor driver layer
 * - ����ά�� X/Y ��������״̬
 * - �ṩ CAN ���ֱ�ӵ��õ��˶�����/ֹͣ�ӿ�
 * - TIM2_IRQHandler_For_Motor() �� TIM2 IRQ �ڱ����ã����ڲ��� STEP ���岢�ݼ�����
 *
 * Լ����
 * - �� .h ������ stm32g0xx_hal.h�������� main.h��
 * - ʹ�ܵ������ʱ��EN ���� + nSLEEP ���ߣ�ʹ�ܶ������޸� DIR
 */


#define MOTOR_AXIS_X     (0u)
#define MOTOR_AXIS_Y     (1u)

#define AXIS_MASK_X      (1u << 0)
#define AXIS_MASK_Y      (1u << 1)

typedef struct
{
    __IO uint32_t steps_remaining;  // ʣ�ಽ��
    __IO uint8_t  direction;        // 0=����, 1=����
    __IO uint8_t  running;          // 0=idle, 1=running
} MotorAxis_t;

extern MotorAxis_t g_motor_axis[2];

/* Motor runtime state */
uint8_t Motor_GetCurrentPct_X(void);
uint8_t Motor_GetCurrentPct_Y(void);

/* Motor state */
bool Motor_IsRunning_X(void);
bool Motor_IsRunning_Y(void);
bool Motor_IsEnabled(void);

bool Motor_IsAxisIdle(uint8_t axis);

void Motor_SetRunCurrent(uint8_t axis);
void Motor_SetHoldCurrent(uint8_t axis);

/* �� main.c ʹ�ã������� CAN ֡ node_id �Ƚ� */
extern uint32_t FDCAN_NodeID;

void Motor_Init(void);

/* EN + nSLEEP ͳһʹ��/ʧ�� */
void Motor_EnableDrivers(uint8_t enable);

/* �˶�����step_count >0 ����, <0 ����, =0 ���� */
void Motor_RequestMove(uint8_t axis, int32_t step_count);

/* ֹͣ */
void Motor_StopAxis(uint8_t axis);
void Motor_StopAll(void);

/* Read-only: remaining steps for axis */
uint32_t Motor_GetRemaining(uint8_t axis);

/* DIR GPIO */
void Motor_Set_X_Direction(GPIO_PinState state);
void Motor_Set_Y_Direction(GPIO_PinState state);

/* TIM2 �жϹ��ӣ��� TIM2_IRQHandler() USER CODE �е��� */
void TIM2_IRQHandler_For_Motor(void);

#endif /* __MOTOR_H */
