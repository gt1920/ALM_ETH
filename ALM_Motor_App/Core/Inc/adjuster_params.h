#pragma once
#include "main.h"
#include <stdint.h>
#include "adjuster_flash.h"



bool AdjusterParams_Load(Adjuster_Params_t *p);
bool AdjusterParams_Save(const Adjuster_Params_t *p);
void AdjusterParams_SetDefaults(Adjuster_Params_t *p, uint32_t node_id);
extern Adjuster_Params_t g_adj_params;
