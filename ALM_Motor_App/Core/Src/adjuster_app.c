#include "adjuster_app.h"
#include "adjuster_motion.h"
#include "adjuster_identify.h"
#include "adjuster_autorun.h"
#include "adjuster_can.h"
#include "adjuster_params.h"
#include <string.h>

AdjusterRuntime_t g_adj_rt;

void AdjusterApp_Init(void)
{
    memset(&g_adj_rt, 0, sizeof(g_adj_rt));
    AdjusterParams_SetDefaults(&g_adj_params, 0);
    AdjusterMotion_Init();
}

void AdjusterApp_Tick1ms(void)
{
    AdjusterIdentify_Tick1ms();
    AdjusterAutoRun_Tick1ms();
    AdjusterMotion_Tick1ms();

    if (++g_adj_rt.hb_tick_ms >= 500)
    {
        g_adj_rt.hb_tick_ms = 0;
        AdjusterCAN_TxHeartbeat();
    }
}
