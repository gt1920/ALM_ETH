# ALM_Motor_Bootloader — Motor Sub-Module IAP Bootloader

STM32G0B1 IAP bootloader for the Motor sub-module. Lives in the bottom 8 KB of internal flash. On boot, checks W25Qxx for a staged `.mot` file (placed there by [ALM_Motor_App](../ALM_Motor_App/) after a CAN-FD OTA), AES-decrypts and verifies it, programs the app region, then jumps to `0x08002000`.

---

## Source Index

| Path | What lives here |
|---|---|
| [Core/Src/main.c](Core/Src/main.c) | Bootloader entry — clocks, GPIO, SPI flash, calls `BL_Run()`, jumps to app |
| [BSP/iap.c](BSP/iap.c) | `BL_Run()`: read W25Qxx, AES-decrypt, validate header, erase + program internal flash, clear staged `.mot`, jump |
| [BSP/aes.c](BSP/aes.c) | AES-256-CBC — **Motor key/IV** (must stay byte-identical to [Copy_Bin_Motor](../Copy_Bin_Motor_Project/) and [ALM_Motor_App/BSP/aes.c](../ALM_Motor_App/BSP/aes.c)) |
| [BSP/W25Qxx.c](BSP/W25Qxx.c) | SPI flash driver |
| [BSP/cpu_id.c](BSP/cpu_id.c) | UID → device_sn (used to enforce SN binding when `.mot` is not Unlock) |

---

## Memory Layout

| Region | Address | Size | Content |
|---|---|---|---|
| Bootloader | `0x08000000` | **8 KB** (0x2000) | This project |
| App | `0x08002000` | 120 KB | [ALM_Motor_App](../ALM_Motor_App/) |
| RAM | `0x20000000` | 144 KB | RW + ZI |

Scatter: [Output/ALM_Motor_Bootloader.sct](Output/ALM_Motor_Bootloader.sct)
Project: [MDK-ARM/ALM_Motor_Bootloader.uvprojx](MDK-ARM/ALM_Motor_Bootloader.uvprojx)

---

## Boot Flow

```
HAL_Init() → clocks → GPIO → SPI → W25Qxx_Init()
   │
   └─ BL_Run()
        ├─ Read W25Qxx[0..15] = encrypted FW_ID header
        ├─ Decrypt (independent CBC, Motor IV copy)
        ├─ magic != 0x47544657 ("GTFW") ─────────── jump 0x08002000
        ├─ board_id != Motor BL_BOARD_ID? ───────── erase W25Q sector 0 + jump
        ├─ fw_sn != 0xA5C3F09E && != g_device_sn? ─ erase + jump (relaxed in commit fdb8913)
        ├─ CRC32(hdr[0..11]) != hdr[12..15]? ────── erase + jump
        ├─ Decrypt payload meta → filesize
        ├─ Erase app sectors, decrypt + program QUADWORDs
        ├─ Erase W25Qxx sector 0 (clear .mot magic)
        └─ jump 0x08002000
```

Bootloader is **shipped as plain `.hex`** — no Copy_Bin step. Flash via SWD once at manufacture.

---

## `.mot` Format

Same envelope as `.cic`, encrypted with Motor key/IV instead of CIC key/IV. See [Copy_Bin_Motor_Project/README.md](../Copy_Bin_Motor_Project/README.md) for byte-level layout and key/IV values.

```
[0..15]   AES-CBC encrypted FW_ID header (independent IV)
            magic(4) | board_id(4) | fw_sn(4) | crc32(4)
[16..31]  AES-CBC encrypted payload meta (chain CBC)
            userstr_len(4) | "LEDFW012"(8) | filesize(4)
[32..end] AES-CBC encrypted firmware blocks (chain continues)
```

---

## Build Output

[Output/ALM_Motor_Bootloader.hex](Output/) — flash directly via Keil/ST-Link to MCU at manufacture.

---

## Cross-References

- App counterpart: [ALM_Motor_App/README.md](../ALM_Motor_App/README.md)
- `.mot` encryption (must match key/IV here): [Copy_Bin_Motor_Project/README.md](../Copy_Bin_Motor_Project/README.md)
- Reference G0B1 bootloader: [G0B1_SPI_Bootloader/](../G0B1_SPI_Bootloader/) (skeleton this was forked from)
- Top-level: [../README.md](../README.md)
