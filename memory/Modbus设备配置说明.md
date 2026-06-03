# Modbus 设备配置说明（TCP / RTU）

本文档说明如何在 `ModbusApp` 中配置 **TCP 型** 和 **RTU 型** Modbus 设备，包括 `device` 文件（JSON）和 `cepmodbus.ini` 文件的写法，以及两者之间的对应关系。

---

## 一、文件位置与作用

| 文件 | 路径 | 作用 |
|------|------|------|
| device 文件 | `../dev/device-xxx.json` | 定义设备基本信息、测点点表、TCP/RTU 连接参数 |
| ini 文件 | `../etc/cepmodbus.ini` | 定义轮询/控制分组、功能码、寄存器地址、数据类型、比例系数 |

> 程序启动时，会扫描 `../dev/` 目录下所有 `.json` 文件作为设备文件；
> 同时读取 `../etc/cepmodbus.ini` 获取每个设备的轮询和控制配置。

---

## 二、device 文件写法

设备类型由 JSON 中的 `"type"` 字段决定，**必须**存在且只能为 `"TCP"` 或 `"RTU"`（大小写不敏感）。

### 2.1 TCP 型设备

```json
{
    "DeviceId": "1",
    "DeviceDesc": "示例TCP设备",
    "Model": "NWKJ_示例模型_1.00",
    "addr": "1",
    "type": "TCP",
    "ip": "192.168.1.100",
    "port": 502,
    "debug": "on",
    "meas_points": [
        {
            "dataIndex": "65537",
            "dataRef": "PROT.SlrMonMMXU.1.StatusWord1",
            "description": "状态字1"
        },
        {
            "dataIndex": "131073",
            "dataRef": "PROT.SlrMonMMXU.1.Voltage",
            "description": "电压"
        }
    ]
}
```

**TCP 必填字段**

| 字段 | 说明 |
|------|------|
| `type` | **必须写 `"TCP"`** |
| `ip` | 设备 IP 地址 |
| `port` | 设备端口号（如 `502`） |

### 2.2 RTU 型设备

```json
{
    "DeviceId": "2",
    "DeviceDesc": "示例RTU设备",
    "Model": "NWKJ_示例模型_1.00",
    "addr": "1",
    "type": "RTU",
    "debug": "on",
    "rtu": {
        "serialPort": "RS485_1",
        "baud": 9600,
        "dataBits": 8,
        "stopBits": 1,
        "parity": "N"
    },
    "meas_points": [
        {
            "dataIndex": "65537",
            "dataRef": "PROT.SlrMonMMXU.1.StatusWord1",
            "description": "状态字1"
        },
        {
            "dataIndex": "131073",
            "dataRef": "PROT.SlrMonMMXU.1.Temperature",
            "description": "温度"
        }
    ]
}
```

**RTU 必填字段**

| 字段 | 说明 |
|------|------|
| `type` | **必须写 `"RTU"`** |
| `rtu.serialPort` | 串口名称，见下方【串口名称映射】 |
| `rtu.baud` | 波特率（如 `9600`） |
| `rtu.dataBits` | 数据位（如 `8`） |
| `rtu.stopBits` | 停止位（如 `1`） |
| `rtu.parity` | 校验位（`N`=无校验, `O`=奇校验, `E`=偶校验） |

> 如果 RTU 设备缺少上述任一 RTU 参数，程序会打印警告但仍会继续加载设备，只是无法创建串口连接。

**串口名称映射**

程序支持使用 `RS485_X` 别名或直接写设备路径：

| 别名 | 默认映射 |
|------|----------|
| `RS485_1` | `/dev/ttyS13` |
| `RS485_2` | `/dev/ttyS2` |
| `RS485_3` | `/dev/ttyS7` |
| `RS485_4` | `/dev/ttyS10`（myir）或 `/dev/ttyS11`（talowe） |
| `RS485_5` | `/dev/ttyS9` |
| `RS485_6` | `/dev/ttyS14` |
| `RS485_7` | `/dev/ttyS12` |
| `RS485_8` | `/dev/ttyS6` |

