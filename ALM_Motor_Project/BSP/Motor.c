#include "Motor.h"
#include "tim.h"
#include "gpio.h"
#include "adjuster_flash.h"

#define STEP_FREQ_MIN   ADJ_SPEED_MIN_STEP_S
#define STEP_FREQ_MAX   ADJ_SPEED_MAX_STEP_S
#define TIM2_BASE_FREQ  1000000UL   // 1 MHz after PSC=63

/* =========================
 * ȫ�ֱ���
 * ========================= */

/* ��ģ�� NodeID���� UID ������ */
uint32_t FDCAN_NodeID = 0;

/* �������״̬ */
MotorAxis_t g_motor_axis[2] =
{
    {0, 0, 0},   // X
    {0, 0, 0}    // Y
};

bool Motor_IsAxisIdle(uint8_t axis)
{
    if (axis > MOTOR_AXIS_Y)
        return true;

    return (g_motor_axis[axis].steps_remaining == 0u) &&
           (g_motor_axis[axis].running == 0u);
}

void Motor_SetRunCurrent(uint8_t axis)
{
    uint16_t current = (axis == MOTOR_AXIS_X) ?
                       g_adj_params.x_run_current :
                       g_adj_params.y_run_current;

    if (axis == MOTOR_AXIS_X)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, current);
    }
    else if (axis == MOTOR_AXIS_Y)
    {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, current);
    }
}

void Motor_SetHoldCurrent(uint8_t axis)
{
    uint16_t run_current = (axis == MOTOR_AXIS_X) ?
                           g_adj_params.x_run_current :
                           g_adj_params.y_run_current;

    /* Hold current percentage uses x_hold_current_pct for both axes by design */
    uint32_t hold = ((uint32_t)run_current * (uint32_t)g_adj_params.x_hold_current_pct) / 100u;

    if (axis == MOTOR_AXIS_X)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint16_t)hold);
    }
    else if (axis == MOTOR_AXIS_Y)
    {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, (uint16_t)hold);
    }
}

/* Read-only: remaining steps for axis */
uint32_t Motor_GetRemaining(uint8_t axis)
{
    if (axis > MOTOR_AXIS_Y)
        return 0;

    return (g_motor_axis[axis].steps_remaining >> 1);
}

/* =========================
 * GPIO / Driver ����
 * ========================= */

void Motor_Set_X_Direction(GPIO_PinState state)
{
    HAL_GPIO_WritePin(X_Dir_GPIO_Port, X_Dir_Pin, state);
}

void Motor_Set_Y_Direction(GPIO_PinState state)
{
    HAL_GPIO_WritePin(Y_Dir_GPIO_Port, Y_Dir_Pin, state);
}

/* ʹ��Ҫ��EN + nSLEEP ͬʱ����/���� */
void Motor_EnableDrivers(uint8_t enable)
{
    GPIO_PinState s = enable ? GPIO_PIN_SET : GPIO_PIN_RESET;

    /* EN */
    HAL_GPIO_WritePin(EN1_GPIO_Port, EN1_Pin, s);
    HAL_GPIO_WritePin(EN2_GPIO_Port, EN2_Pin, s);

    /* nSLEEP */
    HAL_GPIO_WritePin(NSLEEP1_GPIO_Port, NSLEEP1_Pin, s);
    HAL_GPIO_WritePin(NSLEEP2_GPIO_Port, NSLEEP2_Pin, s);
}

/* =========================
 * ��ʼ��
 * ========================= */

//For Debug only Start
volatile uint32_t T2_arr;
//For Debug only End

void Motor_SetStepFreq(uint16_t step_hz)
{
    uint32_t arr;

    /* ---------- �����޷� ---------- */
    if (step_hz < STEP_FREQ_MIN)
        step_hz = STEP_FREQ_MIN;
    else if (step_hz > STEP_FREQ_MAX)
        step_hz = STEP_FREQ_MAX;

    /* ---------- ���� ARR ---------- */
    //arr = (TIM2_BASE_FREQ / (step_hz * 2UL)) - 1UL;
		arr = (TIM2_BASE_FREQ / step_hz / 2) - 1UL;
		
		T2_arr = arr;

    /* ---------- ��ȫ���� ARR ---------- */
    __HAL_TIM_DISABLE(&htim2);          // ֹͣ����������ë��
    __HAL_TIM_SET_AUTORELOAD(&htim2, arr);
    __HAL_TIM_SET_COUNTER(&htim2, 0);   // ��ѡ����λ����
    __HAL_TIM_ENABLE(&htim2);
}


