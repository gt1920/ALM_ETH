/**
  * cc.h - Architecture / compiler abstraction for LwIP on ARM Cortex-M (Keil MDK)
  */
#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#include <stdlib.h>

/* Define byte order: ARM Cortex-M is little-endian */
#define BYTE_ORDER LITTLE_ENDIAN

/* Types (u8_t, u16_t, u32_t, etc.) are defined by lwip/arch.h via stdint.h */

/* Printf formatters */
#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "u"

/* Compiler / packing */
#if defined(__ARMCC_VERSION)
  /* ARM Compiler 5 / 6 (Keil) */
  #define PACK_STRUCT_BEGIN
  #define PACK_STRUCT_STRUCT  __attribute__((packed))
  #define PACK_STRUCT_END
  #define PACK_STRUCT_FIELD(x) x
#elif defined(__GNUC__)
  #define PACK_STRUCT_BEGIN
  #define PACK_STRUCT_STRUCT  __attribute__((packed))
  #define PACK_STRUCT_END
  #define PACK_STRUCT_FIELD(x) x
#endif

/* Platform diagnostics */
#define LWIP_PLATFORM_DIAG(x)   do { } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { } while(0)

/* Random number for DHCP xid etc. – simple pseudo-random from SysTick */
#define LWIP_RAND() ((u32_t)rand())

#endif /* __ARCH_CC_H__ */
