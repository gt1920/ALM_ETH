#ifndef __W25QXX_H__
#define __W25QXX_H__

#include "main.h"
#include "spi.h"

/* W25Q16 Chip Info */
#define W25Q16_CAPACITY          (2 * 1024 * 1024)  /* 2MB = 16Mbit */
#define W25Q16_PAGE_SIZE         256
#define W25Q16_SECTOR_SIZE       4096               /* 4KB */
#define W25Q16_BLOCK_SIZE        65536              /* 64KB */
#define W25Q16_PAGE_COUNT        (W25Q16_CAPACITY / W25Q16_PAGE_SIZE)
#define W25Q16_SECTOR_COUNT      (W25Q16_CAPACITY / W25Q16_SECTOR_SIZE)
#define W25Q16_BLOCK_COUNT       (W25Q16_CAPACITY / W25Q16_BLOCK_SIZE)

/* W25Qxx Command Set */
#define W25X_WriteEnable         0x06
#define W25X_WriteDisable        0x04
#define W25X_ReadStatusReg1      0x05
#define W25X_ReadStatusReg2      0x35
#define W25X_WriteStatusReg      0x01
#define W25X_ReadData            0x03
#define W25X_FastReadData        0x0B
#define W25X_PageProgram         0x02
#define W25X_SectorErase         0x20    /* 4KB */
#define W25X_BlockErase32K       0x52    /* 32KB */
#define W25X_BlockErase64K       0xD8    /* 64KB */
#define W25X_ChipErase           0xC7
#define W25X_PowerDown           0xB9
#define W25X_ReleasePowerDown    0xAB
#define W25X_ManufactDeviceID    0x90
#define W25X_JedecDeviceID       0x9F

/* Status Register Bits */
#define W25X_SR_BUSY             0x01
#define W25X_SR_WEL              0x02

/* CS Pin Control */
#define W25QXX_CS_LOW()          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET)
#define W25QXX_CS_HIGH()         HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET)

/* Timeout */
#define W25QXX_TIMEOUT           5000

/* Function Prototypes */
u16  W25QXX_ReadID(void);
void W25QXX_Init(void);

void W25QXX_Read(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead);
void W25QXX_Write(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite);
void W25QXX_Write_NoCheck(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite);

void W25QXX_EraseSector(u32 SectorAddr);
void W25QXX_EraseBlock(u32 BlockAddr);
void W25QXX_EraseChip(void);

void W25QXX_PowerDown(void);
void W25QXX_WakeUp(void);

#endif
