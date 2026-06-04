# LogicCenter 可视化配置开发计划

本文基于 `memory/LogicCenter可视化配置设计草稿.md`，整理 LogicCenter
可视化配置功能的分阶段开发计划。

开发策略：先做数据模型、导入导出、校验器和点位选择器这些底座能力，再逐步实现
AGC/AVC、计算点、控制转换、在线联动和虚拟设备派生等业务界面。

## 阶段 1：数据模型与导入导出底座

目标：把 `LogicCenter_Config.json` 从裸 `QJsonObject` 变成结构化领域模型。

主要任务：

- 新增 `LogicCenterConfig` 数据结构。
- 支持导入现有 `LogicCenter_Config.json`。
- 支持导出回 `LogicCenter_Config.json`。
- 保留未知字段，避免破坏用户手写或旧版本配置。
- 统一导出字段风格，优先使用 `groupId`、`virtualDeviceId`、`targetType` 等当前推荐写法。

建议模型：

- `LogicComputationPoint`
- `LogicOperand`
- `LogicControlRule`
- `LogicControlTarget`
- `AgcAvcGroup`
- `AgcAvcDevice`
- `LogicOnlineStatusLink`
- `AgcAvcDebugConfig`

阶段完成标准：

- 示例 JSON 可以导入为内存模型。
- 内存模型可以重新导出为 JSON。
- 导入再导出不丢失已知业务字段。
- 未知字段有明确保留策略。

## 阶段 2：校验器

目标：建立统一的 LogicCenter 配置校验能力，为 UI 提示和导出前检查服务。

建议统一问题结构：

```cpp
struct ConfigIssue {
    Severity severity;
    QString module;
    QString objectId;
    QString message;
};
```

基础校验项：

- JSON 结构错误。
- `computation_points.operands` 为空或格式错误。
- 计算公式中的 `{n}` 超出 `operands` 范围。
- 计算公式存在未使用源点。
- AGC/AVC 设备容量异常：`pMax <= pMin`、`qMax <= qMin`。
- AGC/AVC 组总容量为 0。
- `virtualDeviceId` 重复。
- 控制规则 `match.DeviceId + match.dataRef` 重复，提示重复规则会全部执行。
- `targetType` 非法。
- 目标设备或目标点在当前工程中找不到。
- 虚拟设备派生的目标设备不存在。
- 虚拟设备派生的目标点不存在于目标模型点表中。
- `CtrlType` 当前不参与匹配，需要提示用户。
- `drop_operands=false` 会保留源点原始转发，需要提示用户确认。

阶段完成标准：

- 能对导入的 `LogicCenterConfig` 输出问题列表。
- UI 尚未完成时，也可以在日志或摘要中查看校验结果。
- 后续各编辑页面可以复用同一套校验器。

## 阶段 3：点位选择器

目标：实现可复用的设备/点位选择组件，避免用户手工输入 `DeviceId` 和 `dataRef`。

能力要求：

- 从当前 `ConfigProject` 中列出设备。
- 按遥测、遥信、控制过滤点位。
- 支持搜索 `DeviceId`、`dataRef`、展示名、描述。
- 显示所属模型、设备、点位类型。
- 返回选择结果：`DeviceId + dataRef + 点位类型`。
- 支持最近使用。
- 支持从源设备点表批量选择点位。

阶段完成标准：

- AGC/AVC、计算点、控制转换、虚拟设备派生都可以复用该组件。
- 用户在常规配置中基本不需要手工输入点号。

## 阶段 4：LogicCenter 总览页

目标：提供 LogicCenter 配置入口和摘要信息。

展示内容：

- 计算点数量。
- 控制规则数量。
- AGC/AVC 组数量。
- 在线联动数量。
- 虚拟设备派生数量。
- 校验问题数量。
- JSON 导入/导出状态。

入口操作：

- 快速配置。
- AGC/AVC。
- 虚拟设备派生。
- 计算点。
- 控制转换。
- 在线联动。
- 高级与调试。
- JSON 预览与校验。

阶段完成标准：

- 用户可以从现有配置工作区进入 LogicCenter 配置模块。
- 能看到当前 LogicCenter 配置摘要和校验问题数量。

## 阶段 5：AGC/AVC 单虚拟并网点配置

目标：优先支持 90% 常见场景，即一台机器只有一个虚拟并网点。

功能范围：

- 编辑 `groupId`。
- 编辑 `virtualDeviceId`。
- 添加、删除南向设备。
- 为南向设备选择 P 控制点、Q 控制点。
- 为南向设备选择在线状态点。
- 编辑 P/Q 容量上下限。
- 编辑 `scaleP`、`scaleQ`。
- 编辑 `measurement_scale`。
- 编辑 `gate_reverse`。
- 编辑 `agc_follow`、`avc_follow`。

交互策略：

- 当只有一个组时，界面标题显示“虚拟并网点配置”，弱化“组”概念。
- 当存在多个组时，再显示组列表。
- 多组新增能力可以先放到高级入口中。

阶段完成标准：

- 用户可以完成单虚拟并网点 AGC/AVC 配置。
- 能导出有效的 `AgcAvcGroups`。
- 校验器能提示容量、点位、设备等常见问题。

## 阶段 6：计算点模板生成

目标：先支持常见模板，减少手工编写 `computation_points`。

优先模板：

1. 多设备有功求和，生成 `TotalP`。
2. 多设备无功求和，生成 `TotalQ`。
3. 功率因数计算，生成 `Cos`。
4. 单点映射或改名。
5. 遥信 OR。
6. 遥信 AND。

模板配置项：

- 输出设备。
- 输出点号。
- 源设备和源点列表。
- 是否删除源点转发，对应 `drop_operands`。

阶段完成标准：

- 可以通过模板生成常见 `computation_points`。
- AGC/AVC 页面可以提供“生成 TotalP/TotalQ/Cos”的快捷操作。
- 生成后的计算点可以在计算点列表中查看和编辑。

## 阶段 7：控制转换编辑器

目标：实现 `control_rules` 的可视化配置。

功能范围：

- 源设备选择。
- 源控制点选择。
- 不展示 `CtrlType`，运行时按源设备和源控制点匹配，`CtrlType` 复用北向原始下发帧。
- 支持一个源控制点配置多个目标动作。
- 目标动作类型选择：`ctrlcmd` 或 `data_write`。
- 目标设备、目标点选择。
- 表达式编辑。

表达式快捷模板：

- 原值：`{x}`
- 取反：`1 - {x}`
- 比例换算：`{x} * 系数`
- 固定值。
- 加实时值：`{x} + {rt:DeviceId#DataRefer}`

阶段完成标准：

- 用户可以配置常见北向控制映射。
- `{rt:...}` 引用可以通过点位选择器插入。
- 当目标为虚拟设备 `TotalP_Ctrl` 或 `TotalQ_Ctrl` 时，界面提示会进入 AGC/AVC 分配逻辑。

## 阶段 8：在线状态联动

目标：实现 `onlineStatus_link` 的轻量编辑。

功能范围：

- 表格编辑被联动设备和跟随设备。
- 两列都从设备选择器选择。
- 快捷生成：虚拟设备跟随真实设备。
- 快捷生成：派生设备跟随真实设备。
- 校验设备是否存在。

阶段完成标准：

- 用户可以新增、删除、编辑在线状态联动。
- 能导出有效的 `onlineStatus_link`。

## 阶段 9：虚拟设备派生 / 点位批量映射

目标：支持“一个真实南向设备承载多个北向虚拟子设备”的复杂场景。

第一版范围：

- 单源设备到多个目标虚拟设备。
- 点到点原值映射。
- 缩放。
- 偏移。
- 线性变换。
- 取反。
- 自定义公式。
- 批量生成 `computation_points`。
- 校验目标设备是否存在。
- 校验目标点是否存在于目标模型点表中。

后续增强：

- 逆变器映射模板。
- 气象仪映射模板。
- 汇流箱映射模板。
- 自定义 CSV 或表格导入映射。
- 多源设备到一个目标虚拟设备。

阶段完成标准：

- 用户可以从真实设备批量派生多个虚拟设备点位。
- 不需要逐条手工配置点到点 `computation_points`。
- 生成结果可在计算点列表中查看和维护。

## 阶段 10：JSON 预览与高级编辑

目标：为高级用户保留 JSON 级别的检查和编辑入口。

功能范围：

- JSON 只读预览。
- 导出前格式化显示。
- 高级 JSON 编辑开关。
- 高级编辑后重新解析并校验。
- 未知字段保留策略提示。

阶段完成标准：

- 普通用户可以预览最终导出的 JSON。
- 高级用户可以在明确风险提示后修改 JSON。
- 手工修改后能重新进入结构化模型和校验流程。

## 推荐落地顺序

1. `LogicCenterConfig` 结构化模型。
2. JSON 导入/导出。
3. 校验器。
4. 点位选择器。
5. LogicCenter 总览页。
6. 单虚拟并网点 AGC/AVC 页面。
7. `TotalP`、`TotalQ`、`Cos` 计算点模板。
8. 控制转换页面。
9. 在线联动页面。
10. 虚拟设备派生。
11. JSON 预览与高级编辑。

## 第一阶段可交付目标

建议第一轮做到第 7 步：

- 可以导入和导出 LogicCenter 配置。
- 可以查看 LogicCenter 配置摘要。
- 可以校验常见问题。
- 可以配置单虚拟并网点。
- 可以选择南向设备和控制点。
- 可以生成 `TotalP`、`TotalQ`、`Cos` 计算点。
- 可以导出可用的 `LogicCenter_Config.json`。

完成这些后，工具已经能覆盖大部分单虚拟并网点项目。控制转换、在线联动和虚拟设备派生可以作为后续迭代逐步补齐。
