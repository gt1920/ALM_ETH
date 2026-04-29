# ALM ETH — No Router (Project Collection Root)

STM32H563 Ethernet laser-driver controller + STM32G0B1 motor sub-module + PC GUI + encryption tools. Branch **Build_145** | Date 2026-04-29.

This README is the **global index**. Each subproject has its own `README.md` covering the details — open those for source layout, addresses, AES keys, and protocol specifics.

---

## Subproject Index

| Folder | Role | Target | Index |
|---|---|---|---|
| [ALM_CIC_APP/](ALM_CIC_APP/) | Main controller firmware (TCP/UDP, OTA orchestrator) | STM32H563, Keil | [README](ALM_CIC_APP/README.md) |
| [ALM_CIC_Bootloader/](ALM_CIC_Bootloader/) | IAP bootloader — applies `.cic` from W25Q16 | STM32H563, Keil | [README](ALM_CIC_Bootloader/README.md) |
| [ALM_Motor_App/](ALM_Motor_App/) | Motor adjuster firmware (CAN-FD only) | STM32G0B1, Keil | [README](ALM_Motor_App/README.md) |
| [ALM_Motor_Bootloader/](ALM_Motor_Bootloader/) | Motor IAP bootloader — applies `.mot` from W25Qxx | STM32G0B1, Keil | [README](ALM_Motor_Bootloader/README.md) |
| [ALM_USB_Project/](ALM_USB_Project/) | USB-HID ↔ CAN bridge variant (alternate host link) | STM32G0B1, Keil | [README](ALM_USB_Project/README.md) |
| [Copy_Bin_CIC_Project/](Copy_Bin_CIC_Project/) | `.bin` → `.cic` AES-256 encryption tool | .NET 8.0 | [README](Copy_Bin_CIC_Project/README.md) |
| [Copy_Bin_Motor_Project/](Copy_Bin_Motor_Project/) | `.bin` → `.mot` AES-256 encryption tool (Motor key) | .NET 8.0 | [README](Copy_Bin_Motor_Project/README.md) |
| [DT_Controller_Project/](DT_Controller_Project/) | PC GUI (TCP + HID) | WinForms, .NET 4.8 | [README](DT_Controller_Project/README.md) |
| [G0B1_SPI_Bootloader/](G0B1_SPI_Bootloader/) | Reference G0B1 bootloader (not in active chain) | STM32G0B1, Keil | [README](G0B1_SPI_Bootloader/README.md) |

Additional assets:
- [DT Controller.exe.lnk](DT%20Controller.exe.lnk) — shortcut to the active PC GUI build
- Companion repo (DT Controller source/build): `x:/GT_IAP_Done/DT_Controller/DT_Controller_V1.0/DT_Controller_260402_1354/DT Controller`

---

## System Architecture

```
                                   ┌──────────────────────────┐
                                   │  PC — DT Controller      │
                                   │  (WinForms .NET 4.8)     │
                                   └────────┬─────────────────┘
                                            │
                          ┌─────────────────┴────────────────┐
                          │                                  │
                    TCP 40000                          USB-HID (0x0483:0xA7D1)
                  UDP 40001 (DTDR)                            │
                          │                                  │
              ┌───────────▼──────────┐               ┌───────▼──────────┐
              │   ALM_CIC_APP        │               │ ALM_USB_Project  │
              │   STM32H563 (ETH)    │               │ STM32G0B1 (HID)  │
              │   @ 0x08008000       │               │  reference / alt │
              └───────────┬──────────┘               └────────┬─────────┘
                          │                                  │
                ALM_CIC_Bootloader                            │
                  @ 0x08000000                                │
                          │                                  │
                          └──────────────┬───────────────────┘
                                         │
                                  CAN-FD (motion + MUPG OTA relay)
                                         │
                          ┌──────────────▼─────────────┐
                          │      ALM_Motor_App         │
                          │      STM32G0B1             │
                          │      @ 0x08002000          │
                          └──────────────┬─────────────┘
                                         │
                              ALM_Motor_Bootloader
                                @ 0x08000000 (8 KB)
```

---

## Flash Layout — STM32H563 (CIC)

| Region | Address | Size | Content |
|---|---|---|---|
| Bootloader | `0x08000000` | 32 KB | `ALM_CIC_Bootloader` |
| Application | `0x08008000` | 984 KB | `ALM_CIC_APP` |
| Device config | `0x080FE000` | 8 KB | Persisted device name (last sector bank 2) |

W25Q16 (2 MB SPI flash) at `0x000000` = staged `.cic` upgrade (cleared by bootloader after apply).

## Flash Layout — STM32G0B1 (Motor)

| Region | Address | Size | Content |
|---|---|---|---|
| Bootloader | `0x08000000` | 8 KB | `ALM_Motor_Bootloader` |
| Application | `0x08002000` | 120 KB | `ALM_Motor_App` |

Motor uses an external W25Qxx for staged `.mot` upgrades over CAN-FD MUPG.

---

