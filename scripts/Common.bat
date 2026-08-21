@echo off

if /i "%~1"==":InitializeLog" goto :InitializeLog
if /i "%~1"==":WriteLog" goto :WriteLog
if /i "%~1"==":RequireNonNegativeInteger" goto :RequireNonNegativeInteger
if /i "%~1"==":RequirePositiveInteger" goto :RequirePositiveInteger

exit /b 1


:InitializeLog

if not defined PHOENIX_LOG_FILE (
    set "PHOENIX_LOG_FILE=%~dp0..\Logs\install.log"
)

for %%I in ("%PHOENIX_LOG_FILE%") do (
    if not exist "%%~dpI" mkdir "%%~dpI" >nul 2>&1
)

if not exist "%PHOENIX_LOG_FILE%" (
    type nul > "%PHOENIX_LOG_FILE%"
)

exit /b 0


:WriteLog

call "%~f0" :InitializeLog

>>"%PHOENIX_LOG_FILE%" echo([%~2] %~3

exit /b 0


:RequireNonNegativeInteger

set "PHOENIX_VALUE=%~2"

if not defined PHOENIX_VALUE (
    exit /b 1
)

for /f "delims=0123456789" %%A in ("%PHOENIX_VALUE%") do (
    exit /b 1
)

exit /b 0


:RequirePositiveInteger

set "PHOENIX_VALUE=%~2"

if not defined PHOENIX_VALUE (
    exit /b 1
)

if "%PHOENIX_VALUE%"=="0" (
    exit /b 1
)

for /f "delims=0123456789" %%A in ("%PHOENIX_VALUE%") do (
    exit /b 1
)

exit /b 0