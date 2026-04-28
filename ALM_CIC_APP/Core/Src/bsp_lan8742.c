#include "bsp_lan8742.h"
#include "main.h"

#define LAN8742_BCR_REG                      0x00U
#define LAN8742_BSR_REG                      0x01U
#define LAN8742_PHYID1_REG                   0x02U
#define LAN8742_PHYID2_REG                   0x03U
#define LAN8742_SCSR_REG                     0x1FU

#define LAN8742_BCR_SOFT_RESET               0x8000U
#define LAN8742_BSR_LINK_STATUS              0x0004U

#define LAN8742_SCSR_HCDSPEED_MASK           0x001CU
#define LAN8742_SCSR_HCDSPEED_10BT_HD        0x0004U
#define LAN8742_SCSR_HCDSPEED_10BT_FD        0x0014U
#define LAN8742_SCSR_HCDSPEED_100BTX_HD      0x0008U
#define LAN8742_SCSR_HCDSPEED_100BTX_FD      0x0018U

static HAL_StatusTypeDef lan8742_read_reg(BSP_LAN8742_HandleTypeDef *hphy, uint32_t reg, uint32_t *value)
{
  if ((hphy == NULL) || (hphy->heth == NULL) || (value == NULL))
  {
    return HAL_ERROR;
  }
  return HAL_ETH_ReadPHYRegister(hphy->heth, hphy->phy_addr, reg, value);
}

static HAL_StatusTypeDef lan8742_write_reg(BSP_LAN8742_HandleTypeDef *hphy, uint32_t reg, uint32_t value)
{
  if ((hphy == NULL) || (hphy->heth == NULL))
  {
    return HAL_ERROR;
  }
  return HAL_ETH_WritePHYRegister(hphy->heth, hphy->phy_addr, reg, value);
}

HAL_StatusTypeDef BSP_LAN8742_InitOrDetect(BSP_LAN8742_HandleTypeDef *hphy, ETH_HandleTypeDef *heth, uint32_t phy_addr)
{
  uint32_t id1 = 0U;
  uint32_t id2 = 0U;

  if ((hphy == NULL) || (heth == NULL))
  {
    return HAL_ERROR;
  }

  hphy->heth = heth;
  hphy->phy_addr = phy_addr;

  return BSP_LAN8742_ReadID(hphy, &id1, &id2);
}

HAL_StatusTypeDef BSP_LAN8742_Reset(BSP_LAN8742_HandleTypeDef *hphy)
{
  uint32_t bcr = 0U;
  uint32_t tickstart = 0U;
  HAL_StatusTypeDef ret;

  if (hphy == NULL)
  {
    return HAL_ERROR;
  }

  HAL_GPIO_WritePin(ETH_nRST_GPIO_Port, ETH_nRST_Pin, GPIO_PIN_RESET);
  HAL_Delay(2U);
  HAL_GPIO_WritePin(ETH_nRST_GPIO_Port, ETH_nRST_Pin, GPIO_PIN_SET);
  HAL_Delay(2U);

  ret = lan8742_write_reg(hphy, LAN8742_BCR_REG, LAN8742_BCR_SOFT_RESET);
  if (ret != HAL_OK)
  {
    return ret;
  }

  tickstart = HAL_GetTick();
  do
  {
    ret = lan8742_read_reg(hphy, LAN8742_BCR_REG, &bcr);
    if (ret != HAL_OK)
    {
      return ret;
    }
    if ((bcr & LAN8742_BCR_SOFT_RESET) == 0U)
    {
      return HAL_OK;
    }
  } while ((HAL_GetTick() - tickstart) < 100U);

  return HAL_TIMEOUT;
}

