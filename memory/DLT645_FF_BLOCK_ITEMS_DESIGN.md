# DLT645 FF块读取子项表改造说明

## 背景

当前 DLT645APP 的采集配置按 poll 组描述一次读操作，例如：

```ini
yc_poll1=poll_1_11_040004FF_10
yc_type1=ASCII
```

APP 收到返回帧后，按 `PerDataLen` 固定长度切分数据，并用以下规则映射到模型点：

```cpp
dataIndex = (group_no << 16) + (i + 1);
```

这个机制适合同一个 DI 块里每个返回项长度、类型完全一致的场景。但 DLT645 的 `xxxxxxFF` 块读取经常会返回多个子 DI，且每个子项长度、类型不同。例如 `040004FF` 下可能同时包含：

- `04000401`：通讯地址，6 字节，BIN/BCD 类数据
- `0400040C`：生产日期，10 字节，ASCII
- 其他 040004xx 子项，各自长度不同

因此，现有格式无法准确表达“同一次 FF 块读取中每个子项的 DI、位置、长度、类型”。如果仍然只配置一个 `PerDataLen`，APP 只能按固定长度切分，容易出现错位、乱码、点位丢失或 dataIndex 对不上的问题。

## 改造目标

为 DLT645APP 增加“块内子项表”能力：

1. poll 组仍然描述一次南向读操作：功能码、采集 DI、组号。
2. poll 组下面新增多个 item，逐个描述返回数据里的子项。
3. 每个 item 明确包含：
   - 组内序号 entry
   - 子项 DI
   - offset 或自动顺序
   - 数据长度
   - 数据类型
   - 可选描述
4. APP 根据 item 表解析返回数据，并用 `dataIndex=(group_no<<16)+entry` 上报到对应模型点。
5. 旧配置继续可用，未配置 item 表时按当前固定 `PerDataLen` 逻辑运行。

## 建议 ini 格式

### 旧格式保持兼容

原有格式继续支持：

```ini
[dev_645Dev_1]
yc_poll_num=1
yc_poll1=poll_1_11_0202FF00_3
yc_type1=BCD_X
```

含义保持不变：读取 `0202FF00`（A/B/C 相电流数据块），每条数据固定 3 字节，类型 `BCD_X`，返回第 `i` 条映射到 `dataIndex=(1<<16)+(i+1)`。

### 新格式：poll + item 子项表

建议在原 poll 行后增加 item 数量和 item 明细：

```ini
[dev_645Dev_1]
yc_poll_num=1

; poll_<group>_<funCodeHex>_<pollDI>_<defaultLen>
; defaultLen 作为兼容字段保留；当存在 item 表时，解析以 item 为准。
yc_poll1=poll_7_11_040004FF_0
yc_type1=BCD

; 当前 poll 下的块内子项数量
yc_poll1_item_num=2

; item_<entry>_<pointDI>_<offset>_<len>_<type>
; offset 可使用十进制字节偏移；-1 表示按 item 顺序自动累加偏移。
yc_poll1_item1=item_1_04000401_0_6_BIN
yc_poll1_item2=item_12_0400040C_6_10_ASCII
```

字段说明：

| 字段 | 说明 |
| --- | --- |
| `yc_poll1` | 一次南向读操作，读 `040004FF` |
| `yc_poll1_item_num` | 该 poll 下有多少个块内子项 |
| `item_1_04000401_0_6_BIN` | entry=1，子 DI=04000401，从返回数据 offset=0 处取 6 字节，按 BIN 解析 |
| `item_12_0400040C_6_10_ASCII` | entry=12，子 DI=0400040C，从 offset=6 处取 10 字节，按 ASCII 解析 |

`entry` 不必连续。它只用于生成上报点的 `dataIndex`：

```cpp
dataIndex = (group_no << 16) + entry;
```

例如：

- group=7, entry=1 -> `dataIndex=458753`
- group=7, entry=12 -> `dataIndex=458764`

### offset 的两种模式

推荐同时支持两种写法：

#### 显式 offset

```ini
yc_poll1_item1=item_1_04000401_0_6_BIN
yc_poll1_item2=item_12_0400040C_6_10_ASCII
```

