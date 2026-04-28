/**
 * bsp_w25q16.c - Blocking Quad-SPI driver for Winbond W25Q16JV
 *
 * Uses the OCTOSPI1 peripheral already initialised by MX_OCTOSPI1_Init().
 * All operations are blocking (HAL_XSPI_Command + HAL_XSPI_Transmit/Receive),
 * with hardware AutoPolling on RDSR1.BUSY for completion waits.
 *
 * Page program uses 0x32 (Quad Input Page Program, 1-1-4): instruction +
 * address on a single line, data on four lines. Requires SR-2 QE bit = 1
 * which we set in BSP_W25Q_Init() if not already.
 *
 * Read uses 0x03 (standard read, 1-1-1): simple, works at any clock <=50 MHz.
 */
#include "bsp_w25q16.h"
#include "octospi.h"

extern XSPI_HandleTypeDef hxspi1;

/* ---- W25Q opcodes ---- */
#define CMD_WRITE_ENABLE          0x06U
#define CMD_READ_STATUS_1         0x05U
#define CMD_READ_STATUS_2         0x35U
#define CMD_WRITE_STATUS_1_2      0x01U  /* 2 data bytes: SR1, SR2 */
#define CMD_READ_DATA             0x03U
#define CMD_QUAD_PAGE_PROGRAM     0x32U  /* 1-1-4 */
#define CMD_SECTOR_ERASE_4K       0x20U
#define CMD_CHIP_ERASE            0xC7U
#define CMD_JEDEC_ID              0x9FU

#define SR1_BUSY                  0x01U
#define SR2_QE                    0x02U

/* ---- timeouts (ms) ---- */
#define TMO_DEFAULT               100U
#define TMO_PAGE_PROGRAM          10U     /* tPP_max = 3 ms typical */
#define TMO_SECTOR_ERASE          500U    /* tSE_max = 400 ms */
#define TMO_CHIP_ERASE            30000U  /* tCE_max ~25 s for 2 MB */
#define TMO_WRITE_SR              50U

/* ===== low-level helpers ===== */

