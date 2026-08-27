# safe-surface-rendering Specification

## Purpose

定义 Qt 客户端对 SurfaceSpec 的独立封闭校验、复杂度限制、原子应用与失败诊断行为，确保无效更新不会破坏最后一个有效界面。

## Requirements

### Requirement: 客户端独立校验
Qt 5.12.8 客户端 SHALL 独立于后端校验，使用 `QJsonDocument`、`QJsonObject` 和 `QJsonArray` 对每个收到的 SurfaceSpec 执行与共享 Schema 等价的封闭校验，包括协议版本、Surface 与 root 唯一性、Catalog 业务类型与字段成员关系、Row/Column 字段、ID 唯一性、引用有效性和顺序、无环、单父节点、可达性、布局字段位置以及 `weight/justify` 组合。

#### Scenario: 客户端拒绝循环图
- **WHEN** 收到的 SurfaceSpec 包含 children 引用环
- **THEN** Qt 客户端拒绝该规格，且不修改当前 Surface

#### Scenario: 前后端校验结果一致
- **WHEN** 同一组有效和非法共享 Fixture 分别进入服务端 Validator 与 Qt 客户端 Validator
- **THEN** 两端对每个 Fixture 给出一致的接受或拒绝结论

### Requirement: 可配置的复杂度上限
Validator SHALL 强制执行可配置的复杂度限制，默认最多 32 个节点、最深 8 层。

#### Scenario: 拒绝超限文档
- **WHEN** SurfaceSpec 超过任一已配置限制
- **THEN** 系统在通过 Registry 构造组件或修改布局前使校验失败

### Requirement: 原子应用 Surface
Renderer SHALL 在将目标 Surface 作为一次逻辑更新提交前，完整校验新规格并预构建可执行的 Reconciliation Plan。

#### Scenario: 暂存失败时保留旧界面
- **WHEN** 校验、组件构造或变更计划准备失败
- **THEN** 系统释放所有暂存资源，且之前已提交的界面保持可用和不变

### Requirement: 宿主诊断状态
固定 `HostShell` SHALL 提供自然语言输入、由固定 `QScrollArea` 承载的动态 `main` 区域、请求与进度状态，以及显示可执行诊断信息的 `StatusPanel`。

#### Scenario: 非法 JSON 可见但不破坏界面
- **WHEN** 编排响应无法解析为 SurfaceSpec
- **THEN** `StatusPanel` 报告失败，且继续显示最后一个有效 Surface

#### Scenario: 不支持布局可见但不破坏界面
- **WHEN** 编排服务返回 `unsupported_layout`
- **THEN** `StatusPanel` 显示能力限制诊断，且继续显示最后一个有效 Surface
