@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "DRIVERS_DIRECTORY=%~1"

call "%~dp0..\Common.bat" :InitializeLog

if "%DRIVERS_DIRECTORY%"=="" (
    call "%~dp0..\Common.bat" :WriteLog ERROR "No driver directory was supplied."
    exit /b 1
)

if not exist "%DRIVERS_DIRECTORY%" (
    call "%~dp0..\Common.bat" :WriteLog WARNING "Driver payload is not present; no drivers were installed."
    exit /b 0
)

set "ARCHITECTURE=%PROCESSOR_ARCHITECTURE%"
if /i "%ARCHITECTURE%"=="AMD64" set "ARCHITECTURE=amd64"
if /i "%ARCHITECTURE%"=="ARM64" set "ARCHITECTURE=arm64"
if /i "%ARCHITECTURE%"=="x86" set "ARCHITECTURE=x86"

if not exist "%DRIVERS_DIRECTORY%\%ARCHITECTURE%" (
    call "%~dp0..\Common.bat" :WriteLog WARNING "No drivers are available for architecture %ARCHITECTURE%."
    exit /b 0
)

dir /b /s "%DRIVERS_DIRECTORY%\%ARCHITECTURE%\*.inf" >nul 2>&1
if errorlevel 1 (
    call "%~dp0..\Common.bat" :WriteLog WARNING "No INF files are available for architecture %ARCHITECTURE%."
    exit /b 0
)

call "%~dp0..\Common.bat" :WriteLog INFO "Installing %ARCHITECTURE% drivers."
pnputil /add-driver "%DRIVERS_DIRECTORY%\%ARCHITECTURE%\*.inf" /subdirs /install >> "%PHOENIX_LOG_FILE%" 2>&1
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    call "%~dp0..\Common.bat" :WriteLog ERROR "PNPUtil failed. ErrorLevel=%RESULT%"
    exit /b %RESULT%
)

call "%~dp0..\Common.bat" :WriteLog INFO "Driver installation completed."
exit /b 0
