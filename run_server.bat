@echo off
setlocal
cd /d "%~dp0"

set "EXE=build-vs2026\bin\Debug\DustwireMPServer.exe"

if not exist "%EXE%" (
  echo Server binary not found:
  echo   %EXE%
  echo.
  echo Run build.bat first.
  exit /b 1
)

"%EXE%"
exit /b %ERRORLEVEL%
