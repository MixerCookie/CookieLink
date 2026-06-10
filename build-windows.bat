@echo off
chcp 65001 >nul
setlocal

set "VERSION=v1.0.0"
set "BUILD_DIR=build-release-windows"
set "AAX_SDK_PATH=%~dp0aax-sdk-2-9-0"
set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
set "WRAPTOOL=C:\Program Files (x86)\PACEAntiPiracy\Eden\Fusion\Versions\5\wraptool.exe"

cd /d "%~dp0"

echo ==========================================
echo CookieLink Windows Build Script
echo ==========================================

REM Check VS BuildTools
if not exist "%VS_PATH%" (
    echo ERROR: Visual Studio BuildTools not found
    pause
    exit /b 1
)
echo [OK] Visual Studio BuildTools found

REM Check AAX SDK
if not exist "%AAX_SDK_PATH%" (
    echo ERROR: AAX SDK not found at %AAX_SDK_PATH%
    pause
    exit /b 1
)
echo [OK] AAX SDK found

REM Find NSIS
set "MAKENSIS="
where makensis >nul 2>&1
if not errorlevel 1 (
    set "MAKENSIS=makensis"
) else (
    if exist "C:\Program Files (x86)\NSIS\makensis.exe" (
        set "MAKENSIS=C:\Program Files (x86)\NSIS\makensis.exe"
    ) else if exist "C:\Program Files\NSIS\makensis.exe" (
        set "MAKENSIS=C:\Program Files\NSIS\makensis.exe"
    )
)
if defined MAKENSIS (
    echo [OK] NSIS found
) else (
    echo [WARN] NSIS not found, installer packaging will be skipped
)

REM Check wraptool
if exist "%WRAPTOOL%" (
    echo [OK] PACE wraptool found
) else (
    echo [WARN] PACE wraptool not found, AAX signing will be skipped
)

echo.
echo Step 1: Configure CMake
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64

cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DAAX_SDK_PATH="%AAX_SDK_PATH%" -DCMAKE_CXX_FLAGS="/utf-8" -DCMAKE_C_FLAGS="/utf-8"

if errorlevel 1 (
    echo ERROR: CMake configuration failed
    pause
    exit /b 1
)

echo.
echo Step 2: Build all targets
cmake --build "%BUILD_DIR%" --target CookieLink_All --config Release --parallel

if errorlevel 1 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo Step 3: Sign Windows AAX
set "AAX_PATH=%BUILD_DIR%\CookieLink_artefacts\Release\AAX\CookieLink.aaxplugin"
if not exist "%AAX_PATH%" (
    echo [SKIP] AAX plugin not found
    goto :package
)
if not exist "%WRAPTOOL%" (
    echo [SKIP] PACE wraptool not found
    goto :package
)

if not exist "%~dp0pace-credentials.bat" (
    echo [SKIP] pace-credentials.bat not found - create it next to this script with:
    echo   set "PACE_ACCOUNT=your-account"
    echo   set "PACE_PASSWORD=your-password"
    echo   set "PACE_WCGUID=your-wcguid"
    echo   set "PACE_SIGNID=your-cert-thumbprint"
    goto :package
)
call "%~dp0pace-credentials.bat"

"%WRAPTOOL%" sign --verbose --account "%PACE_ACCOUNT%" --password "%PACE_PASSWORD%" --wcguid "%PACE_WCGUID%" --signid "%PACE_SIGNID%" --in "%AAX_PATH%" --out "%AAX_PATH%"

if errorlevel 1 (
    echo.
    echo ERROR: AAX signing failed. Check that:
    echo   1. CookieSign.p12 certificate is imported into Windows cert store
    echo   2. iLok is connected to this PC or iLok Cloud session is open
    echo.
    pause
    exit /b 1
)
echo [OK] AAX signed

:package
if not defined MAKENSIS (
    echo.
    echo [SKIP] NSIS not found, no installer created
    goto :done
)

echo.
echo Step 4: Package installer
if not exist dist mkdir dist
"%MAKENSIS%" /DVERSION=%VERSION% /DBUILD_DIR=%BUILD_DIR% /DOUTFILE=dist\CookieLink-%VERSION%-windows-x64-installer.exe packaging\windows_cookie_link.nsi

if errorlevel 1 (
    echo ERROR: NSIS packaging failed
    pause
    exit /b 1
)

echo.
echo ==========================================
echo SUCCESS!
echo Installer: %CD%\dist\CookieLink-%VERSION%-windows-x64-installer.exe
echo ==========================================

:done
echo.
pause