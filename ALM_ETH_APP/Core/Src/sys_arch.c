/**
  * sys_arch.c - Minimal sys_arch for LwIP NO_SYS=1 (bare-metal)
  *
  * Only sys_now() is needed. It returns the current time in milliseconds
  * using the HAL SysTick.
  */
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "stm32h5xx_hal.h"

u32_t sys_now(void)
{
  return HAL_GetTick();
}
