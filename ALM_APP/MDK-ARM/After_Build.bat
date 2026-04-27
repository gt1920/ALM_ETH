@echo off
REM ============================================================
REM  After_Build.bat  <ProjectName>
REM  Called by Keil "After Build" UserProg2 from MDK-ARM directory.
REM  Invokes Copy_Bin.exe which:
REM    1. Copies ..\Output\<ProjectName>.bin to ..\HEX\ (with timestamp)
REM    2. Copies to ..\HEX\app\app.bin
REM    3. Runs ..\HEX\app\Run.bat  (merges BooLoader.hex + app1.hex)
REM    4. Encrypts .bin to .alm (AES-256, Key/IV from aes.c)
REM ============================================================
setlocal

if "%~1"=="" (
    echo [After_Build] ERROR: no project name argument
    exit /b 1
)

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

echo [After_Build] Done.
endlocal
exit /b 0
