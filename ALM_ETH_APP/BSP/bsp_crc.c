/**
 * bsp_crc.c - CRC calculation routines for IAP and protocol integrity
 *
 * CRC32: IEEE 802.3 (same algorithm as ALM_Motor_Project/BSP/bsp_flash.c)
 * CRC16: CCITT (0x1021) for per-frame data integrity
 */

#include "bsp_crc.h"

/**
 * BSP_CRC32_Calc - IEEE 802.3 CRC32
 * Polynomial: 0x04C11DB7 (reflected 0xEDB88320)
 */
uint32_t BSP_CRC32_Calc(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= buf[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320UL;
            else
                crc >>= 1;
        }
    }

    return ~crc;
}

/**
 * BSP_CRC16_CCITT - CRC16 with polynomial 0x1021, init 0xFFFF
 */
uint16_t BSP_CRC16_CCITT(const uint8_t *buf, uint32_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= ((uint16_t)buf[i] << 8);
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }

    return crc;
}
