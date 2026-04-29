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

## Boot Flow (brick-safe)

```
HAL_Init() → clocks → BL_Run()
   │
   └─ BL_Run()
        ├─ Decrypt staging[0..15] = FW_ID header (independent CBC)
        ├─ magic != "GTFW"     ─────────────────── jump APP (staging intact)
        ├─ board != "MOT" / hdr CRC bad ─────────── jump APP (staging intact)
        ├─ Decrypt staging[16..31]: filesize
        ├─ filesize == 0 || > APP region ────────── jump APP (staging intact)
        ├─ Decrypt staging[32..47]: crc_magic, fw_crc32
        ├─ crc_magic != "MOTC" (pre-CRC .mot) ───── jump APP (staging intact)
        ├─ PASS 1: decrypt staging[48..end], compute plaintext CRC32
        │     mismatch ──────────────────────────── jump APP (staging intact)
        ├─ Erase APP region
        │     erase fail ────────────────────────── NVIC_SystemReset
        ├─ Decrypt + program APP (DOUBLEWORD)
        │     program fail ──────────────────────── NVIC_SystemReset
        ├─ PASS 2: re-read APP, compute CRC32
        │     mismatch ──────────────────────────── NVIC_SystemReset
        ├─ Erase STAGING (clears .mot magic)
        └─ jump 0x08002000
```

**Brick-safety invariants:**
- Failure paths NEVER erase STAGING. Only a fully-verified flash clears it.
- A bad payload, a transient flash fault, or a power loss mid-program can all
  be retried on the next boot from the same .mot.
- `bl_jump_to_app` resets the MCU on bad MSP/reset vector (no `while(1)`),
  so a partially-written APP loops back through BL instead of dead-locking.

Bootloader is **shipped as plain `.hex`** — no Copy_Bin step. Flash via SWD once at manufacture.

> **Format change**: this BL refuses pre-CRC `.mot` files. Rebuild firmware
> with the matching `Copy_Bin_Motor_Project` (CRC block writer) before OTA.

---

## `.mot` Format

Encrypted with Motor key/IV. See [Copy_Bin_Motor_Project/README.md](../Copy_Bin_Motor_Project/README.md) for byte-level layout and key/IV values.

```
[0..15]   AES-CBC encrypted FW_ID header (independent IV)
            magic(4) | board_id(4) | fw_sn(4) | crc32(4)
[16..31]  AES-CBC encrypted metadata block-0 (chain CBC starts)
            userstr_len(4) | "LEDFW012"(8) | filesize(4)
[32..47]  AES-CBC encrypted CRC block (chain continues)
            crc_magic(4,"MOTC") | fw_crc32(4) | reserved(8)
[48..end] AES-CBC encrypted firmware blocks (chain continues)
```

`fw_crc32` covers the plaintext APP `.bin` (`filesize` bytes, no padding).
BL verifies it before erasing APP, and re-verifies after programming.

---

## Build Output

[Output/ALM_Motor_Bootloader.hex](Output/) — flash directly via Keil/ST-Link to MCU at manufacture.

---

## Cross-References

- App counterpart: [ALM_Motor_App/README.md](../ALM_Motor_App/README.md)
- `.mot` encryption (must match key/IV here): [Copy_Bin_Motor_Project/README.md](../Copy_Bin_Motor_Project/README.md)
- Reference G0B1 bootloader: [G0B1_SPI_Bootloader/](../G0B1_SPI_Bootloader/) (skeleton this was forked from)
- Top-level: [../README.md](../README.md)
