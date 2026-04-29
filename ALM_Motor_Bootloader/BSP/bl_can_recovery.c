/**
 * bl_can_recovery.c - BL brick-recovery CAN-FD listener
 *
 * Stripped down port of ALM_Motor_App/BSP/motor_upgrade.c into the
 * Bootloader. Initializes FDCAN1 directly (clock, GPIO PA11/PA12, bit
 * timing identical to the App), then runs the MUPG state machine until
 * a complete .mot has been written into STAGING. On END it triggers
 * NVIC_SystemReset; the next boot picks up STAGING via BL_Run() and
 * programs APP normally.
 *
 * Bit timing matches ALM_Motor_App/Core/Src/fdcan.c so the App's host
 * (CIC App) doesn't need to reconfigure for recovery mode.
 *
 * This file is only compiled into the Bootloader.
 */
#include "bl_can_recovery.h"
#include "main.h"
#include "motor_partition.h"
#include <string.h>

/* MUPG protocol — must match ALM_Motor_App/BSP/motor_upgrade.h */
#define CANID_UPG_START     0x300U
#define CANID_UPG_DATA      0x301U
#define CANID_UPG_END       0x302U
#define CANID_UPG_RESP      0x305U

#define UPG_OK              0x00U
#define UPG_WRONG_BOARD     0x01U
#define UPG_SIZE_ERROR      0x02U
#define UPG_WRITE_ERROR     0x03U
#define UPG_BAD_HEADER      0x04U
#define UPG_SEQ_ERROR       0x05U
#define UPG_NOT_RECEIVING   0x06U
#define UPG_FRAME_TOO_SHORT 0x07U

typedef enum {
    BL_UPG_IDLE    = 0,
    BL_UPG_RECVING = 1,
} BlUpgState_t;

static FDCAN_HandleTypeDef hfdcan;
static BlUpgState_t        s_state;
static uint32_t            s_file_size;
static uint32_t            s_next_offset;

static inline uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* ---- Flash helpers (small; duplicated from iap.c rather than exposing) ---- */

static HAL_StatusTypeDef bl_rec_erase_staging(void)
{
    HAL_StatusTypeDef rc;
    FLASH_EraseInitTypeDef e = {0};
    uint32_t pe = 0xFFFFFFFFUL;

    e.TypeErase = FLASH_TYPEERASE_PAGES;
    e.Banks     = FLASH_BANK_1;
    e.Page      = (MOT_STAGE_BASE - MOT_FLASH_BASE) / MOT_FLASH_SECTOR_SIZE;
    e.NbPages   = MOT_STAGE_SIZE / MOT_FLASH_SECTOR_SIZE;

    HAL_FLASH_Unlock();
    rc = HAL_FLASHEx_Erase(&e, &pe);
    HAL_FLASH_Lock();
    return rc;
}

static HAL_StatusTypeDef bl_rec_write_staging(uint32_t offset,
                                              const uint8_t *src,
                                              uint32_t len)
{
    HAL_StatusTypeDef rc = HAL_OK;
    uint32_t addr = MOT_STAGE_BASE + offset;
    uint32_t i;

    if ((len & 7U) != 0U) return HAL_ERROR;     /* must be doubleword aligned */

    HAL_FLASH_Unlock();
    for (i = 0U; i < len; i += 8U)
    {
        uint64_t dw;
        memcpy(&dw, &src[i], 8U);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, dw) != HAL_OK)
        {
            rc = HAL_ERROR;
            break;
        }
    }
    HAL_FLASH_Lock();
    return rc;
}

/* ---- FDCAN init ---- */

