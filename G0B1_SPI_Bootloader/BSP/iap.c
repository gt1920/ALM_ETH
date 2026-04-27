
#include "iap.h"
#include "AES.h"
#include "Usart_Func.h"
#include "stmflash.h"
#include "W25Qxx.h"
#include "CPU_ID.h"

Stm32IapStruct IGK_IAP;

unsigned long CRC32Table[256];  //CRC校验表

//appxaddr:应用程序起始地址
//appbuf:应用程序CODE.
//appsize:应用程序大小(字节).

void iap_write_appbin(uint32_t appxaddr,uint8_t *appbuf,uint32_t appsize)
{
	#if	(MCU_Series==F10x)

	uint16_t t;
	uint16_t i=0;
	uint16_t iapbuf[1024];
	uint16_t temp;
	uint32_t fwaddr=appxaddr;//当前写入的地址
	uint8_t *dfu=appbuf;

	for(t=0;t<appsize;t+=2)
	{
		temp=(uint16_t)dfu[1]<<8;
		temp+=(uint16_t)dfu[0];
		dfu+=2;//偏移2个字节
		iapbuf[i++]=temp;
		if(i==1024)
		{
			i=0;
			STMFLASH_Write(fwaddr,iapbuf,1024);
			fwaddr+=2048;//偏移2048  16=2*8.所以要乘以2.
		}
	}

	if(i)STMFLASH_Write(fwaddr,iapbuf,i);//将最后一些内容字节写进去.
	#endif

	#if	(MCU_Series==G0xx)
		STMFLASH_Write_64(appxaddr, appbuf, appsize);  //STM32G031 Write 64bit
	#endif

}

//设置栈顶地址
//addr:栈顶地址
__asm void MSR_MSP(uint32_t addr)
{
    MSR MSP, r0 			//set Main Stack value
    BX r14
}


//跳转到应用程序段
//appxaddr:用户代码起始地址.
void iap_load_app(uint32_t appxaddr)
{
    //定义函数指针
  PFunc Jump2App;

	#if	(MCU_Series==G0xx)
	__disable_irq();
	#endif

	if(((*(__IO uint32_t*)appxaddr)&0x2FFE0000)==0x20000000)	//检查栈顶地址是否合法.
	{
		Jump2App=(PFunc)*(__IO uint32_t*)(appxaddr+4);		//用户代码区第二个字为程序开始地址(复位地址)

		MSR_MSP(*(__IO uint32_t*)appxaddr);					//初始化APP堆栈指针(用户代码区的第一个字用于存放栈顶地址)

		#if	(MCU_Series==G0xx)
		for(int i = 0; i < 8; i++)
		{
				NVIC->ICER[i] = 0xFFFFFFFF; // 关闭中断
				NVIC->ICPR[i] = 0xFFFFFFFF; // 清除中断标志位
		}
		#endif

		Jump2App();  //跳转到APP
	}
}

//软复位
void System_ReStart(void)
{
	uint32_t time = 65535;
	while(time--)
	{
		__NOP();
	}
	NVIC_SystemReset();
}


