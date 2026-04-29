
#include "Systick_Task.h"
#include "CAN_Comm.h"
#include "Motor.h"
#include "main.h"
#include "stm32g0xx_hal_fdcan.h"
#include "sha256.h"  // ����SHA256ͷ�ļ�
#include "LED.h"

// ======= ������ȫ�ֱ����������Watch���ڹ۲죩 =======
FDCAN_RxHeaderTypeDef RxHeader_Debug;
uint8_t RxData_Debug[8];

// ����SHA-256���뻺������UID�������֣�
uint8_t sha_input[12];  // 3��32λ��ÿ��4�ֽڣ��ܹ�12�ֽ�

/* ���ͱ�׼ CAN��8 �ֽڣ�
uint8_t buf[8] = {1,2,3,4,5,6,7,8};
CAN_Send_Std(0x123, buf, 8);

���� FD��64 �ֽڣ�
uint8_t bigbuf[64];
for(int i=0;i<64;i++) bigbuf[i] = i;

CAN_Send_FD(0x321, bigbuf, 64);

*/




uint8_t CAN_Receive_Frame(uint16_t *rx_id, uint8_t *rx_buf, uint8_t *rx_len)
{
    FDCAN_RxHeaderTypeDef rxHeader;

    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rxHeader, rx_buf) != HAL_OK)
        return 0;

    *rx_id = rxHeader.Identifier;

    // ��ȡ DLC
    uint32_t dlc = rxHeader.DataLength >> 16;

    // --- Classic CAN��DLC 0~8 ---
    if (dlc <= 8)
    {
        *rx_len = dlc;
        return 1;
    }

    // --- CAN FD��ת�� DLC �� ʵ���ֽ��� ---
    switch (dlc)
    {
        case FDCAN_DLC_BYTES_12: *rx_len = 12; break;
        case FDCAN_DLC_BYTES_16: *rx_len = 16; break;
        case FDCAN_DLC_BYTES_20: *rx_len = 20; break;
        case FDCAN_DLC_BYTES_24: *rx_len = 24; break;
        case FDCAN_DLC_BYTES_32: *rx_len = 32; break;
        case FDCAN_DLC_BYTES_48: *rx_len = 48; break;
        case FDCAN_DLC_BYTES_64: *rx_len = 64; break;
        default: *rx_len = 0;     break;
    }

    return 1;
}


uint8_t CANFD_Send(uint16_t std_id, uint8_t *data, uint8_t len)
{
    FDCAN_TxHeaderTypeDef txHeader;

    // Identifier
    txHeader.Identifier           = std_id;
    txHeader.IdType               = FDCAN_STANDARD_ID;

    // FD frame + Bit Rate Switching
    txHeader.TxFrameType          = FDCAN_DATA_FRAME;
    txHeader.FDFormat             = FDCAN_FD_CAN;
    txHeader.BitRateSwitch        = FDCAN_BRS_ON;

    // Error state
    txHeader.ErrorStateIndicator  = FDCAN_ESI_ACTIVE;

    // DLC -- HAL macros are raw 4-bit values; HAL_FDCAN_AddMessageToTxFifoQ shifts
    // DataLength <<16 internally. (len << 16) double-shifts and zeroes the DLC field.
    if (len <= 8)      txHeader.DataLength = len;
    else if (len <= 12) txHeader.DataLength = FDCAN_DLC_BYTES_12;
    else if (len <= 16) txHeader.DataLength = FDCAN_DLC_BYTES_16;
    else if (len <= 20) txHeader.DataLength = FDCAN_DLC_BYTES_20;
    else if (len <= 24) txHeader.DataLength = FDCAN_DLC_BYTES_24;
    else if (len <= 32) txHeader.DataLength = FDCAN_DLC_BYTES_32;
    else if (len <= 48) txHeader.DataLength = FDCAN_DLC_BYTES_48;
    else                txHeader.DataLength = FDCAN_DLC_BYTES_64;

    // ���ȼ�
    txHeader.TxEventFifoControl   = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker        = 0;

    // д�� FIFO1
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, data) == HAL_OK)
        return 1;

    return 0;
}


/**
 * @brief  �������������ٲ�֡��Classic CAN��
 * @param  data 8�ֽ���������
 * @retval HAL_OK / HAL_ERROR
 */
HAL_StatusTypeDef CAN_Send_Heartbeat(uint8_t CAN_ID, uint8_t *data)
{
    FDCAN_TxHeaderTypeDef txHeader;

    txHeader.Identifier          = CAN_ID;                 // ����ID
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = FDCAN_DLC_BYTES_8;
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_OFF;         // �ٲ�֡������FD����
    txHeader.FDFormat            = FDCAN_CLASSIC_CAN;     // ����CAN
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;

    /* �ȴ� TX FIFO �� */
    if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) == 0)
        return HAL_BUSY;

    /* ���뷢��FIFO */
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, data) != HAL_OK)
        return HAL_ERROR;

    return HAL_OK;
}


uint8_t CAN_Send_Std(uint16_t std_id, uint8_t *data, uint8_t len)
{
    FDCAN_TxHeaderTypeDef TxHeader;
    HAL_StatusTypeDef st;

    TxHeader.Identifier          = std_id;
    TxHeader.IdType              = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType         = FDCAN_DATA_FRAME;
    TxHeader.DataLength          = FDCAN_DLC_BYTES_8;  // �� �ȹ̶� 8 �ֽ�
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch       = FDCAN_BRS_OFF;
    TxHeader.FDFormat            = FDCAN_CLASSIC_CAN;
    TxHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker       = 0;

    st = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, data);
	
		Check_TxFIFO_LED();

    return (st == HAL_OK) ? 0 : 1;
}


