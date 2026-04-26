/**
 * boot_ext_flash.c - Minimal external SPI Flash driver for Bootloader
 *
 * Polling mode only, no DMA. Supports read, erase, and program
 * (needed to clear upgrade metadata after successful copy).
 */

#include "boot_ext_flash.h"
#include "octospi.h"

static HAL_StatusTypeDef send_cmd(uint8_t cmd)
{
    XSPI_RegularCmdTypeDef s = {0};
    s.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    s.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    s.Instruction        = cmd;
    s.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    s.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    s.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    s.AddressMode        = HAL_XSPI_ADDRESS_NONE;
    s.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    s.DataMode           = HAL_XSPI_DATA_NONE;
    s.DummyCycles        = 0;
    s.DQSMode            = HAL_XSPI_DQS_DISABLE;
    return HAL_XSPI_Command(&hxspi1, &s, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

static HAL_StatusTypeDef read_status(uint8_t *sr)
{
    XSPI_RegularCmdTypeDef s = {0};
    s.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    s.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    s.Instruction        = EXTFLASH_CMD_READ_STATUS;
    s.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    s.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    s.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    s.AddressMode        = HAL_XSPI_ADDRESS_NONE;
    s.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    s.DataMode           = HAL_XSPI_DATA_1_LINE;
    s.DataLength         = 1;
    s.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    s.DummyCycles        = 0;
    s.DQSMode            = HAL_XSPI_DQS_DISABLE;

    HAL_StatusTypeDef rc = HAL_XSPI_Command(&hxspi1, &s,
                                            HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK) return rc;
    return HAL_XSPI_Receive(&hxspi1, sr, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

HAL_StatusTypeDef BootFlash_Init(void)
{
    XSPI_RegularCmdTypeDef s = {0};
    uint8_t id[3];
    HAL_StatusTypeDef rc;

    HAL_Delay(1);

    s.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    s.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    s.Instruction        = EXTFLASH_CMD_READ_ID;
    s.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    s.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    s.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    s.AddressMode        = HAL_XSPI_ADDRESS_NONE;
    s.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    s.DataMode           = HAL_XSPI_DATA_1_LINE;
    s.DataLength         = 3;
    s.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    s.DummyCycles        = 0;
    s.DQSMode            = HAL_XSPI_DQS_DISABLE;

    rc = HAL_XSPI_Command(&hxspi1, &s, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK) return rc;

    rc = HAL_XSPI_Receive(&hxspi1, id, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK) return rc;

    /* Verify Macronix MX25L128: 0xC2 0x20 0x18 */
    if (id[0] != 0xC2 || id[1] != 0x20 || id[2] != 0x18)
        return HAL_ERROR;

    return HAL_OK;
}

HAL_StatusTypeDef BootFlash_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    XSPI_RegularCmdTypeDef s = {0};

    if (len == 0) return HAL_OK;

    s.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    s.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    s.Instruction        = EXTFLASH_CMD_READ;
    s.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    s.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    s.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    s.Address            = addr;
    s.AddressMode        = HAL_XSPI_ADDRESS_1_LINE;
    s.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
    s.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
    s.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    s.DataMode           = HAL_XSPI_DATA_1_LINE;
    s.DataLength         = len;
    s.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    s.DummyCycles        = 0;
    s.DQSMode            = HAL_XSPI_DQS_DISABLE;

    HAL_StatusTypeDef rc = HAL_XSPI_Command(&hxspi1, &s,
                                            HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK) return rc;
    return HAL_XSPI_Receive(&hxspi1, buf, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

HAL_StatusTypeDef BootFlash_SectorErase4K(uint32_t addr)
{
    XSPI_RegularCmdTypeDef s = {0};
    HAL_StatusTypeDef rc;

    rc = send_cmd(EXTFLASH_CMD_WRITE_ENABLE);
    if (rc != HAL_OK) return rc;

    s.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    s.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    s.Instruction        = EXTFLASH_CMD_SECTOR_ERASE_4K;
    s.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    s.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    s.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    s.Address            = addr;
    s.AddressMode        = HAL_XSPI_ADDRESS_1_LINE;
    s.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
    s.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
    s.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    s.DataMode           = HAL_XSPI_DATA_NONE;
    s.DummyCycles        = 0;
    s.DQSMode            = HAL_XSPI_DQS_DISABLE;

    rc = HAL_XSPI_Command(&hxspi1, &s, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK) return rc;
    return BootFlash_WaitReady(500);
}

HAL_StatusTypeDef BootFlash_PageProgram(uint32_t addr,
                                        const uint8_t *buf, uint32_t len)
{
    XSPI_RegularCmdTypeDef s = {0};
    HAL_StatusTypeDef rc;

    if (len == 0 || len > 256) return HAL_ERROR;

    rc = send_cmd(EXTFLASH_CMD_WRITE_ENABLE);
    if (rc != HAL_OK) return rc;

    s.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    s.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    s.Instruction        = EXTFLASH_CMD_PAGE_PROGRAM;
    s.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    s.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    s.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    s.Address            = addr;
    s.AddressMode        = HAL_XSPI_ADDRESS_1_LINE;
    s.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
    s.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
    s.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    s.DataMode           = HAL_XSPI_DATA_1_LINE;
    s.DataLength         = len;
    s.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    s.DummyCycles        = 0;
    s.DQSMode            = HAL_XSPI_DQS_DISABLE;

    rc = HAL_XSPI_Command(&hxspi1, &s, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK) return rc;

    rc = HAL_XSPI_Transmit(&hxspi1, (uint8_t *)buf,
                            HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK) return rc;
    return BootFlash_WaitReady(10);
}

HAL_StatusTypeDef BootFlash_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t sr;

    do {
        if (read_status(&sr) != HAL_OK) return HAL_ERROR;
        if ((sr & EXTFLASH_SR_WIP) == 0) return HAL_OK;
    } while ((HAL_GetTick() - start) < timeout_ms);

    return HAL_TIMEOUT;
}
