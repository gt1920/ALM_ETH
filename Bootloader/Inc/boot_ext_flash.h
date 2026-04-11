#ifndef __BOOT_EXT_FLASH_H__
#define __BOOT_EXT_FLASH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Minimal external flash driver for Bootloader (polling only, read-only + erase meta) */

#define EXTFLASH_CMD_READ_ID          0x9F
#define EXTFLASH_CMD_READ             0x03
#define EXTFLASH_CMD_READ_STATUS      0x05
#define EXTFLASH_CMD_WRITE_ENABLE     0x06
#define EXTFLASH_CMD_SECTOR_ERASE_4K  0x20
#define EXTFLASH_CMD_PAGE_PROGRAM     0x02

#define EXTFLASH_SR_WIP               0x01

HAL_StatusTypeDef BootFlash_Init(void);
HAL_StatusTypeDef BootFlash_Read(uint32_t addr, uint8_t *buf, uint32_t len);
HAL_StatusTypeDef BootFlash_SectorErase4K(uint32_t addr);
HAL_StatusTypeDef BootFlash_PageProgram(uint32_t addr, const uint8_t *buf, uint32_t len);
HAL_StatusTypeDef BootFlash_WaitReady(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_EXT_FLASH_H__ */
