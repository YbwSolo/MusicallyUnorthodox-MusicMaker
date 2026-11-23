@echo off
rem Sets up the song engine (ACE-Step 1.5) that MU Music Maker uses. Run by the plugin in its own window.
rem   install_engine_windows.bat "<folder to install into>"
setlocal
title MU Music Maker - setting up the song engine
set "DIR=%~1"
set "ENGINE_REF=ca1e85fe9430179831e6bc6be790c332190a3866"
set "PATH=%USERPROFILE%\.local\bin;%LOCALAPPDATA%\Microsoft\WinGet\Links;%PATH%"

echo.
echo MU Music Maker: setting up the song engine
echo The first time, this downloads about 10 GB, so it can take a while.
echo Leave this window open until it says All done.
echo.

if "%DIR%"=="" goto nofolder
if not exist "%DIR%" mkdir "%DIR%"
if not exist "%DIR%" goto nofolder

echo Step 1 of 4: the Python manager, uv
where uv >nul 2>&1
if not errorlevel 1 goto haveuv
powershell -NoProfile -ExecutionPolicy Bypass -Command "irm https://astral.sh/uv/install.ps1 | iex"
if errorlevel 1 goto nouv
set "PATH=%USERPROFILE%\.local\bin;%PATH%"
where uv >nul 2>&1
if errorlevel 1 goto nouv
:haveuv
echo Ready.
echo.

echo Step 2 of 4: downloading the engine
set "TMPDIR=%TEMP%\mu-engine-download"
if exist "%TMPDIR%" rmdir /s /q "%TMPDIR%"
mkdir "%TMPDIR%"
curl.exe -L --fail --progress-bar -o "%TMPDIR%\engine.zip" "https://github.com/ace-step/ACE-Step-1.5/archive/%ENGINE_REF%.zip"
if errorlevel 1 goto nodownload
tar.exe -xf "%TMPDIR%\engine.zip" -C "%TMPDIR%"
if errorlevel 1 goto nodownload
set "SRC="
for /d %%D in ("%TMPDIR%\ACE-Step-1.5*") do set "SRC=%%D"
if "%SRC%"=="" goto nodownload
if not exist "%SRC%\pyproject.toml" goto nodownload
rem copied over the top, so models that are already there are kept
xcopy "%SRC%\*" "%DIR%\" /E /Y /Q /I >nul
if errorlevel 1 goto nocopy
rmdir /s /q "%TMPDIR%"
cd /d "%DIR%"
echo.

echo Step 3 of 4: installing what the engine needs
uv sync
if errorlevel 1 goto nosync
echo.

echo Step 4 of 4: downloading the models, the big part
uv run --no-sync acestep-download
if errorlevel 1 goto nomodels

echo %DATE% %TIME%> "%DIR%\.mu-installed"
echo.
echo All done.
echo Go back to MU Music Maker and press Start engine. You can close this window.
echo.
pause
exit /b 0

:nofolder
echo Could not create the folder to install into.
goto failed
:nouv
echo Could not install uv. Check the internet connection and run this again.
goto failed
:nodownload
echo The engine could not be downloaded. Check the internet connection and run this again.
goto failed
:nocopy
echo Could not copy the engine into %DIR%
goto failed
:nosync
echo The engine's parts could not be installed. Run this again; it carries on from where it stopped.
goto failed
:nomodels
echo The models could not be downloaded. Run this again; it carries on from where it stopped.
goto failed
:failed
echo.
pause
exit /b 1
