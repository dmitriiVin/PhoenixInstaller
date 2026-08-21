@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "PHOENIX_LOG=%~dp0..\Logs\install-script.log"

rem %1 mode, %2 install.wim, %3 edition, %4 Windows size in GB,
rem %5 disk number, %6 staged payload, %7 Windows partition, %8 Data partition.
set "MODE=%~1"
set "WIM_PATH=%~2"
set "WINDOWS_EDITION=%~3"
set "WINDOWS_SIZE_GB=%~4"
set "DISK_NUMBER=%~5"
set "PAYLOAD_DIRECTORY=%~6"
set "WINDOWS_PARTITION_NUMBER=%~7"
set "DATA_PARTITION_NUMBER=%~8"
set "PHOENIX_LOG_FILE=%~dp0..\Logs\install.log"

call "%~dp0Common.bat" :InitializeLog

echo.
echo ================= PHOENIX DEBUG =================
echo ARG1 MODE=[%MODE%]
echo ARG2 WIM_PATH=[%WIM_PATH%]
echo ARG3 WINDOWS_EDITION=[%WINDOWS_EDITION%]
echo ARG4 WINDOWS_SIZE_GB=[%WINDOWS_SIZE_GB%]
echo ARG5 DISK_NUMBER=[%DISK_NUMBER%]
echo ARG6 PAYLOAD_DIRECTORY=[%PAYLOAD_DIRECTORY%]
echo ARG7 WINDOWS_PARTITION=[%WINDOWS_PARTITION_NUMBER%]
echo ARG8 DATA_PARTITION=[%DATA_PARTITION_NUMBER%]
echo ==================================================
echo.

pause

if "%MODE%"=="" (
    echo ERROR: MODE is empty
    pause
    goto :invalid_arguments
)

if "%WIM_PATH%"=="" (
    echo ERROR: WIM_PATH is empty
    pause
    goto :invalid_arguments
)

if "%PAYLOAD_DIRECTORY%"=="" (
    echo ERROR: PAYLOAD_DIRECTORY is empty
    pause
    goto :invalid_arguments
)

echo.
echo Checking DISK_NUMBER...
call "%~dp0Common.bat" :RequireNonNegativeInteger "%DISK_NUMBER%"
echo DISK_NUMBER ERRORLEVEL=%ERRORLEVEL%

if errorlevel 1 (
    echo ERROR: DISK_NUMBER validation failed
    pause
    goto :invalid_arguments
)

echo.
echo Checking WINDOWS_EDITION...
call "%~dp0Common.bat" :RequirePositiveInteger "%WINDOWS_EDITION%"
echo WINDOWS_EDITION ERRORLEVEL=%ERRORLEVEL%

if errorlevel 1 (
    echo ERROR: WINDOWS_EDITION validation failed
    pause
    goto :invalid_arguments
)

echo.
echo Checking WINDOWS_SIZE_GB...
call "%~dp0Common.bat" :RequirePositiveInteger "%WINDOWS_SIZE_GB%"
echo WINDOWS_SIZE_GB ERRORLEVEL=%ERRORLEVEL%

if errorlevel 1 (
    echo ERROR: WINDOWS_SIZE_GB validation failed
    pause
    goto :invalid_arguments
)

echo.
echo ALL ARGUMENTS ARE VALID
echo.
pause

if not exist "%WIM_PATH%" (
    call "%~dp0Common.bat" :WriteLog ERROR "install.wim was not found."
    goto :failed
)

if not exist "%PAYLOAD_DIRECTORY%\PhoenixSetup.exe" (
    call "%~dp0Common.bat" :WriteLog ERROR "PhoenixSetup.exe is missing from the staged payload."
    goto :failed
)

call "%~dp0Common.bat" :WriteLog INFO "Installation mode: %MODE%"
call "%~dp0Common.bat" :WriteLog INFO "Selected disk: %DISK_NUMBER%"
call "%~dp0Common.bat" :WriteLog INFO "Windows edition: %WINDOWS_EDITION%"
call "%~dp0Common.bat" :WriteLog INFO "Windows partition size: %WINDOWS_SIZE_GB% GB"

if /i "%MODE%"=="clean_disk" goto :clean_disk
if /i "%MODE%"=="reinstall_windows" goto :reinstall_windows
if /i "%MODE%"=="reinstall_windows_and_format_data" goto :reinstall_windows_and_format_data

call "%~dp0Common.bat" :WriteLog ERROR "Unknown installation mode."
goto :failed

:clean_disk
call "%~dp0Common.bat" :WriteLog INFO "Preparing a clean GPT disk."
call "%~dp0Disk\CleanDisk.bat" "%DISK_NUMBER%" "%WINDOWS_SIZE_GB%"
if errorlevel 1 goto :disk_failed
goto :apply_image

