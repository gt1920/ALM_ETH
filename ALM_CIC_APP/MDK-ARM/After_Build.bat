@echo off
REM ============================================================
REM  After_Build.bat  <ProjectName>
REM  Called by Keil "After Build" UserProg2 from MDK-ARM directory.
REM
REM  Three side effects:
REM    1) Merge BL hex + freshly-built App bin into a timestamped
REM       SWD package: ..\HEX\<ProjectName>_Full_Package_yyMMdd_HHmm.hex
REM       (mirrors the Motor App pipeline; runs first so the merge is
REM       independent of Copy_Bin's exit status).
REM    2) Run ..\Copy_Bin\Copy_Bin.exe for the timestamped .bin and
REM       the encrypted .cic.
REM    3) Print App ROM utilization vs the App partition limit.
REM ============================================================
setlocal enabledelayedexpansion

if "%~1"=="" (
    echo [After_Build] ERROR: no project name argument
    exit /b 1
)

set "BIN_SRC=..\Output\%~1.bin"

if not exist "%BIN_SRC%" (
    echo [After_Build] ERROR: %BIN_SRC% not found ^(check Keil "Create Binary" option^)
    exit /b 1
)

REM ---- Merge BL hex + App bin into a timestamped Full_Package.hex ----
REM  Output  : ..\HEX\<ProjectName>_Full_Package_yyMMdd_HHmm.hex
REM  Inputs  : ..\HEX\app\Bootloader\BooLoader.hex (auto-copied by BL build)
REM            ..\Output\%~1.bin                   (this build's App image)
REM  App load address: 0x08008000 (CIC partition: BL=32KB at 0x08000000).
set "BL_HEX=..\HEX\app\Bootloader\BooLoader.hex"
set "SREC=..\HEX\app\srecord\srec_cat.exe"
set "APP_OFFSET=0x8008000"
set "TMP_HEX=..\HEX\app\app1.hex"

if not exist "%BL_HEX%" (
    echo [After_Build] WARNING: %BL_HEX% missing - skipping Full_Package merge
    echo [After_Build]          ^(build ALM_CIC_Bootloader first to populate it^)
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

REM ---- Run Copy_Bin to produce the timestamped .bin and encrypted .cic ----
set "COPY_BIN=..\Copy_Bin\Copy_Bin.exe"

if not exist "%COPY_BIN%" (
    echo [After_Build] ERROR: %COPY_BIN% not found
    exit /b 1
)

echo [After_Build] Running Copy_Bin for %1 ...
"%COPY_BIN%" %1

if errorlevel 1 (
    echo [After_Build] WARNING: Copy_Bin returned an error ^(.cic may not be regenerated^)
    REM Don't fail the build -- Full_Package.hex above is the primary deliverable.
)

REM ---- Report App ROM utilization ----
REM  CIC partition is BL=32KB at 0x08000000 + App from 0x08008000 to 0x080FFFFF
REM  (= 992 KB = 1015808 B). Update APP_LIMIT here if the partition changes.
set "MAP=..\Output\%~1.map"
set "APP_LIMIT=1015808"
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

echo [After_Build] Done.
endlocal
exit /b 0
