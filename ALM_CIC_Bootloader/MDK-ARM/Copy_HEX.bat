@echo off
REM ============================================================
REM  Copy_HEX.bat
REM  Run by Keil "After Build" step (UserProg2) for ALM_CIC_Bootloader.
REM  Copies the freshly-built bootloader hex into ALM_CIC_APP's HEX
REM  archive folder so the Copy_Bin / packaging tool finds it.
REM
REM  Working directory when Keil invokes this is .\MDK-ARM\.
REM ============================================================
setlocal

set "SRC=..\Output\ALM_Bootloader.hex"
set "DST_DIR=..\..\ALM_CIC_APP\HEX\app\Bootloader"
set "DST=%DST_DIR%\BooLoader.hex"

if not exist "%SRC%" (
    echo [Copy_HEX.bat] ERROR: source not found: %SRC%
    exit /b 1
)

if not exist "%DST_DIR%" mkdir "%DST_DIR%"

copy /Y "%SRC%" "%DST%" >nul
if errorlevel 1 (
    echo [Copy_HEX.bat] ERROR: copy failed
    exit /b 1
)

echo [Copy_HEX.bat] OK : %SRC%  ^-^>  %DST%
endlocal
exit /b 0
