@echo off
rem Starts the song engine and leaves it running. Everything it prints goes to the log the plugin shows.
rem   start_engine_windows.bat "<engine folder>" <host> <port> "<log file>"
setlocal
set "DIR=%~1"
set "HOST=%~2"
set "PORT=%~3"
set "LOG=%~4"
if "%HOST%"=="" set "HOST=127.0.0.1"
if "%PORT%"=="" set "PORT=8001"
if "%LOG%"=="" set "LOG=%TEMP%\mu-engine.log"
set "PATH=%USERPROFILE%\.local\bin;%LOCALAPPDATA%\Microsoft\WinGet\Links;%PATH%"
set "TOKENIZERS_PARALLELISM=false"
if "%ACESTEP_INIT_LLM%"=="" set "ACESTEP_INIT_LLM=auto"

echo Starting the engine: %DATE% %TIME%> "%LOG%"
cd /d "%DIR%" 2>>"%LOG%"
if errorlevel 1 goto nofolder

if exist "%DIR%\python_embedded\python.exe" goto embedded
if exist "%DIR%\python_embeded\python.exe" goto embeded

where uv >nul 2>&1
if errorlevel 1 goto nouv
if exist "%DIR%\.venv" goto run
echo Finishing the engine's setup...>> "%LOG%"
uv sync >> "%LOG%" 2>&1
if errorlevel 1 goto nosync
:run
uv run --no-sync acestep-api --host %HOST% --port %PORT% >> "%LOG%" 2>&1
exit /b %ERRORLEVEL%

:embedded
"%DIR%\python_embedded\python.exe" "%DIR%\acestep\api_server.py" --host %HOST% --port %PORT% >> "%LOG%" 2>&1
exit /b %ERRORLEVEL%
:embeded
"%DIR%\python_embeded\python.exe" "%DIR%\acestep\api_server.py" --host %HOST% --port %PORT% >> "%LOG%" 2>&1
exit /b %ERRORLEVEL%

:nofolder
echo The engine folder is missing: %DIR%>> "%LOG%"
exit /b 1
:nouv
echo The Python manager, uv, is missing. Run Set up the engine in Settings.>> "%LOG%"
exit /b 1
:nosync
echo The engine's setup did not finish. Run Set up the engine in Settings.>> "%LOG%"
exit /b 1
