@echo off
setlocal EnableExtensions

cd /d "%~dp0"
if not exist logs mkdir logs

for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set TS=%%i
set LOG=logs\build_%TS%.log

echo ============================================== > "%LOG%"
echo DustwireMP build started %DATE% %TIME% >> "%LOG%"
echo ============================================== >> "%LOG%"

echo [1/2] Configuring CMake...
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo [2/2] Building Debug...
cmake --build build --config Debug >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo.
echo BUILD OK
echo Log: %LOG%
echo Binaries: build\bin\Debug\
exit /b 0

:fail
echo.
echo BUILD FAILED
 echo Log: %LOG%
echo Open the log above and fix the first real compiler/configuration error.
exit /b 1
