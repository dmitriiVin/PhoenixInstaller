@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "WIM_PATH=%~1"
set "WINDOWS_EDITION=%~2"

call "%~dp0..\Common.bat" :InitializeLog

if "%WIM_PATH%"=="" goto :invalid_arguments
if not exist "%WIM_PATH%" goto :invalid_arguments

call "%~dp0..\Common.bat" :RequirePositiveInteger "%WINDOWS_EDITION%"
if errorlevel 1 goto :invalid_arguments

call "%~dp0..\Common.bat" :WriteLog INFO "Applying Windows image index %WINDOWS_EDITION%."
dism /Apply-Image /ImageFile:"%WIM_PATH%" /Index:%WINDOWS_EDITION% /ApplyDir:C:\ >> "%PHOENIX_LOG_FILE%" 2>&1
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    call "%~dp0..\Common.bat" :WriteLog ERROR "DISM Apply-Image failed. ErrorLevel=%RESULT%"
    exit /b %RESULT%
)

call "%~dp0..\Common.bat" :WriteLog INFO "Windows image applied successfully."
exit /b 0

:invalid_arguments
call "%~dp0..\Common.bat" :WriteLog ERROR "ApplyImage requires an existing WIM path and a positive image index."
exit /b 1
