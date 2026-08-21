@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "DISK_NUMBER=%~1"
set "WINDOWS_PARTITION_NUMBER=%~2"
set "DATA_PARTITION_NUMBER=%~3"

call "%~dp0..\Common.bat" :InitializeLog
call "%~dp0..\Common.bat" :RequireNonNegativeInteger "%DISK_NUMBER%"
if errorlevel 1 goto :invalid_arguments

call "%~dp0..\Common.bat" :RequirePositiveInteger "%WINDOWS_PARTITION_NUMBER%"
if errorlevel 1 goto :invalid_arguments

call "%~dp0..\Common.bat" :RequirePositiveInteger "%DATA_PARTITION_NUMBER%"
if errorlevel 1 goto :invalid_arguments

if "%WINDOWS_PARTITION_NUMBER%"=="%DATA_PARTITION_NUMBER%" goto :invalid_arguments

set "DISKPART_FILE=%TEMP%\PhoenixPrepareWindowsAndData-%RANDOM%-%RANDOM%.txt"

> "%DISKPART_FILE%" (
    echo select disk %DISK_NUMBER%
    echo select partition %WINDOWS_PARTITION_NUMBER%
    echo format quick fs=ntfs label="Windows"
    echo assign letter=C noerr
    echo select disk %DISK_NUMBER%
    echo select partition %DATA_PARTITION_NUMBER%
    echo format quick fs=ntfs label="Data"
    echo assign letter=D noerr
)

diskpart /s "%DISKPART_FILE%" >> "%PHOENIX_LOG_FILE%" 2>&1
set "RESULT=%ERRORLEVEL%"
del /q "%DISKPART_FILE%" >nul 2>&1

if not "%RESULT%"=="0" (
    call "%~dp0..\Common.bat" :WriteLog ERROR "DiskPart PrepareWindowsAndData failed. ErrorLevel=%RESULT%"
    exit /b %RESULT%
)

call "%~dp0..\Common.bat" :WriteLog INFO "Windows partition %WINDOWS_PARTITION_NUMBER% and Data partition %DATA_PARTITION_NUMBER% prepared."
exit /b 0

:invalid_arguments
call "%~dp0..\Common.bat" :WriteLog ERROR "PrepareWindowsAndData requires distinct explicit partition numbers."
exit /b 1
