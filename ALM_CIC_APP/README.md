# ALM_CIC_APP — Main Application Firmware

STM32H563VG main controller firmware (Keil MDK-ARM). Runs at `0x08008000` above [ALM_CIC_Bootloader](../ALM_CIC_Bootloader/). Hosts TCP/UDP server on Ethernet, talks CAN-FD to the Motor sub-module, and orchestrates OTA upgrades for both itself (`.cic`) and the Motor module (`.mot`).

---

## Source Index

| Path | What lives here |
|---|---|
| [Core/Src/main.c](Core/Src/main.c) | MCU init (clocks, MPU for ETH DMA SRAM3, ICACHE, OCTOSPI, peripherals); calls `LWIP_APP_Init()` then `while(1)` polling LwIP + UPG_PollReboot |
| [Core/Src/lwip_app.c](Core/Src/lwip_app.c) | LwIP init, polling, link/DHCP state |
| [BSP/tcp_server.c](BSP/tcp_server.c) | TCP listen on **port 40000**, single-client; routes frames to `comm_protocol.c` |
| [BSP/udp_discovery.c](BSP/udp_discovery.c) | UDP discovery on **port 40001**, magic `"DTDR"`, 3 s broadcast |
| [BSP/comm_protocol.c](BSP/comm_protocol.c) | Frame parser (`0xA5` head, see protocol below); CMD_INFO / CMD_MOTION / CMD_PARAM / CMD_UPGRADE / CMD_MODULE_UPGRADE |
| [BSP/CAN_comm.c](BSP/CAN_comm.c) | FDCAN tx/rx; Motor heartbeat polling (5 s timeout); motion report sequencing; CAN ID `0x120` motion |
| [BSP/upgrade_handler.c](BSP/upgrade_handler.c) | CIC self-OTA: writes `.cic` to W25Q[0..], deferred `NVIC_SystemReset` after TCP flush (`UPG_PollReboot()`) |
| [BSP/module_upgrade.c](BSP/module_upgrade.c) | MUPG protocol — relays `.mot` to Motor sub-module over CAN-FD with retransmit |
| [BSP/aes.c](BSP/aes.c), [BSP/aes.h](BSP/aes.h) | AES-256-CBC (copied from Bootloader; CIC key/IV — see [ALM_CIC_Bootloader/README.md](../ALM_CIC_Bootloader/README.md)) |
| [BSP/fw_id.c](BSP/fw_id.c) | `FW_HW_ID` struct embedded in `.bin` — read by Copy_Bin to tag .cic with board_id/fw_sn |
| [BSP/cpu_id.c](BSP/cpu_id.c) | UID → device_sn (byte-identical to Bootloader) |
| [BSP/fw_version.h](BSP/fw_version.h) | `FW_HW_VER = FW_BUILD_NUMBER` (auto-incremented by `MDK-ARM/Inc_Build.bat`) |
| [BSP/fw_build_number.h](BSP/fw_build_number.h) | Auto-generated; **do not hand-edit** |
| [BSP/device_config.c](BSP/device_config.c) | Device-name persistence in last 8 KB sector (`0x080FE000`) |

## Drivers / Middleware

| Path | Notes |
|---|---|
| [Drivers/](Drivers/) | STM32H5 HAL + CMSIS |
| [Middlewares/](Middlewares/) | LwIP TCP/IP stack |
| [Core/Src/octospi.c](Core/Src/octospi.c) | OCTOSPI driver for W25Q16 (indirect mode) |

---

## Memory & Build

- **Scatter**: `MDK-ARM/ALM_ETH/ALM_ETH.sct` — `LR_IROM1 0x08008000 0x000F8000` (992 KB)
- **Vector table**: `system_stm32h5xx.c` — `VECT_TAB_OFFSET = 0x8000`; `main.c` also sets `SCB->VTOR = 0x08008000` belt-and-suspenders
- **MPU**: SRAM3 `0x20050000` (64 KB) marked non-cacheable for ETH DMA
- **External flash**: W25Q16 (2 MB), `0x000000` = staged `.cic` upgrade
- **Project**: [MDK-ARM/ALM_ETH.uvprojx](MDK-ARM/ALM_ETH.uvprojx)
- **CubeMX**: [ALM_ETH.ioc](ALM_ETH.ioc)

## Build Outputs & Post-Build

After Keil build, `MDK-ARM/After_Build.bat` runs:
1. Copies `Output/ALM_ETH.bin` and `Output/ALM_ETH.hex`
2. Invokes [Copy_Bin.exe](../Copy_Bin_CIC_Project/) → produces `HEX/ALM_ETH_<SN-tag>_<yyMMdd>_<HHmm>.cic`
   - SN-tag = `"Unlock"` if `FW_HW_ID.fw_sn == 0xA5C3F09E` (wildcard), otherwise hex SN
3. Merges Bootloader hex + App hex → combined programming file in `HEX/app/`

Outputs land in [HEX/](HEX/) and [Output/](Output/). Examples seen: `ALM_ETH_Unlock_260429_1043.cic`, `V21.cic`, `V22.cic` (manually renamed shipping snapshots).

---

## TCP Protocol (port 40000)

Frame: `[LEN_L][LEN_H][0xA5][CMD][DIR][SEQ_L][SEQ_H][LEN_BODY][SUBCMD][payload...]`

| CMD | Value | Purpose |
|---|---|---|
| `CMD_INFO` | `0x01` | Version query, module list, device name |
| `CMD_MOTION` | `0x10` | Motion command → CAN `0x120` |
| `CMD_PARAM_SET` | `0x20` | Set motor parameter |
| `CMD_UPGRADE` | `0x30` | Self-OTA (.cic) — START / DATA / END |
| `CMD_MODULE_UPGRADE` | `0x31` | Motor module OTA (.mot) — relays to CAN-FD |

UDP discovery: port **40001**, magic `"DTDR"`, 3 s broadcast.

---

## OTA — Self (CIC, `.cic`)

1. **START**: PC sends `file_size(4) + cic[0..15]`. App AES-decrypts the 16 B FW_ID header, validates `magic / board_id / fw_sn / CRC32`, erases W25Q (rejects on mismatch).
2. **DATA**: 128-byte chunks → W25Q, per-packet ACK.
3. **END**: byte-count check → ACK → `UPG_PollReboot()` defers `NVIC_SystemReset` until TCP flush completes.
4. Bootloader takes over on next boot, applies update.

## OTA — Motor (`.mot`) Relay (MUPG)

[BSP/module_upgrade.c](BSP/module_upgrade.c) acts as a passthrough — receives `.mot` from PC over TCP, then drives the Motor module's bootloader over CAN-FD. Recent fixes: END-frame retransmit on timeout (commit `b626f8a`), `CAN_Encode_DLC` double-shift fix for ≤8 B END payloads (commit `ef8841b`).

---

## Cross-References

- Boot chain & `.cic` format: [ALM_CIC_Bootloader/README.md](../ALM_CIC_Bootloader/README.md)
- `.cic` encryption tool: [Copy_Bin_CIC_Project/README.md](../Copy_Bin_CIC_Project/README.md)
- Motor sub-module firmware: [ALM_Motor_App/README.md](../ALM_Motor_App/README.md)
- PC controller: [DT_Controller_Project/README.md](../DT_Controller_Project/README.md)
- Top-level overview: [../README.md](../README.md)
