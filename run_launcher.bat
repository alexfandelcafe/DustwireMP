@echo off
setlocal
cd /d "%~dp0"

set "EXE=build-vs2026\bin\Debug\DustwireMPLauncher.exe"

if not exist "%EXE%" (
  echo Launcher binary not found:
  echo   %EXE%
  echo.
  echo Run build.bat first.
  exit /b 1
)

"%EXE%"
exit /b %ERRORLEVEL%