void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef *fdcanHandle)
{
    GPIO_InitTypeDef        gpio = {0};
    RCC_PeriphCLKInitTypeDef clk  = {0};

    if (fdcanHandle->Instance != FDCAN1) return;

    clk.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    clk.FdcanClockSelection  = RCC_FDCANCLKSOURCE_PCLK1;
    (void)HAL_RCCEx_PeriphCLKConfig(&clk);

    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA11=FDCAN1_RX, PA12=FDCAN1_TX, AF3 — same as App */
    gpio.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF3_FDCAN1;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static HAL_StatusTypeDef bl_rec_can_init(void)
{
    FDCAN_FilterTypeDef filter;

    hfdcan.Instance                  = FDCAN1;
    hfdcan.Init.ClockDivider         = FDCAN_CLOCK_DIV1;
    hfdcan.Init.FrameFormat          = FDCAN_FRAME_FD_BRS;
    hfdcan.Init.Mode                 = FDCAN_MODE_NORMAL;
    hfdcan.Init.AutoRetransmission   = ENABLE;
    hfdcan.Init.TransmitPause        = DISABLE;
    hfdcan.Init.ProtocolException    = DISABLE;
    /* Bit timing copied verbatim from ALM_Motor_App/Core/Src/fdcan.c so the
       host bus parameters do not change between APP-receiver and BL-receiver
       modes. PCLK1 = 64 MHz. */
    hfdcan.Init.NominalPrescaler     = 4;
    hfdcan.Init.NominalSyncJumpWidth = 4;
    hfdcan.Init.NominalTimeSeg1      = 25;
    hfdcan.Init.NominalTimeSeg2      = 6;
    hfdcan.Init.DataPrescaler        = 2;
    hfdcan.Init.DataSyncJumpWidth    = 3;
    hfdcan.Init.DataTimeSeg1         = 12;
    hfdcan.Init.DataTimeSeg2         = 3;
    hfdcan.Init.StdFiltersNbr        = 1;
    hfdcan.Init.ExtFiltersNbr        = 0;
    hfdcan.Init.TxFifoQueueMode      = FDCAN_TX_FIFO_OPERATION;

    if (HAL_FDCAN_Init(&hfdcan) != HAL_OK) return HAL_ERROR;

    /* Accept all standard IDs into RX FIFO 0 — recovery mode does its own
       filtering on CANID_UPG_*. The BL has no node_id binding (App's stored
       node_id may be wiped), so any addressed upgrade frame is honored. */
    filter.IdType       = FDCAN_STANDARD_ID;
    filter.FilterIndex  = 0;
    filter.FilterType   = FDCAN_FILTER_RANGE;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1    = 0x000;
    filter.FilterID2    = 0x7FF;
    if (HAL_FDCAN_ConfigFilter(&hfdcan, &filter) != HAL_OK) return HAL_ERROR;

    return HAL_FDCAN_Start(&hfdcan);
}

/* Translate FDCAN HAL DLC encoding back to a byte count. */
static uint8_t bl_rec_dlc_to_len(uint32_t dlc_enc)
{
    uint32_t dlc = dlc_enc >> 16;
    if (dlc <= 8U) return (uint8_t)dlc;
    switch (dlc)
    {
        case FDCAN_DLC_BYTES_12: return 12U;
        case FDCAN_DLC_BYTES_16: return 16U;
        case FDCAN_DLC_BYTES_20: return 20U;
        case FDCAN_DLC_BYTES_24: return 24U;
        case FDCAN_DLC_BYTES_32: return 32U;
        case FDCAN_DLC_BYTES_48: return 48U;
        case FDCAN_DLC_BYTES_64: return 64U;
        default:                 return 0U;
    }
}

/* Build a 12-byte RESP. Uses node_id = 0xFFFFFFFF (broadcast) since BL has
   no node_id of its own; host correlates by transaction state. */
static void bl_rec_send_resp(uint8_t status, uint32_t last_offset)
{
    FDCAN_TxHeaderTypeDef tx = {0};
    uint8_t f[12];

    f[0] = 0xFFU; f[1] = 0xFFU; f[2] = 0xFFU; f[3] = 0xFFU;
    f[4] = status;
    f[5] = 0U; f[6] = 0U; f[7] = 0U;
    f[8]  = (uint8_t)(last_offset);
    f[9]  = (uint8_t)(last_offset >> 8);
    f[10] = (uint8_t)(last_offset >> 16);
    f[11] = (uint8_t)(last_offset >> 24);

    tx.Identifier          = CANID_UPG_RESP;
    tx.IdType              = FDCAN_STANDARD_ID;
    tx.TxFrameType         = FDCAN_DATA_FRAME;
    tx.DataLength          = FDCAN_DLC_BYTES_12;
    tx.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx.BitRateSwitch       = FDCAN_BRS_ON;
    tx.FDFormat            = FDCAN_FD_CAN;
    tx.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    tx.MessageMarker       = 0;

    (void)HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan, &tx, f);
}

/* ---- MUPG handler (simplified port of motor_upgrade.c::Upgrade_OnCanFrame) ---- */

