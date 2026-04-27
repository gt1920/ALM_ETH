#ifndef __BSP_EXT_FLASH_H__
#define __BSP_EXT_FLASH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Macronix MX25L12835F / MX25L128 command set (SPI mode) */
#define EXTFLASH_CMD_READ_ID          0x9F
#define EXTFLASH_CMD_READ             0x03   /* Normal read  (1-1-1, no dummy) */
#define EXTFLASH_CMD_FAST_READ        0x0B   /* Fast read    (1-1-1, 8 dummy) */
#define EXTFLASH_CMD_QUAD_READ        0x6B   /* Quad output  (1-1-4, 8 dummy) */
#define EXTFLASH_CMD_PAGE_PROGRAM     0x02   /* Page program (1-1-1, max 256B) */
#define EXTFLASH_CMD_QUAD_PP          0x38   /* Quad page program (1-1-4) */
#define EXTFLASH_CMD_SECTOR_ERASE_4K  0x20
#define EXTFLASH_CMD_BLOCK_ERASE_64K  0xD8
#define EXTFLASH_CMD_CHIP_ERASE       0xC7
#define EXTFLASH_CMD_WRITE_ENABLE     0x06
#define EXTFLASH_CMD_WRITE_DISABLE    0x04
#define EXTFLASH_CMD_READ_STATUS      0x05
#define EXTFLASH_CMD_READ_CONFIG      0x15
#define EXTFLASH_CMD_WRITE_STATUS_CFG 0x01   /* Write Status + Config regs */

/* Status register bits */
#define EXTFLASH_SR_WIP               0x01   /* Write In Progress */
#define EXTFLASH_SR_WEL               0x02   /* Write Enable Latch */

/* Config register bits */
#define EXTFLASH_CFG_QUAD_EN          0x40   /* QE bit in Config reg (SR2 for MX) */

/* Geometry */
#define EXTFLASH_PAGE_SIZE            256U
#define EXTFLASH_SECTOR_SIZE_4K       4096U
#define EXTFLASH_BLOCK_SIZE_64K       (64U * 1024U)
#define EXTFLASH_TOTAL_SIZE           (16U * 1024U * 1024U)

/* API */
HAL_StatusTypeDef ExtFlash_Init(void);
HAL_StatusTypeDef ExtFlash_ReadID(uint32_t *id);
HAL_StatusTypeDef ExtFlash_Read(uint32_t addr, uint8_t *buf, uint32_t len);
HAL_StatusTypeDef ExtFlash_PageProgram(uint32_t addr, const uint8_t *buf, uint32_t len);
HAL_StatusTypeDef ExtFlash_SectorErase4K(uint32_t addr);
HAL_StatusTypeDef ExtFlash_BlockErase64K(uint32_t addr);
HAL_StatusTypeDef ExtFlash_ChipErase(void);
HAL_StatusTypeDef ExtFlash_WaitReady(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_EXT_FLASH_H__ */