:reinstall_windows
call "%~dp0Common.bat" :RequirePositiveInteger "%WINDOWS_PARTITION_NUMBER%"
if errorlevel 1 goto :invalid_arguments

call "%~dp0Common.bat" :WriteLog INFO "Formatting the selected Windows partition."
call "%~dp0Disk\PrepareWindows.bat" "%DISK_NUMBER%" "%WINDOWS_PARTITION_NUMBER%"
if errorlevel 1 goto :disk_failed
goto :apply_image

:reinstall_windows_and_format_data
call "%~dp0Common.bat" :RequirePositiveInteger "%WINDOWS_PARTITION_NUMBER%"
if errorlevel 1 goto :invalid_arguments

call "%~dp0Common.bat" :RequirePositiveInteger "%DATA_PARTITION_NUMBER%"
if errorlevel 1 goto :invalid_arguments

if "%WINDOWS_PARTITION_NUMBER%"=="%DATA_PARTITION_NUMBER%" goto :invalid_arguments

call "%~dp0Common.bat" :WriteLog INFO "Formatting the selected Windows and Data partitions."
call "%~dp0Disk\PrepareWindowsAndData.bat" "%DISK_NUMBER%" "%WINDOWS_PARTITION_NUMBER%" "%DATA_PARTITION_NUMBER%"
if errorlevel 1 goto :disk_failed

:apply_image
call "%~dp0Windows\ApplyImage.bat" "%WIM_PATH%" "%WINDOWS_EDITION%"
if errorlevel 1 goto :image_failed

call "%~dp0Windows\SetupBoot.bat"
if errorlevel 1 goto :boot_failed

call :StageFirstBoot "%PAYLOAD_DIRECTORY%"
if errorlevel 1 goto :stage_failed

call "%~dp0Common.bat" :WriteLog INFO "Reboot required."
echo Windows installation complete. Restarting the computer.

if exist "%SystemRoot%\System32\wpeutil.exe" (
    wpeutil reboot
) else (
    shutdown /r /t 0
)

exit /b 0

:StageFirstBoot
set "STAGED_PAYLOAD=%~1"

if not exist "C:\Windows\System32\Config\SOFTWARE" (
    call "%~dp0Common.bat" :WriteLog ERROR "The applied Windows SOFTWARE hive was not found."
    exit /b 1
)

if not exist "C:\Phoenix" mkdir "C:\Phoenix" >nul 2>&1

xcopy "%STAGED_PAYLOAD%\*" "C:\Phoenix\" /E /I /H /R /Y >nul
if errorlevel 2 (
    call "%~dp0Common.bat" :WriteLog ERROR "Unable to copy the first-boot payload."
    exit /b 1
)

if not exist "C:\Phoenix\Logs" mkdir "C:\Phoenix\Logs" >nul 2>&1
copy /Y "%PHOENIX_LOG_FILE%" "C:\Phoenix\Logs\install.log" >nul

reg load "HKLM\PhoenixOffline" "C:\Windows\System32\Config\SOFTWARE" >nul 2>&1
if errorlevel 1 (
    call "%~dp0Common.bat" :WriteLog ERROR "Unable to load the offline SOFTWARE registry hive."
    exit /b 1
)

reg add "HKLM\PhoenixOffline\Microsoft\Windows\CurrentVersion\RunOnce" /v "PhoenixSetup" /t REG_SZ /d "C:\Phoenix\PhoenixSetup.exe --config C:\Phoenix\config.json" /f >nul 2>&1
set "REG_RESULT=%ERRORLEVEL%"
reg unload "HKLM\PhoenixOffline" >nul 2>&1

if not "%REG_RESULT%"=="0" (
    call "%~dp0Common.bat" :WriteLog ERROR "Unable to register PhoenixSetup in RunOnce."
    exit /b 1
)

call "%~dp0Common.bat" :WriteLog INFO "PhoenixSetup was registered for the first Windows boot."
copy /Y "%PHOENIX_LOG_FILE%" "C:\Phoenix\Logs\install.log" >nul
exit /b 0

:invalid_arguments
call "%~dp0Common.bat" :WriteLog ERROR "Invalid Install.bat arguments."
goto :failed

:disk_failed
call "%~dp0Common.bat" :WriteLog ERROR "Disk preparation failed."
goto :failed

:image_failed
call "%~dp0Common.bat" :WriteLog ERROR "DISM Apply-Image failed."
goto :failed

:boot_failed
call "%~dp0Common.bat" :WriteLog ERROR "Boot configuration failed."
goto :failed

:stage_failed
call "%~dp0Common.bat" :WriteLog ERROR "First-boot staging failed."

:failed
echo Installation failed. See "%PHOENIX_LOG_FILE%" for details.
exit /b 1
