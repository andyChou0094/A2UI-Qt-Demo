## ADDED Requirements

### Requirement: 按稳定 ID 和类型复用
当注册业务叶子的组件 ID 和 component type 均未变化时，Reconciler SHALL 复用现有 QWidget，包括其父布局或同级顺序发生变化的情况。Renderer MAY 在提交时重建其拥有的 Row/Column 布局容器，但 SHALL NOT 因此重建未变业务叶子。

#### Scenario: 重排时保留实例
- **WHEN** 新的有效目标移动一个既有节点，但不改变其 ID 或类型
- **THEN** 同一个 QWidget 对象被插入新的布局位置，且其状态保持不变

#### Scenario: 重建布局容器不重建业务叶子
- **WHEN** Renderer 为有效新目标重建 Row/Column 容器且 Calculator、Clock、NotePad 与 History 的 ID 和类型未变化
- **THEN** 这些业务 QWidget 的对象身份、输入、计时、未提交文本和选择状态保持不变

### Requirement: 确定性创建和替换实例
Reconciler SHALL 为每个新的业务叶子 ID 创建 Registry 实例；当同一个业务叶子 ID 对应不同 component type 时，SHALL 替换既有实例。Renderer 拥有的 Row/Column 节点 SHALL NOT 通过业务 Registry 创建。

#### Scenario: 类型变化时替换对象
- **WHEN** 有效目标为一个既有 ID 指定了不同的已注册类型
- **THEN** 旧 QWidget 在提交后退出使用，并由新构造的 QWidget 表示该 ID

### Requirement: 提交后销毁已移除实例
Reconciler SHALL 销毁并释放成功提交的目标中明确缺失的业务 QWidget；之后重新添加时 SHALL 使用新的稳定 ID 和全新实例状态。

#### Scenario: 移除后重新添加产生全新状态
- **WHEN** 一个 NotePad 业务叶子被成功移除，后续 LayoutPlan 又申请新的 NotePad
- **THEN** 旧对象已被销毁，后续 QWidget 使用不同 ID 和新初始化状态

### Requirement: 失败更新不得销毁活动实例
Reconciler SHALL 将破坏性生命周期操作推迟到有效暂存目标可以提交之后执行。

#### Scenario: 替换组件构造失败
- **WHEN** 暂存阶段无法构造所需替换 QWidget
- **THEN** 当前显示的 QWidget 均不会被销毁，也不会从已提交 Surface 中脱离