//CRC32校验，
//pBuf:数据缓存区
//pBufSize:实际长度
uint8_t check_data(uint8_t *pBuf ,uint16_t pBufSize)	//1:成功 0:失败
{
	uint32_t get_PC_crcVel=0;
    uint32_t CRC_PC_vel = 0;
	uint8_t *pbuff;
	if(pBufSize<5)
    {
        return 0;
    }
	pbuff = pBuf+pBufSize-4;

	CRC_PC_vel = pbuff[0]<<24|pbuff[1]<<16|pbuff[2]<<8|pbuff[3];	//接收到的CRC32

	get_PC_crcVel 	 = CRC32Calculate(pBuf,pBufSize-4);	//MCU端计算的CRC32

	if(get_PC_crcVel == CRC_PC_vel)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

//IAP初始化
void IAP_BootLoad_Init(void)
{
	StateLED_GPIO_Port->BRR = StateLED_Pin;

	AES_Decode_Flag = 0;

	// 读取 MCU UID → 生成本机 SN（供后续 SN 比对使用）
	get_cpu_id();

	//CRC校验的表
	CRC32TableCreate();
	//初始化
	IGK_IAP.UpdataStart = 0;
	IGK_IAP.UpdataOk = 0;
	IGK_IAP.FW_ID_Match = 0;
	IGK_IAP.SN_Match = 0;
	IGK_IAP.WateTime = 0;
	IGK_IAP.RxBuff = Uart1_Rx;//接收缓存指针
	IGK_IAP.TxBuff = Uart1_Tx;//发送缓存指针
	IGK_IAP.RxLength = &Uart1_Rx_length;//接收长度
	IGK_IAP.SendFunc = Uart1_Start_DMA_Tx;//发送函数

	USART1->CR1 |= (1<<4);  //Enable IDLE interrupt
	USART1->CR1 |= (1<<6);  //Enable TC interrupt

	//DMX_IN;

	HAL_UART_Receive_DMA(&huart1, Uart1_Rx, UART1_RX_LEN);
}

//IAP数据接收处理，放到串口中断
void IAP_BootLoad_UpData(void)
{
    unsigned int crc=0;

    //上位机请求刷固件
    if((IGK_IAP.RxBuff[0] == Device_Family) && (IGK_IAP.RxBuff[1] == 0x01))
    {
				AES_Decode_Flag = 1;  //Updating Processing

        if(check_data(IGK_IAP.RxBuff,*IGK_IAP.RxLength))//crc校验成功
        {
            //开始刷固件标志位
            IGK_IAP.UpdataStart = 1;
            //获取固件大小
            IGK_IAP.FirmwareSize = IGK_IAP.RxBuff[2]<<24 | IGK_IAP.RxBuff[3]<<16 | IGK_IAP.RxBuff[4]<<8 | IGK_IAP.RxBuff[5];
            //获取总包数
            IGK_IAP.PackageNum = IGK_IAP.RxBuff[6]<<8 | IGK_IAP.RxBuff[7];
            //应答
            IGK_IAP.TxBuff[0] = Device_Family;
            IGK_IAP.TxBuff[1] = 0x81;

            IGK_IAP.TxBuff[2] = IGK_IAP.RxBuff[2];
            IGK_IAP.TxBuff[3] = IGK_IAP.RxBuff[3];
            IGK_IAP.TxBuff[4] = IGK_IAP.RxBuff[4];
            IGK_IAP.TxBuff[5] = IGK_IAP.RxBuff[5];

            IGK_IAP.TxBuff[6] = IGK_IAP.RxBuff[6];
            IGK_IAP.TxBuff[7] = IGK_IAP.RxBuff[7];

            crc = CRC32Calculate(IGK_IAP.TxBuff,8);

            IGK_IAP.TxBuff[8] = crc>>24;
            IGK_IAP.TxBuff[9] = crc>>16;
            IGK_IAP.TxBuff[10] = crc>>8;
            IGK_IAP.TxBuff[11] = crc&0xff;

            IGK_IAP.SendFunc(12);
        }
    }
    else if((IGK_IAP.RxBuff[0] == Device_Family) && (IGK_IAP.RxBuff[1] == 0x02))
    {
        //crc校验成功，写入Flash
        if(check_data(IGK_IAP.RxBuff,*IGK_IAP.RxLength))
        {
            //数据包
            //获取包序号
            IGK_IAP.PackageIndex = IGK_IAP.RxBuff[2]<<8 | IGK_IAP.RxBuff[3];

            // 第一个包：校验加密 FW_ID 头 (RxBuff[4..19] 为 16B 密文头)
            // 明文布局: magic(4)+board(2)+ver(2)+sn(4)+crc32(4)
            // 独立 CBC：本地 IV 副本，不污染全局 IV 以免后续 payload 解密链错位
            if(IGK_IAP.PackageIndex == 0)
            {
                uint8_t hdr[16];
                uint8_t iv_local[16];
                uint32_t fw_magic, fw_sn, hdr_crc, calc_crc, my_sn;
                uint16_t fw_board;

                for(uint8_t i = 0; i < 16; i++) hdr[i] = IGK_IAP.RxBuff[4 + i];
                for(uint8_t i = 0; i < 16; i++) iv_local[i] = IV[i];
                Aes_IV_key256bit_Decode(iv_local, hdr, Key);

                fw_magic = hdr[0] | ((uint32_t)hdr[1]<<8) | ((uint32_t)hdr[2]<<16) | ((uint32_t)hdr[3]<<24);
                fw_board = hdr[4] | ((uint16_t)hdr[5]<<8);
                fw_sn    = hdr[8] | ((uint32_t)hdr[9]<<8) | ((uint32_t)hdr[10]<<16) | ((uint32_t)hdr[11]<<24);
                hdr_crc  = hdr[12]| ((uint32_t)hdr[13]<<8)| ((uint32_t)hdr[14]<<16)| ((uint32_t)hdr[15]<<24);
                calc_crc = CRC32Calculate(hdr, 12);

                my_sn = ((uint32_t)cpu_id.SPC_8bit_UID_0 << 24)
                      | ((uint32_t)cpu_id.SPC_8bit_UID_1 << 16)
                      | ((uint32_t)cpu_id.SPC_8bit_UID_2 <<  8)
                      |  (uint32_t)cpu_id.SPC_8bit_UID_3;

                // CRC 失败或 magic/board 不对 → 视为非法固件（密钥错/文件损坏/伪造头）
                IGK_IAP.FW_ID_Match = (calc_crc == hdr_crc &&
                                       fw_magic == FW_ID_MAGIC &&
                                       fw_board == FW_ID_BOARD_ID) ? 1 : 0;
                IGK_IAP.SN_Match    = (IGK_IAP.FW_ID_Match &&
                                       (fw_sn == FW_SN_WILDCARD || fw_sn == my_sn)) ? 1 : 0;
            }

            // FW_ID 与 SN 都匹配才写 Flash，不匹配只应答不写
            if(IGK_IAP.FW_ID_Match && IGK_IAP.SN_Match)
            {
                iap_write_appbin(AES_APP_ADDR+IGK_IAP.PackageIndex*PackageSize,&IGK_IAP.RxBuff[4],*IGK_IAP.RxLength-8);
            }

            //应答
            IGK_IAP.TxBuff[0] = Device_Family;
            IGK_IAP.TxBuff[1] = 0x82;

            IGK_IAP.TxBuff[2] = IGK_IAP.RxBuff[2];
            IGK_IAP.TxBuff[3] = IGK_IAP.RxBuff[3];
            //成功
            IGK_IAP.TxBuff[4] = 0x01;

            crc = CRC32Calculate(IGK_IAP.TxBuff,5);

            IGK_IAP.TxBuff[5] = crc>>24;
            IGK_IAP.TxBuff[6] = crc>>16;
            IGK_IAP.TxBuff[7] = crc>>8;
            IGK_IAP.TxBuff[8] = crc&0xff;

            IGK_IAP.SendFunc(9);

						//判断是否发送完成
            if(IGK_IAP.PackageIndex == (IGK_IAP.PackageNum-1))
            {
                //升级完成标志位
                IGK_IAP.UpdataOk = 1;
								AES_Decode_Flag = 2;  //Updating Processing
            }

        }
        else//crc校验失败，返回错误代码
        {
             //应答
            IGK_IAP.TxBuff[0] = Device_Family;
            IGK_IAP.TxBuff[1] = 0x82;

            IGK_IAP.TxBuff[2] = IGK_IAP.RxBuff[2];
            IGK_IAP.TxBuff[3] = IGK_IAP.RxBuff[3];
            //失败
            IGK_IAP.TxBuff[4] = 0x02;
            crc = CRC32Calculate(IGK_IAP.TxBuff,5);
            IGK_IAP.TxBuff[5] = crc>>24;
            IGK_IAP.TxBuff[6] = crc>>16;
            IGK_IAP.TxBuff[7] = crc>>8;
            IGK_IAP.TxBuff[8] = crc&0xff;
            IGK_IAP.SendFunc(9);
        }

    }
}
//生成CRC校验表
void CRC32TableCreate(void)
{
    unsigned int c;
    unsigned int i, j;

    for (i = 0; i < 256; i++) {
        c = (unsigned int)i;
        for (j = 0; j < 8; j++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        CRC32Table[i] = c;
    }
}
//计算CRC校验
unsigned int CRC32Calculate(uint8_t *pBuf ,uint16_t pBufSize)
{
    unsigned int retCRCValue=0xffffffff;
    unsigned char *pData;
    pData=(unsigned char *)pBuf;
     while(pBufSize--)
     {
         retCRCValue=CRC32Table[(retCRCValue ^ *pData++) & 0xFF]^ (retCRCValue >> 8);
     }
     return retCRCValue^0xffffffff;
}

/* ==================== W25Q SPI Flash Upgrade Path ==================== */

/* Clear W25Q firmware info sector (erase MAGIC flag) */
void W25Q_FW_ClearFlag(void)
{
    W25QXX_EraseSector(W25Q_FW_INFO_ADDR);
}

/* Calculate CRC32 of firmware stored in W25Q.
   Reuses Uart1_Rx as temp read buffer (safe — UART path has timed out). */
static uint32_t W25Q_FW_CalcCRC(uint32_t size)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t addr = W25Q_FW_DATA_ADDR;
    uint32_t remaining = size;

    while (remaining > 0)
    {
        uint16_t chunk = (remaining > 1024) ? 1024 : (uint16_t)remaining;
        W25QXX_Read(Uart1_Rx, addr, chunk);

        for (uint16_t i = 0; i < chunk; i++)
            crc = CRC32Table[(crc ^ Uart1_Rx[i]) & 0xFF] ^ (crc >> 8);

        addr      += chunk;
        remaining -= chunk;
    }
    return crc ^ 0xFFFFFFFF;
}

/* Read firmware from W25Q, AES-256 decode, write to internal Flash.
   Mirrors Write_Flash_After_AES_Decode() but reads from W25Q instead of
   internal Flash. */
static void W25Q_FW_FlashAndDecode(uint8_t *IV_IN_OUT, uint8_t *key256bit)
{
    uint8_t  temp[16];
    uint32_t app_size;
    uint16_t read_count;
    uint32_t w25q_addr;
    uint32_t flash_dest;

    /* 跳过 W25Q 开头的 16B 密文头 (Header 独立 CBC, 已在 TryUpgrade 阶段校验).
       紧随其后是独立 CBC 的 16B Metadata 块 (auto-fill + app size), 再是 Payload CBC 链. */
    w25q_addr  = W25Q_FW_DATA_ADDR + FW_ID_HEADER_SIZE;
    flash_dest = FLASH_APP1_ADDR;

    W25QXX_Read(temp, w25q_addr, 16);
    Aes_IV_key256bit_Decode(IV_IN_OUT, temp, key256bit);

    app_size = ((uint32_t)temp[15]<<24) | ((uint32_t)temp[14]<<16) |
               ((uint32_t)temp[13]<<8)  | temp[12];

    read_count = app_size / 16;
    if (app_size % 16 != 0) read_count++;

    w25q_addr += 16;  /* now points to first data block */

    for (uint16_t i = 0; i < read_count; i++)
    {
        if ((flash_dest < FLASH_BASE) ||
            (flash_dest >= (FLASH_BASE + FLASH_PAGE_SIZE * FLASH_PAGE_NB)))
            return;

        HAL_FLASH_Unlock();

        /* Erase internal Flash page at page boundary */
        if (flash_dest % FLASH_PAGE_SIZE == 0)
        {
            FLASH_EraseInitTypeDef ef = {0};
            ef.TypeErase = FLASH_TYPEERASE_PAGES;
            ef.Banks     = FLASH_BANK_1;
            ef.Page      = (flash_dest - FLASH_BASE) / FLASH_PAGE_SIZE;
            ef.NbPages   = 1;
            uint32_t pe  = 0;
            HAL_FLASHEx_Erase(&ef, &pe);
        }

        /* Read 16 bytes from W25Q and AES decode */
        W25QXX_Read(temp, w25q_addr + (uint32_t)i * 16, 16);
        Aes_IV_key256bit_Decode(IV_IN_OUT, temp, key256bit);

        /* Write 16 bytes to internal Flash (2 × double-word) */
        uint32_t src = (uint32_t)temp;
        for (uint8_t w = 0; w < 2; w++)
        {
            uint32_t d1 = *(uint32_t*)src; src += 4;
            uint64_t d2 = *(uint32_t*)src; src += 4;
            uint64_t dw = d1 | (d2 << 32);
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, flash_dest, dw) == HAL_OK)
                flash_dest += 8;
        }

        HAL_FLASH_Lock();
    }

    AES_Decode_Flag = 3;
}