> 也可直接写完整路径，如 `"serialPort": "/dev/ttyUSB0"`。

> `RS485_4` 的映射受 `cepmodbus.ini` 中 `[Base]` 的 `hw_variant` 影响：
> - `hw_variant=myir` → `/dev/ttyS10`
> - `hw_variant=talowe` → `/dev/ttyS11`

### 2.3 通用字段说明（TCP / RTU 共有）

| 字段 | 必填 | 说明 |
|------|------|------|
| `DeviceId` | ✅ | 设备唯一 ID（数字字符串），如 `"1"` |
| `DeviceDesc` | ❌ | 设备描述 |
| `Model` | ✅ | 关联的模型名称，必须在 `../model/` 中存在对应 `.json` |
| `addr` | ✅ | Modbus 从站地址（如 `"1"`） |
| `debug` | ❌ | 写 `"on"` / `"1"` / `"true"` 可开启该设备的 Modbus 帧级调试日志 |
| `meas_points` | ✅ | 测点列表，每个测点包含 `dataIndex` 和 `dataRef` |

### 2.4 dataIndex 的编码规则

`dataIndex` 是一个 32 位整数，**高 16 位表示 `group_no`（分组号），低 16 位表示组内序号**。

例如：
- `dataIndex = 65537` → `group_no = 1`（`65537 >> 16 = 1`），组内序号 = 1
- `dataIndex = 131073` → `group_no = 2`（`131073 >> 16 = 2`），组内序号 = 1

> 在 ini 文件中的 `yx_poll1` / `yc_poll1` 等配置里，`group_no` 就是通过这个高 16 位来与测点对应的。

---

## 三、cepmodbus.ini 写法

`cepmodbus.ini` 采用标准 INI 格式，每个设备对应一个 `[dev_X]` 组，`X` 为 `DeviceId`。

```ini
[Base]
frameInterval=100
hw_variant=myir
YX_UploadPeriod=60
YC_UploadPeriod=10

[dev_1]
yx_type=BIT
yc_type=WORD
yt_type=WORD

yx_poll_num=1
yc_poll_num=1
yk_set_num=1
yt_set_num=0

yx_poll1=_1_2_0_8
yc_poll1=_2_3_0_6

yk_set1=_1_1_6_100

yc_scale=1.0
```

### 字段说明

#### 1. 全局配置 `[Base]`

| 键 | 默认值 | 说明 |
|----|--------|------|
| `frameInterval` | `100` | 总线调度帧间隔（毫秒），控制设备间轮询间隔 |
| `hw_variant` | 空 | 硬件版本，影响 `RS485_4` 映射（`myir` / `talowe`） |
| `YX_UploadPeriod` | `60` | 遥信周期上送间隔（秒） |
| `YC_UploadPeriod` | `10` | 遥测周期上送间隔（秒） |

#### 2. 设备级配置 `[dev_X]`

**数据类型**

| 键 | 默认值 | 说明 |
|----|--------|------|
| `yx_type` | `BIT` | 遥信数据类型（BIT / POCKETBIT / WORD 等） |
| `yc_type` | `WORD` | 遥测数据类型（WORD / FLOAT / DWORD / FLOAT_L / DWORD_L 等） |
| `yt_type` | `WORD` | 遥调数据类型 |

**轮询组数量**

| 键 | 说明 |
|----|------|
| `yx_poll_num` | 遥信轮询组数量 |
| `yc_poll_num` | 遥测轮询组数量 |

**轮询组定义**

格式：`_groupNo_funCode_startAddr_regNum`

```ini
yx_poll1=_1_2_0_8
```

| 段 | 示例值 | 说明 |
|----|--------|------|
| `groupNo` | `1` | 分组号，对应 `dataIndex` 的高 16 位 |
| `funCode` | `2` | Modbus 功能码（`2`=读离散输入, `1`=读线圈, `3`=读保持寄存器, `4`=读输入寄存器） |
| `startAddr` | `0` | 起始寄存器地址 |
| `regNum` | `8` | 要读取的寄存器数量 |

