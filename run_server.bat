@echo off
cd /d "%~dp0"
if not exist build\bin\Debug\DustwireMPServer.exe (
  echo Server binary not found. Run build.bat first.
  exit /b 1
)
build\bin\Debug\DustwireMPServer.exe
