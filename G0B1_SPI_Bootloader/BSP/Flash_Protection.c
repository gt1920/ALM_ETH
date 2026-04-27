
#include "Flash_Protection.h"

FLASH_OBProgramInitTypeDef OBConfig;

void Flash_Protection(void)
{	
	HAL_FLASHEx_OBGetConfig(&OBConfig);  // 获取当前的Option Byte配置

	OBConfig.OptionType |= OPTIONBYTE_WRP;
  OBConfig.OptionType |= OPTIONBYTE_RDP;
	
	if(OBConfig.RDPLevel != OB_RDP_LEVEL_1)  //0xBB
	{
		OBConfig.RDPLevel = OB_RDP_LEVEL_1;  // 将RDP设置为0xBB
		
		HAL_FLASH_Unlock();  								//解锁Flash
		HAL_FLASH_OB_Unlock();							//解锁OB
		
		HAL_FLASHEx_OBProgram(&OBConfig);  	// 将新的Option Byte配置写入Flash
		
		HAL_FLASH_OB_Launch();							//这句话一定要加上，否则重启之后RDP不生效
	
		HAL_FLASH_OB_Lock();								//锁OB
		HAL_FLASH_Lock();  									//锁Flash		
	}
}
