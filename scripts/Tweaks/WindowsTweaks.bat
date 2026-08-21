@echo off
setlocal EnableExtensions DisableDelayedExpansion

call "%~dp0..\Common.bat" :InitializeLog
call "%~dp0..\Common.bat" :WriteLog WARNING "No policy-specific Windows tweaks are defined in this project yet."
exit /b 0
