#ifndef __CAN_MOTOR_H
#define __CAN_MOTOR_H

#include "main.h"

/*
 * Motion Control Command (CAN ID = 0x120)
 
	Byte 0..3   NodeID   (uint32, LE)
	Byte 4      Axis     (0 = X, 1 = Y)
	Byte 5      Dir      (0 = +, 1 = -)
	Byte 6..7   Reserved
	Byte 8..11  Step     (uint32, LE, always positive)
	Byte 12..15 Reserved

 */

extern uint16_t rx_id;
extern uint8_t  rx_buf[64];

typedef struct
{
    uint32_t node_id;
    uint8_t  axis;     // 0=X, 1=Y
    uint8_t  dir;      // 0=+, 1=-
    uint32_t step;     // always positive
} CAN_MotionCmd_t;


typedef struct
{
    uint32_t node_id;
    uint8_t  axis_mask;
    uint8_t  flags;
    int32_t  step_count;
} CAN_ControlFrame_t;

extern CAN_MotionCmd_t frame;

void CAN_Task(void);

void Process_Command(const CAN_MotionCmd_t *cmd);
void CAN_Parse_ControlFrame(const uint8_t *data, CAN_MotionCmd_t *out);

/* Ω” ’÷Ÿ≤√÷°£®Classic CAN / FIFO0£© */
uint8_t CAN_Receive_Arb_Frame(uint16_t *rx_id, uint8_t *rx_buf);

void CAN_Motor_Status_Task(void);

#endif /* __CAN_MOTOR_H */
