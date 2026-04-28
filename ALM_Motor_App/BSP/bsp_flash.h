#pragma once

#include "main.h"
#include <stdint.h>

/* Flash ²Ù×÷ */
void BSP_Flash_Erase(uint32_t addr);
void BSP_Flash_Program(uint32_t addr, const uint8_t *buf, uint32_t len);

/* CRC32 ¼ÆËã */
uint32_t BSP_CRC32_Calc(const uint8_t *buf, uint32_t len);
