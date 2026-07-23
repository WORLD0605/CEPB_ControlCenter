# CEPB ControlCenter Windows 编译说明

本工程是 64 位 Windows Qt/C++ 桌面程序，使用 CMake + Ninja 构建。

## 1. 推荐环境清单

为避免 Qt 库与编译器 ABI 不匹配，优先使用与当前开发机一致的组合：

| 项目 | 推荐版本/选项 | 是否必需 |
| --- | --- | --- |
| 操作系统 | Windows 10/11 64 位 | 是 |
| Git for Windows | 当前稳定版；当前开发机为 2.51.2 | 获取源码时必需 |
| Qt | Qt 6.8.3，`LLVM-MinGW 64-bit` | 是 |
| C/C++ 编译器 | Qt Tools 中的 LLVM-MinGW 17.0.6 64 位 | 是 |
| CMake | 3.16 及以上；当前开发机为 3.30.5 | 是 |
| Ninja | 当前开发机为 1.12.1 | 是 |
| Qt Creator | 随 Qt 安装，推荐安装 | 可选，但适合日常开发 |

Qt 安装器中建议勾选：

1. `Qt 6.8.3` 下的 `LLVM-MinGW 64-bit`。
2. `Developer and Designer Tools` 下的 Qt Creator、CMake、Ninja 和 LLVM-MinGW 17.0.6 64 位工具链。

不需要额外安装 Visual Studio、OpenSSL 或 libssh2。工程内已包含 `third_party/libssh2`，Windows 下使用系统 WinCNG 加密后端。

> 不要把 `llvm-mingw_64` 版 Qt 与 MSVC 或普通 MinGW 编译器混用。若选择其他 Qt 套件，必须同时使用该套件对应的编译器。当前正式验证组合是 Qt 6.8.3 + LLVM-MinGW 17.0.6。

## 2. 获取完整源码

推荐将工程放到不含中文和特殊字符的短路径，例如 `D:\Work\CEPB_ControlCenter`。

源码必须包含以下内容：

- 根目录 `CMakeLists.txt`
- `src`
- `third_party/libssh2`
- `app_icon.rc`

不要复制其他电脑已有的 `build`、`build-release` 或 `build-verify` 目录；这些目录的缓存包含原电脑的绝对路径。

## 3. 使用 Qt Creator 编译（推荐）

1. 启动 Qt Creator，选择“打开项目”，打开根目录的 `CMakeLists.txt`。
2. 在 Kit 选择页选择 `Desktop Qt 6.8.3 LLVM-MinGW 64-bit`。
3. 构建配置选择 `Debug`（调试）或 `Release`（发布）。
4. 点击“配置项目”，再点击左下角锤子图标构建。
5. 构建完成后，目标文件通常位于所选构建目录下的 `CEPB_ControlCenter.exe`。

如果 Kit 列表中没有上述选项，请在 Qt Creator 的“首选项/Preferences → Kits”中检查 Qt 6.8.3、LLVM-MinGW、CMake 和 Ninja 是否都被正确识别。

## 4. 使用 PowerShell 命令行编译

以下示例假设 Qt 安装在 `C:\Qt`。若安装目录不同，只修改 `$QtRoot`，不要照搬当前开发机的盘符。

```powershell
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
$OutputEncoding = [Console]::OutputEncoding

$QtRoot = 'C:\Qt'
$QtPrefix = Join-Path $QtRoot '6.8.3\llvm-mingw_64'
$QtTools = Join-Path $QtRoot 'Tools'
$LlvmBin = Join-Path $QtTools 'llvm-mingw1706_64\bin'
$CMakeBin = Join-Path $QtTools 'CMake_64\bin'
$NinjaBin = Join-Path $QtTools 'Ninja'

$env:Path = "$CMakeBin;$NinjaBin;$LlvmBin;$QtPrefix\bin;$env:Path"

cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH="$QtPrefix" `
  -DCMAKE_C_COMPILER="$LlvmBin\x86_64-w64-mingw32-clang.exe" `
  -DCMAKE_CXX_COMPILER="$LlvmBin\x86_64-w64-mingw32-clang++.exe" `
  -DCMAKE_MAKE_PROGRAM="$NinjaBin\ninja.exe"

