@echo off
REM ============================================================
REM  Inc_Build.bat
REM  Called by Keil "Before Build/Rebuild" UserProg from MDK-ARM.
REM  Increments FW_BUILD_NUMBER in ..\BSP\fw_build_number.h so
REM  every build gets a unique version number reported to the PC.
REM ============================================================
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Inc_Build.ps1"
exit /b %ERRORLEVEL%
