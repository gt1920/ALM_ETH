#ifndef __IAP_H__
#define __IAP_H__
#include "main.h"
	
#define IAP_Bootloat_SIZE 		0x4800	// Bootloader占用 18KB (0x4800)。对应: FLASH_APP1_ADDR=0x08004800, AES_APP_ADDR=0x08005000
#define PackageSize						1024  	//iap package size=1K

#define HDMI_Player_Device	0xA0
#define LED_Bar							0xA1
#define Laser_Driver				0xA2
#define LED_Parcan      		0xA3

#define Device_Family	HDMI_Player_Device

// FW_ID 固件硬件匹配标识
#define FW_ID_MAGIC      0x47544657   // "GTFW" 小端
#define FW_ID_BOARD_ID   0x0B87
#define FW_ID_HEADER_SIZE  16         // 密文头: magic(4) + board_id(2) + fw_ver(2) + fw_sn(4) + crc32(4), AES-256-CBC 加密
#define FW_SN_WILDCARD   0xA5C3F09E   // 万能密码 .gt 头 fw_sn == 此值则跳过 SN 比对

/* ---- W25Q Firmware Storage Layout (must match App IAP_Sim.h) ---- */
#define W25Q_FW_DATA_ADDR    0x000000
#define W25Q_FW_INFO_ADDR    0x1FF000   /* Last 4KB sector of W25Q16 */
#define W25Q_FW_INFO_MAGIC   0x4E455746 /* "NEWF" — valid new firmware */

typedef struct
{
    uint32_t Magic;
    uint32_t FirmwareSize;
    uint32_t CRC32;
    uint16_t PackageNum;
    uint16_t Reserved;
} W25Q_FW_Info_t;

//����һ������ָ�����
typedef  void (*PFunc)(void);							
typedef  void (*PFunc_U16)(uint16_t);							

typedef struct
{
    uint8_t UpdataStart;//��ʼ���±�־
    uint8_t UpdataOk;//������ɱ�־
    uint32_t FirmwareSize;//�̼���С
		uint16_t PackageIndex;//������
    uint16_t PackageNum;//�ְ���
    uint8_t FW_ID_Match;  // FW_ID校验: 1=匹配, 0=不匹配
    uint8_t SN_Match;     // SN校验: 1=匹配或通配, 0=不匹配
    uint16_t WateTime;//�ȴ���λ������ˢ�̼���ʱʱ��
    uint8_t* RxBuff;//��������ָ��
    uint8_t* TxBuff;//��������ָ��
    uint16_t* RxLength;//���ճ���
    PFunc_U16 SendFunc;//���ͺ���ָ�� 
}Stm32IapStruct;


extern Stm32IapStruct IGK_IAP;

//IAP��ʼ��
void IAP_BootLoad_Init(void);
void IAP_BootLoad_UpData(void);
void iap_load_app(uint32_t appxaddr);
void System_ReStart(void);
//����CRCУ��
unsigned int CRC32Calculate(uint8_t *pBuf ,uint16_t pBufSize);
//����CRCУ���
void CRC32TableCreate(void);

void iap_monition(void);

// W25Q SPI Flash升级路径
uint8_t W25Q_FW_TryUpgrade(void);
void W25Q_FW_ClearFlag(void);

#endif
