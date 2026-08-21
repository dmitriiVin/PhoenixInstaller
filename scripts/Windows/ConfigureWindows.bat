@echo off
setlocal EnableExtensions DisableDelayedExpansion

call "%~dp0..\Common.bat" :InitializeLog
call "%~dp0..\Common.bat" :WriteLog INFO "Configuring the built-in support account."

set "POWERSHELL=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%POWERSHELL%" (
    call "%~dp0..\Common.bat" :WriteLog ERROR "PowerShell is required to locate the built-in Administrator account by SID."
    exit /b 1
)

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\Tweaks\Security.ps1" -LogPath "%PHOENIX_LOG_FILE%"
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    call "%~dp0..\Common.bat" :WriteLog ERROR "Support account configuration failed. ErrorLevel=%RESULT%"
    exit /b %RESULT%
)

call "%~dp0..\Common.bat" :WriteLog INFO "Basic Windows configuration completed."
exit /b 0
