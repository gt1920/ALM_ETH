/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>   // offsetof

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

extern uint32_t TSB;
	
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern FDCAN_HandleTypeDef hfdcan1;

void Check_TxFIFO_LED(void);

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define X_Step_Pin GPIO_PIN_0
#define X_Step_GPIO_Port GPIOA
#define X_Dir_Pin GPIO_PIN_1
#define X_Dir_GPIO_Port GPIOA
#define EN1_Pin GPIO_PIN_2
#define EN1_GPIO_Port GPIOA
#define NSLEEP1_Pin GPIO_PIN_3
#define NSLEEP1_GPIO_Port GPIOA
#define Y_Step_Pin GPIO_PIN_6
#define Y_Step_GPIO_Port GPIOA
#define Y_Dir_Pin GPIO_PIN_7
#define Y_Dir_GPIO_Port GPIOA
#define EN2_Pin GPIO_PIN_0
#define EN2_GPIO_Port GPIOB
#define NSLEEP2_Pin GPIO_PIN_1
#define NSLEEP2_GPIO_Port GPIOB
#define SPI_CS1_Pin GPIO_PIN_6
#define SPI_CS1_GPIO_Port GPIOB
#define SPI_CS2_Pin GPIO_PIN_7
#define SPI_CS2_GPIO_Port GPIOB
#define Stat_LED_Pin GPIO_PIN_8
#define Stat_LED_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
