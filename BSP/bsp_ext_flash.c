/**
 * bsp_ext_flash.c - Macronix MX25L128 external SPI Flash driver via OCTOSPI1
 *
 * Uses HAL XSPI indirect mode.  All commands use standard SPI (1-line)
 * for instruction and address; data phase uses 1-line for simplicity
 * and maximum compatibility.  Quad-I/O can be enabled later if needed.
 *
 * OCTOSPI1 is already initialised by MX_OCTOSPI1_Init() in octospi.c.
 */

#include "bsp_ext_flash.h"
#include "octospi.h"
#include <string.h>

/* ---- private helpers --------------------------------------------------- */

/**
 * Send a simple 1-byte command with no address / data (e.g. WREN, WRDI).
 */
static HAL_StatusTypeDef send_command_only(uint8_t cmd)
{
    XSPI_RegularCmdTypeDef sCmd = {0};

    sCmd.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    sCmd.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    sCmd.Instruction        = cmd;
    sCmd.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    sCmd.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    sCmd.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    sCmd.AddressMode        = HAL_XSPI_ADDRESS_NONE;
    sCmd.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    sCmd.DataMode           = HAL_XSPI_DATA_NONE;
    sCmd.DummyCycles        = 0;
    sCmd.DQSMode            = HAL_XSPI_DQS_DISABLE;

    return HAL_XSPI_Command(&hxspi1, &sCmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

/**
 * Issue Write Enable (WREN) command.
 */
static HAL_StatusTypeDef write_enable(void)
{
    return send_command_only(EXTFLASH_CMD_WRITE_ENABLE);
}

/**
 * Read status register (1 byte).
 */
static HAL_StatusTypeDef read_status(uint8_t *sr)
{
    XSPI_RegularCmdTypeDef sCmd = {0};

    sCmd.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    sCmd.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    sCmd.Instruction        = EXTFLASH_CMD_READ_STATUS;
    sCmd.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    sCmd.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    sCmd.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    sCmd.AddressMode        = HAL_XSPI_ADDRESS_NONE;
    sCmd.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    sCmd.DataMode           = HAL_XSPI_DATA_1_LINE;
    sCmd.DataLength         = 1;
    sCmd.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    sCmd.DummyCycles        = 0;
    sCmd.DQSMode            = HAL_XSPI_DQS_DISABLE;

    HAL_StatusTypeDef rc = HAL_XSPI_Command(&hxspi1, &sCmd,
                                            HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK)
        return rc;

    return HAL_XSPI_Receive(&hxspi1, sr, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

/* ---- public API -------------------------------------------------------- */

/**
 * ExtFlash_Init - reset the flash and verify communication.
 * OCTOSPI1 peripheral is already initialised by CubeMX code.
 */
HAL_StatusTypeDef ExtFlash_Init(void)
{
    uint32_t id = 0;
    HAL_StatusTypeDef rc;

    /* Small delay after power-on (tVSL) */
    HAL_Delay(1);

    /* Verify we can talk to the chip */
    rc = ExtFlash_ReadID(&id);
    if (rc != HAL_OK)
        return rc;

    /* Macronix MX25L128: manufacturer 0xC2, memory type 0x20, capacity 0x18 */
    if ((id & 0x00FFFFFFU) != 0x001820C2U)
        return HAL_ERROR;

    return HAL_OK;
}

/**
 * ExtFlash_ReadID - read JEDEC ID (3 bytes: manufacturer + type + capacity).
 */
HAL_StatusTypeDef ExtFlash_ReadID(uint32_t *id)
{
    XSPI_RegularCmdTypeDef sCmd = {0};
    uint8_t buf[3] = {0};
    HAL_StatusTypeDef rc;

    sCmd.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    sCmd.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    sCmd.Instruction        = EXTFLASH_CMD_READ_ID;
    sCmd.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    sCmd.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    sCmd.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    sCmd.AddressMode        = HAL_XSPI_ADDRESS_NONE;
    sCmd.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    sCmd.DataMode           = HAL_XSPI_DATA_1_LINE;
    sCmd.DataLength         = 3;
    sCmd.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    sCmd.DummyCycles        = 0;
    sCmd.DQSMode            = HAL_XSPI_DQS_DISABLE;

    rc = HAL_XSPI_Command(&hxspi1, &sCmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK)
        return rc;

    rc = HAL_XSPI_Receive(&hxspi1, buf, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK)
        return rc;

    *id = ((uint32_t)buf[2] << 16) | ((uint32_t)buf[1] << 8) | buf[0];
    return HAL_OK;
}

/**
 * ExtFlash_Read - read data from flash (normal read, 1-1-1, no dummy).
 */
HAL_StatusTypeDef ExtFlash_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    XSPI_RegularCmdTypeDef sCmd = {0};
    HAL_StatusTypeDef rc;

    if (len == 0)
        return HAL_OK;

    sCmd.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    sCmd.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    sCmd.Instruction        = EXTFLASH_CMD_READ;
    sCmd.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    sCmd.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    sCmd.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    sCmd.Address            = addr;
    sCmd.AddressMode        = HAL_XSPI_ADDRESS_1_LINE;
    sCmd.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
    sCmd.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
    sCmd.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    sCmd.DataMode           = HAL_XSPI_DATA_1_LINE;
    sCmd.DataLength         = len;
    sCmd.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    sCmd.DummyCycles        = 0;
    sCmd.DQSMode            = HAL_XSPI_DQS_DISABLE;

    rc = HAL_XSPI_Command(&hxspi1, &sCmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK)
        return rc;

    return HAL_XSPI_Receive(&hxspi1, buf, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

/**
 * ExtFlash_PageProgram - program up to 256 bytes within one page.
 *
 * Caller must ensure (addr % 256 + len) <= 256.
 * This function issues WREN, sends the program command, then waits
 * until the internal write cycle completes.
 */
HAL_StatusTypeDef ExtFlash_PageProgram(uint32_t addr,
                                       const uint8_t *buf, uint32_t len)
{
    XSPI_RegularCmdTypeDef sCmd = {0};
    HAL_StatusTypeDef rc;

    if (len == 0 || len > EXTFLASH_PAGE_SIZE)
        return HAL_ERROR;

    rc = write_enable();
    if (rc != HAL_OK)
        return rc;

    sCmd.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    sCmd.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    sCmd.Instruction        = EXTFLASH_CMD_PAGE_PROGRAM;
    sCmd.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    sCmd.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    sCmd.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    sCmd.Address            = addr;
    sCmd.AddressMode        = HAL_XSPI_ADDRESS_1_LINE;
    sCmd.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
    sCmd.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
    sCmd.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    sCmd.DataMode           = HAL_XSPI_DATA_1_LINE;
    sCmd.DataLength         = len;
    sCmd.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    sCmd.DummyCycles        = 0;
    sCmd.DQSMode            = HAL_XSPI_DQS_DISABLE;

    rc = HAL_XSPI_Command(&hxspi1, &sCmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK)
        return rc;

    rc = HAL_XSPI_Transmit(&hxspi1, (uint8_t *)buf,
                            HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK)
        return rc;

    /* Wait for the internal program cycle to finish (typ. 0.7ms, max 3ms) */
    return ExtFlash_WaitReady(10);
}

/**
 * ExtFlash_SectorErase4K - erase a 4KB sector.
 */
HAL_StatusTypeDef ExtFlash_SectorErase4K(uint32_t addr)
{
    XSPI_RegularCmdTypeDef sCmd = {0};
    HAL_StatusTypeDef rc;

    rc = write_enable();
    if (rc != HAL_OK)
        return rc;

    sCmd.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    sCmd.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    sCmd.Instruction        = EXTFLASH_CMD_SECTOR_ERASE_4K;
    sCmd.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    sCmd.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    sCmd.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    sCmd.Address            = addr;
    sCmd.AddressMode        = HAL_XSPI_ADDRESS_1_LINE;
    sCmd.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
    sCmd.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
    sCmd.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    sCmd.DataMode           = HAL_XSPI_DATA_NONE;
    sCmd.DummyCycles        = 0;
    sCmd.DQSMode            = HAL_XSPI_DQS_DISABLE;

    rc = HAL_XSPI_Command(&hxspi1, &sCmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK)
        return rc;

    /* Sector erase typ. 36ms, max 300ms */
    return ExtFlash_WaitReady(500);
}

/**
 * ExtFlash_BlockErase64K - erase a 64KB block.
 */
HAL_StatusTypeDef ExtFlash_BlockErase64K(uint32_t addr)
{
    XSPI_RegularCmdTypeDef sCmd = {0};
    HAL_StatusTypeDef rc;

    rc = write_enable();
    if (rc != HAL_OK)
        return rc;

    sCmd.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    sCmd.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    sCmd.Instruction        = EXTFLASH_CMD_BLOCK_ERASE_64K;
    sCmd.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    sCmd.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    sCmd.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    sCmd.Address            = addr;
    sCmd.AddressMode        = HAL_XSPI_ADDRESS_1_LINE;
    sCmd.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
    sCmd.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
    sCmd.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    sCmd.DataMode           = HAL_XSPI_DATA_NONE;
    sCmd.DummyCycles        = 0;
    sCmd.DQSMode            = HAL_XSPI_DQS_DISABLE;

    rc = HAL_XSPI_Command(&hxspi1, &sCmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
    if (rc != HAL_OK)
        return rc;

    /* Block erase typ. 0.7s, max 3.5s */
    return ExtFlash_WaitReady(5000);
}

/**
 * ExtFlash_ChipErase - erase the entire chip.
 */
HAL_StatusTypeDef ExtFlash_ChipErase(void)
{
    HAL_StatusTypeDef rc;

    rc = write_enable();
    if (rc != HAL_OK)
        return rc;

    rc = send_command_only(EXTFLASH_CMD_CHIP_ERASE);
    if (rc != HAL_OK)
        return rc;

    /* Chip erase typ. 50s, max 200s */
    return ExtFlash_WaitReady(250000);
}

/**
 * ExtFlash_WaitReady - poll status register until WIP clears or timeout.
 */
HAL_StatusTypeDef ExtFlash_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t sr;
    HAL_StatusTypeDef rc;

    do
    {
        rc = read_status(&sr);
        if (rc != HAL_OK)
            return rc;

        if ((sr & EXTFLASH_SR_WIP) == 0)
            return HAL_OK;

    } while ((HAL_GetTick() - start) < timeout_ms);

    return HAL_TIMEOUT;
}
