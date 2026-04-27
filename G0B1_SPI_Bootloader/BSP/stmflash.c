
#include "stmflash.h"

#include "iap.h"
#include "AES.h"

uint32_t RamSource;
uint32_t RamSource_Buf[8];
uint8_t AES_Decode_Flag;

#if	(MCU_Series==F10x)

uint16_t STMFLASH_BUF[Flash_Page_Size/2];//�����2K�ֽ�

void STMFLASH_Write(uint32_t WriteAddr,uint16_t *pBuffer,uint16_t NumToWrite)	
{	
	uint32_t secpos;	   //������ַ
	uint16_t secoff;	   //������ƫ�Ƶ�ַ(16λ�ּ���)
	uint16_t secremain; //������ʣ���ַ(16λ�ּ���)	   
 	uint16_t i;    
	uint32_t offaddr;   //ȥ��0X08000000��ĵ�ַ
	if(WriteAddr<FLASH_BASE||(WriteAddr>=(FLASH_BASE+1024*FLASH_PAGE_SIZE*FLASH_PAGE_NB)))return;//�Ƿ���ַ
	
	HAL_FLASH_Unlock();  //����
	
	offaddr=WriteAddr-FLASH_BASE;		//ʵ��ƫ�Ƶ�ַ.
	secpos=offaddr/Flash_Page_Size;			//������ַ  0~127 for STM32F103RBT6
	secoff=(offaddr%Flash_Page_Size)/2;		//�������ڵ�ƫ��(2���ֽ�Ϊ������λ.)
	secremain=Flash_Page_Size/2-secoff;		//����ʣ��ռ��С   
	
	if(NumToWrite<=secremain)secremain=NumToWrite;//�����ڸ�������Χ
	
	while(1) 
	{	
		STMFLASH_Read(secpos*Flash_Page_Size+FLASH_BASE,STMFLASH_BUF,Flash_Page_Size/2);//������������������
		for(i=0;i<secremain;i++)//У������
		{
			if(STMFLASH_BUF[secoff+i]!=0XFFFF)
				break;//��Ҫ����  	  
		}
		if(i<secremain)//��Ҫ����
		{
			//FLASH_ErasePage(secpos*Flash_Page_Size+FLASH_BASE);//�����������

			FLASH_EraseInitTypeDef My_Flash	=		{0}; 																				//���� FLASH_EraseInitTypeDef �ṹ��Ϊ My_Flash
			My_Flash.TypeErase 							= 	FLASH_TYPEERASE_PAGES; 											//����Flashִ��ҳ��ֻ����������
			My_Flash.PageAddress  					=		secpos*Flash_Page_Size+FLASH_BASE;
			My_Flash.NbPages 								= 	1;                        									//˵��Ҫ������ҳ�����˲���������Min_Data = 1��Max_Data =(���ҳ��-��ʼҳ��ֵ)֮���ֵ
							
			uint32_t 	PageError = 0;                    			//����PageError,������ִ�����������ᱻ����Ϊ������FLASH��ַ
			
			HAL_FLASHEx_Erase(&My_Flash, &PageError);  				//���ò�����������	
		
			for(i=0;i<secremain;i++)//����
			{
				STMFLASH_BUF[i+secoff]=pBuffer[i];	  
			}
			STMFLASH_Write_NoCheck(secpos*Flash_Page_Size+FLASH_BASE,STMFLASH_BUF,Flash_Page_Size/2);//д����������  
		}
		else 
		STMFLASH_Write_NoCheck(WriteAddr,pBuffer,secremain);//д�Ѿ������˵�,ֱ��д������ʣ������. 				   
		if(NumToWrite==secremain)
			break;//д�������
		else//д��δ����
		{
			secpos++;				//������ַ��1
			secoff=0;				//ƫ��λ��Ϊ0 	 
		   	pBuffer+=secremain;  	//ָ��ƫ��
			WriteAddr+=secremain;	//д��ַƫ��	   
		   	NumToWrite-=secremain;	//�ֽ�(16λ)���ݼ�
			if(NumToWrite>(Flash_Page_Size/2))secremain=Flash_Page_Size/2;//��һ����������д����
			else secremain=NumToWrite;//��һ����������д����
		}	 
	};	
	HAL_FLASH_Lock();  //����
}

//��ȡָ����ַ�İ���(16λ����)
//faddr:����ַ(�˵�ַ����Ϊ2�ı���!!)
//����ֵ:��Ӧ����.
uint16_t STMFLASH_ReadHalfWord(uint32_t faddr)
{
	return *(__IO uint16_t*)faddr; 
}

//��ָ����ַ��ʼ����ָ�����ȵ�����
//ReadAddr:��ʼ��ַ
//pBuffer:����ָ��
//NumToWrite:����(16λ)��
void STMFLASH_Read(uint32_t ReadAddr,uint16_t *pBuffer,uint16_t NumToRead)   	
{
	uint16_t i;
	for(i=0;i<NumToRead;i++)
	{
		pBuffer[i]=STMFLASH_ReadHalfWord(ReadAddr);//��ȡ2���ֽ�.
		ReadAddr+=2;//ƫ��2���ֽ�.	
	}
}

