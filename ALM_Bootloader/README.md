# ALM_Bootloader

STM32H563VG IAP bootloader. On every boot it reads external W25Q16 to see
if a `.gt` upgrade file was staged by ALM_APP; if so it decrypts, verifies
board identity + device SN, programs internal flash, then jumps to the user
app at 0x08008000.

---

## Memory Layout

### Internal Flash (1 MB)

| Range | Size | Purpose |
|---|---|---|
| `0x08000000 – 0x08007FFF` | 32 KB | **Bootloader** (this project) |
| `0x08008000 – 0x080FDFFF` | 984 KB | App code (ALM_APP) |
| `0x080FE000 – 0x080FFFFF` | 8 KB | App's `device_config` storage (last sector bank 2) |

ALM_APP scatter: `LR_IROM1 0x08008000 0x000F8000`.
`system_stm32h5xx.c` sets `VECT_TAB_OFFSET = 0x8000` in `SystemInit()`.

### External W25Q16 (2 MB)

| Range | Purpose |
|---|---|
| `0x000000` onwards | `.gt` file written by ALM_APP (Copy_Bin output, byte-for-byte) |
| `0x1FF000 – 0x1FFFFF` | Not used by bootloader anymore |

---

## Boot Flow

```
HAL_Init() → clocks → get_cpu_id() → GPIO → ICACHE → OCTOSPI → W25Q init
                          │
                          └─ populates g_device_sn from 96-bit UID
                                       │
                                   BL_Run()
                                       │
          Read W25Q[0..15] = encrypted FW_ID header
          Decrypt (independent CBC, iv = copy of global IV)
                    │
          magic != 0x47544657 ("GTFW")? ──────────────── jump 0x08008000
                    │
          board_id != BL_BOARD_ID ("ETH")?
          fw_sn != WILDCARD && fw_sn != g_device_sn?
          CRC32(hdr[0..11]) != hdr[12..15]?
                    │ any YES
          erase W25Q sector 0 ──────────────────────────jump 0x08008000
                    │ all pass
          Read W25Q[16..31] = encrypted payload block-0
          Decrypt (chain CBC, iv = fresh copy of global IV)
          extract filesize from plaintext[12..15]
                    │
          erase internal flash sectors 4..N
                    │
          for each firmware block (k=0..ceil(fw_size/16)-1):
              read W25Q[32 + k*16], decrypt (chain), write QUADWORD to flash
                    │
          erase W25Q sector 0 (clears .gt magic → no re-flash next boot)
                    │
                jump 0x08008000
```

---

## `.gt` File Format (produced by Copy_Bin, consumed directly by bootloader)

No intermediate processing required from the host PC.

```
Offset  Size  Content
  0    16 B  Encrypted FW_ID header — independent CBC (iv = global IV copy)
               Plaintext: magic(4,LE) | board_id(4,LE) | fw_sn(4,LE) | crc32_of_12B(4,LE)
 16    16 B  Encrypted payload block-0 — chain CBC (iv = global IV copy, independent chain)
               Plaintext: userstr_len(4) | "LEDFW012"(8) | filesize(4,LE)
 32     ∞    Encrypted firmware blocks — chain continues
               Plaintext: firmware[0..15], firmware[16..31], ...
               Last block: firmware tail + timestamp padding (1..16 B)
```

Total `.gt` size = `32 + filesize + paddingSize`  where `paddingSize ∈ [1, 16]`.

---

## Checks Performed by Bootloader

| Check | Condition to PASS | Failure action |
|---|---|---|
| Upgrade pending | Decrypted magic == `0x47544657` | jump to app (no erase) |
| Board identity | `board_id == 0x00485445` ("ETH") | erase W25Q sector 0 + jump |
| Device SN | `fw_sn == 0xA5C3F09E` (wildcard) OR `fw_sn == g_device_sn` | erase + jump |
| Header CRC | `CRC32(hdr[0..11]) == hdr[12..15]` | erase + jump |
| Firmware size | `0 < fw_size <= 992 KB` | erase + jump |

"Erase W25Q sector 0" clears the `.gt` magic so the bootloader does not
loop on every boot if the firmware is rejected.

---

## AES-256-CBC Crypto

Key and IV live in [BSP/aes.c](BSP/aes.c). **Both must stay in sync with
the Copy_Bin encrypter.**

### Key (32 bytes, AES-256)

ASCII: `Lp7kZ4cN9bX2vQ6mT8yJ3sD5fW0rH1aE`

```
4c 70 37 6b 5a 34 63 4e   39 62 58 32 76 51 36 6d
54 38 79 4a 33 73 44 35   66 57 30 72 48 31 61 45
```

### IV (16 bytes)

ASCII: `u3F9hM2zE6vK1oQ5`

```
75 33 46 39 68 4d 32 7a   45 36 76 4b 31 6f 51 35
```

