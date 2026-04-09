#ifndef MOTOR_FLASH_H
#define MOTOR_FLASH_H

#include "main.h" // 或者根据你的实际芯片使用对应的HAL库头文件
#include "Motor.h" // 假设你在另一个头文件中定义了MotorState结构体

// Flash 操作起始地址（修改为 page 62）
#define FLASH_START_ADDRESS 0x0801F000
#define FLASH_END_ADDRESS   0x08020000  // 假设存储电机状态的数据大小不超过2KB
#define DEFAULT_NODE_ID     0xFFFFFFFF  // 默认的NodeID，用来判断是否是全新芯片

// 函数声明
/*
void Read_MotorState_From_FLASH(MotorState *motor_state);
void Write_MotorState_To_FLASH(MotorState *motor_state);
void Erase_MotorState_Flash(void);
*/

#endif // MOTOR_FLASH_H
