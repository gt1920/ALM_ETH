#include "Motor_Flash.h"
#include "string.h"  // 用于 memset 和 memcpy

/*
// 擦除 FLASH 中的电机状态部分
void Erase_MotorState_Flash(void) {
    HAL_FLASH_Unlock();  // 解锁FLASH写入

    // 初始化擦除结构体
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;

    // 设置擦除参数
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;  // 擦除类型：页面擦除
    EraseInitStruct.Banks = FLASH_BANK_1;                // 设置擦除的银行（根据需要选择 FLASH_BANK_1 或 FLASH_BANK_2）
    EraseInitStruct.Page = 62;                           // 擦除的页面索引，根据实际情况设置
    EraseInitStruct.NbPages = 1;                         // 擦除的页数（此处为 1 页）

    // 执行擦除操作
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        // 错误处理：擦除失败
        // 例如，点亮一个LED表示擦除失败
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); // 例如点亮LED表示擦除失败
    }

    HAL_FLASH_Lock();  // 锁住FLASH，防止其他写入
}

// 写入电机状态到 FLASH
void Write_MotorState_To_FLASH(MotorState *motor_state) {
    Erase_MotorState_Flash();  // 擦除FLASH中的电机状态部分

    HAL_FLASH_Unlock();  // 解锁FLASH写入

    // 将设备状态（包括X和Y）写入到FLASH
    uint32_t *flash_ptr = (uint32_t *)FLASH_START_ADDRESS;
    uint32_t *motor_state_ptr = (uint32_t *)motor_state;
    for (int i = 0; i < sizeof(MotorState) / 4; i++) {
        // 使用 HAL_FLASH_Program 写入数据
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_FAST, (uint32_t)(flash_ptr + i), *(motor_state_ptr + i));
    }

    HAL_FLASH_Lock();  // 锁住FLASH，防止其他写入
}

// 从 FLASH 读取电机状态
void Read_MotorState_From_FLASH(MotorState *motor_state) {
    // 检查是否为全新芯片，假设通过NodeID判断
    uint32_t *node_id_in_flash = (uint32_t *)FLASH_START_ADDRESS;
		
    // 如果是全新芯片（NodeID为默认值），设置默认值
    if (*node_id_in_flash == DEFAULT_NODE_ID) {
		
			
        // 设置设备默认值
        motor_state->NodeID = FDCAN_NodeID;  // 默认NodeID
        Init_Device_Name(motor_state);  // 初始化设备名

        // 初始化固件版本和出厂日期
        motor_state->firmware_version = 10000;  // 固件版本 1.0.0
        motor_state->MFC_Year = 25;   // 出厂年份
        motor_state->MFC_Month = 1;     // 出厂月份
        motor_state->MFC_Day = 1;       // 出厂日期

        // 设置 X 电机默认值
        motor_state->X.pos = 0;
        motor_state->X.center_point = 0;
        motor_state->X.limit_range = 20000;  // 假设默认限位为1000
        motor_state->X.frequency = 200;     // 默认频率为500Hz
        motor_state->X.motor_state = 0;     // 默认电机状态为闲置
        motor_state->X.direction = 0;       // 默认电机方向为正向
        motor_state->X.enable = 0;          // 默认禁用电机
        motor_state->X.target_step = 0;     // 默认目标步进数为0

        // 设置 Y 电机默认值
        motor_state->Y.position = 0;
        motor_state->Y.center_point = 0;
        motor_state->Y.limit_range = 20000;  // 假设默认限位为1000
        motor_state->Y.frequency = 200;     // 默认频率为500Hz
        motor_state->Y.motor_state = 0;     // 默认电机状态为闲置
        motor_state->Y.direction = 0;       // 默认电机方向为正向
        motor_state->Y.enable = 0;          // 默认禁用电机
        motor_state->Y.target_step = 0;     // 默认目标步进数为0

        // 如果是全新芯片，则写入默认值到FLASH
        //Write_MotorState_To_FLASH(motor_state);
    } else {
        // 如果不是全新芯片，正常读取设备状态（包含X和Y电机状态）
        //memcpy(motor_state, (void *)FLASH_START_ADDRESS, sizeof(MotorState));
				
    }
}
*/