优点：最准确，适合 FF 返回内容固定但子项 entry 不连续的情况。

#### 自动 offset

```ini
yc_poll1_item1=item_1_04000401_-1_6_BIN
yc_poll1_item2=item_12_0400040C_-1_10_ASCII
```

APP 按 item 顺序自动累加 offset：

- item1 offset=0, len=6
- item2 offset=6, len=10

优点：编辑简单。前提是 item 顺序和返回数据顺序一致。

## 数据类型

item 的 `type` 建议限制为当前 DLT645APP 已支持或计划支持的类型：

| 类型 | 说明 |
| --- | --- |
| `BIN` | 按单字节十六进制字符串上报 |
| `BCD` | 多字节按 DLT645 小端顺序反转后作为 BCD 字符串 |
| `BCD_X` | BCD，1 位小数 |
| `BCD_XX` | BCD，2 位小数 |
| `BCD_XXX` | BCD，3 位小数 |
| `BCD_XXXX` | BCD，4 位小数 |
| `ASCII` | 按 ASCII 字符串读取后反转 |

## DLT645APP 需要修改的位置

以下文件名基于当前 DLT645APP 代码结构。

### 1. 数据结构

建议在 `DLT645device.h` 或当前 `poll_data` 定义处新增子项结构。

示例：

```cpp
struct poll_item_data
{
    int entry_no = 0;
    quint32 point_di = 0;
    int offset = -1;
    int data_len = 0;
    QString data_type;
};

struct poll_data
{
    int data_type;
    int group_no;
    int fun_code;
    quint32 DIcode;
    int PerDataLen;
    QString mtype;

    QList<poll_item_data> items;
};
```

保留 `PerDataLen` 和 `mtype`，用于旧格式兼容。

### 2. ini 解析

当前 `proDLT645private.cpp` 解析 `yc_pollN/yx_pollN` 时只读取 poll 行和 `yc_typeN/yx_typeN`。

需要增加：

```ini
yc_pollN_item_num
yc_pollN_item1
yc_pollN_item2
...
```

解析伪代码：

```cpp
QString itemNumKey = QString("%1_poll%2_item_num").arg(prefix).arg(k + 1);
int itemNum = setting.value(itemNumKey, 0).toInt();

for (int i = 1; i <= itemNum; ++i) {
    QString itemKey = QString("%1_poll%2_item%3").arg(prefix).arg(k + 1).arg(i);
    QString line = setting.value(itemKey, "").toString();
    // item_<entry>_<pointDI>_<offset>_<len>_<type>
    poll_item_data item;
    item.entry_no = line.section("_", 1, 1).toInt();
    item.point_di = line.section("_", 2, 2).toUInt(nullptr, 16);
    item.offset = line.section("_", 3, 3).toInt();
    item.data_len = line.section("_", 4, 4).toInt();
    item.data_type = line.section("_", 5, 5).trimmed();
    m_pdata.items.append(item);
}
```

如果 `item_num=0` 或没有 item 配置，则走旧逻辑。

### 3. 返回数据解析

当前 `DLT645device.cpp::ProcessReceiveData` 使用固定长度循环：

```cpp
int loopnum = (frame.data_len - 4) / pdata.PerDataLen;
for (int i = 0; i < loopnum; i++) {
    int dataIndex = (pdata.group_no << 16) + (i + 1);
    ...
    frame.data + i * pdata.PerDataLen
}
```

建议改成：

```cpp
if (!pdata.items.isEmpty()) {
    int autoOffset = 0;
    for (const poll_item_data &item : pdata.items) {
        int offset = item.offset >= 0 ? item.offset : autoOffset;
        int len = item.data_len;
        if (offset < 0 || len <= 0 || offset + len > frame.data_len - 4) {
            // 打日志并跳过
            if (item.offset < 0) {
                autoOffset += qMax(0, len);
            }
            continue;
        }

        int dataIndex = (pdata.group_no << 16) + item.entry_no;
        MeasPointInfo *mp = m_mapMeas.value(dataIndex, nullptr);
        if (!mp) {
            if (item.offset < 0) {
                autoOffset += len;
            }
            continue;
        }

        QVariant val = parseDlt645Value(frame.data + offset, len, item.data_type);
        // 上报 val

        if (item.offset < 0) {
            autoOffset += len;
        }
    }
    return;
}

// 旧逻辑保留
```

