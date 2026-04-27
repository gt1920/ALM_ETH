#include "W25Qxx.h"

static u8  W25QXX_SectorBuf[W25Q16_SECTOR_SIZE];

/* -------------------- Low-Level SPI Helpers -------------------- */

static u8 SPI1_ReadWriteByte(u8 TxData)
{
    u8 RxData = 0;
    HAL_SPI_TransmitReceive(&hspi1, &TxData, &RxData, 1, W25QXX_TIMEOUT);
    return RxData;
}

/* -------------------- Basic Commands -------------------- */

static void W25QXX_WriteEnable(void)
{
    W25QXX_CS_LOW();
    SPI1_ReadWriteByte(W25X_WriteEnable);
    W25QXX_CS_HIGH();
}

static u8 W25QXX_ReadSR1(void)
{
    u8 sr;
    W25QXX_CS_LOW();
    SPI1_ReadWriteByte(W25X_ReadStatusReg1);
    sr = SPI1_ReadWriteByte(0xFF);
    W25QXX_CS_HIGH();
    return sr;
}

static void W25QXX_WaitBusy(void)
{
    while ((W25QXX_ReadSR1() & W25X_SR_BUSY) == W25X_SR_BUSY);
}

/* -------------------- Public Functions -------------------- */

u16 W25QXX_ReadID(void)
{
    u16 id = 0;
    W25QXX_CS_LOW();
    SPI1_ReadWriteByte(W25X_ManufactDeviceID);
    SPI1_ReadWriteByte(0x00);
    SPI1_ReadWriteByte(0x00);
    SPI1_ReadWriteByte(0x00);
    id  = (u16)SPI1_ReadWriteByte(0xFF) << 8;
    id |= SPI1_ReadWriteByte(0xFF);
    W25QXX_CS_HIGH();
    return id;
}

void W25QXX_Init(void)
{
    W25QXX_CS_HIGH();
}

/* -------------------- Read -------------------- */

void W25QXX_Read(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
    u16 i;
    W25QXX_CS_LOW();
    SPI1_ReadWriteByte(W25X_ReadData);
    SPI1_ReadWriteByte((u8)(ReadAddr >> 16));
    SPI1_ReadWriteByte((u8)(ReadAddr >> 8));
    SPI1_ReadWriteByte((u8)(ReadAddr));
    for (i = 0; i < NumByteToRead; i++)
    {
        pBuffer[i] = SPI1_ReadWriteByte(0xFF);
    }
    W25QXX_CS_HIGH();
}

/* -------------------- Page Program -------------------- */

static void W25QXX_WritePage(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 i;
    W25QXX_WriteEnable();
    W25QXX_CS_LOW();
    SPI1_ReadWriteByte(W25X_PageProgram);
    SPI1_ReadWriteByte((u8)(WriteAddr >> 16));
    SPI1_ReadWriteByte((u8)(WriteAddr >> 8));
    SPI1_ReadWriteByte((u8)(WriteAddr));
    for (i = 0; i < NumByteToWrite; i++)
    {
        SPI1_ReadWriteByte(pBuffer[i]);
    }
    W25QXX_CS_HIGH();
    W25QXX_WaitBusy();
}

/* -------------------- Write (no erase check) -------------------- */

void W25QXX_Write_NoCheck(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 pageRemain;
    pageRemain = W25Q16_PAGE_SIZE - (WriteAddr % W25Q16_PAGE_SIZE);
    if (NumByteToWrite <= pageRemain)
        pageRemain = NumByteToWrite;

    while (1)
    {
        W25QXX_WritePage(pBuffer, WriteAddr, pageRemain);
        if (NumByteToWrite == pageRemain)
            break;
        else
        {
            pBuffer      += pageRemain;
            WriteAddr    += pageRemain;
            NumByteToWrite -= pageRemain;
            if (NumByteToWrite > W25Q16_PAGE_SIZE)
                pageRemain = W25Q16_PAGE_SIZE;
            else
                pageRemain = NumByteToWrite;
        }
    }
}

