@echo off
:: ============================================================================
:: Qt + MSVC2022 Environment Setup Script
:: Prepares a clean environment and runs qmake safely for Visual Studio builds
:: Author: ChatGPT for Timothy Peer
:: ============================================================================

:: Backup original PATH to restore after
set "OLD_PATH=%PATH%"

:: Temporarily reduce PATH to avoid "input line too long" error
set "PATH=C:\Windows\System32;C:\Windows"

:: Setup MSVC 2022 x64 environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo [ERROR] Failed to load Visual Studio environment.
    goto :end
)

:: Setup Qt 6.9.0 MSVC2022_64 environment
set "QT_DIR=C:\Qt\v6.9\6.9.0\msvc2022_64"
set "PATH=%QT_DIR%\bin;%PATH%"

:: Change to project directory (optional - customize as needed)
cd /d "C:\Users\tim\source\repos\vs2017\Claude\Alpha\Alpha Emulator\ABA"

:: Run qmake to generate Visual Studio .vcxproj
echo [INFO] Running qmake...
qmake qtvars.pro -tp vc -early "CONFIG -= debug release debug_and_release" "CONFIG += debug warn_off"

:: Restore original PATH
:end
set "PATH=%OLD_PATH%"
echo [INFO] Environment restored.
pause
