
#include "adjuster_check.h"
#include "adjuster_CAN_Poll.h"

void check_data(uint32_t now)
{
		static uint32_t last_tick = 0;

		if (now - last_tick < 50)
				return;

		last_tick = now;

		CAN_Poll();
}
