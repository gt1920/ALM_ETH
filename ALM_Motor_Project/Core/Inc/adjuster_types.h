#pragma once
#include "main.h"
#include <stdint.h>

typedef enum {
    AXIS_X  = 0x01,
    AXIS_Y  = 0x02,
    AXIS_XY = 0x03
} AxisMask_t;

typedef enum {
    MOTION_RELATIVE = 0x00,
    MOTION_ABSOLUTE = 0x01,
    MOTION_HOME     = 0x02
} MotionMode_t;
