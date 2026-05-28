---
name: project-architecture
description: CEPB ControlCenter 上位机项目的整体架构与设备侧 APP 组成
metadata:
  type: project
---

## 设备侧 APP 架构

项目用于连接一台嵌入式 Linux 设备，设备上运行三个 APP：

1. **北向 APP** —— 负责与云端/上层系统交互
2. **逻辑中心 APP** —— 负责业务逻辑调度
3. **南向数据采集 APP** —— 负责与现场设备通信
   - 当前示例：`cepiec104`（IEC 60870-5-104 协议采集 APP）
   - Debug Console 端口：`6666`
   - 现场设备 IP：`192.168.2.10`

每个 APP 都内置了 `DebugConsole`，支持通过 TCP 远程连接并发送文本命令。

## 上位机技术栈

- **Qt 版本**：6.9.1（MinGW 工具链）
- **构建系统**：CMake（Qt6 优先，回退 Qt5）
- **通信协议**：复用设备侧已有的 debug console（纯文本行协议）
- **核心类**：`DebugConsoleClient`
  - 封装 `QTcpSocket`
  - 行缓冲（按 `\n` 切分）
  - Prompt 识别（用于区分命令回复边界）
  - 支持命令回复收集 + 主动日志流分离

## 代码目录结构

```
CEPB_ControlCenter/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── network/
│   │   ├── debug_console_client.h
│   │   └── debug_console_client.cpp
│   └── ui/
│       ├── mainwindow.h
│       └── mainwindow.cpp
├── memory/
│   └── ... (本文档)
└── .gitignore
```

## 已知决策

- **不复用 device_client 旧实现**：已删除旧版 `DeviceClient`，全面使用 `DebugConsoleClient`。
- **UI 采用 Qt Widgets**：当前版本为纯代码布局（无 .ui 文件），深色主题日志视图。
- **Prompt 模式**：根据 APP 名称动态推导，如 `cepiec104>`。

## Why

设备侧已有成熟的 debug console 架构（命令注册、日志分类广播、Tab 补全）。上位机直接作为 TCP 客户端接入，零改动设备端即可实现控制、查询、实时数据展示。