//������д��
//WriteAddr:��ʼ��ַ
//pBuffer:����ָ��
//NumToWrite:����(16λ)��   
void STMFLASH_Write_NoCheck(uint32_t WriteAddr,uint16_t *pBuffer,uint16_t NumToWrite)
{ 			 		 
	uint16_t i;
	for(i=0;i<NumToWrite;i++)
	{
		//FLASH_ProgramHalfWord(WriteAddr,pBuffer[i]);
		//#if	(MCU_Series==F10x)
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,WriteAddr,pBuffer[i]);
	  WriteAddr+=2;//��ַ����2.
	}  
}
#endif

#if	(MCU_Series==G0xx)

uint8_t get_rem_data(uint8_t data)			//�����ж�
{
	uint8_t re_data=0;
	switch (data)
	{
		case 0: re_data=0; break;
		case 1: re_data=7; break;
		case 2: re_data=6; break;
		case 3: re_data=5; break;
		case 4: re_data=4; break;
		case 5: re_data=3; break;
		case 6: re_data=2; break;
		case 7: re_data=1; break;
	}
	return re_data;
}

//��ָ����ַ��ʼд��ָ�����ȵ�����
//ReadAddr:��ʼ��ַ
//pBuffer:����ָ��
//NumToWrite:д�����ݳ���
void STMFLASH_Write_64(uint32_t WriteAddr,uint8_t*pBuffer,uint16_t NumToWrite)	
{
	uint16_t i;
	uint64_t data_write;
	uint16_t len =NumToWrite;   
	uint8_t	len_flag=0;
 	
	if(NumToWrite>FLASH_PAGE_SIZE)
	{
		return;
	}
	
	if( (WriteAddr<FLASH_BASE) ||(WriteAddr>=(FLASH_BASE+FLASH_PAGE_SIZE*FLASH_PAGE_NB)))
	{
		return;//�Ƿ���ַ
	}
	HAL_FLASH_Unlock();  //����	
	
	len_flag=len%8;
	
	len=len+get_rem_data(len_flag);								//���ݳ���8�ֽڶ����ж�
	
	if(WriteAddr%FLASH_PAGE_SIZE==0)							//д���˴�һҳ��ʼ��ַд��Ҫ����
	{			
		//STMFLASH_Erase(((WriteAddr - FLASH_BASE) / FLASH_PAGE_SIZE),1);
		
		FLASH_EraseInitTypeDef My_Flash={0}; 					//���� FLASH_EraseInitTypeDef �ṹ��Ϊ My_Flash
		My_Flash.TypeErase = FLASH_TYPEERASE_PAGES; 			//����Flashִ��ҳ��ֻ����������
		My_Flash.Banks = FLASH_BANK_1;							//G0B1双Bank，必须指定Bank1
		My_Flash.Page  	=	(WriteAddr - FLASH_BASE) / FLASH_PAGE_SIZE;
		My_Flash.NbPages = 1;                        			//˵��Ҫ������ҳ�����˲���������Min_Data = 1��Max_Data =(���ҳ��-��ʼҳ��ֵ)֮���ֵ

		uint32_t 	PageError = 0;                    			//����PageError,������ִ�����������ᱻ����Ϊ������FLASH��ַ
		HAL_FLASHEx_Erase(&My_Flash, &PageError);  				//���ò�����������	
	}
	
	for(i=0;i<len/8;i++)//����									//8�ֽ����ݶ������
	{
		uint32_t data_1=0;
		uint64_t data_2=0;
		data_1=(pBuffer[8*i+0])|(pBuffer[8*i+1]<<8)|(pBuffer[8*i+2]<<16)|(pBuffer[8*i+3]<<24); 		//
		
		data_2=(pBuffer[8*i+4])|(pBuffer[8*i+5]<<8)|(pBuffer[8*i+6]<<16)|(pBuffer[8*i+7]<<24); 		
		
		data_write= data_1| (data_2<<32);	
		
		if(	HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,WriteAddr,data_write) 	 == HAL_OK)
			WriteAddr+=8;//��ַ����2.		
	}
	
	HAL_FLASH_Lock();												//����
}
#endif


	uint8_t Temp[16];  //ԭ�������ݻ���
	uint32_t AppSize=0;  //�������Ĵ�С
	uint8_t wirteTime=0;  //һ��������д�����	
	uint16_t readTime=0,readDataCount=0;   //��ȡ�����ٽ��ܵĴ�����ÿ�ν���16���ֽڣ�
	uint32_t FlashReadAddr;
	uint32_t FlashDestination;
	
	uint32_t data_1;
	uint64_t data_2;
	uint64_t data_write;
	
