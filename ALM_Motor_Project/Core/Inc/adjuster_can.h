#pragma once
#include "main.h"
#include <stdint.h>

#define CANID_MOTION_CMD    0x120
#define CANID_MOTION_STATUS 0x121
#define CANID_IDENT_AUTORUN 0x126
#define CANID_PARAM_QUERY  	0x127
#define CANID_PARAM_REPORT 	0x128
#define CANID_HEARTBEAT     0x203





uint32_t Build_NodeID_From_UID(void);

void AdjusterCAN_OnRx(uint32_t id, uint32_t idType, const uint8_t *data, uint8_t len);
void AdjusterCAN_TxStatus(void);
void AdjusterCAN_TxHeartbeat(void);
void AdjusterCAN_TxParamReport(void);

uint8_t Adjuster_IsMyNode(uint32_t target_node_id);
void CAN_Report_MotorStatus(void);
void CAN_TxService(void);
