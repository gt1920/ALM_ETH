#ifndef __BSP_CRC_H__
#define __BSP_CRC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* IEEE 802.3 CRC32 (reflected polynomial 0xEDB88320) */
uint32_t BSP_CRC32_Calc(const uint8_t *buf, uint32_t len);

/* CRC16-CCITT (polynomial 0x1021, init 0xFFFF) for per-frame checks */
uint16_t BSP_CRC16_CCITT(const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_CRC_H__ */
