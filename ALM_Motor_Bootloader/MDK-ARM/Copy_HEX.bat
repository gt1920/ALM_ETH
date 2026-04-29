@echo off
REM ============================================================
REM  Copy_HEX.bat
REM  Run by Keil "After Build" step (UserProg2) for ALM_Motor_Bootloader.
REM
REM  Two destinations:
REM    1) ..\HEX\ALM_Motor_Bootloader.hex
REM       — local archive of the latest BL hex.
REM    2) ..\..\ALM_Motor_App\HEX\app\Bootloader\BooLoader.hex
REM       — picked up by ALM_Motor_App\HEX\app\Run.bat to merge with the
REM         App image into a single Full_Package.hex.
REM
REM  Working directory when Keil invokes this is .\MDK-ARM\.
REM ============================================================
setlocal

set "SRC=..\Output\ALM_Motor_Bootloader.hex"

set "DST1_DIR=..\HEX"
set "DST1=%DST1_DIR%\ALM_Motor_Bootloader.hex"

set "DST2_DIR=..\..\ALM_Motor_App\HEX\app\Bootloader"
set "DST2=%DST2_DIR%\BooLoader.hex"

if not exist "%SRC%" (
    echo [Copy_HEX.bat] ERROR: source not found: %SRC%
    exit /b 1
)

if not exist "%DST1_DIR%" mkdir "%DST1_DIR%"
copy /Y "%SRC%" "%DST1%" >nul
if errorlevel 1 (
    echo [Copy_HEX.bat] ERROR: copy to BL HEX archive failed
    exit /b 1
)
echo [Copy_HEX.bat] OK : %SRC%  ^-^>  %DST1%

if not exist "%DST2_DIR%" mkdir "%DST2_DIR%"
copy /Y "%SRC%" "%DST2%" >nul
if errorlevel 1 (
    echo [Copy_HEX.bat] ERROR: copy to App Bootloader slot failed
    exit /b 1
)
echo [Copy_HEX.bat] OK : %SRC%  ^-^>  %DST2%

endlocal
exit /b 0
