/* ============================================================================
 *  fw_version.h — single source of truth for the FW version reported to PC.
 *
 *  Wire format (4 bytes): FW_HW_VER . BUILD_DAY . BUILD_MONTH . BUILD_YEAR
 *    - FW_HW_VER:  build counter, auto-incremented by MDK-ARM\Inc_Build.bat
 *                  before each build. The number itself lives in
 *                  fw_build_number.h — do NOT edit that file by hand.
 *                  (8-bit on the wire, so it wraps at 256.)
 *    - BUILD_*:    derived from __DATE__ at compile time.
 *
 *  Used by:
 *    - BSP/comm_protocol.c   (TCP version-info reply)
 *    - BSP/udp_discovery.c   (UDP discovery announcement)
 * ========================================================================== */
#ifndef __FW_VERSION_H__
#define __FW_VERSION_H__

#include "fw_build_number.h"

#define FW_HW_VER   FW_BUILD_NUMBER

#define BUILD_DAY   ((__DATE__[4] == ' ' ? 0 : ((__DATE__[4] - '0') * 10)) + (__DATE__[5] - '0'))
#define BUILD_MONTH ( \
    __DATE__[0] == 'J' ? (__DATE__[1] == 'a' ? 1 : (__DATE__[2] == 'n' ? 6 : 7)) : \
    __DATE__[0] == 'F' ? 2 : \
    __DATE__[0] == 'M' ? (__DATE__[2] == 'r' ? 3 : 5) : \
    __DATE__[0] == 'A' ? (__DATE__[1] == 'p' ? 4 : 8) : \
    __DATE__[0] == 'S' ? 9 : \
    __DATE__[0] == 'O' ? 10 : \
    __DATE__[0] == 'N' ? 11 : 12)
#define BUILD_YEAR  (((__DATE__[9] - '0') * 10) + (__DATE__[10] - '0'))

#endif /* __FW_VERSION_H__ */