/* -------------------- Write (with auto erase) -------------------- */

void W25QXX_Write(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u32 secPos;
    u16 secOff;
    u16 secRemain;
    u16 i;
    u8  needErase;

    secPos    = WriteAddr / W25Q16_SECTOR_SIZE;
    secOff    = WriteAddr % W25Q16_SECTOR_SIZE;
    secRemain = W25Q16_SECTOR_SIZE - secOff;

    if (NumByteToWrite <= secRemain)
        secRemain = NumByteToWrite;

    while (1)
    {
        W25QXX_Read(W25QXX_SectorBuf, secPos * W25Q16_SECTOR_SIZE, W25Q16_SECTOR_SIZE);

        needErase = 0;
        for (i = 0; i < secRemain; i++)
        {
            if (W25QXX_SectorBuf[secOff + i] != 0xFF)
            {
                needErase = 1;
                break;
            }
        }

        if (needErase)
        {
            W25QXX_EraseSector(secPos * W25Q16_SECTOR_SIZE);
            for (i = 0; i < secRemain; i++)
            {
                W25QXX_SectorBuf[secOff + i] = pBuffer[i];
            }
            W25QXX_Write_NoCheck(W25QXX_SectorBuf, secPos * W25Q16_SECTOR_SIZE, W25Q16_SECTOR_SIZE);
        }
        else
        {
            W25QXX_Write_NoCheck(pBuffer, WriteAddr, secRemain);
        }

        if (NumByteToWrite == secRemain)
            break;
        else
        {
            secPos++;
            secOff = 0;
            pBuffer        += secRemain;
            WriteAddr      += secRemain;
            NumByteToWrite -= secRemain;
            if (NumByteToWrite > W25Q16_SECTOR_SIZE)
                secRemain = W25Q16_SECTOR_SIZE;
            else
                secRemain = NumByteToWrite;
        }
    }
}

/* -------------------- Erase -------------------- */

void W25QXX_EraseSector(u32 SectorAddr)
{
    W25QXX_WriteEnable();
    W25QXX_WaitBusy();
    W25QXX_CS_LOW();
    SPI1_ReadWriteByte(W25X_SectorErase);
    SPI1_ReadWriteByte((u8)(SectorAddr >> 16));
    SPI1_ReadWriteByte((u8)(SectorAddr >> 8));
    SPI1_ReadWriteByte((u8)(SectorAddr));
    W25QXX_CS_HIGH();
    W25QXX_WaitBusy();
}

void W25QXX_EraseBlock(u32 BlockAddr)
{
    W25QXX_WriteEnable();
    W25QXX_WaitBusy();
    W25QXX_CS_LOW();
    SPI1_ReadWriteByte(W25X_BlockErase64K);
    SPI1_ReadWriteByte((u8)(BlockAddr >> 16));
    SPI1_ReadWriteByte((u8)(BlockAddr >> 8));
    SPI1_ReadWriteByte((u8)(BlockAddr));
    W25QXX_CS_HIGH();
    W25QXX_WaitBusy();
}

void W25QXX_EraseChip(void)
{
    W25QXX_WriteEnable();
    W25QXX_WaitBusy();
    W25QXX_CS_LOW();
    SPI1_ReadWriteByte(W25X_ChipErase);
    W25QXX_CS_HIGH();
    W25QXX_WaitBusy();
}

/* -------------------- Power Management -------------------- */

void W25QXX_PowerDown(void)
{
    W25QXX_CS_LOW();
    SPI1_ReadWriteByte(W25X_PowerDown);
    W25QXX_CS_HIGH();
}

void W25QXX_WakeUp(void)
{
    W25QXX_CS_LOW();
    SPI1_ReadWriteByte(W25X_ReleasePowerDown);
    W25QXX_CS_HIGH();
}
