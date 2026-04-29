# ALM_Motor_App — Motor Sub-Module Firmware

STM32G0B1 motor adjuster firmware (Keil MDK-ARM). Runs above [ALM_Motor_Bootloader](../ALM_Motor_Bootloader/) at `0x08002800` (App region was 56 KB, now 54 KB after BL grew by one page for brick-recovery support). Talks **only** CAN-FD — no TCP, no USB. The CIC App ([../ALM_CIC_APP](../ALM_CIC_APP/)) is its sole host.

---

## Source Index

| Path | What lives here |
|---|---|
| [Core/Src/main.c](Core/Src/main.c) | MCU init, timer, FDCAN setup |
| [Core/Src/adjuster_app.c](Core/Src/adjuster_app.c) | Top-level state machine; ties motion / params / identify / autorun together |
| [Core/Src/adjuster_motion.c](Core/Src/adjuster_motion.c) | Move, limit detection, `GotoFactory` position |
| [Core/Src/adjuster_can.c](Core/Src/adjuster_can.c) | CAN rx dispatch (`AdjusterCAN_OnRx`) + status / heartbeat / param-report tx |
| [Core/Src/adjuster_identify.c](Core/Src/adjuster_identify.c) | Auto-calibration routine |
| [Core/Src/adjuster_autorun.c](Core/Src/adjuster_autorun.c) | Periodic movement sequence |
| [Core/Src/adjuster_params.c](Core/Src/adjuster_params.c) | Param persistence: name, current, frequency, accel/decel, factory pos/date |
| [Core/Inc/adjuster_types.h](Core/Inc/adjuster_types.h) | `Axis` / `MotionMode` enums |
| [BSP/aes.c](BSP/aes.c) | AES-256-CBC — **Motor key/IV (distinct from CIC key)**; see Copy_Bin_Motor for byte values |
| [BSP/fw_id.c](BSP/fw_id.c) | `FW_HW_ID` struct (magic `0x47544657 "GTFW"`, board_id, fw_sn, crc32) |
| [BSP/cpu_id.c](BSP/cpu_id.c) | UID → SN (same algorithm as CIC) |
| [BSP/upgrade_handler.c](BSP/upgrade_handler.c) | CAN-FD MUPG receiver — stages `.mot` to W25Qxx, reboots into bootloader |
| `readme.txt` | Original Chinese API summary (11+2 functions) |

## Public API (from `readme.txt`)

```
Params:    AdjusterParams_Set{Name,Current,Frequency,AccelDecel,FactoryPos,FactoryDate}
Motion:    AdjusterMotion_{Start, Stop, WouldHitLimit, GotoFactory}
Identify:  AdjusterIdentify_{Start, Stop, IsActive}
AutoRun:   AdjusterAutoRun_{Start, Stop, IsActive}
CAN:       AdjusterCAN_{OnRx, TxStatus, TxHeartbeat, TxParamReport}
```

The only CAN entry point is `AdjusterCAN_OnRx(id, data, len)` from the FDCAN rx callback.

---

## Memory & Build

- **Scatter**: `Output/ALM_Motor_App.sct` — App at `0x08002800`, IROM 54 KB (0xD800). Bootloader sits in `0x08000000–0x080027FF` (10 KB).
- **Project**: [MDK-ARM/ALM_Motor.uvprojx](MDK-ARM/ALM_Motor.uvprojx)
- **CubeMX**: [ALM.ioc](ALM.ioc) (STM32G0B1)
- **Build counter**: `MDK-ARM/Inc_Build.bat` auto-increments `fw_build_number.h` on each build

## Build Outputs & Post-Build

After Keil build, `MDK-ARM/After_Build.bat` invokes [Copy_Bin.exe](../Copy_Bin_Motor_Project/):
- Reads `Output/ALM_Motor_App.bin`
- Encrypts to `HEX/ALM_Motor_App_<SN-tag>_<yyMMdd>_<HHmm>.mot`
  - SN-tag = `"Unlock"` if `fw_sn == 0xA5C3F09E`, otherwise hex SN
- **No bootloader merge** — Motor bootloader is flashed separately via SWD

Examples seen: `ALM_Motor_App_Unlock_260429_1046.mot`, `v19.mot` (manual snapshot).

---

## OTA — How `.mot` Reaches This Module

```
PC (DT Controller)
  └─ TCP CMD_MODULE_UPGRADE → CIC App
        └─ CAN-FD MUPG frames → ALM_Motor_App.upgrade_handler.c
              └─ Stage to W25Qxx → reboot → ALM_Motor_Bootloader applies
```

`.mot` format: FW_ID hdr (16B) + metadata block-0 (16B) + CRC block (16B) +
AES-CBC payload, encrypted with **Motor** key/IV. The CRC block carries a
`fw_crc32` over the plaintext payload that the bootloader verifies before
erasing APP and again after programming — see
[ALM_Motor_Bootloader/README.md](../ALM_Motor_Bootloader/README.md).

---

## Constants

| Name | Value |
|---|---|
| `FW_ID_MAGIC` | `0x47544657` ("GTFW") |
| `FW_SN_WILDCARD` | `0xA5C3F09E` |
| `USERSTR` | `"LEDFW012"` (8 B) |
| Motor `board_id` | (see [BSP/fw_id.c](BSP/fw_id.c)) |

---

## Cross-References

- Bootloader counterpart: [ALM_Motor_Bootloader/README.md](../ALM_Motor_Bootloader/README.md)
- `.mot` encryption tool & key/IV bytes: [Copy_Bin_Motor_Project/README.md](../Copy_Bin_Motor_Project/README.md)
- Host (CIC App) MUPG relay: [ALM_CIC_APP/BSP/module_upgrade.c](../ALM_CIC_APP/BSP/module_upgrade.c)
- Top-level: [../README.md](../README.md)
