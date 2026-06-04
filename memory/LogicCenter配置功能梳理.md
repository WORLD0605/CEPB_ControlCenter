# LogicCenter 配置功能梳理

当前程序启动后固定读取 `/home/cepgateway/app/cepLogicCenter/etc/LogicCenter_Config.json`。配置加载成功后，会驱动四类能力：遥测/遥信计算点、控制命令拆分和变换、AGC/AVC 虚拟并网点分配、设备在线状态联动。

本文按配置项说明当前代码实际支持的功能。

## 顶层结构

| 配置项 | 类型 | 是否必填 | 功能 |
| --- | --- | --- | --- |
| `computation_points` | array | 否 | 从上送 `DataSpont` 中取源点，按公式生成新的遥测/遥信点，并可剔除源点转发。 |
| `control_rules` | array | 否 | 对北向 `CtrlCmd` 做匹配、表达式换算、一拆多转发，或生成内部 `DataWrite`。 |
| `AgcAvcGroups` | array | 否 | 多组 AGC/AVC 虚拟并网点配置。优先级高于 `AgcAvc`。 |
| `AgcAvc` | object | 否 | 旧版单组 AGC/AVC 配置。没有 `AgcAvcGroups` 时自动作为一组使用。 |
| `onlineStatus_link` | array | 否 | DevUpdate 在线状态跟随关系。 |
| `AgcAvc_debug` | object | 否 | 定时打印 AGC/AVC 运行态。 |

## `computation_points`

`computation_points` 用于在收到 `DataSpont` 后生成计算点。程序会缓存命中的 `operands`，当同一个计算点的所有源点都在新鲜度窗口内时计算输出。当前新鲜度窗口在代码中固定为 60000 ms。

每个计算点字段：

| 字段 | 类型 | 默认值 | 功能 |
| --- | --- | --- | --- |
| `DeviceId` | string | 空 | 计算结果输出设备 ID。 |
| `dataRef` | string | 空 | 计算结果输出点号。输出 payload 中字段名会变成 `DataRefer`。 |
| `formula` | string | 空 | 计算公式。使用 `{1}`、`{2}` 引用 `operands` 中第 1、2 个源点。 |
| `description` | string | 空 | 说明文字，仅解析保存，不参与运行逻辑。 |
| `drop_operands` | bool | `true` | 命中源点后是否从原始转发帧里删除源点。多个计算点引用同一源点时，只要任一配置为 `false`，该源点就不会被剔除。 |
| `operands` | array | 必须为数组 | 源点列表，顺序对应公式中的 `{1}..{n}`。 |

`operands` 子项：

| 字段 | 类型 | 功能 |
| --- | --- | --- |
| `DeviceId` | string | 源点设备 ID。 |
| `dataRef` | string | 源点点号。运行时用上送帧里的 `DataRefer` 匹配。 |

公式支持数字、空格、`+ - * /`、括号、一元正负号，以及 `sqrt(x)`、`sqr(x)`、`square(x)`、`pow(x,y)`。除零、非法表达式、结果为 NaN/Inf 会导致该次计算失败。

输出行为：

- 原始 `DataSpont` 会转发到 `ServiceChannel` 方向；命中且配置剔除的源点会从转发帧删除。
- 计算结果会构造新的 `DataSpont`。如果原始帧是遥信 topic 或 ServiceId 为 `discrete`，上层会把计算结果的 `ServiceId` 修正为 `discrete`；否则为 `analog`。
- 计算结果也会旁路送给 AGC/AVC worker，用于更新虚拟并网点 `TotalP`、`TotalQ` 的实测值。

## `control_rules`

`control_rules` 用于处理北向下发的 `CtrlCmd`。收到命令后，程序按 `DeviceId + dataRef` 查找规则；当前 `CtrlType` 会被读取和记录，但不参与匹配。找不到规则时，该控制命令会被丢弃并输出错误日志。