void Motor_Init(void)
{
		Motor_SetStepFreq(g_adj_params.x_step_freq_hz);
		
    /* STEP ��ʼ�͵�ƽ */
    HAL_GPIO_WritePin(X_Step_GPIO_Port, X_Step_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(Y_Step_GPIO_Port, Y_Step_Pin, GPIO_PIN_RESET);

    /* Ĭ�Ϸ��� */
    Motor_Set_X_Direction(GPIO_PIN_RESET);
    Motor_Set_Y_Direction(GPIO_PIN_RESET);

    /* ===============================
     * �������ƣ�TIM1_CH2 -> VREF��
     * Period = 100
     * 20% ռ�ձ� => Pulse = 20
     * =============================== */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, g_adj_params.x_run_current);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, g_adj_params.y_run_current);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);	

    /* ʹ��������EN + nSLEEP�� */
    Motor_EnableDrivers(1u);

    /* ���״̬ */
    g_motor_axis[MOTOR_AXIS_X].steps_remaining = 0;
    g_motor_axis[MOTOR_AXIS_X].direction       = 0;
    g_motor_axis[MOTOR_AXIS_X].running         = 0;

    g_motor_axis[MOTOR_AXIS_Y].steps_remaining = 0;
    g_motor_axis[MOTOR_AXIS_Y].direction       = 0;
    g_motor_axis[MOTOR_AXIS_Y].running         = 0;
}

/* =========================
 * ֹͣ����
 * ========================= */

void Motor_StopAxis(uint8_t axis)
{
    if (axis > MOTOR_AXIS_Y)
        return;

    g_motor_axis[axis].steps_remaining = 0;
    g_motor_axis[axis].running = 0;

    /* STEP ���ͣ�����ͣ�ڸߵ�ƽ�� */
    if (axis == MOTOR_AXIS_X)
        HAL_GPIO_WritePin(X_Step_GPIO_Port, X_Step_Pin, GPIO_PIN_RESET);
    else
        HAL_GPIO_WritePin(Y_Step_GPIO_Port, Y_Step_Pin, GPIO_PIN_RESET);

    if (!g_motor_axis[MOTOR_AXIS_X].running &&
        !g_motor_axis[MOTOR_AXIS_Y].running)
    {
        HAL_TIM_Base_Stop_IT(&htim2);
    }
}

void Motor_StopAll(void)
{
    Motor_StopAxis(MOTOR_AXIS_X);
    Motor_StopAxis(MOTOR_AXIS_Y);
}

/* =========================
 * �˶�����
 * ========================= */

void Motor_RequestMove(uint8_t axis, int32_t step_count)
{
    if (axis > MOTOR_AXIS_Y)
        return;

    if (step_count == 0)
        return;

    uint8_t  dir   = (step_count < 0) ? 1u : 0u;
    uint32_t steps = (step_count < 0) ?
                     (uint32_t)(-step_count) :
                     (uint32_t)( step_count);

    Motor_SetRunCurrent(axis);

    g_motor_axis[axis].direction       = dir;
    g_motor_axis[axis].steps_remaining = steps;
    g_motor_axis[axis].running         = 1u;

    /* ������� */
    if (axis == MOTOR_AXIS_X)
        Motor_Set_X_Direction(dir ? GPIO_PIN_SET : GPIO_PIN_RESET);
    else
        Motor_Set_Y_Direction(dir ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* ����������ʱ�� */
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    HAL_TIM_Base_Start_IT(&htim2);
}

/* =========================
 * TIM2 IRQ Hook
 * ========================= */

/*
 * ˵����
 * - ÿ�� TIM2 Update �жϣ�
 *   - Toggle STEP
 *   - �ݼ���Ӧ��� steps_remaining
 * - ��Ϊ����ԭ���̱���һ�£����ı��ٶ�����
 */
void TIM2_IRQHandler_For_Motor(void)
{
    if (!__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE))
        return;

    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);

    /* X �� */
    if (g_motor_axis[MOTOR_AXIS_X].running &&
        g_motor_axis[MOTOR_AXIS_X].steps_remaining > 0)
    {
        HAL_GPIO_TogglePin(X_Step_GPIO_Port, X_Step_Pin);
        g_motor_axis[MOTOR_AXIS_X].steps_remaining--;

        if (g_motor_axis[MOTOR_AXIS_X].steps_remaining == 0)
            g_motor_axis[MOTOR_AXIS_X].running = 0;
    }

    /* Y �� */
    if (g_motor_axis[MOTOR_AXIS_Y].running &&
        g_motor_axis[MOTOR_AXIS_Y].steps_remaining > 0)
    {
        HAL_GPIO_TogglePin(Y_Step_GPIO_Port, Y_Step_Pin);
        g_motor_axis[MOTOR_AXIS_Y].steps_remaining--;

        if (g_motor_axis[MOTOR_AXIS_Y].steps_remaining == 0)
            g_motor_axis[MOTOR_AXIS_Y].running = 0;
    }

    /* ���ᶼ������ͣ TIM2������ STEP ���� */
    if (!g_motor_axis[MOTOR_AXIS_X].running &&
        !g_motor_axis[MOTOR_AXIS_Y].running)
    {
        HAL_TIM_Base_Stop_IT(&htim2);
        HAL_GPIO_WritePin(X_Step_GPIO_Port, X_Step_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(Y_Step_GPIO_Port, Y_Step_Pin, GPIO_PIN_RESET);
    }
}
