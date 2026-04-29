# Copy_Bin_CIC_Project — `.cic` Encryption Tool

C# / .NET 8.0 console post-build tool. Reads [ALM_CIC_APP](../ALM_CIC_APP/)'s `.bin` output, AES-256-CBC encrypts it into the `.cic` upgrade package, and merges Bootloader+App `.hex` files for SWD-flashing at manufacture.

Invoked automatically by `ALM_CIC_APP/MDK-ARM/After_Build.bat` after every Keil build.

---

## Source Index

| Path | What lives here |
|---|---|
| [Copy_Bin/Program.cs](Copy_Bin/Program.cs) | Main: reads `.bin`, builds FW_ID header, AES encrypts, writes `.cic`, copies & merges hex |
| [Copy_Bin/AesCustom.cs](Copy_Bin/AesCustom.cs) | AES-256-CBC implementation (must stay byte-identical to bootloader's `aes.c`) |
| [Copy_Bin.sln](Copy_Bin.sln), `Copy_Bin/Copy_Bin.csproj` | Project files (TargetFramework: `net8.0-windows7.0`) |

---

## AES Configuration (CIC)

> **Must stay in sync** with [ALM_CIC_Bootloader/BSP/aes.c](../ALM_CIC_Bootloader/BSP/aes.c) and [ALM_CIC_APP/BSP/aes.c](../ALM_CIC_APP/BSP/aes.c).

**Key (32 B, AES-256)** — ASCII `Lp7kZ4cN9bX2vQ6mT8yJ3sD5fW0rH1aE`:
```
4c 70 37 6b 5a 34 63 4e  39 62 58 32 76 51 36 6d
54 38 79 4a 33 73 44 35  66 57 30 72 48 31 61 45
```

**IV (16 B)** — ASCII `u3F9hM2zE6vK1oQ5`:
```
75 33 46 39 68 4d 32 7a  45 36 76 4b 31 6f 51 35
```

Two independent CBC chains per `.cic` (header chain + payload chain) — see [ALM_CIC_Bootloader/README.md](../ALM_CIC_Bootloader/README.md) for the chain split.

---

## `.cic` File Layout

```
[0..15]   AES-CBC encrypted FW_ID header (independent CBC, IV = global IV copy)
            plaintext: magic(4) | board_id(4) | fw_sn(4) | crc32(4)
[16..31]  AES-CBC encrypted payload meta (fresh chain, IV = global IV copy)
            plaintext: userstr_len(4) | "LEDFW012"(8) | filesize(4)
[32..end] AES-CBC encrypted firmware blocks (chain continues)
            last block: tail + 1..16 B timestamp padding
```

Total `.cic` size = `32 + filesize + paddingSize`.

---

## Constants

| Name | Value |
|---|---|
| `FW_ID_MAGIC` | `0x47544657` ("GTFW") |
| CIC `board_id` | `0x00485445` ("ETH\0") |
| `FW_SN_WILDCARD` | `0xA5C3F09E` |
| `USERSTR` | `"LEDFW012"` |

---

## Output Naming

```
HEX/<basename>_<SN-tag>_<yyMMdd>_<HHmm>.cic
```
- `<SN-tag>` = `Unlock` if `fw_sn == 0xA5C3F09E`, otherwise the hex SN
- Examples: `ALM_ETH_Unlock_260429_1043.cic`

The script also copies the `.bin` to `HEX/app/app.bin` and runs `Run.bat` to merge Bootloader + App hex into a single programming file.

---

## Build / Run

```powershell
dotnet build Copy_Bin.sln -c Release
# binary: Copy_Bin/bin/Release/net8.0-windows7.0/Copy_Bin.exe
```

Invocation pattern (from `After_Build.bat`):
```
Copy_Bin.exe <bin_path> <out_dir> <name>
```

---

## Sister Tool

The Motor module has a parallel encryption pipeline with **distinct AES key/IV**:
[Copy_Bin_Motor_Project/README.md](../Copy_Bin_Motor_Project/README.md).

---

## Cross-References

- Decryption side / boot consumer: [ALM_CIC_Bootloader/README.md](../ALM_CIC_Bootloader/README.md)
- Pre-build firmware (input `.bin`): [ALM_CIC_APP/](../ALM_CIC_APP/)
- Top-level: [../README.md](../README.md)
