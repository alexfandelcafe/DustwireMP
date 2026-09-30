@echo off
setlocal
cd /d "%~dp0"

set "EXE=build-vs2026\bin\Debug\DustwireMPBuildProbe.exe"

if not exist "%EXE%" (
  echo Build probe binary not found:
  echo   %EXE%
  echo.
  echo Run build.bat first.
  pause
  exit /b 1
)

if "%~1"=="" (
  echo.
  set /p "RDR_PATH=Enter the full path to RDR.exe: "
  echo.
  if "%RDR_PATH%"=="" (
    echo No RDR.exe path supplied.
    pause
    exit /b 2
  )
  "%EXE%" "%RDR_PATH%"
) else (
  "%EXE%" "%~1"
)

set "RC=%ERRORLEVEL%"

echo.
echo Probe exit code: %RC%
echo.
echo This tool only reads the executable file.
echo It does not launch RDR.exe and does not inject DustwireMPClientModule.dll.
echo.
pause
exit /b %RC%