HAL_StatusTypeDef BSP_LAN8742_ReadID(BSP_LAN8742_HandleTypeDef *hphy, uint32_t *id1, uint32_t *id2)
{
  HAL_StatusTypeDef ret;

  ret = lan8742_read_reg(hphy, LAN8742_PHYID1_REG, id1);
  if (ret != HAL_OK)
  {
    return ret;
  }

  ret = lan8742_read_reg(hphy, LAN8742_PHYID2_REG, id2);
  if (ret != HAL_OK)
  {
    return ret;
  }

  if (((*id1) == 0x0000U && (*id2) == 0x0000U) || ((*id1) == 0xFFFFU && (*id2) == 0xFFFFU))
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

HAL_StatusTypeDef BSP_LAN8742_GetLinkState(BSP_LAN8742_HandleTypeDef *hphy, BSP_LAN8742_LinkStateTypeDef *link)
{
  uint32_t bsr = 0U;
  HAL_StatusTypeDef ret;

  if (link == NULL)
  {
    return HAL_ERROR;
  }

  ret = lan8742_read_reg(hphy, LAN8742_BSR_REG, &bsr);
  if (ret != HAL_OK)
  {
    return ret;
  }

  ret = lan8742_read_reg(hphy, LAN8742_BSR_REG, &bsr);
  if (ret != HAL_OK)
  {
    return ret;
  }

  *link = ((bsr & LAN8742_BSR_LINK_STATUS) != 0U) ? BSP_LAN8742_LINK_UP : BSP_LAN8742_LINK_DOWN;
  return HAL_OK;
}

HAL_StatusTypeDef BSP_LAN8742_GetSpeed(BSP_LAN8742_HandleTypeDef *hphy, BSP_LAN8742_SpeedTypeDef *speed)
{
  uint32_t scsr = 0U;
  uint32_t mode;
  HAL_StatusTypeDef ret;

  if (speed == NULL)
  {
    return HAL_ERROR;
  }

  ret = lan8742_read_reg(hphy, LAN8742_SCSR_REG, &scsr);
  if (ret != HAL_OK)
  {
    return ret;
  }

  mode = scsr & LAN8742_SCSR_HCDSPEED_MASK;
  if ((mode == LAN8742_SCSR_HCDSPEED_10BT_HD) || (mode == LAN8742_SCSR_HCDSPEED_10BT_FD))
  {
    *speed = BSP_LAN8742_SPEED_10M;
  }
  else if ((mode == LAN8742_SCSR_HCDSPEED_100BTX_HD) || (mode == LAN8742_SCSR_HCDSPEED_100BTX_FD))
  {
    *speed = BSP_LAN8742_SPEED_100M;
  }
  else
  {
    *speed = BSP_LAN8742_SPEED_UNKNOWN;
  }

  return HAL_OK;
}

HAL_StatusTypeDef BSP_LAN8742_GetDuplex(BSP_LAN8742_HandleTypeDef *hphy, BSP_LAN8742_DuplexTypeDef *duplex)
{
  uint32_t scsr = 0U;
  uint32_t mode;
  HAL_StatusTypeDef ret;

  if (duplex == NULL)
  {
    return HAL_ERROR;
  }

  ret = lan8742_read_reg(hphy, LAN8742_SCSR_REG, &scsr);
  if (ret != HAL_OK)
  {
    return ret;
  }

  mode = scsr & LAN8742_SCSR_HCDSPEED_MASK;
  if ((mode == LAN8742_SCSR_HCDSPEED_10BT_HD) || (mode == LAN8742_SCSR_HCDSPEED_100BTX_HD))
  {
    *duplex = BSP_LAN8742_DUPLEX_HALF;
  }
  else if ((mode == LAN8742_SCSR_HCDSPEED_10BT_FD) || (mode == LAN8742_SCSR_HCDSPEED_100BTX_FD))
  {
    *duplex = BSP_LAN8742_DUPLEX_FULL;
  }
  else
  {
    *duplex = BSP_LAN8742_DUPLEX_UNKNOWN;
  }

  return HAL_OK;
}

HAL_StatusTypeDef BSP_LAN8742_DumpBasicStatus(BSP_LAN8742_HandleTypeDef *hphy, BSP_LAN8742_BasicStatusTypeDef *status)
{
  HAL_StatusTypeDef ret;
  BSP_LAN8742_LinkStateTypeDef link;
  BSP_LAN8742_SpeedTypeDef speed;
  BSP_LAN8742_DuplexTypeDef duplex;

  if (status == NULL)
  {
    return HAL_ERROR;
  }

  ret = BSP_LAN8742_ReadID(hphy, &status->id1, &status->id2);
  if (ret != HAL_OK)
  {
    return ret;
  }

  ret = lan8742_read_reg(hphy, LAN8742_BSR_REG, &status->bsr);
  if (ret != HAL_OK)
  {
    return ret;
  }

  ret = lan8742_read_reg(hphy, LAN8742_BSR_REG, &status->bsr);
  if (ret != HAL_OK)
  {
    return ret;
  }

  ret = lan8742_read_reg(hphy, LAN8742_SCSR_REG, &status->scsr);
  if (ret != HAL_OK)
  {
    return ret;
  }

  ret = BSP_LAN8742_GetLinkState(hphy, &link);
  if (ret != HAL_OK)
  {
    return ret;
  }

  ret = BSP_LAN8742_GetSpeed(hphy, &speed);
  if (ret != HAL_OK)
  {
    return ret;
  }

  ret = BSP_LAN8742_GetDuplex(hphy, &duplex);
  if (ret != HAL_OK)
  {
    return ret;
  }

  status->link = link;
  status->speed = speed;
  status->duplex = duplex;

  return HAL_OK;
}

HAL_StatusTypeDef BSP_LAN8742_GetBasicStatus(BSP_LAN8742_HandleTypeDef *hphy,
                                             BSP_LAN8742_LinkStateTypeDef *link,
                                             BSP_LAN8742_SpeedTypeDef *speed,
                                             BSP_LAN8742_DuplexTypeDef *duplex)
{
  HAL_StatusTypeDef ret;

  if ((link == NULL) || (speed == NULL) || (duplex == NULL))
  {
    return HAL_ERROR;
  }

  ret = BSP_LAN8742_GetLinkState(hphy, link);
  if (ret != HAL_OK)
  {
    return ret;
  }

  if (*link == BSP_LAN8742_LINK_DOWN)
  {
    *speed = BSP_LAN8742_SPEED_UNKNOWN;
    *duplex = BSP_LAN8742_DUPLEX_UNKNOWN;
    return HAL_OK;
  }

  ret = BSP_LAN8742_GetSpeed(hphy, speed);
  if (ret != HAL_OK)
  {
    return ret;
  }

  ret = BSP_LAN8742_GetDuplex(hphy, duplex);
  if (ret != HAL_OK)
  {
    return ret;
  }

  return HAL_OK;
}
