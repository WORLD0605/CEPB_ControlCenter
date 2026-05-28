---
name: communication-protocol
description: 上位机与设备 APP 之间的 debug console 通信协议细节
metadata:
  type: reference
---

## 传输层

- **协议**：TCP
- **端口**：每个 APP 独立端口（如 cepiec104 → 6666）
- **编码**：UTF-8

## 应用层协议

纯文本行协议：

```
客户端发送：help\n
服务端回复：
Commands:
  clear   清空当前终端
  debug   控制远端日志输出: ...
  help    列出可用命令
  ...
cepiec104>
```

### 关键特征

1. **Prompt**：每个 APP 有固定 prompt（如 `cepiec104>`），通常**不跟换行**。
2. **命令回复**：发送命令后，服务端先输出结果（多行，以 `\n` 结尾），最后输出 prompt 表示结束。
3. **主动日志**：通过 `broadcastLog()` 推送，每行以 `\n` 结尾，**不带 prompt**。
4. **日志分类**：通过 `debug <category> <on|off>` 控制远程日志输出。
   - 分类：`104`, `mqtt`, `misc`, `data`, `ctrl`, `all`

## DebugConsoleClient 状态机

```
Idle 状态：
  - 收到非 prompt 行 → logLineReceived()
  - 收到 prompt → 忽略

Executing 状态（已发命令）：
  - 收到非 prompt 行 → 追加到 reply buffer
  - 收到 prompt + buffer 非空 → commandReplyReceived()
  - 收到 prompt + buffer 为空 → 忽略（前置/冗余 prompt）
```

### 行缓冲处理

TCP 流可能拆包/粘包，因此客户端内部维护 `m_readBuffer`：
- 按 `\n` 切分完整行
- 剩余无换行内容若匹配 prompt，通过 `flushPendingPrompt()` 处理

## 常用命令速查

| 命令 | 作用 |
|------|------|
| `help` | 列出可用命令 |
| `ping` | 存活检测 |
| `uptime` | 程序运行时长 |
| `mqtt` | MQTT 连接状态 |
| `debug data on` | 打开数据日志远程输出 |
| `debug list` | 查看日志分类开关状态 |
| `quit` | 退出远端 APP |