//��ȡ����������������д�뵽APP
void Write_Flash_After_AES_Decode(uint8_t *IV_IN_OUT, uint8_t *key256bit)
{
	RamSource = 0;
	FlashReadAddr = AES_APP_ADDR + FW_ID_HEADER_SIZE;  // 跳过 FW_ID 密文头 (16字节: 12B 字段 + 4B CRC32, 独立 CBC 已在 TryUpgrade 阶段校验)
	FlashDestination=FLASH_APP1_ADDR;	
	
	STMFLASH_Read_Byte(FlashReadAddr,Temp,16);//��ȡͷ�ļ��������Զ�������+���������ֽڣ�
		
	Aes_IV_key256bit_Decode(IV_IN_OUT,Temp,key256bit);//�����õ��Զ�������+�ļ���С
	
	AppSize=(Temp[15]<<24)+(Temp[14]<<16)+(Temp[13]<<8)+Temp[12];
	
	//������������ȡ����
	readDataCount=AppSize/16;	

	if(AppSize%16!=0)
	{
		readDataCount+=1;
	}

	FlashReadAddr+=16;//ƫ��16���ֽڣ�ƫ�ƺ󣬵�ǰ��ַΪ������ͷ�ֽ�	

	for(readTime=0;readTime<readDataCount;readTime++)  //1Row
	{
			if( (FlashDestination<FLASH_BASE) ||(FlashDestination>=(FLASH_BASE+FLASH_PAGE_SIZE*FLASH_PAGE_NB)))
			{
				return;//�Ƿ���ַ
			}
	
			HAL_FLASH_Unlock();  //����
	
			if(FlashDestination%FLASH_PAGE_SIZE==0)
			{				
				FLASH_EraseInitTypeDef My_Flash	=	{0}; 					          //���� FLASH_EraseInitTypeDef �ṹ��Ϊ My_Flash
				My_Flash.TypeErase 							= FLASH_TYPEERASE_PAGES;  //����Flashִ��ҳ��ֻ����������
				My_Flash.Banks									= FLASH_BANK_1;           //G0B1双Bank，必须指定Bank1
				My_Flash.Page  									=	(FlashDestination - FLASH_BASE) / FLASH_PAGE_SIZE;
				My_Flash.NbPages 								= 1;                      //˵��Ҫ������ҳ�����˲���������Min_Data = 1��Max_Data =(���ҳ��-��ʼҳ��ֵ)֮���ֵ

				uint32_t 	PageError = 0;                    			        //����PageError,������ִ�����������ᱻ����Ϊ������FLASH��ַ
				HAL_FLASHEx_Erase(&My_Flash, &PageError);  				        //���ò�����������
			}
			
			STMFLASH_Read_Byte(FlashReadAddr+readTime*16,Temp,16);//��ȡ
			
			Aes_IV_key256bit_Decode(IV_IN_OUT,Temp,key256bit);//����
			
			RamSource = (uint32_t)Temp;				
							
			for(wirteTime=0;wirteTime<2;wirteTime++)
			{													
				RamSource_Buf[wirteTime] = *(uint32_t*)RamSource;
				
				#if (MCU_Series==G0xx)
				data_1 = *(uint32_t*)RamSource;
				RamSource+= 4;
				
				data_2 =  *(uint32_t*)RamSource;
				RamSource+= 4;
				
				data_write= data_1| (data_2<<32);
				
				if(	HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,FlashDestination,data_write) 	 == HAL_OK)
				
				FlashDestination += 8;
				
				#endif														
			}
			
			HAL_FLASH_Lock();    //����
	}
	
	AES_Decode_Flag = 3;	
}

//��ָ����ַ��ʼ����ָ�����ȵ�����
//ReadAddr:��ʼ��ַ
//pBuffer:����ָ��
//NumToWrite:����(16λ)��
void STMFLASH_Read_Byte(uint32_t ReadAddr,uint8_t *pBuffer,uint16_t NumToRead)   	
{
	while(NumToRead--)
	{
		*(pBuffer++) = *((__IO uint8_t*)ReadAddr++) ;
	}
}

void STMFLASH_Erase(uint32_t WriteAddr, uint8_t NumToErase)
{
	HAL_FLASH_Unlock();  //����
	
	FLASH_EraseInitTypeDef My_Flash	=		{0};																				//���� FLASH_EraseInitTypeDef �ṹ��Ϊ My_Flash
	My_Flash.TypeErase 							= 	FLASH_TYPEERASE_PAGES; 											//����Flashִ��ҳ��ֻ����������
	My_Flash.Banks									=		FLASH_BANK_1;														//G0B1双Bank，必须指定Bank

	#if (MCU_Series==F10x)
	My_Flash.PageAddress  					=		WriteAddr;
	#endif

	#if (MCU_Series==G0xx)
	My_Flash.Page  									=		WriteAddr;
	#endif

	My_Flash.NbPages 								= 	NumToErase;                        									//˵��Ҫ������ҳ�����˲���������Min_Data = 1��Max_Data =(���ҳ��-��ʼҳ��ֵ)֮���ֵ

	uint32_t 	PageError = 0;                    			//����PageError,������ִ�����������ᱻ����Ϊ������FLASH��ַ

	HAL_FLASHEx_Erase(&My_Flash, &PageError);  				//���ò�����������
	
	HAL_FLASH_Lock();  //����
}

