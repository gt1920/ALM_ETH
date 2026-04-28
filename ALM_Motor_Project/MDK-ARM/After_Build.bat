@echo off
REM ============================================================
REM  After_Build.bat <ProjectName>
REM  Called by Keil "After Build" UserProg from MDK-ARM directory.
REM  Invokes ..\Copy_Bin\Copy_Bin.exe which encrypts ..\Output\<ProjectName>.bin
REM  to ..\HEX\<ProjectName>_<snTag>_yyMMdd_HHmm.mot using the Motor AES key.
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
