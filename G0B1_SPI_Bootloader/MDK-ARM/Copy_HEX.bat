@echo off
REM =================================================================
REM  G0B1_SPI_Bootloader After-Build script
REM    1) Archive timestamped hex to this project's HEX/ folder
REM    2) Copy as fixed name BooLoader.hex to App/HEX/app/Bootloader/
REM  CWD = Keil project dir (MDK-ARM)
REM =================================================================

set filename=G0B1_Bootloader_115200
set srcdir=..\Output
set archivedir=..\HEX
set appdir=..\..\App\HEX\app\Bootloader

REM ---- Date stamp YYYYMMDD ----
set datevar=%date:~0,4%%date:~5,2%%date:~8,2%

REM ---- Time stamp HHMMSS (pad leading zero on hour) ----
set timevar=%time:~0,2%
if /i %timevar% LSS 10 (set timevar=0%time:~1,1%)
set timevar=%timevar%%time:~3,2%%time:~6,2%

set copyfilename=%filename%_%datevar%_%timevar%

REM ---- 1. Archive ----
copy /Y %srcdir%\%filename%.hex %archivedir%\%copyfilename%.hex

REM ---- 2. Fixed-name copy into App project ----
copy /Y %srcdir%\%filename%.hex %appdir%\BooLoader.hex

REM ---- 3. Report firmware size in hex ----
for %%F in (%srcdir%\%filename%.bin) do (
    powershell -NoProfile -Command "Write-Host ('Firmware size: 0x{0:X} ({1} bytes, limit 0x4800, used {2:N1}%%)' -f %%~zF, %%~zF, (%%~zF / 18432 * 100))"
)
