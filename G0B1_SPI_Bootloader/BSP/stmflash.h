#ifndef __STMFLASH_H__
#define __STMFLASH_H__

#include "main.h"

#if	(MCU_Series==F10x)
#define Flash_Page_Size				1024  //Page size, F103=1K, G0=2K
#define AES_APP_ADDR					(0x08000000+IAP_Bootloat_SIZE+Flash_Page_Size) 	//��д������һҳ
#define FLASH_APP1_ADDR				(0x08000000+IAP_Bootloat_SIZE) 	//+8*1024	//��һ��Ӧ�ó�����ʼ��ַ(�����FLASH)

#define FLASH_PAGE_NB 0x7F  //For stm32f103 only
#endif

#if	(MCU_Series==G0xx)
#define Flash_Page_Size				2048  //Page size, F103=1K, G0=2K
#define AES_APP_ADDR					(0x08000000+IAP_Bootloat_SIZE+Flash_Page_Size) 	//加密固件暂存区，隔1页(与G031一致)
#define FLASH_APP1_ADDR				(0x08000000+IAP_Bootloat_SIZE) 	//+8*1024	//��һ��Ӧ�ó�����ʼ��ַ(�����FLASH)
#endif 

extern uint8_t AES_Decode_Flag;

#if	(MCU_Series==F10x)
void STMFLASH_Write(uint32_t WriteAddr,uint16_t *pBuffer,uint16_t NumToWrite);
void STMFLASH_Write_NoCheck(uint32_t WriteAddr,uint16_t *pBuffer,uint16_t NumToWrite);
uint16_t STMFLASH_ReadHalfWord(uint32_t faddr);
void STMFLASH_Read(uint32_t ReadAddr,uint16_t *pBuffer,uint16_t NumToRead);
#endif

#if	(MCU_Series==G0xx)
void STMFLASH_Write_64(uint32_t WriteAddr,uint8_t *pBuffer,uint16_t NumToWrite);
#endif

/*************** AES Decode **********************/
void Write_Flash_After_AES_Decode(uint8_t *IV_IN_OUT, uint8_t *key256bit);
void STMFLASH_Read_Byte(uint32_t ReadAddr,uint8_t *pBuffer,uint16_t NumToRead);

void STMFLASH_Erase(uint32_t WriteAddr, uint8_t NumToErase);

#endif
