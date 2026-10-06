@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Verify.ps1" -Mode Project %*
exit /b %errorlevel%