### Two independent CBC chains in each `.gt`

```
Encrypt side (Copy_Bin):               Decrypt side (bootloader):
  iv_hdr = IV.copy()                     iv_hdr = IV.copy()
  FW_ID_hdr_cipher = Enc(iv_hdr, plainHdr)
                                         plainHdr = Dec(iv_hdr, FW_ID_hdr_cipher)

  iv_pay = IV.copy()  [fresh, ≠ iv_hdr]  iv_pay = IV.copy()  [fresh, ≠ iv_hdr]
  block_0_cipher = Enc(iv_pay, block_0)
  block_1_cipher = Enc(iv_pay, block_1)  plainBlk_0 = Dec(iv_pay, block_0_cipher)
  ...                                    plainBlk_1 = Dec(iv_pay, block_1_cipher)
```

---

## Device SN Algorithm (cpu_id.c — shared with ALM_APP, byte-identical)

```
foldLotNum = Uid1[31:8] fold XOR with Uid2[31:0] fold XOR  → 1 byte

SPC_0 = (Uid1 & 0xFF) XOR foldLotNum     ← WAF_NUM + cross-batch dispersion
SPC_1 = (Uid0 >> 16) & 0xFF
SPC_2 = (Uid0 >>  8) & 0xFF
SPC_3 =  Uid0        & 0xFF

g_device_sn = (SPC_0 << 24) | (SPC_1 << 16) | (SPC_2 << 8) | SPC_3
```

Read `g_device_sn` from the Keil Watch window when the target board is
halted in the bootloader to find its SN for binding a firmware build.

---

## Board ID & Wildcards

| Constant | Value | Meaning |
|---|---|---|
| `BL_BOARD_ID` | `0x00485445` ("ETH\0") | This board's hardware variant |
| `FW_SN_WILDCARD` | `0xA5C3F09E` | fw_sn value meaning "any MCU may install" |
| `FW_ID_MAGIC` | `0x47544657` ("GTFW") | Marks a valid .gt header |

**To bind a firmware build to a specific MCU:**
1. Flash the target MCU with any build, read `g_device_sn` from Keil Watch.
2. Set `FW_HW_ID.fw_sn = <that SN>` in [ALM_APP/BSP/fw_id.c](../ALM_APP/BSP/fw_id.c) before building.
3. Copy_Bin will read the value and embed it in the `.gt` header.
4. The bootloader will reject the `.gt` on any other MCU.

**Default (wildcard):** `FW_HW_ID.fw_sn = 0xA5C3F09E` — any MCU may install.

---

## Source Layout

```
ALM_Bootloader/
├── Core/
│   ├── Src/  main.c · gpio.c · icache.c · octospi.c
│   │         stm32h5xx_it.c · stm32h5xx_hal_msp.c · system_stm32h5xx.c
│   └── Inc/  main.h · gpio.h · icache.h · octospi.h
├── BSP/
│   ├── bsp_w25q16.{c,h}   Quad-SPI driver (HAL_XSPI indirect mode)
│   ├── aes.{c,h}           Software AES-128/192/256 + CBC
│   ├── cpu_id.{c,h}        UID→SN algorithm (byte-identical to ALM_APP)
│   └── iap.{c,h}           BL_Run() — complete bootloader logic
└── MDK-ARM/
    ├── ALM_Bootloader.uvprojx   ← open in Keil
    ├── Copy_HEX.bat             auto-copies .hex to ALM_APP/HEX/app/Bootloader/
    └── ALM_Bootloader/ALM_Bootloader.sct   IROM = 32 KB @ 0x08000000
```

---

## ALM_APP Side (OTA receive — to be implemented)

When the ALM_APP receives a new `.gt` file over the network:
1. Erase W25Q sectors covering the file size (starting at 0x000000).
2. Write `.gt` bytes to W25Q starting at 0x000000 in 256-byte pages.
3. Call `NVIC_SystemReset()`.
4. Bootloader boots, finds a valid `.gt`, flashes, erases it, jumps to new app.

**No separate "upgrade pending" flag needed.** Detecting magic in the
decrypted header is the upgrade-pending signal.

---

## Configuration Knobs ([BSP/iap.h](BSP/iap.h))

| Macro | Default | Meaning |
|---|---|---|
| `BL_BOARD_ID` | `0x00485445` ("ETH") | Board identity — firmware with different board_id is rejected |
| `BL_W25Q_GT_ADDR` | `0x000000` | W25Q start address of the `.gt` blob |
| `BL_APP_START_ADDR` | `0x08008000` | Internal flash app base (must match scatter + VECT_TAB_OFFSET) |

---

## CRC32 Compatibility

The bootloader's `bl_crc32()` (bit-by-bit, poly `0xEDB88320`) produces the
same result as:
- Copy_Bin's `Crc32()` (256-entry table, C#)
- G0B1's `CRC32Calculate()` (256-entry table, C)
