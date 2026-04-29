@echo off
REM ============================================================
REM  After_Build.bat <ProjectName>
REM  Called by Keil "After Build" UserProg from MDK-ARM directory.
REM
REM  Three side effects:
REM    1) Copy ..\Output\<ProjectName>.bin -> ..\HEX\app\app.bin
REM       so ..\HEX\app\Run.bat can merge it with BooLoader.hex into
REM       Full_Package.hex without a manual copy step.
REM    2) Run ..\Copy_Bin\Copy_Bin.exe to encrypt the same .bin into
REM       ..\HEX\<ProjectName>_<snTag>_yyMMdd_HHmm.mot for OTA.
REM    3) Merge BooLoader.hex + .bin into a timestamped SWD package:
REM         ..\HEX\<ProjectName>_Full_Package_yyMMdd_HHmm.hex
REM       (skipped with a warning if BooLoader.hex is missing).
REM ============================================================
setlocal enabledelayedexpansion

if "%~1"=="" (
    echo [After_Build] ERROR: no project name argument
    exit /b 1
)

set "BIN_SRC=..\Output\%~1.bin"
set "BIN_DST_DIR=..\HEX\app"
set "BIN_DST=%BIN_DST_DIR%\app.bin"

if not exist "%BIN_SRC%" (
    echo [After_Build] ERROR: %BIN_SRC% not found ^(check Keil "Create Binary" option^)
    exit /b 1
)

if not exist "%BIN_DST_DIR%" mkdir "%BIN_DST_DIR%"

copy /Y "%BIN_SRC%" "%BIN_DST%" >nul
if errorlevel 1 (
    echo [After_Build] ERROR: copy %BIN_SRC% -^> %BIN_DST% failed
    exit /b 1
)
echo [After_Build] OK : %BIN_SRC% -^> %BIN_DST%

REM Use the canonical (fresh) Copy_Bin from Copy_Bin_Motor_Project's Release
REM build, NOT the stale ..\Copy_Bin\ snapshot that lives next to the App
REM source. The stale local copy was the source of a brick-OTA bug after the
REM .mot CRC-block format change: it was producing pre-CRC .mot files that
REM the new BL rightly refused to flash.
set "COPY_BIN=..\..\Copy_Bin_Motor_Project\Copy_Bin\bin\Release\net8.0-windows7.0\Copy_Bin.exe"

if not exist "%COPY_BIN%" (
    echo [After_Build] ERROR: %COPY_BIN% not found
    echo [After_Build]        ^(rebuild Copy_Bin_Motor_Project first: dotnet build -c Release^)
    exit /b 1
)

echo [After_Build] Running Copy_Bin for %1 ...
"%COPY_BIN%" %1

if errorlevel 1 (
    echo [After_Build] Copy_Bin returned an error
    exit /b 1
)

REM ---- Report App ROM utilization against MOT_APP_SIZE (0xD000 = 53248 B) ----
set "MAP=..\Output\%~1.map"
set "APP_LIMIT=53248"
set "ROM_BYTES="
if exist "%MAP%" (
    for /f "tokens=11" %%A in ('findstr /C:"Total ROM Size" "%MAP%"') do set "ROM_BYTES=%%A"
    if defined ROM_BYTES (
        set /a "PCT_X10=!ROM_BYTES! * 1000 / %APP_LIMIT%"
        set /a "PCT_INT=!PCT_X10! / 10"
        set /a "PCT_DEC=!PCT_X10! - !PCT_INT! * 10"
        set /a "FREE_BYTES=%APP_LIMIT% - !ROM_BYTES!"
        echo [After_Build] App ROM: !ROM_BYTES! / %APP_LIMIT% B  ^(!PCT_INT!.!PCT_DEC!%%, free !FREE_BYTES! B^)
    )
)

REM ---- Merge BL hex + App bin into a timestamped Full_Package.hex ----
REM  Output  : ..\HEX\<ProjectName>_Full_Package_yyMMdd_HHmm.hex
REM  Inputs  : ..\HEX\app\Bootloader\BooLoader.hex (auto-copied by BL build)
REM            ..\Output\%~1.bin                   (this build's App image)
REM  App load address: 0x08003000 (must match motor_partition.h MOT_APP_BASE).
set "BL_HEX=..\HEX\app\Bootloader\BooLoader.hex"
set "SREC=..\HEX\app\srecord\srec_cat.exe"
set "APP_OFFSET=0x8003000"
set "TMP_HEX=..\HEX\app\app1.hex"

if not exist "%BL_HEX%" (
    echo [After_Build] WARNING: %BL_HEX% missing - skipping Full_Package merge
    echo [After_Build]          ^(build ALM_Motor_Bootloader first to populate it^)
    goto :merge_done
)
if not exist "%SREC%" (
    echo [After_Build] WARNING: %SREC% missing - skipping Full_Package merge
    goto :merge_done
)

REM Generate yyMMdd_HHmm timestamp via PowerShell (locale-independent).
for /f "tokens=*" %%T in ('powershell -NoProfile -Command "Get-Date -Format yyMMdd_HHmm"') do set "TS=%%T"
if not defined TS (
    echo [After_Build] WARNING: timestamp generation failed - skipping Full_Package merge
    goto :merge_done
)

set "FULL_HEX=..\HEX\%~1_Full_Package_!TS!.hex"

"%SREC%" "%BIN_SRC%" -Binary -offset %APP_OFFSET% -o "%TMP_HEX%" -Intel
if errorlevel 1 (
    echo [After_Build] WARNING: srec_cat .bin -^> .hex failed - skipping Full_Package merge
    goto :merge_done
)
"%SREC%" "%BL_HEX%" -Intel "%TMP_HEX%" -Intel -o "!FULL_HEX!" -Intel
if errorlevel 1 (
    echo [After_Build] WARNING: srec_cat merge failed - Full_Package not produced
    if exist "%TMP_HEX%" del "%TMP_HEX%"
    goto :merge_done
)
if exist "%TMP_HEX%" del "%TMP_HEX%"
echo [After_Build] OK : Full_Package -^> !FULL_HEX!

:merge_done

echo [After_Build] Done.
endlocal
exit /b 0
