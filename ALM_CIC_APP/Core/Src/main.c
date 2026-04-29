/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dcache.h"
#include "eth.h"
#include "fdcan.h"
#include "icache.h"
#include "octospi.h"
#include "usb.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_lan8742.h"
#include "bsp_w25q16.h"
#include "cpu_id.h"
#include "lwip_app.h"
#include "tcp_server.h"
#include "upgrade_handler.h"
#include "module_upgrade.h"
#include "CAN_comm.h"
#include "systick_task.h"
#include "udp_discovery.h"
#include "device_config.h"
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
__IO uint32_t TSB;

uint32_t now;

/* Debug checkpoint: increment at each init step; read in debugger to find hang point.
   0=start, 1=VTOR, 2=HAL_Init, 3=get_cpu_id, 4=SystemClock, 5=GPIO, 6=DCACHE,
   7=ETH, 8=FDCAN, 9=ICACHE, 10=USB, 11=OCTOSPI, 12=LAN_Init, 13=LAN_Reset,
   14=LAN_Status, 15=W25Q, 16=DeviceCfg, 17=LWIP, 18=FDCAN_start, 19=TCP, 20=UDP,
   21=while(1) */
volatile uint8_t g_dbg_step = 0U;

/* Device SN (g_device_sn) is now defined in BSP/cpu_id.c and computed by
   get_cpu_id() so it matches the value the bootloader derives from the
   same MCU. Anything in this project that reads g_device_sn just needs
   the extern in cpu_id.h. */

uint32_t ETH_Start_IT_Stat;
BSP_LAN8742_HandleTypeDef hphy;
BSP_LAN8742_LinkStateTypeDef phy_link;
BSP_LAN8742_SpeedTypeDef phy_speed;
BSP_LAN8742_DuplexTypeDef phy_duplex;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define LAN8742A_PHY_ADDR 1U

static void MPU_Config(void)
{
  MPU_Attributes_InitTypeDef MPU_Attr = {0};
  MPU_Region_InitTypeDef     MPU_Region = {0};

  HAL_MPU_Disable();

  /* Attribute 0: Normal memory, non-cacheable (inner + outer) */
  MPU_Attr.Number     = MPU_ATTRIBUTES_NUMBER0;
  MPU_Attr.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);
  HAL_MPU_ConfigMemoryAttributes(&MPU_Attr);

  /* Region 0: SRAM3 0x20050000 – 0x2005FFFF (64KB) for ETH DMA,
     uses Attribute 0 (non-cacheable) */
  MPU_Region.Enable           = MPU_REGION_ENABLE;
  MPU_Region.Number           = MPU_REGION_NUMBER0;
  MPU_Region.BaseAddress      = 0x20050000;
  MPU_Region.LimitAddress     = 0x2005FFFF;
  MPU_Region.AttributesIndex  = MPU_ATTRIBUTES_NUMBER0;
  MPU_Region.AccessPermission = MPU_REGION_ALL_RW;
  MPU_Region.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_Region.IsShareable      = MPU_ACCESS_INNER_SHAREABLE;
  HAL_MPU_ConfigRegion(&MPU_Region);

  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void check_data(uint32_t now)
{
  static uint32_t last_tick = 0U;

  if ((now - last_tick) < 500U)
  {
    return;
  }

  last_tick = now;

  (void)BSP_LAN8742_GetBasicStatus(&hphy, &phy_link, &phy_speed, &phy_duplex);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* Confirm vector table is at app base (already set by SystemInit, but be explicit). */
  SCB->VTOR = 0x08008000UL;
  __DSB();
  __ISB();
  g_dbg_step = 1U;   /* VTOR set */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  g_dbg_step = 2U;   /* HAL_Init done */

  /* USER CODE BEGIN Init */
  get_cpu_id();
  MPU_Config();
  g_dbg_step = 3U;   /* get_cpu_id + MPU done */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
  g_dbg_step = 4U;   /* SystemClock done */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  g_dbg_step = 5U;   /* GPIO done */
  MX_DCACHE1_Init();
  g_dbg_step = 6U;   /* DCACHE done */
  MX_ETH_Init();
  g_dbg_step = 7U;   /* ETH done */
  MX_FDCAN1_Init();
  g_dbg_step = 8U;   /* FDCAN done */
  MX_ICACHE_Init();
  g_dbg_step = 9U;   /* ICACHE done */
  MX_USB_PCD_Init();
  g_dbg_step = 10U;  /* USB done */
  MX_OCTOSPI1_Init();
  g_dbg_step = 11U;  /* OCTOSPI done */
  /* USER CODE BEGIN 2 */

  if (BSP_LAN8742_InitOrDetect(&hphy, &heth, LAN8742A_PHY_ADDR) != HAL_OK)
  {
    Error_Handler();
  }
  g_dbg_step = 12U;  /* LAN8742 InitOrDetect done */

  if (BSP_LAN8742_Reset(&hphy) != HAL_OK)
  {
    Error_Handler();
  }
  g_dbg_step = 13U;  /* LAN8742 Reset done */

  if (BSP_LAN8742_GetBasicStatus(&hphy, &phy_link, &phy_speed, &phy_duplex) != HAL_OK)
  {
    Error_Handler();
  }
  g_dbg_step = 14U;  /* LAN8742 GetBasicStatus done */

  (void)BSP_W25Q_Init();
  g_dbg_step = 15U;  /* W25Q init done */

  DeviceConfig_Init();
  g_dbg_step = 16U;  /* DeviceConfig done */

  LWIP_APP_Init();
  g_dbg_step = 17U;  /* LwIP init done */

  {
    FDCAN_FilterTypeDef filter = {0};
    filter.IdType       = FDCAN_STANDARD_ID;
    filter.FilterIndex  = 0;
    filter.FilterType   = FDCAN_FILTER_RANGE;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1    = 0x000;
    filter.FilterID2    = 0x7FF;
    HAL_FDCAN_ConfigFilter(&hfdcan1, &filter);
    HAL_FDCAN_Start(&hfdcan1);
    HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
  }
  g_dbg_step = 18U;  /* FDCAN start done */

  TCP_Server_Init();
  g_dbg_step = 19U;  /* TCP server done */

  UDP_Discovery_Init();
  g_dbg_step = 20U;  /* UDP discovery done */

  MUR_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  g_dbg_step = 21U;  /* entering while(1) */
  while (1)
  {
		now = HAL_GetTick();

		check_data(now);

		/* CAN_LED (PE3) ACT-style flicker — driven by every TX or RX
		   event flagged inside CAN_comm.c. */
		CAN_ActLed_Tick(now);

		/* LwIP: process received frames + timers (DHCP etc.) */
		LWIP_APP_Poll();

		/* Deferred reboot after OTA upgrade END — fires after LWIP flush */
		UPG_PollReboot();

		/* Drive module-upgrade CAN-FD relay after a Motor .mot is staged. */
		MUR_PollRelay();

		/* UDP discovery: broadcast announcement every 3s */
		UDP_Discovery_Poll();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_CSI;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.CSIState = RCC_CSI_ON;
  RCC_OscInitStruct.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_CSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 125;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
