# SurfaceSpec v0 布局语义

本文档定义 JSON Schema 无法跨节点表达、但服务端与 Qt Validator 必须共同执行的语义规则。

## 默认值与确定性映射

- `gap` 缺省为 `medium`，`none/small/medium/large` 依次映射为 0/4/8/16 个逻辑像素。
- Row/Column 容器的 layout margin 固定为 0，SurfaceSpec 不提供 margin 或 padding 字段。
- `align` 缺省为 `stretch`；该值不设置交叉轴 alignment，也不改写业务 QWidget 的 `QSizePolicy`。
- `justify` 缺省为 `start`；`weight` 缺省为 0，允许范围为整数 0 到 10。

## weight 与 justify

- root 节点不得声明 `weight`。
- `weight` 只描述节点作为父布局直接子项时的相对伸展系数。
- 任一直接子项 `weight>0` 时，父 Row/Column 的 `justify` 必须为 `start`，Renderer 只设置对应的 `QBoxLayout` stretch factor，不添加主轴 spacer。
- 所有直接子项 `weight=0` 时，Renderer 才按 `justify` 添加确定性的主轴 stretch spacer。

## 空容器与单子项

- 空 Row/Column 不添加任何 spacer。
- 单子项的 `spaceBetween` 等同 `start`。
- 单子项的 `spaceAround` 与 `spaceEvenly` 等同 `center`，即在子项首尾添加等权 stretch spacer。

## 无权重时的主轴 spacer 模式

- `start`：内容之后添加一个 stretch spacer。
- `center`：内容之前和之后各添加一个等权 stretch spacer。
- `end`：内容之前添加一个 stretch spacer。
- `spaceBetween`：只在相邻内容项之间添加等权 stretch spacer。
- `spaceAround`：首尾 spacer 的 stretch factor 为 1，相邻内容项之间为 2。
- `spaceEvenly`：首尾及相邻内容项之间的 spacer stretch factor 均为 1。

## 图与复杂度不变量

- 节点 ID 全局唯一，root 必须存在；children 引用保持声明顺序且均指向已声明节点。
- 每个非 root 节点恰有一个父节点，所有节点从 root 可达，图中不得存在环。
- 默认最多 32 个节点、最深 8 层；Catalog 的 `multiple=false` 类型最多出现一次。
- `Row`、`Column` 不是 Catalog 业务类型；其他节点类型必须存在于 Effective Catalog。
