@echo off
REM ============================================================
REM  After_Build.bat <ProjectName>
REM  Called by Keil "After Build" UserProg from MDK-ARM directory.
REM
REM  Two side effects:
REM    1) Copy ..\Output\<ProjectName>.bin -> ..\HEX\app\app.bin
REM       so ..\HEX\app\Run.bat can merge it with BooLoader.hex into
REM       Full_Package.hex without a manual copy step.
REM    2) Run ..\Copy_Bin\Copy_Bin.exe to encrypt the same .bin into
REM       ..\HEX\<ProjectName>_<snTag>_yyMMdd_HHmm.mot for OTA.
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

set "COPY_BIN=..\Copy_Bin\Copy_Bin.exe"

if not exist "%COPY_BIN%" (
    echo [After_Build] ERROR: %COPY_BIN% not found
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

echo [After_Build] Done.
endlocal
exit /b 0
