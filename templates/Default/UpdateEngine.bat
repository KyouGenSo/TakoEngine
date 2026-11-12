@echo off
rem Installs or updates TakoEngine. See the top of UpdateEngine.ps1 for the options
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0UpdateEngine.ps1" %*
pause
