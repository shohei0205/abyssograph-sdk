@echo off
setlocal enabledelayedexpansion

rem Abyssograph add-on SDK -- build the samples.
rem
rem Usage: build.cmd [Debug|Release]    (default: Release)
rem
rem The DLLs land in bin\. Copy them next to AbyssographHost.dll to try them out.
rem
rem CMake is taken from the Visual Studio installation first, because that copy
rem always knows the generator for the Visual Studio it ships with. A CMake on
rem PATH is only used when Visual Studio cannot be found -- an older CMake may
rem otherwise pick a generator for a Visual Studio you no longer build with.

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

cd /d "%~dp0"

set "CMAKE=cmake"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%i"
)
if defined VSPATH (
    set "VSCMAKE=!VSPATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if exist "!VSCMAKE!" set "CMAKE=!VSCMAKE!"
)

rem -A x64 needs a Visual Studio generator (the default on Windows).
"%CMAKE%" -S . -B build -A x64
if errorlevel 1 exit /b 1

"%CMAKE%" --build build --config %CONFIG%
if errorlevel 1 exit /b 1

echo.
echo Built %CONFIG% into "%~dp0bin".
exit /b 0
