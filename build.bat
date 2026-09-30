@echo off
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"

if not exist logs mkdir logs

for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set TS=%%i
if not defined TS set TS=%DATE:/=-%_%TIME::=-%

set "BUILD_DIR=build-vs2026"
set "GENERATOR=Visual Studio 18 2026"
set "LOG=logs\build_%TS%.log"

(
    echo ============================================================
    echo DustwireMP build
    echo Started: %DATE% %TIME%
    echo Root: %CD%
    echo Generator: %GENERATOR%
    echo Build directory: %BUILD_DIR%
    echo ============================================================
    echo.
    echo [TOOLCHAIN]
    where cmake
    cmake --version
    where git
    git --version
    echo.
    echo [1/3] Configure CMake / fetch pinned ENet
) > "%LOG%"

echo [1/3] Configure CMake / fetch pinned ENet...
cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" -A x64 >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo [2/3] Build Debug...
echo. >> "%LOG%"
echo [2/3] Build Debug... >> "%LOG%"
cmake --build "%BUILD_DIR%" --config Debug --parallel >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo [3/3] Run protocol and ENet tests...
echo. >> "%LOG%"
echo [3/3] Run protocol and ENet tests... >> "%LOG%"
ctest --test-dir "%BUILD_DIR%" -C Debug --output-on-failure >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo.
echo ============================================================
echo BUILD OK
echo ============================================================
echo Log: %LOG%
echo Binaries: %BUILD_DIR%\bin\Debug\
exit /b 0

:fail
echo.
echo ============================================================
echo BUILD FAILED
echo ============================================================
echo Log: %LOG%
echo.
echo The complete CMake/compiler/test output was redirected to:
echo   %LOG%
echo.
echo Start at the first CMake, C++, linker, or test error in that file.
exit /b 1