cmake --build build --parallel
```

编译成功后运行：

```powershell
.\build\CEPB_ControlCenter.exe
```

如果以前用另一套 Qt、另一种编译器或另一台电脑生成过 `build`，请把上面命令中的 `-B build` 和后续的 `cmake --build build` 同时改成一个全新的目录名，例如 `build-local`，其余参数保持不变。这样不必删除旧缓存。

## 5. Release 编译与发布包

只生成 Release 可执行文件：

```powershell
cmake -S . -B build-release-local -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="$QtPrefix" `
  -DCMAKE_C_COMPILER="$LlvmBin\x86_64-w64-mingw32-clang.exe" `
  -DCMAKE_CXX_COMPILER="$LlvmBin\x86_64-w64-mingw32-clang++.exe" `
  -DCMAKE_MAKE_PROGRAM="$NinjaBin\ninja.exe"

cmake --build build-release-local --parallel
```

仅把 `.exe` 复制给其他电脑通常无法运行，因为还缺少 Qt DLL 和平台插件。工程提供了发布脚本；先保证根目录的 `build` 已成功配置过，再执行：

```powershell
.\scripts\package.bat
```

脚本会重新执行 Release 构建、调用 `windeployqt`、复制编译器运行库并生成 `CEPB_ControlCenter_v<版本>_Release.zip`。

发布脚本还会尝试从根目录 `app_binaries` 复制 8 个设备端 APP 升级二进制。该目录未随 Git 仓库提交；如果只需要 ControlCenter 桌面程序，可以忽略脚本中的 Missing 警告；如果要交付 APP 升级功能所需文件，请先向负责人取得对应二进制并放入该目录。

## 6. 编译完成后的检查

```powershell
Test-Path .\build\CEPB_ControlCenter.exe
.\build\CEPB_ControlCenter.exe
```

至少确认：

- 程序能正常启动，图标和中文界面显示正常。
- 调试版本从构建目录运行没有缺少 DLL 的提示。
- 发布 ZIP 解压到一个干净目录后能够启动。
- 修改代码后重新构建前，先关闭正在运行的 `CEPB_ControlCenter.exe`。

本工程当前没有配置 CTest 测试用例。若 `ctest` 输出 `No tests were found!!!`，表示没有测试可运行，不能视为测试通过；编译和最终链接成功才是当前基础验证结果。

## 7. 常见问题

### CMake 找不到 Qt

典型提示包含 `Could not find QT`、`Qt6Config.cmake` 或 `Qt5Config.cmake`。检查 `$QtPrefix` 是否确实指向类似 `C:\Qt\6.8.3\llvm-mingw_64` 的目录，并确认其中存在 `lib\cmake\Qt6\Qt6Config.cmake`。

### 编译器不可用或链接出现大量未定义符号

首先确认 Qt 套件和编译器属于同一套工具链。本工程推荐 `llvm-mingw_64` Qt 配合 `llvm-mingw1706_64`，不要混用 MSVC 或 `mingw1310_64`。

### `failed to write output 'CEPB_ControlCenter.exe': Permission denied`

通常不是代码错误，而是程序仍在运行，Windows 锁住了输出文件。先关闭窗口，或检查进程：

```powershell
Get-Process CEPB_ControlCenter -ErrorAction SilentlyContinue
```

确认是本工程的进程后关闭它，再执行 `cmake --build build --parallel`。

### 提示找不到 `third_party/libssh2`

源码不完整。重新克隆仓库，或重新取得完整源码包；无需从网上单独下载另一份 libssh2。

### 可执行文件在同事电脑上提示缺少 DLL 或 `qwindows.dll`

这是部署问题，不是编译问题。不要只复制构建目录中的 exe，使用 `scripts\package.bat` 生成发布 ZIP。

### 中文输出乱码

不要依赖 PowerShell profile，执行命令前显式设置 UTF-8：

```powershell
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
$OutputEncoding = [Console]::OutputEncoding
```
