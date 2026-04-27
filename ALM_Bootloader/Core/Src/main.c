/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : ALM_Bootloader main entry
  *
  * Minimal bootloader skeleton: clocks + GPIO + OCTOSPI1 + W25Q16 driver.
  * After init, exercises the SPI flash with a 256 B write/read round-trip
  * into Write_Buf / Read_Buf, which can be inspected in the Keil Watch
  * window. Then idles in HAL_Delay so the debugger can attach freely.
  ******************************************************************************
  */
#include "main.h"
#include "gpio.h"
#include "icache.h"
#include "octospi.h"
#include "bsp_w25q16.h"

/* ---------- debug buffers (watch in Keil) ---------- */
uint8_t Write_Buf[W25Q_PAGE_SIZE];
uint8_t Read_Buf [W25Q_PAGE_SIZE];

/* Flash test address (first sector) */
#define BL_TEST_ADDR     0x00000000UL

/* ---------- function prototypes ---------- */
void SystemClock_Config(void);

int main(void)
{
  uint32_t i;

  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_ICACHE_Init();
  MX_OCTOSPI1_Init();

  /* Verify W25Q16 is present and enable Quad mode */
  (void)BSP_W25Q_Init();

  /* Fill Write_Buf with a deterministic pattern */
  for (i = 0U; i < W25Q_PAGE_SIZE; i++)
  {
    Write_Buf[i] = (uint8_t)(i ^ 0xA5U);
  }

  /* Erase the test sector (4 KB), program 256 B, then read back */
  (void)BSP_W25Q_EraseSector4K(BL_TEST_ADDR);
  (void)BSP_W25Q_PageProgramQuad(BL_TEST_ADDR, Write_Buf, W25Q_PAGE_SIZE);
  (void)BSP_W25Q_Read(BL_TEST_ADDR, Read_Buf, W25Q_PAGE_SIZE);

  /* Idle so the debugger can inspect Write_Buf / Read_Buf */
  while (1)
  {
    HAL_Delay(500U);
  }
}

/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }

  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { (void)file; (void)line; }
#endif
