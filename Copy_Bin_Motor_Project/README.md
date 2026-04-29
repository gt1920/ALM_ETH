# Copy_Bin_Motor_Project — `.mot` Encryption Tool

C# / .NET 8.0 console post-build tool. Reads [ALM_Motor_App](../ALM_Motor_App/)'s `.bin` output and AES-256-CBC encrypts it into the `.mot` upgrade package consumed by [ALM_Motor_Bootloader](../ALM_Motor_Bootloader/).

Same envelope/algorithm as `.cic` — but **different key and IV** (Motor key, not CIC key). Invoked from `ALM_Motor_App/MDK-ARM/After_Build.bat`.

---

## Source Index

| Path | What lives here |
|---|---|
| [Copy_Bin/Program.cs](Copy_Bin/Program.cs) | Main: reads `.bin`, extracts `FW_HW_ID`, encrypts, writes `.mot` (no hex merge — Motor bootloader is plain hex) |
| [Copy_Bin/AesCustom.cs](Copy_Bin/AesCustom.cs) | AES-256-CBC (must stay byte-identical to Motor's `aes.c`) |
| [Copy_Bin.sln](Copy_Bin.sln) | .NET 8.0 |

---

## AES Configuration (Motor — DIFFERENT FROM CIC)

> **Must stay in sync** with [ALM_Motor_Bootloader/BSP/aes.c](../ALM_Motor_Bootloader/BSP/aes.c) and [ALM_Motor_App/BSP/aes.c](../ALM_Motor_App/BSP/aes.c).

**Key (32 B, AES-256)** — first bytes `0xD1 0x16 0x0B 0xF7 ...` (random, distinct from CIC).
**IV (16 B)** — first bytes `0xCD 0x11 0xC5 0xE4 ...`.

Authoritative bytes live in `Copy_Bin/Program.cs` and the matching firmware `aes.c`. **Never reuse the CIC key** — the bootloaders distinguish modules by key.

---

## `.mot` Layout

```
[0..15]   AES-CBC encrypted FW_ID header (independent CBC)
            magic(4) | board_id(4) | fw_sn(4) | crc32(4)
[16..31]  AES-CBC encrypted metadata block-0 (chain CBC starts)
            userstr_len(4) | "LEDFW012"(8) | filesize(4)
[32..47]  AES-CBC encrypted CRC block (chain continues)
            crc_magic(4,"MOTC"=0x4D4F5443) | fw_crc32(4) | reserved(8)
[48..end] AES-CBC encrypted firmware blocks (chain continues)
```

`fw_crc32` is the CRC32 (poly 0xEDB88320, init 0xFFFFFFFF, final XOR
0xFFFFFFFF) of the **plaintext** APP `.bin`, exactly `filesize` bytes.
The Motor bootloader verifies it twice: before erasing APP, and after
programming completes. This makes the OTA path brick-safe.

> **Format change**: this CRC block is required by the new
> ALM_Motor_Bootloader. Older `.mot` files (no CRC block) will be rejected
> by new BLs. Older BLs cannot consume new `.mot` files. Flash the new BL
> via SWD before pushing the first new-format `.mot`.

---

## Constants

| Name | Value |
|---|---|
| `FW_ID_MAGIC` | `0x47544657` ("GTFW") |
| Motor `board_id` | (defined in `Program.cs` + Motor `iap.h`) |
| `FW_SN_WILDCARD` | `0xA5C3F09E` |
| `USERSTR` | `"LEDFW012"` |

---

## Output Naming

```
HEX/<basename>_<SN-tag>_<yyMMdd>_<HHmm>.mot
```
- `<SN-tag>` = `Unlock` if `fw_sn == 0xA5C3F09E`, else hex SN
- Examples: `ALM_Motor_App_Unlock_260429_1046.mot`, `v19.mot`

**No bootloader merge** — Motor bootloader is flashed once at manufacture; OTA only updates the app region.

---

## Build / Run

```powershell
dotnet build Copy_Bin.sln -c Release
```

Invoked from `After_Build.bat` after each Motor App Keil build.

---

## Sister Tool

CIC firmware has a parallel pipeline with the **CIC key** (different bytes):
[Copy_Bin_CIC_Project/README.md](../Copy_Bin_CIC_Project/README.md).

---

## Cross-References

- Decryption side: [ALM_Motor_Bootloader/README.md](../ALM_Motor_Bootloader/README.md)
- Source firmware: [ALM_Motor_App/README.md](../ALM_Motor_App/README.md)
- OTA relay (PC → CIC App → CAN → Motor): [ALM_CIC_APP/BSP/module_upgrade.c](../ALM_CIC_APP/BSP/module_upgrade.c)
- Top-level: [../README.md](../README.md)
