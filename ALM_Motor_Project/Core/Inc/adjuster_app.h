#pragma once
#include "main.h"
#include <stdint.h>

typedef enum {
    REJECT_NONE          = 0x00,
    REJECT_LIMIT_HIT     = 0x01,
    REJECT_BUSY_AUTORUN  = 0x02,
    REJECT_INVALID_STATE = 0x03,
    REJECT_PARAM_ERROR   = 0x04,
} RejectReason_t;

typedef struct
{
    int32_t pos_x;
    int32_t pos_y;
    uint32_t remain_x;
    uint32_t remain_y;

    uint8_t running;
    uint8_t done;
    uint8_t limit_hit;
    uint8_t auto_running;
    uint8_t busy;

    RejectReason_t reject_reason;
    uint32_t hb_tick_ms;
} AdjusterRuntime_t;

void AdjusterApp_Init(void);
void AdjusterApp_Tick1ms(void);

extern AdjusterRuntime_t g_adj_rt;
