
#include "adjuster_CAN_Poll.h"

#include <string.h>

#include "CAN_Motor.h"
#include "adjuster_motor_task.h"
#include "Motor.h"
#include "adjuster_can.h"

//uint8_t Adjuster_IsMyNode(uint32_t target_node_id);
//void Motor_StartFromCommand(const CAN_MotionCmd_t *cmd);

volatile uint8_t g_can_rx_has_new_data = 0;

/* Convert FDCAN DLC value to payload length in bytes */
static inline uint8_t FDCAN_DLC_ToBytes(uint32_t dlc)
{
    switch (dlc)
    {
        case FDCAN_DLC_BYTES_0:  return 0;
        case FDCAN_DLC_BYTES_1:  return 1;
        case FDCAN_DLC_BYTES_2:  return 2;
        case FDCAN_DLC_BYTES_3:  return 3;
        case FDCAN_DLC_BYTES_4:  return 4;
        case FDCAN_DLC_BYTES_5:  return 5;
        case FDCAN_DLC_BYTES_6:  return 6;
        case FDCAN_DLC_BYTES_7:  return 7;
        case FDCAN_DLC_BYTES_8:  return 8;
        case FDCAN_DLC_BYTES_12: return 12;
        case FDCAN_DLC_BYTES_16: return 16;
        case FDCAN_DLC_BYTES_20: return 20;
        case FDCAN_DLC_BYTES_24: return 24;
        case FDCAN_DLC_BYTES_32: return 32;
        case FDCAN_DLC_BYTES_48: return 48;
        case FDCAN_DLC_BYTES_64: return 64;
        default:                 return 0;
    }
}

//Just for debug Start
volatile FDCAN_RxHeaderTypeDef rxHeader_Record_temp;
volatile uint8_t rxData_Record_temp[64];
//Just for debug End

void CAN_Poll(void)
{
    FDCAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[64];

    /* Drain RX FIFO0 and hand each message to the adjuster CAN handler */
    while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0)
    {
        if (HAL_FDCAN_GetRxMessage(&hfdcan1,
                                   FDCAN_RX_FIFO0,
                                   &rxHeader,
                                   rxData) != HAL_OK)
        {
            break;
        }

        uint8_t payload_len = FDCAN_DLC_ToBytes(rxHeader.DataLength);

        g_can_rx_has_new_data = 1;

        AdjusterCAN_OnRx(rxHeader.Identifier,
                         rxHeader.IdType,
                         rxData,
                         payload_len);
    }
		
		//Just for debug Start
		rxHeader_Record_temp = rxHeader;
		memcpy((void *)rxData_Record_temp, rxData, 64);
		//Just for debug End
}

