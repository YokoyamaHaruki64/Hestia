@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Verify.ps1" -Mode Solution %*
exit /b %errorlevel%
