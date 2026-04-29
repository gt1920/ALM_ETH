@echo off
REM ============================================================
REM  Run.bat -- merge BL hex + App bin into a single Full_Package.hex
REM
REM  Memory layout (must match motor_partition.h):
REM    Bootloader : 0x08000000 .. 0x08002FFF   (12 KB,   6 pages)
REM    Application: 0x08003000 .. 0x0800FFFF   (52 KB,  26 pages)
REM    Staging    : 0x08010000 .. 0x0801F7FF   (62 KB,  31 pages)
REM    Params     : 0x0801F800 .. 0x0801FFFF   ( 2 KB,   1 page,  preserved across OTA)
REM
REM  Inputs  : .\app.bin                       (Keil-built App, no header)
REM            .\Bootloader\BooLoader.hex      (auto-copied here by ALM_Motor_Bootloader/MDK-ARM/Copy_HEX.bat)
REM  Output  : .\Full_Package.hex              (BL + App, single SWD flash)
REM ============================================================

REM Convert app.bin -> app1.hex with App load address 0x08003000
.\srecord\srec_cat.exe .\app.bin -Binary -offset 0x8003000 -o .\app1.hex -Intel

REM Merge BL hex + App hex into Full_Package.hex
.\srecord\srec_cat.exe .\Bootloader\BooLoader.hex -Intel .\app1.hex -Intel -o .\Full_Package.hex -Intel

del app1.hex
