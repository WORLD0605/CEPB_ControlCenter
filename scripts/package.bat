@echo off
setlocal enabledelayedexpansion

echo ==========================================
echo   CEPB Control Center 发布打包脚本
echo ==========================================

set "SOURCE_DIR=%~dp0.."
set "BUILD_DIR=%SOURCE_DIR%\build"
set "RELEASE_DIR=%SOURCE_DIR%\build-release"
set "APP_VERSION=unknown"

:: 检查 build 目录是否存在
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [错误] 找不到 %BUILD_DIR%\CMakeCache.txt
    echo        请先成功构建一次工程，再运行此脚本。
    pause
    exit /b 1
)

:: 从 CMakeCache.txt 解析工具路径
echo [1/6] 读取构建配置...
for /f "tokens=2 delims==" %%a in ('type "%BUILD_DIR%\CMakeCache.txt" ^| findstr "^CMAKE_PREFIX_PATH:STRING="') do set "QT_PREFIX=%%a"
for /f "tokens=2 delims==" %%a in ('type "%BUILD_DIR%\CMakeCache.txt" ^| findstr "^CMAKE_CXX_COMPILER:FILEPATH="') do set "CXX_COMPILER=%%a"
for /f "tokens=2 delims==" %%a in ('type "%BUILD_DIR%\CMakeCache.txt" ^| findstr "^CMAKE_COMMAND:INTERNAL="') do set "CMAKE_EXE=%%a"
for /f "tokens=2 delims==" %%a in ('type "%BUILD_DIR%\CMakeCache.txt" ^| findstr "^CMAKE_MAKE_PROGRAM:FILEPATH="') do set "NINJA_EXE=%%a"

if "%QT_PREFIX%"=="" (
    echo [错误] 无法从 CMakeCache.txt 中解析 Qt 路径。
    pause
    exit /b 1
)

echo        Qt 路径:  %QT_PREFIX%
echo        编译器:   %CXX_COMPILER%
echo        CMake:    %CMAKE_EXE%
echo        Ninja:    %NINJA_EXE%

for /f "tokens=3" %%a in ('findstr /R /C:"project(CEPB_ControlCenter VERSION" "%SOURCE_DIR%\CMakeLists.txt"') do set "APP_VERSION=%%a"
echo        版本号:   %APP_VERSION%

:: 编译 Release
echo [2/6] 编译 Release 版本...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"
"%CMAKE_EXE%" -B "%RELEASE_DIR%" -S "%SOURCE_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_PREFIX%" -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%"
if errorlevel 1 (
    echo [错误] CMake 配置失败。
    pause
    exit /b 1
)
"%CMAKE_EXE%" --build "%RELEASE_DIR%"
if errorlevel 1 (
    echo [错误] 编译失败。
    pause
    exit /b 1
)

:: 部署 Qt 依赖
echo [3/6] 部署 Qt 依赖...
set "PATH=%QT_PREFIX%\bin;%PATH%"
"%QT_PREFIX%\bin\windeployqt.exe" --release --no-translations --no-compiler-runtime "%RELEASE_DIR%\CEPB_ControlCenter.exe"
if errorlevel 1 (
    echo [警告] windeployqt 返回非零退出码，检查部署结果...
)

if not exist "%RELEASE_DIR%\platforms\qwindows.dll" (
    echo [警告] Qt 平台插件未部署，执行手动依赖部署...
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

:: 复制编译器运行时库
echo [4/6] 复制编译器运行时库...
for %%i in ("%CXX_COMPILER%") do set "COMPILER_BIN_DIR=%%~dpi"
copy /Y "%COMPILER_BIN_DIR%\libc++.dll" "%RELEASE_DIR%\" >nul
copy /Y "%COMPILER_BIN_DIR%\libunwind.dll" "%RELEASE_DIR%\" >nul
copy /Y "%COMPILER_BIN_DIR%\libwinpthread-1.dll" "%RELEASE_DIR%\" >nul

:: 复制说明文件
echo [5/7] 添加说明文件...
copy /Y "%~dp0README_RELEASE.txt" "%RELEASE_DIR%\README.txt" >nul

:: 清理构建系统中间文件
echo [6/7] 清理构建中间文件...
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

:: 打包
echo [7/7] 打包...
set "ZIP_NAME=CEPB_ControlCenter_v%APP_VERSION%_Release.zip"
cd /d "%SOURCE_DIR%"
if exist "%ZIP_NAME%" del "%ZIP_NAME%"
powershell -NoProfile -Command "Compress-Archive -Path '%RELEASE_DIR%\*' -DestinationPath '%ZIP_NAME%' -Force"

echo ==========================================
echo   打包完成!
echo   文件: %SOURCE_DIR%\%ZIP_NAME%
echo   内含文件:
dir /b "%RELEASE_DIR%"
echo ==========================================
if /I not "%CEPB_NO_PAUSE%"=="1" pause
