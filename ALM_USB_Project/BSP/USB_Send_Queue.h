
#pragma once

#include "main.h"

extern uint16_t usb_drop_cnt;

bool USB_Send_Queue(const uint8_t *data, uint8_t len);
void USB_Tx_Task_10ms(void);