> 每一组 `yx_pollN` / `yc_pollN` 定义一次 Modbus 读请求。程序会按组号 `groupNo` 将读回的数据映射到对应 `dataIndex` 的测点上。

**控制组定义**

格式：`_groupNo_entryNo_funCode_regAddr`

```ini
yk_set1=_1_1_6_100
```

| 段 | 示例值 | 说明 |
|----|--------|------|
| `groupNo` | `1` | 遥控/遥调分组号 |
| `entryNo` | `1` | 组内条目号 |
| `funCode` | `6` | 控制功能码（`5`=写单线圈, `6`=写单寄存器, `15`=写多线圈, `16`=写多寄存器） |
| `regAddr` | `100` | 控制目标寄存器地址 |

**比例系数**

| 键 | 默认值 | 说明 |
|----|--------|------|
| `yc_scale` | `1.0` | 遥测全局比例系数 |
| `yt_scale` | `1.0` | 遥调全局比例系数 |
| `yc_scale1` | `yc_scale` | 第 1 组遥测的独立比例系数（覆盖全局） |
| `yt_scale1` | `yt_scale` | 第 1 组遥调的独立比例系数 |

---

## 四、device 文件与 ini 文件的对应关系

### 1. DeviceId ↔ [dev_X]

- device 文件中的 `"DeviceId": "1"` → ini 文件中的 `[dev_1]` 组
- 程序通过 `DeviceId` 将 device 文件和 ini 中的配置关联到同一台设备

### 2. type ↔ 连接创建

- **TCP**：读取 `"ip"` + `"port"`，调用 `modbus_new_tcp(ip, port)` 创建 TCP 连接上下文
- **RTU**：读取 `"rtu"` 对象中的 `serialPort`、`baud`、`dataBits`、`stopBits`、`parity`，调用 `modbus_new_rtu()` 创建串口连接上下文

### 3. dataIndex ↔ groupNo

- device 中 `meas_points[].dataIndex` 的高 16 位 = ini 中 `yx_pollN` / `yc_pollN` 的 `groupNo`
- 例如：`dataIndex = 65537`（`groupNo = 1`）的数据点，会接收 `yx_poll1=_1_...` 或 `yc_poll1=_1_...` 读回的数据

### 4. 测点 ↔ 模型

- device 文件中的 `Model` 字段指向 `../model/` 下的模型文件
- 模型文件定义了 `services`、`DOs`、`datatype` 等，用于北向数据上报格式
- `dataRef` 的格式为 `LDname.LNtype.LNinst.DOname`，必须与模型中的 DO 定义一致

---

## 五、完整配置示例

### 5.1 TCP 设备示例

**device 文件：`../dev/device-示例温控器_1.json`**

```json
{
    "DeviceId": "1",
    "DeviceDesc": "示例温控器",
    "Model": "NWKJ_温控器_1.00",
    "addr": "1",
    "type": "TCP",
    "ip": "192.168.1.50",
    "port": 502,
    "debug": "on",
    "meas_points": [
        {
            "dataIndex": "65537",
            "dataRef": "PROT.SlrMonMMXU.1.RunStatus",
            "description": "运行状态"
        },
        {
            "dataIndex": "131073",
            "dataRef": "PROT.SlrMonMMXU.1.Temperature",
            "description": "当前温度"
        }
    ]
}
```

**ini 配置：**

```ini
[dev_1]
yx_type=BIT
yc_type=WORD

yx_poll_num=1
yc_poll_num=1

yx_poll1=_1_2_0_16
yc_poll1=_2_3_0_4

yc_scale=1.0
```

### 5.2 RTU 设备示例

**device 文件：`../dev/device-示例电表_2.json`**

