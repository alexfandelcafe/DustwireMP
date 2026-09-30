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
  pause
  exit /b 1
)

echo DustwireMP launcher:
echo   %EXE%
echo.

if "%~1"=="" (
  echo Usage:
  echo   run_launcher.bat "C:\path\to\RDR.exe"
  echo.
  echo.
  "%EXE%"
) else (
  echo RDR.exe:
  echo   %~1
  echo.
  "%EXE%" "%~1"
)

set "RC=%ERRORLEVEL%"

echo.
echo Launcher exit code: %RC%
echo Log:
echo   %LOG%
echo.
pause
exit /b %RC%