uint8_t CAN_Send_FD(uint16_t std_id, uint8_t *data, uint8_t len)
{
    FDCAN_TxHeaderTypeDef txHeader = {0};

    txHeader.Identifier          = std_id;   // 0x123
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = FDCAN_DLC_BYTES_12;
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_ON;
    txHeader.FDFormat            = FDCAN_FD_CAN;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, data) != HAL_OK)
        return 1;   // Err

    return 0;       // OK
}






// CAN ���ջص�
void CAN_Receive_Callback(void)
{
    // ֱ���� Debug �����洢���������� Watch ���ڲ鿴
    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &RxHeader_Debug, RxData_Debug) == HAL_OK)
    {
        CAN_Message_t msg;

        // ���� = Byte0(��) + Byte1(��)
        msg.cmd = (RxData_Debug[0] << 8) | RxData_Debug[1];

        // ���ݴ� Byte2 ��ʼ
        for (int i = 0; i < 8; i++)
            msg.data[i] = RxData_Debug[i + 2];

        CAN_Process_Message(&msg);
    }

}


void CAN_Process_Message(CAN_Message_t *msg)
{
    switch (msg->cmd)
    {
        case 0x20:  // �����豸����
            Set_Device_Name(msg);
            break;
        case 0x10:  // ����˶�����
            Motor_Control(msg);
            break;
        case 0x15:  // ����Ƶ��
            Set_Frequency(msg);
            break;
        case 0x30:  // �������ĵ�
            Set_Center_Point(msg);
            break;
        case 0x31:  // ������λ��Χ
            Set_Limit_Range(msg);
            break;
        case 0x33:  // ǿ���趨λ��
            Force_Set_Position(msg);
            break;
        case 0x201:  // ״̬�ϱ�
            Report_Status(msg);
            break;
        case 0x202:  // ACK/ERRӦ��
            Handle_ACK_ERR(msg);
            break;
        case 0x203:  // ����
            //Heartbeat();
            break;
        default:
            // ��Ч����
            break;
    }
}

// �����豸����
void Set_Device_Name(CAN_Message_t *msg)
{
	/*
    uint16_t node_id = (msg->data[0] << 8) | msg->data[1];  // NodeID
    char device_name[9] = {0};
    for (int i = 0; i < 8; i++) {
        device_name[i] = msg->data[i + 2];  // DeviceName[0..7]
    }
	*/
}

void Motor_Control(CAN_Message_t *msg)
{

}

void Set_Frequency(CAN_Message_t *msg)
{

}

void Set_Center_Point(CAN_Message_t *msg)
{

}

void Set_Limit_Range(CAN_Message_t *msg)
{

}

void Force_Set_Position(CAN_Message_t *msg)
{

}

void Report_Status(CAN_Message_t *msg)
{

}


void Handle_ACK_ERR(CAN_Message_t *msg)
{

}



/**
 * @brief Send Status Feedback (0x121)
 * Format (16 Bytes CAN FD):
 *   Byte0-3: SourceNodeID (uint32)
 *   Byte4: AxisMask
 *   Byte5: Status (0=idle, 1=running, 2=error)
 *   Byte6: ErrorCode
 *   Byte7: Reserved
 *   Byte8-11: RemainingSteps (int32)
 *   Byte12-15: CurrentPosition (int32)
 * @param node_id: Source Node ID
 * @param axis_mask: Axis Mask
 * @param status: Current status
 * @param error_code: Error code if any
 * @param remaining_steps: Remaining steps to move
 * @param current_position: Current position
 */
void Send_Status_Feedback(uint32_t node_id, uint8_t axis_mask, uint8_t status, 
                          uint8_t error_code, int32_t remaining_steps, int32_t current_position)
{
    uint8_t tx_data[16];

    // Pack Status Feedback structure
    tx_data[0] = (node_id >> 24) & 0xFF;
    tx_data[1] = (node_id >> 16) & 0xFF;
    tx_data[2] = (node_id >> 8) & 0xFF;
    tx_data[3] = (node_id >> 0) & 0xFF;
    
    tx_data[4] = axis_mask;
    tx_data[5] = status;
    tx_data[6] = error_code;
    tx_data[7] = 0;  // Reserved
    
    // RemainingSteps (int32)
    tx_data[8] = (remaining_steps >> 24) & 0xFF;
    tx_data[9] = (remaining_steps >> 16) & 0xFF;
    tx_data[10] = (remaining_steps >> 8) & 0xFF;
    tx_data[11] = (remaining_steps >> 0) & 0xFF;
    
    // CurrentPosition (int32)
    tx_data[12] = (current_position >> 24) & 0xFF;
    tx_data[13] = (current_position >> 16) & 0xFF;
    tx_data[14] = (current_position >> 8) & 0xFF;
    tx_data[15] = (current_position >> 0) & 0xFF;

    // Send as CAN FD frame (16 bytes)
    CANFD_Send(CAN_ID_STATUS_FEEDBACK, tx_data, 16);
}

