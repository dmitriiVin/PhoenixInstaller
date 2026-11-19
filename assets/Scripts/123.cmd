@echo off
REM Разрешить подключения по RDP
REG ADD "HKLM\SYSTEM\CurrentControlSet\Control\Terminal Server" /v fDenyTSConnections /t REG_DWORD /d 0 /f
reg add "HKLM\SYSTEM\CurrentControlSet\Control\Terminal Server\WinStations\RDP-Tcp" /v UserAuthentication /t REG_DWORD /d 0 /f

REM Отключить спящий режим STANDBY
powercfg -change -standby-timeout-ac 0
REM Отключить гибернацию
powercfg -H OFF
REM Отключить остановку HDD
powercfg -change -disk-timeout-ac 0
powercfg -change -monitor-timeout-ac 20

REM Отключить UAC
REG ADD "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System" /v EnableLUA /t REG_DWORD /d 0 /f
REG ADD "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System" /v EnableLinkedConnections /t REG_DWORD /d 1 /f
REG ADD "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System" /v LocalAccountTokenFilterPolicy /t REG_DWORD /d 1 /f

REM DNS-суффиксы
REG ADD "HKLM\System\CurrentControlSet\Services\TCPIP\Parameters" /V SearchList /T Reg_SZ /D "regions.tax.nalog.ru,dpc.tax.nalog.ru,cafns.tax.nalog.ru,tax.nalog.ru" /f

REM Отключить SmartScreen
REG ADD "HKLM\SOFTWARE\Policies\Microsoft\Windows\System" /v EnableSmartScreen /t REG_DWORD /d 0 /f