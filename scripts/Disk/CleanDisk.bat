@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "DISK_NUMBER=%~1"
set "WINDOWS_SIZE_GB=%~2"

call "%~dp0..\Common.bat" :InitializeLog
call "%~dp0..\Common.bat" :RequireNonNegativeInteger "%DISK_NUMBER%"
if errorlevel 1 goto :invalid_arguments

call "%~dp0..\Common.bat" :RequirePositiveInteger "%WINDOWS_SIZE_GB%"
if errorlevel 1 goto :invalid_arguments

if %WINDOWS_SIZE_GB% LSS 100 goto :invalid_arguments

set /a WINDOWS_SIZE_MB=%WINDOWS_SIZE_GB% * 1024
set "DISKPART_FILE=%TEMP%\PhoenixCleanDisk-%RANDOM%-%RANDOM%.txt"

> "%DISKPART_FILE%" (
    echo select disk %DISK_NUMBER%
    echo detail disk
    echo clean
    echo convert gpt
    echo create partition efi size=260
    echo format quick fs=fat32 label="SYSTEM"
    echo assign letter=S
    echo create partition msr size=16
    echo create partition primary size=%WINDOWS_SIZE_MB%
    echo format quick fs=ntfs label="Windows"
    echo assign letter=C
    echo create partition primary
    echo format quick fs=ntfs label="Data"
    echo assign letter=D
)

call "%~dp0..\Common.bat" :WriteLog INFO "Cleaning disk %DISK_NUMBER% and creating the GPT layout."
diskpart /s "%DISKPART_FILE%" >> "%PHOENIX_LOG_FILE%" 2>&1
set "RESULT=%ERRORLEVEL%"
del /q "%DISKPART_FILE%" >nul 2>&1

if not "%RESULT%"=="0" (
    call "%~dp0..\Common.bat" :WriteLog ERROR "DiskPart CleanDisk failed. ErrorLevel=%RESULT%"
    exit /b %RESULT%
)

call "%~dp0..\Common.bat" :WriteLog INFO "Disk %DISK_NUMBER% prepared successfully."
exit /b 0

:invalid_arguments
call "%~dp0..\Common.bat" :WriteLog ERROR "CleanDisk requires a disk number and a Windows size of at least 100 GB."
exit /b 1