建议把当前 BIN/BCD/ASCII 的解析逻辑抽成函数：

```cpp
QVariant parseDlt645Value(const char *data, int len, const QString &type);
```

这样旧逻辑和 item 新逻辑可以复用同一套类型解析。

### 4. dataIndex 查找逻辑

不需要改变模型文件中的 `dataIndex` 语义。

模型点仍然只需要：

```json
{
  "dataRef": "PROT.SlrMonMMXU.1.ManufactureDate",
  "dataIndex": "458764"
}
```

APP 继续用：

```cpp
group = dataIndex >> 16;
entry = dataIndex & 0xFFFF;
```

区别只是：新 item 模式下，`entry` 来自配置里的 item，而不是返回数组的自然序号。

## 配置工具后续变化

等 DLT645APP 支持 item 表后，配置工具可以按以下方式导出：

### 设备编辑器字段

当前已有字段：

- 点位 DI
- 采集 DI
- 数据类型
- 字节数
- 组号
- 序号
- dataIndex

需要新增或复用字段：

- 块内 offset：可选，默认 `-1` 表示自动累加

### 导出策略

对于普通单点读取：

```ini
yc_poll1=poll_1_11_02020100_3
yc_type1=BCD_X
```

可以继续使用旧格式。

对于 FF 块读取且存在多个子项：

```ini
yc_poll1=poll_7_11_040004FF_0
yc_poll1_item_num=2
yc_poll1_item1=item_1_04000401_0_6_BIN
yc_poll1_item2=item_12_0400040C_6_10_ASCII
```

`dataIndex` 按 `group_no + entry` 生成：

```text
dataIndex = (group_no << 16) + entry
```

### 编辑方式变化

用户仍然填写每个点的点位 DI。

区别是：

1. `采集DI` 可以多个点相同，例如都为 `040004FF`。
2. `序号` 表示该点在当前 poll 组里的逻辑 entry，用来生成 dataIndex，不要求连续。
3. `offset` 表示该点在返回数据体中的字节偏移。
4. `字节数/数据类型` 变成子项级配置，不再要求同一个 FF 下完全一致。

## 兼容性建议

1. 旧 ini 没有 item 表时，完全按旧逻辑处理。
2. 新 ini 有 item 表时，以 item 表为准。
3. 如果 item 表存在但某一行格式错误，只跳过该 item，不要导致整个设备不可用。
4. 日志中建议输出：
   - group_no
   - pollDI
   - item entry
   - item DI
   - offset
   - len
   - type
   - dataIndex
5. 配置工具可以在 APP 支持前继续导出旧格式；APP 改造完成后再切换到 item 表导出。

## 示例：040004FF

假设设备返回的 `040004FF` 数据区依次包含：

| 子 DI | 含义 | offset | 长度 | 类型 | entry |
| --- | --- | ---: | ---: | --- | ---: |
| `04000401` | 通讯地址 | 0 | 6 | BIN | 1 |
| `0400040C` | 生产日期 | 6 | 10 | ASCII | 12 |

推荐配置：

```ini
[dev_645Dev_1]
yc_poll_num=1
yc_poll1=poll_7_11_040004FF_0
yc_type1=BCD
yc_poll1_item_num=2
yc_poll1_item1=item_1_04000401_0_6_BIN
yc_poll1_item2=item_12_0400040C_6_10_ASCII
```

对应 device JSON 中点位：

```json
[
  {
    "dataRef": "PROT.SlrMonMMXU.1.read_corr_address",
    "description": "读通信地址",
    "dataIndex": "458753"
  },
  {
    "dataRef": "PROT.SlrMonMMXU.1.ManufactureDate",
    "description": "生产日期",
    "dataIndex": "458764"
  }
]
```

其中：

```text
458753 = (7 << 16) + 1
458764 = (7 << 16) + 12
```