/* Check W25Q for new firmware. If found and CRC valid, flash it.
   Returns 1 if new firmware was written, 0 otherwise. */
uint8_t W25Q_FW_TryUpgrade(void)
{
    W25Q_FW_Info_t info;
    uint8_t fw_hdr[FW_ID_HEADER_SIZE];
    uint8_t iv_local[16];
    uint32_t fw_magic;
    uint16_t fw_board;
    uint32_t fw_sn;
    uint32_t hdr_crc, calc_crc;
    uint32_t my_sn;

    /* 1) Read info sector */
    W25QXX_Read((uint8_t*)&info, W25Q_FW_INFO_ADDR, sizeof(info));

    if (info.Magic != W25Q_FW_INFO_MAGIC)
        return 0;   /* No new firmware */

    /* 2) Read 16B 密文头 → 本地 IV 副本独立 CBC 解密 → 校验 magic/board/sn/crc */
    W25QXX_Read(fw_hdr, W25Q_FW_DATA_ADDR, FW_ID_HEADER_SIZE);
    for (uint8_t i = 0; i < 16; i++) iv_local[i] = IV[i];
    Aes_IV_key256bit_Decode(iv_local, fw_hdr, Key);

    fw_magic = fw_hdr[0] | ((uint32_t)fw_hdr[1]<<8) |
               ((uint32_t)fw_hdr[2]<<16) | ((uint32_t)fw_hdr[3]<<24);
    fw_board = fw_hdr[4] | ((uint16_t)fw_hdr[5]<<8);
    fw_sn    = fw_hdr[8] | ((uint32_t)fw_hdr[9]<<8) |
               ((uint32_t)fw_hdr[10]<<16) | ((uint32_t)fw_hdr[11]<<24);
    hdr_crc  = fw_hdr[12]| ((uint32_t)fw_hdr[13]<<8)|
               ((uint32_t)fw_hdr[14]<<16)| ((uint32_t)fw_hdr[15]<<24);
    calc_crc = CRC32Calculate(fw_hdr, 12);

    if (calc_crc != hdr_crc || fw_magic != FW_ID_MAGIC || fw_board != FW_ID_BOARD_ID)
    {
        W25Q_FW_ClearFlag();
        return 0;   /* Header CRC or FW_ID mismatch */
    }

    /* 2b) Verify SN — 通配或命中本机 UID 才放行 */
    my_sn = ((uint32_t)cpu_id.SPC_8bit_UID_0 << 24)
          | ((uint32_t)cpu_id.SPC_8bit_UID_1 << 16)
          | ((uint32_t)cpu_id.SPC_8bit_UID_2 <<  8)
          |  (uint32_t)cpu_id.SPC_8bit_UID_3;

    if (fw_sn != FW_SN_WILDCARD && fw_sn != my_sn)
    {
        W25Q_FW_ClearFlag();
        return 0;   /* SN mismatch */
    }

    /* 3) Verify CRC32 */
    if (W25Q_FW_CalcCRC(info.FirmwareSize) != info.CRC32)
    {
        W25Q_FW_ClearFlag();
        return 0;   /* CRC mismatch */
    }

    /* 4) All checks passed — flash from W25Q */
    W25Q_FW_FlashAndDecode(IV, Key);

    /* 5) Clear flag so Bootloader won't re-flash on next boot */
    W25Q_FW_ClearFlag();

    return 1;
}

