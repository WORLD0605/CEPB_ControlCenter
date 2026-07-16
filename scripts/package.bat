@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ==========================================
echo   CEPB Control Center release package
echo ==========================================

set "SOURCE_DIR=%~dp0.."
set "BUILD_DIR=%SOURCE_DIR%\build"
set "RELEASE_DIR=%SOURCE_DIR%\build-release"
set "APP_BIN_SRC=%SOURCE_DIR%\app_binaries"
set "APP_BIN_RELEASE=%RELEASE_DIR%\app_binaries"
set "SYSTEMD_SRC=%SOURCE_DIR%\scripts\systemd"
set "SYSTEMD_RELEASE=%RELEASE_DIR%\systemd"
set "DEVICE_SRC=%SOURCE_DIR%\scripts\device"
set "DEVICE_RELEASE=%RELEASE_DIR%\device"
set "APP_VERSION=unknown"

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [ERROR] Cannot find %BUILD_DIR%\CMakeCache.txt
    echo         Build the project once before packaging.
    pause
    exit /b 1
)

echo [1/8] Reading build configuration...
for /f "tokens=2 delims==" %%a in ('type "%BUILD_DIR%\CMakeCache.txt" ^| findstr "^CMAKE_PREFIX_PATH:STRING="') do set "QT_PREFIX=%%a"
for /f "tokens=2 delims==" %%a in ('type "%BUILD_DIR%\CMakeCache.txt" ^| findstr "^CMAKE_CXX_COMPILER:FILEPATH="') do set "CXX_COMPILER=%%a"
for /f "tokens=2 delims==" %%a in ('type "%BUILD_DIR%\CMakeCache.txt" ^| findstr "^CMAKE_COMMAND:INTERNAL="') do set "CMAKE_EXE=%%a"
for /f "tokens=2 delims==" %%a in ('type "%BUILD_DIR%\CMakeCache.txt" ^| findstr "^CMAKE_MAKE_PROGRAM:FILEPATH="') do set "NINJA_EXE=%%a"

if "%CMAKE_EXE%"=="" (
    for /f "delims=" %%a in ('where cmake 2^>nul') do if "!CMAKE_EXE!"=="" set "CMAKE_EXE=%%a"
)
if "%NINJA_EXE%"=="" (
    for /f "delims=" %%a in ('where ninja 2^>nul') do if "!NINJA_EXE!"=="" set "NINJA_EXE=%%a"
)

if "%QT_PREFIX%"=="" (
    echo [ERROR] Cannot resolve Qt path from CMakeCache.txt.
    pause
    exit /b 1
)
if "%CMAKE_EXE%"=="" (
    echo [ERROR] Cannot resolve cmake.exe from CMakeCache.txt or PATH.
    pause
    exit /b 1
)
if "%NINJA_EXE%"=="" (
    echo [ERROR] Cannot resolve ninja.exe from CMakeCache.txt or PATH.
    pause
    exit /b 1
)

echo         Qt:       %QT_PREFIX%
echo         Compiler: %CXX_COMPILER%
echo         CMake:    %CMAKE_EXE%
echo         Ninja:    %NINJA_EXE%

for /f "tokens=3" %%a in ('findstr /R /C:"project(CEPB_ControlCenter VERSION" "%SOURCE_DIR%\CMakeLists.txt"') do set "APP_VERSION=%%a"
echo         Version:  %APP_VERSION%

echo [2/8] Building Release...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"
"%CMAKE_EXE%" -B "%RELEASE_DIR%" -S "%SOURCE_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_PREFIX%" -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%"
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    pause
    exit /b 1
)
"%CMAKE_EXE%" --build "%RELEASE_DIR%"
if errorlevel 1 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo [3/8] Deploying Qt runtime...
set "PATH=%QT_PREFIX%\bin;%PATH%"
"%QT_PREFIX%\bin\windeployqt.exe" --release --no-translations --no-compiler-runtime "%RELEASE_DIR%\CEPB_ControlCenter.exe"
if errorlevel 1 (
    echo [WARN] windeployqt returned a non-zero exit code; checking deployment output...
)

