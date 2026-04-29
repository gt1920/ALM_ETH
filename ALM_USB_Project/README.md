# ALM_USB_Project — USB-HID ↔ CAN Bridge (Variant Host Interface)

STM32G0B1 firmware for the **USB-HID host interface** variant of the ALM controller. Acts as a USB-CDC/HID ↔ CAN-FD bridge between PC ([DT_Controller](../DT_Controller_Project/) HID mode) and the laser driver / Motor module CAN bus.

This is a **separate hardware variant** — alternative to the Ethernet [ALM_CIC_APP](../ALM_CIC_APP/) on STM32H563. Same CAN-side protocol; different host link (USB-HID instead of TCP).

> Status as of Build_145: appears unchanged since the project was forked (last touched 2025-04-11). Not part of the active OTA work — included as a reference / parallel track.

---

## Source Index

| Path | What lives here |
|---|---|
| [Core/Src/main.c](Core/Src/main.c) | MCU init, USB device init, FDCAN init, main loop |
| [Core/Src/fdcan.c](Core/Src/fdcan.c) | FDCAN peripheral setup |
| [Core/Src/usart.c](Core/Src/usart.c), [Core/Src/tim.c](Core/Src/tim.c) | UART / timer init |
| [BSP/usb_comm.c](BSP/usb_comm.c) | USB-HID rx/tx framing |
| [BSP/USB_Send_Queue.c](BSP/USB_Send_Queue.c) | Outbound queue from CAN → USB |
| [BSP/CAN_comm.c](BSP/CAN_comm.c) | CAN tx/rx, mirrors CIC App's CAN ID set |
| [BSP/Systick_Task.c](BSP/Systick_Task.c) | 1 ms tick task scheduling |
| [USB_Device/](USB_Device/) | STM32Cube USB Device middleware (Custom HID class) |

---

## USB Identity

| Field | Value |
|---|---|
| VID | `0x0483` (STMicroelectronics) |
| PID | `0xA7D1` |
| Class | Custom HID (64 B IN / 64 B OUT reports) |

[DT_Controller](../DT_Controller_Project/) enumerates by `(VID, PID)` to find the device.

---

## Build

- **Project**: [MDK-ARM/AL_USB.uvprojx](MDK-ARM/AL_USB.uvprojx)
- **CubeMX**: [AL_USB.ioc](AL_USB.ioc) (STM32G0B1KBUx)
- **Output**: [Output/](Output/) — `.axf / .bin / .hex`
- **No post-build encryption** — flashed as plain `.hex` via SWD; no OTA pipeline for this variant

---

## Relationship to Other Subprojects

- **Same CAN protocol** as [ALM_CIC_APP](../ALM_CIC_APP/) — frame layout in [BSP/CAN_comm.c](BSP/CAN_comm.c) mirrors the H563 side
- **No bootloader / no OTA** — this variant is not in the encrypted-upgrade pipeline
- **Host counterpart**: HID branch of [DT_Controller](../DT_Controller_Project/) (same `Form1.cs`, switches transport)

---

## Cross-References

- ETH-host equivalent (active branch): [ALM_CIC_APP/README.md](../ALM_CIC_APP/README.md)
- Top-level: [../README.md](../README.md)