```json
{
    "DeviceId": "2",
    "DeviceDesc": "示例电表",
    "Model": "NWKJ_电表_1.00",
    "addr": "3",
    "type": "RTU",
    "debug": "on",
    "rtu": {
        "serialPort": "RS485_1",
        "baud": 9600,
        "dataBits": 8,
        "stopBits": 1,
        "parity": "N"
    },
    "meas_points": [
        {
            "dataIndex": "65537",
            "dataRef": "PROT.SlrMonMMXU.1.RunStatus",
            "description": "运行状态"
        },
        {
            "dataIndex": "131073",
            "dataRef": "PROT.SlrMonMMXU.1.VoltageA",
            "description": "A相电压"
        },
        {
            "dataIndex": "131074",
            "dataRef": "PROT.SlrMonMMXU.1.VoltageB",
            "description": "B相电压"
        },
        {
            "dataIndex": "131075",
            "dataRef": "PROT.SlrMonMMXU.1.VoltageC",
            "description": "C相电压"
        }
    ]
}
```

**ini 配置：**

```ini
[dev_2]
yx_type=BIT
yc_type=WORD

yx_poll_num=1
yc_poll_num=1

; 遥信轮询：groupNo=1, funCode=2(读离散输入), startAddr=0, regNum=8
yx_poll1=_1_2_0_8

; 遥测轮询：groupNo=2, funCode=3(读保持寄存器), startAddr=0, regNum=6
yc_poll1=_2_3_0_6

yc_scale=0.1
```

> 同一 `RS485_1` 串口下的多台 RTU 设备，只要 `serialPort` 相同，程序会自动复用同一个串口句柄（`m_mapRTUByPort`），不需要重复打开串口。

---

## 六、常见问题

### 1. TCP 设备连接不上？

- 检查 device 文件中 `ip` 和 `port` 是否填写正确
- 确认网络可达（ping 通目标 IP）
- 查看程序日志是否有 `"Could not connect TCP"` 或 `"Failed to create Modbus TCP context"`
- TCP 连接有 30 秒定时重连机制（`slot_timer_connectmsg`），断线后会自动尝试恢复

### 2. RTU 设备连接不上？

- 检查 device 文件中 `rtu` 对象的 `serialPort`、`baud`、`dataBits`、`stopBits`、`parity` 是否全部填写
- 若使用 `RS485_X` 别名，确认 `hw_variant` 配置正确（影响 `RS485_4` 映射）
- 检查串口设备文件是否存在（如 `/dev/ttyS13`）
- 查看程序日志是否有 `"Could not connect serial port"` 或 `"Failed to create Modbus RTU context"`
- 确认串口未被其他程序占用

### 3. 数据点没有值？

- 检查 `dataIndex` 的高 16 位是否与 `yx_pollN` / `yc_pollN` 的 `groupNo` 一致
- 检查 `regNum`（寄存器数量）是否足够覆盖该组所有测点
- 检查 `funCode` 是否与设备实际支持的 Modbus 功能码匹配
- 检查 `mtype`（数据类型）是否设置正确，如 `FLOAT` 占 2 个寄存器，`WORD` 占 1 个

### 4. 如何开启调试？

- 在 device 文件中添加 `"debug": "on"`，可开启该设备的 Modbus 帧级调试输出
- 程序启动参数加 `-debug` 可开启全局 qDebug 日志

---

## 七、参考代码位置

- 设备 JSON 解析与 `type` 校验：[promodbusprivate.cpp:111-128](promodbusprivate.cpp#L111-L128)
- TCP 连接创建逻辑：[promodbusprivate.cpp:222-253](promodbusprivate.cpp#L222-L253)
- RTU 连接创建逻辑：[promodbusprivate.cpp:131-219](promodbusprivate.cpp#L131-L219)
- RS485 别名映射：[promodbusprivate.cpp:62-75](promodbusprivate.cpp#L62-L75)
- ini 轮询配置解析：[promodbusprivate.cpp:255-355](promodbusprivate.cpp#L255-L355)
- TCP 重连机制：[modbusgateway.cpp:282-299](modbusgateway.cpp#L282-L299)
- 数据解析与 `dataIndex` 映射：[modbusdevice.cpp:471-598](modbusdevice.cpp#L471-L598)