/* ==================== Main Bootloader Monitor ==================== */

void iap_monition(void)
{
	//如果没有收到更新固件请求，等待计数++到超时跳转
	if(IGK_IAP.UpdataStart==0)
	{
			IGK_IAP.WateTime++;
	}
	else
	{
			IGK_IAP.WateTime = 0;
	}

	//升级完成或者等待复位计数器超时，跳转到用户程序
	if((IGK_IAP.UpdataOk||(IGK_IAP.WateTime>=5)))
	{
			/* ---- a) 串口升级完成 ---- */
			if(IGK_IAP.UpdataOk)
			{
				HAL_Delay(100);  // 等待最后一帧应答DMA发送完成

				if(IGK_IAP.FW_ID_Match && IGK_IAP.SN_Match)
				{
					Write_Flash_After_AES_Decode(IV,Key);
				}

				W25Q_FW_ClearFlag();  // 清除W25Q旧标志(防残留)
				System_ReStart();
			}

			/* ---- b) 串口无升级请求，检查W25Q ---- */
			if(W25Q_FW_TryUpgrade())
			{
				/* b1b) W25Q固件有效，已写入内部Flash */
				System_ReStart();
			}
			/* b1a) CRC不通过或无标志，继续跳转现有App */

			//检查用户程序有效入口码，有效跳转，无效芯片复位
			if(((*(__IO uint32_t*)(FLASH_APP1_ADDR+4))&0xFF000000)==0x08000000) //判断是否为0X08XXXXXX.
			{
					if(AES_Decode_Flag == 0)
					{
						iap_load_app(FLASH_APP1_ADDR);  //执行FLASH APP代码
					}
					if(AES_Decode_Flag == 3)
					{
						System_ReStart();
					}

			}
			else
			{
					//软复位
					System_ReStart();
			}


	}

	HAL_Delay(50);
}