每条规则字段：

| 字段 | 类型 | 功能 |
| --- | --- | --- |
| `match` | object | 源控制点匹配条件。 |
| `targets` | array | 目标控制点列表。允许一条源命令拆成多条目标命令或内部写点。 |

`match` 字段：

| 字段 | 类型 | 功能 |
| --- | --- | --- |
| `DeviceId` | string | 源控制设备 ID，必填。 |
| `dataRef` | string | 源控制点号，必填。 |
| `Description` | string | 控制转换规则说明，可选。 |

`CtrlType` 不再作为 `match` 配置字段；运行时复用北向原始 `CtrlCmd` 帧中的 `CtrlType`。

`targets` 子项：

| 字段 | 类型 | 默认值 | 功能 |
| --- | --- | --- | --- |
| `DeviceId` | string | 空 | 目标设备 ID，必填。 |
| `dataRef` | string | 空 | 目标点号，必填。 |
| `expr` | string | 空 | 目标值表达式，必填。 |
| `targetType` / `target_type` | string | `ctrlcmd` | `ctrlcmd` 表示生成新的控制命令；`data_write` 表示生成内部 `DataWrite`。 |

表达式中的 `{x}` 表示源命令 `CtrlVal`。还支持 `{rt:DeviceId#DataRefer}` 引用最近一次从 `DataSpont` 或计算结果中缓存到的实时值。表达式计算能力与 `computation_points.formula` 相同。

特殊行为：

- 当目标是已配置的 AGC/AVC 虚拟设备且目标点号包含 `TotalP_Ctrl` 或 `TotalQ_Ctrl` 时，该目标不会直接发南向，而是进入 AGC/AVC 分配逻辑。
- 虚拟设备的选择/取消类 `CtrlType`：`0`、`2`、`3`、`5` 不触发控制规则副作用，直接交给 AGC/AVC 返回成功应答。
- 源点号包含 `PTGTotalP_Ctrl` 且源值为 `100`，目标为虚拟 `TotalP_Ctrl` 时，程序会忽略 `expr`，改用该虚拟组所有 `pMax` 之和作为目标值。
- 若某条规则同时包含虚拟总控 `ctrlcmd` 和 `data_write`，且总控被 gate 条件拦截，配套的 `data_write` 会被抑制。
- `ctrlcmd` 输出的 `CtrlVal` 固定格式为 3 位小数；AGC/AVC 分配到南向设备时再按整数格式输出。

## `AgcAvcGroups` / `AgcAvc`

AGC/AVC 配置用于把一个虚拟并网点的总有功/总无功控制，按在线设备容量比例拆分到多个南向设备。支持多组虚拟并网点；每组必须有唯一的 `virtualDeviceId`。

组字段：

| 字段 | 类型 | 默认值 | 功能 |
| --- | --- | --- | --- |
| `groupId` / `group_id` | string | `default` | 组标识，用于日志和运行态区分。 |
| `virtualDeviceId` / `virtual_device_id` | string | `999` | 虚拟并网点设备 ID。命中该 ID 的控制和上送会进入 AGC/AVC 逻辑。 |
| `devicelist` | array | 空 | 南向设备或逻辑逆变器列表。 |
| `measurement_scale` | object | `1.0` | 虚拟总有功/总无功实测值缩放，用于统一单位。 |
| `gate_reverse` | object | 全 `false` | 对投退、远方、闭锁等状态点做 0/1 反相解释。 |
| `follow` | object | 关闭 | 旧版跟随配置，同时应用到 AGC 和 AVC。 |
| `agc_follow` | object | 关闭 | AGC 独立功率跟随配置。 |
| `avc_follow` | object | 关闭 | AVC 独立功率跟随配置。 |

`devicelist` 子项：

