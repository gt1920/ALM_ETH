# G0B1_SPI_Bootloader — Reference G0B1 Bootloader

Reference / template bootloader for STM32G0B1 chips. Generic UART + SPI flash IAP skeleton — **not used in the active build chain**. Kept here as the source [ALM_Motor_Bootloader](../ALM_Motor_Bootloader/) was forked from.

---

## Source Index

| Path | What lives here |
|---|---|
| [Core/Src/main.c](Core/Src/main.c) | Entry, peripheral init |
| [BSP/iap.c](BSP/iap.c) | IAP routines: erase, write, CRC32 |
| [BSP/AES.c](BSP/AES.c) | AES-256 reference implementation |
| [BSP/W25Qxx.c](BSP/W25Qxx.c) | SPI flash driver |
| [BSP/CPU_ID.c](BSP/CPU_ID.c) | UID retrieval (predecessor of the SN algorithm now in `cpu_id.c`) |
| [BSP/Usart_Func.c](BSP/Usart_Func.c) | UART bootstrap / upload (115200 baud) |
| [BSP/stmflash.c](BSP/stmflash.c) | STM internal flash erase/write |

---

## Memory Layout

| Region | Address | Size |
|---|---|---|
| Bootloader | `0x08000000` | (see [Output/G0B1_Bootloader_115200.sct](Output/)) |
| App | (board-specific) | (board-specific) |

---

## Project Files

- **CubeMX**: [B87_Boot.ioc](B87_Boot.ioc)
- **Keil**: [MDK-ARM/B87_Boot.uvprojx](MDK-ARM/B87_Boot.uvprojx)
- **Build script**: [Copy_HEX.bat](Copy_HEX.bat)
- **Output**: [Output/](Output/) and [HEX/](HEX/)

---

## Status

**Reference only** — no active development. Kept here for:
- Comparing against the active [ALM_Motor_Bootloader](../ALM_Motor_Bootloader/) for divergence
- Re-using the UART bootstrap path if a board ever needs UART (the active Motor bootloader uses SPI flash + CAN-FD, not UART)

If you need the canonical Motor bootloader, use [ALM_Motor_Bootloader/](../ALM_Motor_Bootloader/) instead.

---

## Cross-References

- Active Motor bootloader (forked from this): [ALM_Motor_Bootloader/README.md](../ALM_Motor_Bootloader/README.md)
- Top-level: [../README.md](../README.md)