if not exist "%RELEASE_DIR%\platforms\qwindows.dll" (
    echo [WARN] Qt platform plugin was not deployed; copying common runtime files manually...
    copy /Y "%QT_PREFIX%\bin\Qt6Core.dll" "%RELEASE_DIR%\" >nul
    copy /Y "%QT_PREFIX%\bin\Qt6Gui.dll" "%RELEASE_DIR%\" >nul
    copy /Y "%QT_PREFIX%\bin\Qt6Network.dll" "%RELEASE_DIR%\" >nul
    copy /Y "%QT_PREFIX%\bin\Qt6Widgets.dll" "%RELEASE_DIR%\" >nul
    if exist "%QT_PREFIX%\bin\Qt6Svg.dll" copy /Y "%QT_PREFIX%\bin\Qt6Svg.dll" "%RELEASE_DIR%\" >nul
    for %%d in (generic iconengines imageformats networkinformation platforminputcontexts platforms styles tls) do (
        if exist "%QT_PREFIX%\plugins\%%d" (
            if not exist "%RELEASE_DIR%\%%d" mkdir "%RELEASE_DIR%\%%d"
            copy /Y "%QT_PREFIX%\plugins\%%d\*.dll" "%RELEASE_DIR%\%%d\" >nul
        )
    )
)

echo [4/8] Copying compiler runtime...
for %%i in ("%CXX_COMPILER%") do set "COMPILER_BIN_DIR=%%~dpi"
for %%r in (libc++.dll libunwind.dll libwinpthread-1.dll libstdc++-6.dll libgcc_s_seh-1.dll libgcc_s_dw2-1.dll) do (
    if exist "%COMPILER_BIN_DIR%\%%r" (
        copy /Y "%COMPILER_BIN_DIR%\%%r" "%RELEASE_DIR%\" >nul
    )
)

echo [5/8] Adding README...
copy /Y "%~dp0README_RELEASE.txt" "%RELEASE_DIR%\README.txt" >nul

echo [6/8] Adding APP upgrade binaries...
if not exist "%APP_BIN_RELEASE%" mkdir "%APP_BIN_RELEASE%"
if exist "%APP_BIN_SRC%\README.md" copy /Y "%APP_BIN_SRC%\README.md" "%APP_BIN_RELEASE%\" >nul
for %%a in (North_CEP North_101 North_104 North_Mqtt LogicCenter South_Modbus South_104 South_645) do (
    if exist "%APP_BIN_SRC%\%%a" (
        copy /Y "%APP_BIN_SRC%\%%a" "%APP_BIN_RELEASE%\" >nul
        echo         [OK] %%a
    ) else (
        echo         [WARN] Missing %APP_BIN_SRC%\%%a
    )
)

echo       Adding APP systemd services...
if not exist "%SYSTEMD_RELEASE%" mkdir "%SYSTEMD_RELEASE%"
for %%s in ("%SYSTEMD_SRC%\*.service") do (
    copy /Y "%%~fs" "%SYSTEMD_RELEASE%\" >nul
    echo         [OK] %%~nxs
)

echo       Adding device network tools...
if not exist "%DEVICE_RELEASE%" mkdir "%DEVICE_RELEASE%"
copy /Y "%DEVICE_SRC%\cepb-network-apply" "%DEVICE_RELEASE%\" >nul
copy /Y "%DEVICE_SRC%\install-cepb-network" "%DEVICE_RELEASE%\" >nul
copy /Y "%DEVICE_SRC%\network.conf.example" "%DEVICE_RELEASE%\" >nul

echo [7/8] Cleaning build files...
rmdir /s /q "%RELEASE_DIR%\CMakeFiles" 2>nul
rmdir /s /q "%RELEASE_DIR%\CEPB_ControlCenter_autogen" 2>nul
rmdir /s /q "%RELEASE_DIR%\.qt" 2>nul
del "%RELEASE_DIR%\CMakeCache.txt" 2>nul
del "%RELEASE_DIR%\build.ninja" 2>nul
del "%RELEASE_DIR%\cmake_install.cmake" 2>nul
del "%RELEASE_DIR%\CPackConfig.cmake" 2>nul
del "%RELEASE_DIR%\CPackSourceConfig.cmake" 2>nul
del "%RELEASE_DIR%\.ninja_deps" 2>nul
del "%RELEASE_DIR%\.ninja_log" 2>nul
del "%RELEASE_DIR%\compile_commands.json" 2>nul

echo [8/8] Creating zip...
set "ZIP_NAME=CEPB_ControlCenter_v%APP_VERSION%_Release.zip"
cd /d "%SOURCE_DIR%"
if exist "%ZIP_NAME%" del "%ZIP_NAME%"
powershell -NoProfile -Command "Compress-Archive -Path '%RELEASE_DIR%\*' -DestinationPath '%ZIP_NAME%' -Force"

echo ==========================================
echo   Package complete.
echo   File: %SOURCE_DIR%\%ZIP_NAME%
echo   Contents:
dir /b "%RELEASE_DIR%"
echo ==========================================
if /I not "%CEPB_NO_PAUSE%"=="1" pause
