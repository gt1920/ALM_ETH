# ALM_Motor_Bootloader — Motor Sub-Module IAP Bootloader

STM32G0B1 IAP bootloader for the Motor sub-module. Lives in the bottom 10 KB of internal flash. On boot, decrypts and verifies a staged `.mot` (written by [ALM_Motor_App](../ALM_Motor_App/) after a CAN-FD OTA), programs APP, then jumps to `0x08002800`. If APP's vector table is invalid (brick scenario), the BL itself brings up FDCAN1 and accepts a fresh OTA — see [BSP/bl_can_recovery.c](BSP/bl_can_recovery.c).

---

## Source Index

| Path | What lives here |
|---|---|
| [Core/Src/main.c](Core/Src/main.c) | Bootloader entry — clocks, calls `BL_Run()` |
| [BSP/iap.c](BSP/iap.c) | `BL_Run()`: read STAGING, AES-decrypt, validate header + payload CRC, erase + program APP, verify, clear staging, jump |
| [BSP/bl_can_recovery.c](BSP/bl_can_recovery.c) | `BL_CanRecovery_RunForever()`: brick-mode FDCAN1 listener + MUPG state machine that re-stages a `.mot` when APP is unbootable |
| [BSP/aes.c](BSP/aes.c) | AES-256-CBC — **Motor key/IV** (must stay byte-identical to [Copy_Bin_Motor](../Copy_Bin_Motor_Project/) and [ALM_Motor_App/BSP/aes.c](../ALM_Motor_App/BSP/aes.c)) |
| [BSP/motor_partition.h](BSP/motor_partition.h) | Flash partition layout (must match App's copy byte-for-byte) |

---

## Memory Layout

| Region | Address | Size | Content |
|---|---|---|---|
| Bootloader | `0x08000000` | **10 KB** (0x2800) | This project (5 pages) |
| App | `0x08002800` | 54 KB (0xD800) | [ALM_Motor_App](../ALM_Motor_App/) (27 pages) |
| Staging | `0x08010000` | 64 KB (0x10000) | Encrypted `.mot` slot (32 pages) |
| RAM | `0x20000000` | 144 KB | RW + ZI |

The BL grew from 8 KB → 10 KB to make room for the FDCAN brick-recovery
listener. APP shrank by the same one page (2 KB).

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
        └─ jump 0x08002800
```

**Brick-safety invariants:**
- Failure paths NEVER erase STAGING. Only a fully-verified flash clears it.
- A bad payload, a transient flash fault, or a power loss mid-program can all
  be retried on the next boot from the same .mot.
- `bl_jump_to_app` enters CAN brick-recovery on bad MSP/Reset vector (no
  `while(1)` and no plain reset loop), so a corrupt APP can be re-flashed
  over the wire without SWD access.

## Brick-Recovery Mode

When `BL_Run()` decides the APP region is unbootable (MSP not in SRAM, or
Reset_Handler outside APP flash), it does **not** jump and does **not** plain-
reset. It calls `BL_CanRecovery_RunForever()` which:

1. Initializes FDCAN1 with the same bit timing as the App (1 Mbit nominal /
   2 Mbit data) so the host doesn't need to reconfigure.
2. Listens on `0x300`/`0x301`/`0x302` for the standard MUPG protocol (same
   wire format as `ALM_Motor_App/BSP/motor_upgrade.c`).
3. Erases STAGING on a valid START frame and writes incoming bytes.
4. On END, sends an OK RESP and triggers `NVIC_SystemReset()`.
5. The next boot re-enters `BL_Run()`, which now sees a valid STAGING magic
   and runs the normal verify-and-program flow → APP is recovered.

**Limitations:**
- *Single-bricked-module recovery only.* The BL can't read the App's saved
  `node_id` (the App's params page lives at `0x0801F800` inside STAGING and
  may itself be wiped). Recovery accepts upgrade frames addressed to any
  node_id; if you have multiple bricked modules on the same bus, isolate or
  power them up one at a time.
- Recovery RESPs use `node_id = 0xFFFFFFFF` so the host can distinguish them
  from normal App-mode RESPs.

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
