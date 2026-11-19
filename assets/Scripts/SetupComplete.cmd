@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ============================================================
REM  Windows 10 22H2 post-install script (pure ASCII, CRLF)
REM  Deployed by $OEM$ to: C:\Windows\Setup\Scripts\setup.cmd
REM
REM  Modes:
REM    (no arg)  -> specialize pass, SYSTEM context.
REM                 Renames built-in Administrator (SID -500) to
REM                 "systemsupport", sets password, enables account,
REM                 applies RDP/UAC/power/DNS/SmartScreen tuning.
REM    firstlogon -> oobeSystem FirstLogonCommands, user context.
REM                 Disables AutoLogon (exactly 1 auto-login),
REM                 launches PSetup if present.
REM
REM  Log: C:\Windows\Temp\setup.log
REM ============================================================

set "LOG=C:\Windows\Temp\setup.log"
set "ACC=systemsupport"
set "PWD=12345678"
set "PSETUP=C:\Programs\PostInstall\PSetup.exe"

if /I "%~1"=="firstlogon" goto :firstlogon

cd /d %SystemDrive%\

>>"%LOG%" echo ==================================================
>>"%LOG%" echo [specialize] start: %DATE% %TIME%
>>"%LOG%" echo cwd=%CD% sysdrive=%SystemDrive% windir=%WINDIR%
whoami >>"%LOG%" 2>&1

REM ---- rename built-in Administrator -> systemsupport ----
REM ---- account is always selected by SID -500, so the ----
REM ---- localized name (Russian) does not matter        ----
net user %ACC% >nul 2>&1
if not errorlevel 1 (
  >>"%LOG%" echo [1] account %ACC% already exists
  goto :have_account
)

>>"%LOG%" echo [2] try Rename-LocalUser by SID
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { $a = Get-LocalUser | Where-Object { $_.SID.Value -like '*-500' }; if (-not $a) { throw 'no SID -500 account found' }; Rename-LocalUser -Name $a.Name -NewName '%ACC%' -ErrorAction Stop; 'RENAMED: ' + $a.Name } catch { 'PS-ERROR: ' + $_.Exception.Message }" >>"%LOG%" 2>&1
net user %ACC% >nul 2>&1
if not errorlevel 1 goto :have_account

>>"%LOG%" echo [3] try wmic rename by SID
wmic useraccount where "sid like '%-500'" call rename name="%ACC%" >>"%LOG%" 2>&1
>>"%LOG%" echo [3] wmic exit=!errorlevel!
net user %ACC% >nul 2>&1
if not errorlevel 1 goto :have_account

>>"%LOG%" echo [4] WARN: rename failed, creating separate admin account
net user %ACC% %PWD% /add /expires:never >>"%LOG%" 2>&1
powershell -NoProfile -ExecutionPolicy Bypass -Command "Add-LocalGroupMember -SID S-1-5-32-544 -Member '%ACC%'" >>"%LOG%" 2>&1

:have_account

REM ---- password, enable account, never expire ----
net user %ACC% %PWD% /active:yes /expires:never >>"%LOG%" 2>&1
wmic useraccount where "name='%ACC%'" set PasswordExpires=false >>"%LOG%" 2>&1

REM ---- allow RDP ----
REG ADD "HKLM\SYSTEM\CurrentControlSet\Control\Terminal Server" /v fDenyTSConnections /t REG_DWORD /d 0 /f >>"%LOG%" 2>&1

REM ---- power plan ----
powercfg -change -standby-timeout-ac 0 >>"%LOG%" 2>&1
powercfg -change -disk-timeout-ac 0 >>"%LOG%" 2>&1
powercfg -change -monitor-timeout-ac 20 >>"%LOG%" 2>&1
powercfg -h off >>"%LOG%" 2>&1

REM ---- UAC ----
REG ADD "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System" /v EnableLUA /t REG_DWORD /d 0 /f >>"%LOG%" 2>&1
REG ADD "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System" /v EnableLinkedConnections /t REG_DWORD /d 1 /f >>"%LOG%" 2>&1
REG ADD "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System" /v LocalAccountTokenFilterPolicy /t REG_DWORD /d 1 /f >>"%LOG%" 2>&1

REM ---- DNS suffixes ----
REG ADD "HKLM\System\CurrentControlSet\Services\TCPIP\Parameters" /v SearchList /t REG_SZ /d "regions.tax.nalog.ru,dpc.tax.nalog.ru,cafns.tax.nalog.ru,tax.nalog.ru" /f >>"%LOG%" 2>&1

REM ---- SmartScreen off ----
REG ADD "HKLM\SOFTWARE\Policies\Microsoft\Windows\System" /v EnableSmartScreen /t REG_DWORD /d 0 /f >>"%LOG%" 2>&1

>>"%LOG%" echo [specialize] done: %DATE% %TIME%
exit /b 0

:firstlogon
>>"%LOG%" echo ==================================================
>>"%LOG%" echo [firstlogon] start: %DATE% %TIME%
whoami >>"%LOG%" 2>&1

REM ---- disable AutoLogon: official workaround for the ----
REM ---- LogonCount +1 known issue (exactly 1 auto-login) ----
REG ADD "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v AutoLogonCount /t REG_DWORD /d 0 /f >>"%LOG%" 2>&1
REG ADD "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v AutoAdminLogon /t REG_SZ /d 0 /f >>"%LOG%" 2>&1
REM ---- keep DefaultPassword and DefaultUserName intact ----

REM ---- launch PSetup if present ----
if exist "%PSETUP%" (
  >>"%LOG%" echo [firstlogon] launching PSetup
  start "" "%PSETUP%"
) else (
  >>"%LOG%" echo [firstlogon] PSetup not found: %PSETUP%
)

>>"%LOG%" echo [firstlogon] done: %DATE% %TIME%
exit /b 0
