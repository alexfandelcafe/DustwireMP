@echo off
setlocal
cd /d "%~dp0"

set "EXE=build-vs2026\bin\Debug\DustwireMPBuildProbe.exe"

if not exist "%EXE%" (
  echo Build probe binary not found:
  echo   %EXE%
  echo.
  echo Run build.bat first.
  exit /b 1
)

if "%~1"=="" (
  "%EXE%" "%~dp0RDR.exe"
) else (
  "%EXE%" "%~1"
)

echo.
echo Probe output above.
echo.
echo This tool only reads the executable file; it does not launch RDR.exe
echo and does not inject DustwireMPClientModule.dll.
exit /b %ERRORLEVEL%