static W25Q_Status_t send_simple_cmd(uint8_t opcode)
{
    XSPI_RegularCmdTypeDef c = {0};
    c.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    c.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    c.Instruction        = opcode;
    c.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    c.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    c.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    c.AddressMode        = HAL_XSPI_ADDRESS_NONE;
    c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    c.DataMode           = HAL_XSPI_DATA_NONE;
    c.DummyCycles        = 0;
    c.DQSMode            = HAL_XSPI_DQS_DISABLE;
    return (HAL_XSPI_Command(&hxspi1, &c, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK)
           ? W25Q_OK : W25Q_ERR;
}

static W25Q_Status_t read_reg(uint8_t opcode, uint8_t *val)
{
    XSPI_RegularCmdTypeDef c = {0};
    c.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    c.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    c.Instruction        = opcode;
    c.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    c.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    c.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    c.AddressMode        = HAL_XSPI_ADDRESS_NONE;
    c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    c.DataMode           = HAL_XSPI_DATA_1_LINE;
    c.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    c.DataLength         = 1;
    c.DummyCycles        = 0;
    c.DQSMode            = HAL_XSPI_DQS_DISABLE;

    if (HAL_XSPI_Command(&hxspi1, &c, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) return W25Q_ERR;
    if (HAL_XSPI_Receive(&hxspi1, val, HAL_XSPI_TIMEOUT_DEFAULT_VALUE)  != HAL_OK) return W25Q_ERR;
    return W25Q_OK;
}

static W25Q_Status_t wait_busy(uint32_t timeout_ms)
{
    XSPI_RegularCmdTypeDef c = {0};
    c.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    c.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    c.Instruction        = CMD_READ_STATUS_1;
    c.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    c.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    c.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    c.AddressMode        = HAL_XSPI_ADDRESS_NONE;
    c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    c.DataMode           = HAL_XSPI_DATA_1_LINE;
    c.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    c.DataLength         = 1;
    c.DummyCycles        = 0;
    c.DQSMode            = HAL_XSPI_DQS_DISABLE;

    if (HAL_XSPI_Command(&hxspi1, &c, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) return W25Q_ERR;

    XSPI_AutoPollingTypeDef p = {0};
    p.MatchValue    = 0x00;
    p.MatchMask     = SR1_BUSY;
    p.MatchMode     = HAL_XSPI_MATCH_MODE_AND;
    p.AutomaticStop = HAL_XSPI_AUTOMATIC_STOP_ENABLE;
    p.IntervalTime  = 0x10;

    return (HAL_XSPI_AutoPolling(&hxspi1, &p, timeout_ms) == HAL_OK)
           ? W25Q_OK : W25Q_TMOUT;
}

static W25Q_Status_t write_enable(void)
{
    return send_simple_cmd(CMD_WRITE_ENABLE);
}

static W25Q_Status_t set_qe_bit(void)
{
    uint8_t sr1 = 0, sr2 = 0;
    if (read_reg(CMD_READ_STATUS_1, &sr1) != W25Q_OK) return W25Q_ERR;
    if (read_reg(CMD_READ_STATUS_2, &sr2) != W25Q_OK) return W25Q_ERR;

    if (sr2 & SR2_QE) return W25Q_OK;       /* already set */

    sr2 |= SR2_QE;

    if (write_enable() != W25Q_OK) return W25Q_ERR;

    /* 0x01 writes both SR1 and SR2 in one transaction (compatible with all W25Q16 variants) */
    XSPI_RegularCmdTypeDef c = {0};
    c.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    c.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    c.Instruction        = CMD_WRITE_STATUS_1_2;
    c.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    c.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    c.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    c.AddressMode        = HAL_XSPI_ADDRESS_NONE;
    c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    c.DataMode           = HAL_XSPI_DATA_1_LINE;
    c.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    c.DataLength         = 2;
    c.DummyCycles        = 0;
    c.DQSMode            = HAL_XSPI_DQS_DISABLE;

    if (HAL_XSPI_Command(&hxspi1, &c, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) return W25Q_ERR;

    uint8_t buf[2] = { sr1, sr2 };
    if (HAL_XSPI_Transmit(&hxspi1, buf, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) return W25Q_ERR;

    return wait_busy(TMO_WRITE_SR);
}

/* ===== public API ===== */

W25Q_Status_t BSP_W25Q_ReadJEDEC(uint32_t *id)
{
    if (id == NULL) return W25Q_PARAM;

    XSPI_RegularCmdTypeDef c = {0};
    c.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    c.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    c.Instruction        = CMD_JEDEC_ID;
    c.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    c.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    c.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    c.AddressMode        = HAL_XSPI_ADDRESS_NONE;
    c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    c.DataMode           = HAL_XSPI_DATA_1_LINE;
    c.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    c.DataLength         = 3;
    c.DummyCycles        = 0;
    c.DQSMode            = HAL_XSPI_DQS_DISABLE;

    if (HAL_XSPI_Command(&hxspi1, &c, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) return W25Q_ERR;

    uint8_t b[3] = {0};
    if (HAL_XSPI_Receive(&hxspi1, b, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) return W25Q_ERR;

    *id = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
    return W25Q_OK;
}

W25Q_Status_t BSP_W25Q_ReadStatus1(uint8_t *sr) { return read_reg(CMD_READ_STATUS_1, sr); }
W25Q_Status_t BSP_W25Q_ReadStatus2(uint8_t *sr) { return read_reg(CMD_READ_STATUS_2, sr); }

W25Q_Status_t BSP_W25Q_Init(void)
{
    uint32_t id = 0;
    if (BSP_W25Q_ReadJEDEC(&id) != W25Q_OK) return W25Q_ERR;
    if ((id & 0x00FFFFFFU) != W25Q_JEDEC_W25Q16) return W25Q_NOID;

    return set_qe_bit();
}

W25Q_Status_t BSP_W25Q_EraseSector4K(uint32_t addr)
{
    if (addr >= W25Q_FLASH_SIZE) return W25Q_PARAM;
    addr &= ~(W25Q_SECTOR_SIZE - 1U);

    if (write_enable() != W25Q_OK) return W25Q_ERR;

    XSPI_RegularCmdTypeDef c = {0};
    c.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    c.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    c.Instruction        = CMD_SECTOR_ERASE_4K;
    c.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    c.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    c.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    c.Address            = addr;
    c.AddressMode        = HAL_XSPI_ADDRESS_1_LINE;
    c.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
    c.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
    c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    c.DataMode           = HAL_XSPI_DATA_NONE;
    c.DummyCycles        = 0;
    c.DQSMode            = HAL_XSPI_DQS_DISABLE;

    if (HAL_XSPI_Command(&hxspi1, &c, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) return W25Q_ERR;

    return wait_busy(TMO_SECTOR_ERASE);
}

W25Q_Status_t BSP_W25Q_EraseChip(void)
{
    if (write_enable() != W25Q_OK) return W25Q_ERR;
    if (send_simple_cmd(CMD_CHIP_ERASE) != W25Q_OK) return W25Q_ERR;
    return wait_busy(TMO_CHIP_ERASE);
}

W25Q_Status_t BSP_W25Q_PageProgramQuad(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    if (buf == NULL || len == 0U || len > W25Q_PAGE_SIZE) return W25Q_PARAM;
    if ((addr & (W25Q_PAGE_SIZE - 1U)) + len > W25Q_PAGE_SIZE) return W25Q_PARAM;
    if (addr + len > W25Q_FLASH_SIZE) return W25Q_PARAM;

    if (write_enable() != W25Q_OK) return W25Q_ERR;

    XSPI_RegularCmdTypeDef c = {0};
    c.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    c.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    c.Instruction        = CMD_QUAD_PAGE_PROGRAM;
    c.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    c.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    c.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    c.Address            = addr;
    c.AddressMode        = HAL_XSPI_ADDRESS_1_LINE;
    c.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
    c.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
    c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    c.DataMode           = HAL_XSPI_DATA_4_LINES;       /* <-- the "Quad" part */
    c.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    c.DataLength         = len;
    c.DummyCycles        = 0;
    c.DQSMode            = HAL_XSPI_DQS_DISABLE;

    if (HAL_XSPI_Command(&hxspi1, &c, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) return W25Q_ERR;
    if (HAL_XSPI_Transmit(&hxspi1, (uint8_t *)buf, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) return W25Q_ERR;

    return wait_busy(TMO_PAGE_PROGRAM);
}

W25Q_Status_t BSP_W25Q_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    if (buf == NULL || len == 0U) return W25Q_PARAM;
    if (addr + len > W25Q_FLASH_SIZE) return W25Q_PARAM;

    while (len > 0U)
    {
        uint32_t page_off = addr & (W25Q_PAGE_SIZE - 1U);
        uint32_t chunk    = W25Q_PAGE_SIZE - page_off;
        if (chunk > len) chunk = len;

        W25Q_Status_t s = BSP_W25Q_PageProgramQuad(addr, buf, chunk);
        if (s != W25Q_OK) return s;

        addr += chunk;
        buf  += chunk;
        len  -= chunk;
    }
    return W25Q_OK;
}

W25Q_Status_t BSP_W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (buf == NULL || len == 0U) return W25Q_PARAM;
    if (addr + len > W25Q_FLASH_SIZE) return W25Q_PARAM;

    XSPI_RegularCmdTypeDef c = {0};
    c.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
    c.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
    c.Instruction        = CMD_READ_DATA;
    c.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
    c.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    c.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    c.Address            = addr;
    c.AddressMode        = HAL_XSPI_ADDRESS_1_LINE;
    c.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
    c.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
    c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
    c.DataMode           = HAL_XSPI_DATA_1_LINE;
    c.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
    c.DataLength         = len;
    c.DummyCycles        = 0;
    c.DQSMode            = HAL_XSPI_DQS_DISABLE;

    if (HAL_XSPI_Command(&hxspi1, &c, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) return W25Q_ERR;

    return (HAL_XSPI_Receive(&hxspi1, buf, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK)
           ? W25Q_OK : W25Q_ERR;
}

