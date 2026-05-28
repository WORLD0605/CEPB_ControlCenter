---
name: known-issues
description: 开发过程中遇到的已知问题及解决方案
metadata:
  type: feedback
---

## Qt 网络代理导致连接失败

**现象**：Windows 下 `QTcpSocket` 连接设备时，报错 `The proxy type is invalid for this operation`。

**解决**：在创建 socket 后立即设置 `NoProxy`。

```cpp
m_socket->setProxy(QNetworkProxy::NoProxy);
```

**Why**：Windows 系统或某些软件可能配置了网络代理，Qt 默认会尝试使用系统代理连接，导致失败。

---

## Windows 控制台 UTF-8 乱码

**现象**：PowerShell / CMD 中运行命令行测试程序时，设备返回的中文显示为乱码。

**解决**：
1. 程序入口设置控制台代码页：
   ```cpp
   #ifdef Q_OS_WIN
   SetConsoleOutputCP(CP_UTF8);
   #endif
   ```
2. 安装自定义 message handler，强制 `QString::toUtf8()` 输出：
   ```cpp
   static void utf8MessageHandler(QtMsgType type, const QMessageLogContext&, const QString &msg) {
       QByteArray utf8 = msg.toUtf8();
       FILE *out = (type == QtInfoMsg) ? stdout : stderr;
       fprintf(out, "%s\n", utf8.constData());
       fflush(out);
   }
   qInstallMessageHandler(utf8MessageHandler);
   ```

**Why**：Qt 默认日志处理程序使用 `toLocal8Bit()`（Windows 下为 GBK），与 UTF-8 控制台不匹配。

---

## Qt 6.9.1 商业许可证校验

**现象**：MinGW 版 Qt 6.9.1 编译时触发 License Service 验证，提示 `Unauthorized. Please login with your Qt Account`。

**临时解决**：构建时设置环境变量：
```bash
export QTFRAMEWORK_BYPASS_LICENSE_CHECK=1
```

**长期解决**：安装 Qt **开源版（LGPL/GPL）**，通过 Qt 在线安装器选择个人/开源义务路径，不会有商业许可证校验。

---

## TCP 拆包导致 Prompt 与数据分离

**现象**：一次 `readyRead` 可能只收到 prompt（11 字节），下一次才收到完整回复内容。

**解决**：`DebugConsoleClient` 内部已实现行缓冲和 `flushPendingPrompt()`，确保无论 TCP 如何拆包，都能正确识别完整行和结尾 prompt。