## Encryption Domains — Two Distinct AES-256-CBC Keys

| Pipeline | Key/IV | Tool | Bootloader |
|---|---|---|---|
| `.cic` (CIC App) | "Lp7kZ4cN9b…" / "u3F9hM2zE6vK1oQ5" | [Copy_Bin_CIC](Copy_Bin_CIC_Project/) | [ALM_CIC_Bootloader](ALM_CIC_Bootloader/) |
| `.mot` (Motor App) | random bytes (D1 16 0B F7… / CD 11 C5 E4…) | [Copy_Bin_Motor](Copy_Bin_Motor_Project/) | [ALM_Motor_Bootloader](ALM_Motor_Bootloader/) |

Both share the same envelope (16 B FW_ID hdr + 16 B payload meta + chain-CBC firmware). Magic `0x47544657` ("GTFW"), userstr `"LEDFW012"`, wildcard SN `0xA5C3F09E`.

---

## OTA Pipelines

### CIC self-OTA (`.cic`)
```
PC → TCP CMD_UPGRADE START (file_size + cic[0..15] for device-side validation)
       Device: AES-decrypt header, check magic/board/sn/CRC → reject or erase W25Q
   → DATA (128 B chunks, per-packet ACK)
   → END (byte-count check) → ACK → UPG_PollReboot() → NVIC_SystemReset()
   → Bootloader applies update on next boot
```

### Motor module OTA (`.mot`) — relayed
```
PC → TCP CMD_MODULE_UPGRADE → CIC App relay (module_upgrade.c)
   → CAN-FD MUPG START / DATA / END → Motor App stages to W25Qxx
   → Motor reboots → Motor Bootloader decrypts + flashes app region
```

---

## Versioning

`FW_HW_VER = FW_BUILD_NUMBER` — auto-incremented by `MDK-ARM/Inc_Build.bat` on every Keil build (file `BSP/fw_build_number.h`, do not hand-edit). Reported in `CMD_INFO` and UDP discovery.

---

## Recent Work (Build_142 → Build_145)

Pulled from `git log`. Authoritative source: `git log` itself.

| Commit | Change |
|---|---|
| `ef8841b` | Fix OTA END frame loss: `CAN_Encode_DLC` double-shift on ≤8 B payloads |
| `b626f8a` | OTA: retransmit END frame on timeout + Motor handles duplicate END |
| `c4ae801` | DT Controller: full UI refresh after both `.cic` and `.mot` upgrades |
| `ec839ad` | PC auto-refresh after Module OTA + Motor LED blink during OTA |
| `03a0017` / `c15129a` | Expose `g_upg`; add OTA tx/rx debug counters (CIC + Motor) |
| `f2b9f62` | DT Controller: stop `GET_MODULE_INFO` spam after Module click |
| `d85c3a3` | Rename `ALM_APP → ALM_CIC_APP`, `ALM_Bootloader → ALM_CIC_Bootloader`, `Copy_Bin_Project → Copy_Bin_CIC_Project` |
| `904d59b` | MUPG protocol fix: don't pre-write hdr to W25Q in START |
| `acc749c` | Rename `ALM_Motor_Project → ALM_Motor_App`; auto-increment FW |
| `M1`–`M6` (4e982bd..ec10225) | Motor partition + bootloader skeleton + Copy_Bin_Motor + CAN-FD upgrade receiver + relax fw_sn check + device-side relay + DT Controller right-click upgrade |
| `581364b` | Align Motor SN with CIC UID→SN algorithm (byte-identical) |
| `f7ac802` | Rename encrypted firmware extension `.alm → .cic` |

---

## Build / Flash Workflow

| Step | Tool / Where |
|---|---|
| Build CIC App | Keil → `ALM_CIC_APP/MDK-ARM/ALM_ETH.uvprojx` (auto-runs `Copy_Bin_CIC` post-build) |
| Build CIC Bootloader | Keil → `ALM_CIC_Bootloader/MDK-ARM/ALM_CIC_Bootloader.uvprojx` |
| Build Motor App | Keil → `ALM_Motor_App/MDK-ARM/ALM_Motor.uvprojx` (auto-runs `Copy_Bin_Motor` post-build) |
| Build Motor Bootloader | Keil → `ALM_Motor_Bootloader/MDK-ARM/ALM_Motor_Bootloader.uvprojx` |
| Build PC GUI | VS → `DT_Controller_Project/DT Controller/DT Controller.csproj` |
| Initial factory flash (CIC) | Merged hex from `ALM_CIC_APP/HEX/app/` via SWD |
| Initial factory flash (Motor) | Bootloader + App hex separately via SWD |
| OTA update (CIC) | DT Controller → right-click ETH device → Upgrade → `.cic` |
| OTA update (Motor) | DT Controller → right-click Module → Upgrade → `.mot` |

---

## Memory Aid — Directory README Quick-Look

> When opening this project in a future session, start by reading the relevant subproject README rather than the source. Each one is structured as `Source Index → Memory & Build → Protocol/Format → Cross-References`.
