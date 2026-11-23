@echo off
rem Stops whatever is answering on the engine's port.
rem   stop_engine_windows.bat <port>
setlocal
set "PORT=%~1"
if "%PORT%"=="" set "PORT=8001"
for /f "tokens=5" %%a in ('netstat -ano ^| findstr /r /c:":%PORT% .*LISTENING"') do taskkill /F /T /PID %%a >nul 2>&1
exit /b 0
