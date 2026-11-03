@echo off
setlocal

echo [findFD Build Script]
echo Finding Visual Studio environment...

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Error: vswhere.exe not found. Visual Studio might not be installed.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

if "%VS_PATH%"=="" (
    echo Error: Visual Studio C++ toolset was not found.
    exit /b 1
)

echo Visual Studio Path: %VS_PATH%
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"

echo Compiling findFD (Release Build)...
cl.exe /std:c++17 /EHsc /utf-8 /W4 /O2 /Oi /Ot /GL /DNDEBUG /MT /Fe:findFD.exe main.cpp /link /LTCG /OPT:REF /OPT:ICF

if %ERRORLEVEL% equ 0 (
    echo.
    echo ========================================
    echo Build Succeeded: findFD.exe
    echo ========================================
    del /q main.obj >nul 2>&1
) else (
    echo.
    echo Build Failed!
    exit /b %ERRORLEVEL%
)