static void bl_rec_handle_frame(uint32_t id, const uint8_t *data, uint8_t len)
{
    /* Recovery mode does not enforce data[0..3] == FDCAN_NodeID — see header. */

    switch (id)
    {
    case CANID_UPG_START:
    {
        if (len < 24U) { bl_rec_send_resp(UPG_FRAME_TOO_SHORT, 0); return; }

        uint32_t file_size = rd_le32(&data[4]);

        /* file_size sanity: hdr(16) + meta(16) + crc(16) + ≥1 payload block.
           Lower bound mirrors motor_upgrade.c after the CRC-block format
           change; upper bound is the staging region size. */
        if (file_size < 64U || file_size > MOT_STAGE_SIZE)
        {
            bl_rec_send_resp(UPG_SIZE_ERROR, 0);
            return;
        }

        /* No board check here — host has to send a valid hdr; if it isn't
           valid the BL_Run() flow on the next boot will refuse to flash and
           leave APP / STAGING untouched (already brick-safe). */

        if (bl_rec_erase_staging() != HAL_OK)
        {
            bl_rec_send_resp(UPG_WRITE_ERROR, 0);
            return;
        }
        if (bl_rec_write_staging(0U, &data[8], 16U) != HAL_OK)
        {
            bl_rec_send_resp(UPG_WRITE_ERROR, 0);
            return;
        }

        s_state       = BL_UPG_RECVING;
        s_file_size   = file_size;
        s_next_offset = 16U;
        bl_rec_send_resp(UPG_OK, 16U);
        break;
    }

    case CANID_UPG_DATA:
    {
        if (s_state != BL_UPG_RECVING)
        {
            bl_rec_send_resp(UPG_NOT_RECEIVING, 0);
            return;
        }
        if (len < 12U)
        {
            bl_rec_send_resp(UPG_FRAME_TOO_SHORT, s_next_offset);
            return;
        }

        uint32_t offset    = rd_le32(&data[4]);
        uint32_t chunk_len = (uint32_t)len - 8U;

        if (offset != s_next_offset)
        {
            bl_rec_send_resp(UPG_SEQ_ERROR, s_next_offset);
            return;
        }
        if (offset + chunk_len > s_file_size)
        {
            bl_rec_send_resp(UPG_SIZE_ERROR, s_next_offset);
            return;
        }
        if ((chunk_len & 7U) != 0U)
        {
            bl_rec_send_resp(UPG_FRAME_TOO_SHORT, s_next_offset);
            return;
        }

        if (bl_rec_write_staging(offset, &data[8], chunk_len) != HAL_OK)
        {
            bl_rec_send_resp(UPG_WRITE_ERROR, s_next_offset);
            return;
        }

        s_next_offset += chunk_len;
        bl_rec_send_resp(UPG_OK, s_next_offset);
        break;
    }

    case CANID_UPG_END:
    {
        if (s_state != BL_UPG_RECVING)
        {
            bl_rec_send_resp(UPG_NOT_RECEIVING, 0);
            return;
        }
        if (len < 8U)
        {
            bl_rec_send_resp(UPG_FRAME_TOO_SHORT, s_next_offset);
            return;
        }

        uint32_t total = rd_le32(&data[4]);
        if (total != s_next_offset || total != s_file_size)
        {
            bl_rec_send_resp(UPG_SIZE_ERROR, s_next_offset);
            return;
        }

        bl_rec_send_resp(UPG_OK, s_next_offset);

        /* Give the TX FIFO a brief moment to flush the RESP onto the bus
           before we yank power on the peripheral via reset. */
        HAL_Delay(20U);
        NVIC_SystemReset();
        break;
    }

    default:
        break;
    }
}

/* ---- Entry: spin forever on the FDCAN RX FIFO ---- */

void BL_CanRecovery_RunForever(void)
{
    s_state       = BL_UPG_IDLE;
    s_file_size   = 0U;
    s_next_offset = 0U;

    if (bl_rec_can_init() != HAL_OK)
    {
        /* CAN init failed — nothing else we can do; spin so the watchdog
           (if any) or operator can react. */
        while (1) { __NOP(); }
    }

    for (;;)
    {
        FDCAN_RxHeaderTypeDef hdr;
        uint8_t buf[64];

        if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan, FDCAN_RX_FIFO0) == 0U)
            continue;

        if (HAL_FDCAN_GetRxMessage(&hfdcan, FDCAN_RX_FIFO0, &hdr, buf) != HAL_OK)
            continue;

        bl_rec_handle_frame(hdr.Identifier, buf,
                            bl_rec_dlc_to_len(hdr.DataLength));
    }
}