| 字段 | 类型 | 默认值 | 功能 |
| --- | --- | --- | --- |
| `DeviceId` | string | 必填 | 南向设备 ID。 |
| `ctrlDataRefP` / `ctrl_dataref_p` | string | `PROT.SlrInvCtlGGIO.1.P_Ctrl` | 有功分配下发点。 |
| `ctrlDataRefQ` / `ctrl_dataref_q` | string | `PROT.SlrInvCtlGGIO.1.Q_Ctrl` | 无功分配下发点。 |
| `onlineDeviceId` / `online_device_id` | string | 当前 `DeviceId` | 在线遥信所属设备 ID。 |
| `onlineDataRef` / `online_dataref` | string | 空 | 在线遥信点。配置后，该逻辑设备在线状态由该遥信值决定；未配置时由 `DevUpdate` 决定。 |
| `onlineOkValue` / `online_ok_value` | int | `1` | 在线遥信等于该值时判定在线。 |
| `pMax` | int | `0` | 有功上限。 |
| `pMin` | int | `0` | 有功下限。 |
| `qMax` | int | `0` | 无功上限。 |
| `qMin` | int | `0` | 无功下限。 |
| `scaleP` / `scale_p` | number | `1.0` | 有功下发缩放系数。 |
| `scaleQ` / `scale_q` | number | `1.0` | 无功下发缩放系数。 |
| `scale` | number | `1.0` | 旧兼容字段。未单独配置 `scaleP` / `scaleQ` 时，同时应用到两者。 |

分配逻辑：

- 仅在线且容量大于 0 的设备参与分配。
- AGC 按 `pMax - pMin` 作为容量权重，AVC 按 `qMax - qMin` 作为容量权重。
- 每台分配值会限制在自身 `pMin..pMax` 或 `qMin..qMax` 范围内，再乘以 `scaleP` 或 `scaleQ` 作为实际下发值。
- 最后一台设备承接前面整数比例分配后的余量。
- 每台设备之间当前固定间隔约 100 ms 下发。

Gate 条件：

虚拟 `TotalP_Ctrl` / `TotalQ_Ctrl` 会在以下情况被拒绝执行，并返回 `CtrlCmdResp`，`ErrorCode=100`：

- 对应 AGC/AVC 未投入：`enable=false`。
- 开环：`openloop=true`。
- 非远方：`distant=false`。
- 总闭锁：`lock=true`。
- 目标值超出本组配置容量合计范围。
- 没有可用的已配置目标设备。

状态点识别通过点号末段后缀完成：

| 后缀 | 更新状态 |
| --- | --- |
| `_enable_ctrl` / `_enable` | 投退 |
| `_distant_ctrl` / `_distant` | 远方/就地 |
| `_openloop_ctrl` / `_openloop` | 开环/闭环 |
| `_lock_ctrl` / `_lock` | 总闭锁 |
| `_poweruplock_ctrl` / `_uplock_ctrl` / `_poweruplock` / `_uplock` | 增闭锁 |
| `_powerdownlock_ctrl` / `_downlock_ctrl` / `_powerdownlock` / `_downlock` | 减闭锁 |
| 包含 `TotalP_CtrlVal` / `TotalQ_CtrlVal` | 目标反馈值 |

`gate_reverse` 字段：

| 字段 | 功能 |
| --- | --- |
| `enable` | 反相解释投退值。 |
| `distant` | 反相解释远方值。 |
| `lock` | 反相解释总闭锁值。 |
| `uplock` | 反相解释增闭锁值。 |
| `downlock` | 反相解释减闭锁值。 |
| `openloop` | 反相解释开环值。 |

`measurement_scale` 字段：

| 字段 | 功能 |
| --- | --- |
| `totalP` / `total_p` | 更新实时总有功时乘以该系数。 |
| `totalQ` / `total_q` | 更新实时总无功时乘以该系数。 |

功率跟随：

`follow`、`agc_follow`、`avc_follow` 字段结构相同：

