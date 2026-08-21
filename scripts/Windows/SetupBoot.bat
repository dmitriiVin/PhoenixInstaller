@echo off
setlocal EnableExtensions DisableDelayedExpansion

call "%~dp0..\Common.bat" :InitializeLog

if not exist "C:\Windows\System32\bcdboot.exe" (
    call "%~dp0..\Common.bat" :WriteLog ERROR "The applied Windows bcdboot.exe was not found."
    exit /b 1
)

call "%~dp0..\Common.bat" :WriteLog INFO "Installing UEFI boot files."
bcdboot C:\Windows /s S: /f UEFI >> "%PHOENIX_LOG_FILE%" 2>&1
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    call "%~dp0..\Common.bat" :WriteLog ERROR "BCDBoot failed. ErrorLevel=%RESULT%"
    exit /b 1
)

call "%~dp0..\Common.bat" :WriteLog INFO "UEFI boot files installed successfully."
exit /b 0
