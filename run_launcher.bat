@echo off
setlocal
cd /d "%~dp0"

set "EXE=build-vs2026\bin\Debug\DustwireMPLauncher.exe"
set "LOG=%~dp0logs\launcher.log"

if not exist "%EXE%" (
  echo Launcher binary not found:
  echo   %EXE%
  echo.
  echo Run build.bat first.
  exit /b 1
)

echo DustwireMP launcher:
echo   %EXE%
echo.
echo Launcher log:
echo   %LOG%
echo.

"%EXE%"
set "RC=%ERRORLEVEL%"

echo.
echo Launcher exit code: %RC%
echo Log: %LOG%
exit /b %RC%
