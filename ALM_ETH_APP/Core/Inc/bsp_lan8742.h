#ifndef BSP_LAN8742_H
#define BSP_LAN8742_H

#ifdef __cplusplus
extern "C" {
#endif

#include "eth.h"

typedef enum
{
  BSP_LAN8742_LINK_DOWN = 0,
  BSP_LAN8742_LINK_UP,
  BSP_LAN8742_LINK_UNKNOWN
} BSP_LAN8742_LinkStateTypeDef;

typedef enum
{
  BSP_LAN8742_SPEED_10M = 0,
  BSP_LAN8742_SPEED_100M,
  BSP_LAN8742_SPEED_UNKNOWN
} BSP_LAN8742_SpeedTypeDef;

typedef enum
{
  BSP_LAN8742_DUPLEX_HALF = 0,
  BSP_LAN8742_DUPLEX_FULL,
  BSP_LAN8742_DUPLEX_UNKNOWN
} BSP_LAN8742_DuplexTypeDef;

typedef struct
{
  ETH_HandleTypeDef *heth;
  uint32_t phy_addr;
} BSP_LAN8742_HandleTypeDef;

typedef struct
{
  uint32_t id1;
  uint32_t id2;
  uint32_t bsr;
  uint32_t scsr;
  BSP_LAN8742_LinkStateTypeDef link;
  BSP_LAN8742_SpeedTypeDef speed;
  BSP_LAN8742_DuplexTypeDef duplex;
} BSP_LAN8742_BasicStatusTypeDef;

HAL_StatusTypeDef BSP_LAN8742_InitOrDetect(BSP_LAN8742_HandleTypeDef *hphy, ETH_HandleTypeDef *heth, uint32_t phy_addr);
HAL_StatusTypeDef BSP_LAN8742_Reset(BSP_LAN8742_HandleTypeDef *hphy);
HAL_StatusTypeDef BSP_LAN8742_ReadID(BSP_LAN8742_HandleTypeDef *hphy, uint32_t *id1, uint32_t *id2);
HAL_StatusTypeDef BSP_LAN8742_GetLinkState(BSP_LAN8742_HandleTypeDef *hphy, BSP_LAN8742_LinkStateTypeDef *link);
HAL_StatusTypeDef BSP_LAN8742_GetSpeed(BSP_LAN8742_HandleTypeDef *hphy, BSP_LAN8742_SpeedTypeDef *speed);
HAL_StatusTypeDef BSP_LAN8742_GetDuplex(BSP_LAN8742_HandleTypeDef *hphy, BSP_LAN8742_DuplexTypeDef *duplex);
HAL_StatusTypeDef BSP_LAN8742_DumpBasicStatus(BSP_LAN8742_HandleTypeDef *hphy, BSP_LAN8742_BasicStatusTypeDef *status);
HAL_StatusTypeDef BSP_LAN8742_GetBasicStatus(BSP_LAN8742_HandleTypeDef *hphy,
                                             BSP_LAN8742_LinkStateTypeDef *link,
                                             BSP_LAN8742_SpeedTypeDef *speed,
                                             BSP_LAN8742_DuplexTypeDef *duplex);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LAN8742_H */
