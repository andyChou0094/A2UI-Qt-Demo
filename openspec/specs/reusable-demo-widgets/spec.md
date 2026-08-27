# reusable-demo-widgets Specification

## Purpose

定义演示业务 QWidget 在单一动态 HostShell 的不同 Surface 中共享实现的复用合同，以及本地状态、持久化 API 交互、组件隔离和多实例运行语义。

## Requirements

### Requirement: 共享不透明的可嵌入 QWidget 实现
系统 SHALL 将 `Calculator`、`CalculationHistory`、`CalculationStats`、`Clock` 和 `NotePad` 实现为可嵌入、非顶层的 C++ QWidget 类，并由动态 Renderer 在不同合法 Surface 中使用相同的类，不得复制或改造它们的内部控件树。

#### Scenario: 不同 Surface 使用相同实现
- **WHEN** 同一组件类型出现在默认、导入或模型编排的动态 Surface 中
- **THEN** 每个实例均由同一个已注册 C++ QWidget 类构造

### Requirement: Registry 强制执行业务叶子合同
Registry SHALL 使用 `std::function<QWidget *(QWidget *parent)>` 工厂创建业务叶子；注册对象 SHALL 能在 GUI 线程构造、支持 resize、reparent 和 QObject 所有权，并自行管理内部布局、信号槽、状态和 size hint/policy。Renderer SHALL NOT 读取其子控件、建立业务连接或覆盖其内部布局、样式或 `QSizePolicy`。

#### Scenario: 拒绝顶层窗口工厂
- **WHEN** 一个工厂返回依赖 `Qt::Window` 顶层语义且不能嵌入布局的 QWidget
- **THEN** Registry 拒绝将其作为业务叶子注册或实例化

#### Scenario: 通过固定 Adapter 接入遗留组件
- **WHEN** 遗留组件本身不符合嵌入合同但项目提供了符合合同的 Adapter QWidget
- **THEN** Catalog 只暴露 Adapter 对应业务类型且 Renderer 仍将其视为不透明叶子

### Requirement: 有状态的本地行为
`Calculator` SHALL 在本地按预定义按钮执行四则计算且不得解释任意脚本；`Clock` 与 `NotePad` SHALL 分别保留计时和未提交文本状态，且三者均不依赖 Renderer 创建信号槽连接。

#### Scenario: 重排后保留本地状态
- **WHEN** 未发生变化的 Calculator、Clock 或 NotePad 业务叶子因 Surface 更新而移动
- **THEN** 其对象身份、当前输入或文本、Qt 允许情况下的焦点以及运行状态均保持不变

#### Scenario: 非法计算不入库
- **WHEN** Calculator 遇到除零或非法输入
- **THEN** 它显示本地错误且不调用创建计算记录的业务 API

### Requirement: 预定义计算记录业务合同
`CalculationService` SHALL 从可信应用配置和 C++14 代码获取本地回环 API 地址、HTTP method、超时和请求响应结构，并 SHALL 提供异步创建、查询、修改备注、删除和查询摘要操作；任何编排产物 SHALL NOT 提供这些请求细节。

#### Scenario: 计算请求独立于编排
- **WHEN** Calculator 成功得到本地结果并请求持久化
- **THEN** `CalculationService` 调用预定义 `POST /api/calculations` 合同，且不从 Catalog、LayoutPlan 或 SurfaceSpec 读取 URL、method、body 结构或 Action

### Requirement: Calculator 创建持久化记录
Calculator SHALL 在本地计算成功后通过 `CalculationService` 创建包含 expression 和 result 的记录；若写入失败，Calculator SHALL 保留本地结果并显示失败状态，但 SHALL NOT 将其标记为已持久化。

#### Scenario: 成功计算并创建记录
- **WHEN** 用户通过预定义按钮完成一次有效四则运算且业务 API 可用
- **THEN** Calculator 显示结果并创建一条后端计算记录

#### Scenario: 写入失败保留本地结果
- **WHEN** 本地计算成功但创建记录请求超时或失败
- **THEN** Calculator 保留计算结果、显示持久化错误且后续仍可继续使用

### Requirement: History 提供计算记录 CRUD
CalculationHistory SHALL 在启动时、每 2 秒和用户手动刷新时查询按创建时间倒序排列的最近 50 条记录，并 SHALL 支持通过预定义 PATCH 仅修改 note、通过预定义 DELETE 删除记录。

#### Scenario: 查询最近记录
- **WHEN** CalculationHistory 启动或轮询周期到达
- **THEN** 它通过 `GET /api/calculations?limit=50` 更新可见记录且不直接访问 SQLite

#### Scenario: 修改备注并删除记录
- **WHEN** 用户在 History 中保存备注后删除同一记录
- **THEN** 组件先调用 `PATCH /api/calculations/{id}` 更新 note，再调用 `DELETE /api/calculations/{id}` 删除记录

### Requirement: Stats 展示后端摘要
CalculationStats SHALL 在启动时及每 2 秒通过 `GET /api/calculations/summary` 查询记录总数与最新记录，并 SHALL 在请求失败时保留最后有效摘要并显示过期或错误状态。

#### Scenario: 展示最新摘要
- **WHEN** 后端存在至少一条计算记录且 Stats 完成轮询
- **THEN** Stats 显示当前记录总数及最新 expression/result

### Requirement: 组件仅通过后端持久化状态联动
Calculator、CalculationHistory 和 CalculationStats SHALL NOT 直接连接跨组件信号、读取彼此对象或使用共享事件总线；成功创建记录后，History 与 Stats SHALL 最迟在两个 2 秒轮询周期内仅通过各自 API 查询反映变化。

#### Scenario: 后端介导的可见联动
- **WHEN** Calculator 成功创建一条记录且 History 与 Stats 正常轮询
- **THEN** 两个查询组件在 4 秒内显示对应记录或摘要变化，且不存在 Calculator 到它们的直接通知

### Requirement: SQLite 由业务后端独占并持久化
Mock Business API SHALL 独占 SQLite 访问并使用文件数据库保存 `id`、`expression`、`result`、`note`、`createdAt`、`updatedAt`；自动化测试 SHALL 使用独立临时数据库，Qt 前端 SHALL NOT 链接 QtSql 或直接访问数据库文件。

#### Scenario: 后端重启后保留数据
- **WHEN** 一条记录成功写入文件数据库且 Mock Business API 使用同一数据库文件重启
- **THEN** 后续 GET 查询仍返回该记录

### Requirement: 组件多实例
当 Catalog 条目允许多实例时，系统 SHALL 支持同时存在多个 Calculator、CalculationHistory、CalculationStats、Clock 或 NotePad 实例；同类查询组件可以共享后端数据，但 SHALL 保持独立 QWidget 对象和本地显示状态。

#### Scenario: 两个 Calculator 共存
- **WHEN** 一个有效 Surface 包含两个 ID 不同的 Calculator 业务叶子
- **THEN** Renderer 创建并维护两个相互独立的 Calculator QWidget 实例
