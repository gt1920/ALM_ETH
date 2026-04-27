#ifndef __BSP_W25Q16_H__
#define __BSP_W25Q16_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* W25Q16JV: 2 MB / 16 Mbit, 4 KB sector, 256 B page */
#define W25Q_FLASH_SIZE        (2U * 1024U * 1024U)
#define W25Q_PAGE_SIZE         256U
#define W25Q_SECTOR_SIZE       4096U
#define W25Q_BLOCK64_SIZE      (64U * 1024U)
#define W25Q_PAGES_PER_SECTOR  (W25Q_SECTOR_SIZE / W25Q_PAGE_SIZE)

/* JEDEC ID = 0x EF 40 15  (Winbond | SPI | 2^21 bytes) */
#define W25Q_JEDEC_W25Q16      0x00EF4015UL

typedef enum {
    W25Q_OK     = 0,
    W25Q_ERR    = 1,
    W25Q_TMOUT  = 2,
    W25Q_PARAM  = 3,
    W25Q_NOID   = 4,
} W25Q_Status_t;

/* ---- public API (all blocking) ---- */
W25Q_Status_t BSP_W25Q_Init(void);
W25Q_Status_t BSP_W25Q_ReadJEDEC(uint32_t *id);
W25Q_Status_t BSP_W25Q_ReadStatus1(uint8_t *sr);
W25Q_Status_t BSP_W25Q_ReadStatus2(uint8_t *sr);
W25Q_Status_t BSP_W25Q_EraseSector4K(uint32_t addr);
W25Q_Status_t BSP_W25Q_EraseChip(void);
/* PageProgramQuad: <=256 B, must not cross a 256 B page boundary */
W25Q_Status_t BSP_W25Q_PageProgramQuad(uint32_t addr, const uint8_t *buf, uint32_t len);
/* Write: any length, splits at page boundaries internally */
W25Q_Status_t BSP_W25Q_Write(uint32_t addr, const uint8_t *buf, uint32_t len);
/* Read: standard 0x03 read, single line, any length */
W25Q_Status_t BSP_W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_W25Q16_H__ */
