@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "PACKAGE_ID=%~1"
set "INSTALLER_PATH=%~2"
set "INSTALLER_ARGUMENTS=%~3"

call "%~dp0..\Common.bat" :InitializeLog

if "%PACKAGE_ID%"=="" goto :invalid_arguments
if "%INSTALLER_PATH%"=="" goto :invalid_arguments

if not exist "%INSTALLER_PATH%" (
    call "%~dp0..\Common.bat" :WriteLog ERROR "Installer for package %PACKAGE_ID% is missing."
    exit /b 1
)

call "%~dp0..\Common.bat" :WriteLog INFO "Installing package %PACKAGE_ID%."
start "Phoenix package: %PACKAGE_ID%" /wait "%INSTALLER_PATH%" %INSTALLER_ARGUMENTS%
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    call "%~dp0..\Common.bat" :WriteLog ERROR "Package %PACKAGE_ID% failed. ErrorLevel=%RESULT%"
    exit /b %RESULT%
)

call "%~dp0..\Common.bat" :WriteLog INFO "Package %PACKAGE_ID% installed successfully."
exit /b 0

:invalid_arguments
call "%~dp0..\Common.bat" :WriteLog ERROR "InstallPrograms requires a package id and installer path."
exit /b 1
