@echo off
setlocal
rem Runs the Release build with --profile: frame times at the top left, FPS at the top right
set "folder=%~dp0Generated\outputs\Release"
if exist "%folder%\__ProjectName__.exe" goto run
echo "%folder%\__ProjectName__.exe" was not found. Build the Release configuration first
pause
exit /b 1
:run
start "" /d "%folder%" "%folder%\__ProjectName__.exe" --profile
