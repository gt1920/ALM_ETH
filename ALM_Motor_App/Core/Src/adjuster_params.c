#include "adjuster_params.h"
#include <string.h>

bool AdjusterParams_Load(Adjuster_Params_t *p){ return false; }
bool AdjusterParams_Save(const Adjuster_Params_t *p){ return true; }

void AdjusterParams_SetDefaults(Adjuster_Params_t *p, uint32_t node_id)
{
    memset(p, 0, sizeof(*p));
    p->node_id = node_id;
}
