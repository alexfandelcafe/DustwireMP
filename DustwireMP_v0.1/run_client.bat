@echo off
cd /d "%~dp0"
if not exist build\bin\Debug\DustwireMPClient.exe (
  echo Client binary not found. Run build.bat first.
  exit /b 1
)
build\bin\Debug\DustwireMPClient.exe