| 字段 | 默认值 | 功能 |
| --- | --- | --- |
| `enable` | `false` | 是否启用周期跟随补偿。 |
| `period_ms` | `5000`，最小钳制为 `500` | 跟随周期。 |
| `step` | `100`，最小钳制为 `1` | 每次补偿步长。 |
| `tolerance` | `0.1`，范围钳制为 `0..1` | 允许误差比例。 |

启用后，程序在总控命令执行后记录目标值，并周期比较虚拟点实时 `TotalP` / `TotalQ`。低于 `target*(1-tolerance)` 时增加下发目标，高于 `target*(1+tolerance)` 时减少下发目标，每次调整 `step`。

## `onlineStatus_link`

用于修正和补充 `DevUpdate` 在线状态。

| 字段 | 类型 | 功能 |
| --- | --- | --- |
| `DeviceId` | string | 需要被联动的设备。 |
| `LinkToDeviceId` | string | 状态来源设备。 |

运行行为：

- 程序先缓存收到的 `DevUpdate` 中各设备状态。
- 如果本帧中出现 `DeviceId`，其 `Status` 会被替换为 `LinkToDeviceId` 的最新状态。
- 如果本帧没有出现 `DeviceId`，但已经知道 `LinkToDeviceId` 的状态，程序会追加一条该设备的 `DevUpdate`。
- 状态值支持字符串 `online/offline`、bool、数字；输出统一为 `online` 或 `offline`。

## `AgcAvc_debug`

| 字段 | 类型 | 默认值 | 功能 |
| --- | --- | --- | --- |
| `enable` | bool | `false` | 是否启用定时打印。 |
| `interval_ms` | int | `5000`，最小钳制为 `500` | 打印周期。 |
| `max_list` | int | `50`，小于 0 时钳制为 `0` | 离线设备列表最多打印数量。 |

当前会打印每个 AGC/AVC 组的设备总数、在线数、离线数、AGC/AVC gate 状态和目标值。

## 当前程序整体能做到的事

- 接收南向 `DataSpont`，按配置生成虚拟设备点、聚合点、转置点、反馈点。
- 对源点进行过滤，避免源点和聚合点重复上送；也可通过 `drop_operands=false` 保留源点。
- 接收北向 `CtrlCmd`，做点号映射、数值换算、一拆多、内部 `DataWrite`。
- 支持控制换算引用实时遥测值，适合做带当前值补偿的控制目标。
- 把虚拟并网点总有功/无功控制分配到多个南向设备，支持多并网点多组配置。
- 按设备在线状态和容量比例分配 AGC/AVC 目标值，并做上下限保护。
- 通过投退、远方、闭锁、开环和容量范围 gate 条件阻断不应执行的总控。
- 支持通过 DevUpdate 或独立在线遥信点判定 AGC/AVC 设备是否参与分配。
- 自动维护上下调闭锁状态：目标达到配置总上限或下限时，通过内部 `DataWrite` 写出对应闭锁点。
- 支持虚拟并网点实测总功率缩放和周期跟随补偿。
- 支持 DevUpdate 在线状态联动，让虚拟设备或派生设备跟随真实设备在线状态。
- 支持定时打印 AGC/AVC 运行态，便于现场排查。

## 注意事项

- JSON 文件本身不能写注释。
- `CtrlType` 当前不参与 `control_rules` 匹配；同一个 `DeviceId + dataRef` 配多条规则时会全部执行。
- `computation_points.operands` 若不是数组，解析器会设置错误信息但当前未立即 `return false`；实际配置仍应保证是数组。
- 顶层 `Voltage_AutoCtrl`、`Temperature_AutoCtrl` 以及现有样例里的 `publish_gap_ms`、`AgcAvc_debug.print_agcavc_online` 当前代码没有解析使用。
- AGC/AVC 设备唯一 key 当前优先使用 `DeviceId#ctrlDataRefP`。同一设备配置多个逻辑单元时，应确保有功控制点能区分逻辑单元。
