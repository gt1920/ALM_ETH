
#pragma once
#include "main.h"
#include <stdint.h>

extern volatile uint8_t g_can_rx_has_new_data;

void CAN_Poll(void);
//void AdjusterCAN_OnRx(uint32_t id, uint32_t idType, const uint8_t *data, uint8_t len);  // CAN �� Motor ��Ψһ���
