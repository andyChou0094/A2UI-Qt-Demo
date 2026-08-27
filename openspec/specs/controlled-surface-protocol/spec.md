# controlled-surface-protocol Specification

## Purpose

定义受控 SurfaceSpec v0 协议、Catalog 权限边界，以及布局和展示字段的确定性语义。

## Requirements

### Requirement: Catalog 是业务编排能力的唯一依据
系统 SHALL 维护一份项目自有 Catalog，描述每个可编排业务叶子的 type、version、description、multiple、允许的展示参数和布局说明，并 SHALL 拒绝 Catalog 未授权的业务类型或字段；Renderer 拥有的 `Row`、`Column` SHALL 作为协议布局节点单独校验，不作为业务 QWidget Catalog 条目。

#### Scenario: 拒绝未知业务组件
- **WHEN** LayoutPlan 或 SurfaceSpec 引用了 Catalog 中不存在的业务组件类型
- **THEN** 系统在修改任何活动 QWidget 树之前使校验失败

### Requirement: 完整的单 Surface 规格
SurfaceSpec v0 SHALL 使用协议版本 `0.1`、Surface ID `main`、唯一 root ID，以及包含唯一节点 ID 和有序 children 引用的组件列表，描述一个完整目标 Surface。

#### Scenario: 接受完整有效目标
- **WHEN** 每个节点均可从声明的 root 唯一到达，且每个有序 children 引用均有效
- **THEN** 该文档可以进入语义校验和渲染阶段

### Requirement: Row 和 Column 的明确能力边界
协议 SHALL 仅开放可递归嵌套的 `Row` 与 `Column` 布局节点，用于表达正交切分布局，并 SHALL NOT 表达 Grid、跨行列、重叠、wrap、Splitter、Dock 或响应式断点。

#### Scenario: 拒绝不可表达布局
- **WHEN** 用户请求 Grid 跨列、重叠、拖拽分栏、Dock、wrap 或响应式断点
- **THEN** Agent Service 返回 `unsupported_layout` 且当前 Surface 保持不变，不生成近似布局

### Requirement: 确定性的 gap 和容器边界
`Row` 与 `Column` 的 `gap` SHALL 仅允许 `none`、`small`、`medium` 或 `large`，默认值为 `medium`，Renderer SHALL 分别映射为 0、4、8、16 个逻辑像素；布局容器 margin SHALL 固定为 0，外层留白 SHALL 由 HostShell 管理。

#### Scenario: gap token 被确定性映射
- **WHEN** Renderer 渲染 `gap=large` 的有效 Row 或 Column
- **THEN** 相邻内容项使用 16 个逻辑像素的固定间距且容器自身 margin 为 0

### Requirement: 无权重时的主轴分布
当 `Row` 或 `Column` 的所有直接子项 `weight` 均为 0 时，`justify` SHALL 仅允许 `start`、`center`、`end`、`spaceBetween`、`spaceAround` 或 `spaceEvenly`，默认值为 `start`，并 SHALL 按已记录的 stretch spacer 模式确定性实现。空容器 SHALL 不添加 spacer；单子项 `spaceBetween` SHALL 等同 `start`，单子项 `spaceAround` 与 `spaceEvenly` SHALL 等同 `center`。

#### Scenario: 单子项 spaceAround 居中
- **WHEN** 无正权重的 Row 只有一个直接子项且 `justify=spaceAround`
- **THEN** Renderer 在该子项首尾添加等权 stretch spacer 使其沿主轴居中

### Requirement: 相对子项权重与 justify 互斥规则
非 root 节点作为父布局直接子项时 SHALL 接受 0 到 10 的整数 `weight`，默认值为 0；正权重 SHALL 映射为父级 `QBoxLayout` stretch factor。当任一直接子项 `weight>0` 时，父布局 `justify` SHALL 为 `start` 且 Renderer SHALL NOT 添加主轴分布 spacer；其他组合 SHALL 被拒绝。

#### Scenario: 使用相对尺寸突出业务叶子
- **WHEN** 两个相邻节点的 weight 分别为 1 和 2 且父容器 `justify=start`
- **THEN** 父布局应用 1 和 2 的 stretch factor，且不要求像素尺寸或主轴 spacer

#### Scenario: 拒绝权重与居中分布组合
- **WHEN** 一个直接子项 `weight>0` 且父容器 `justify=center`
- **THEN** 系统在修改任何活动 QWidget 树之前拒绝该 SurfaceSpec

### Requirement: 交叉轴对齐尊重业务组件尺寸策略
`align` SHALL 仅允许 `start`、`center`、`end` 或 `stretch`，默认值为 `stretch`。`start/center/end` SHALL 按布局方向映射交叉轴 `Qt::Alignment`；`stretch` SHALL 不设置交叉轴 alignment，也 SHALL NOT 覆盖业务 QWidget 的 `QSizePolicy`、size hint 或最小/最大尺寸。

#### Scenario: stretch 不改写业务叶子策略
- **WHEN** Renderer 在 `align=stretch` 的容器中放置一个具有固定横向 QSizePolicy 的业务 QWidget
- **THEN** Renderer 不改写该 QSizePolicy，最终尺寸继续受组件自身策略约束

### Requirement: 固定滚动宿主处理溢出
HostShell SHALL 使用不受 Agent 控制的 `QScrollArea` 承载 `main` Surface，并 SHALL 在内容最小尺寸超过视口时提供滚动，而不是要求 SurfaceSpec 生成响应式布局或裁剪内容。

#### Scenario: 最小尺寸超过视口
- **WHEN** 有效 Surface 的合计最小尺寸超过动态区域视口
- **THEN** HostShell 显示所需滚动条且 SurfaceSpec 保持不变

### Requirement: 禁止可执行行为和任意展示权限
SurfaceSpec、LayoutPlan 和 Catalog SHALL NOT 包含由 AI 生成的 URL、HTTP method、请求参数或 body、Action、数据绑定、信号槽定义、脚本、QSS、颜色、字体、绝对坐标、任意 margin 或 padding，以及像素、最小或最大尺寸。

#### Scenario: 拒绝禁止字段
- **WHEN** 其他部分有效的业务叶子包含任意 QSS、URL、Action 或像素宽度字段
- **THEN** Schema 或语义校验拒绝该文档
