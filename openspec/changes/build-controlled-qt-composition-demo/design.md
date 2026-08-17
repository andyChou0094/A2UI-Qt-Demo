## Context

本 Demo 验证一个范围严格的架构命题：Agent 可以重新排列预先注册、可嵌入的 C++ Qt Widgets，同时业务组件保留原有实现、状态、内部连接和预定义业务行为。成功结论只代表受控编排机制可行，不代表 SurfaceSpec 可以生成任意前端，也不代表公司真实组件或生产网络环境已经完成兼容验证。

前端基线固定为 Qt 5.12.8、GCC 7.3.0 至 9.3.0 和 ISO C++14；Qt 模块限定为 Core、Widgets、Network、Test。两项已接受的架构约束继续主导设计：Agent 只能编排 Catalog 中注册的业务叶子；最终稳定组件身份由确定性的 Surface Compiler 而非 LLM 管理。编排请求链路与计算记录业务请求链路严格分离。

## Goals / Non-Goals

**目标：**

- 演示“自然语言 → 受限 LayoutPlan → 确定性 SurfaceSpec → 既有业务 QWidget 编排”的完整链路。
- 用递归 `Row/Column` 覆盖 Demo 所需的侧栏、上下分区、嵌套面板、2×2 区域和常见 Dashboard，并精确定义布局语义。
- 在固定传统窗口和动态 Surface 中复用完全相同、符合嵌入合同的有状态 QWidget 类。
- 在仅布局变化时复用未变业务叶子实例并保持其本地状态。
- 让 Calculator、CalculationHistory 与 CalculationStats 只通过独立后端 API 和 SQLite 状态形成可观察联动。
- 将所有远程规格视为不可信输入，失败或请求不可表达时保留最后一个有效界面。
- 在 Qt 5.12.8/C++14 下实现不阻塞 GUI 的网络和封闭协议校验，并在 GCC 7.3.0 与 9.3.0 两端验证构建。

**非目标：**

- 接入或认证公司真实 QWidget 组件。
- 声称 QWidget 叶子能够覆盖 QMainWindow、Dock、模态窗口、QML、Graphics View、非视觉对象或任意前端生成需求。
- 允许 Agent 检查或修改组件内部控件、信号、槽、脚本、QSS、URL、请求参数、Action 或数据绑定。
- 支持 Grid、跨行列、重叠、wrap、Splitter、Dock、响应式断点、多 Surface、多窗口、权限或 Agent 定义的跨组件行为。
- 支持 Streaming、WebSocket、MCP、AG-UI 或正式 A2UI 兼容。

## Decisions

### 固定 Qt 5.12.8、GCC 7.3.0 至 9.3.0 与 C++14

构建显式使用 `-std=c++14`，只使用 Qt 5.12.8 已存在的 Core、Widgets、Network、Test API。所有 C++ 依赖必须与实际 Qt 包、目标 GCC 和 libstdc++ ABI 匹配，不混用不同工具链生成的二进制插件。Qt 5.12.8 已归档，因此 HTTP 仅连接可信应用配置指定的本地回环地址，Demo 不作生产安全结论。

Qt 5.12 没有内建 JSON Schema Validator。共享 JSON Schema 保持协议权威，服务端按 Schema 校验；Qt 客户端使用 `QJsonDocument`、`QJsonObject`、`QJsonArray` 实现等价的封闭结构和语义校验，并用共享 Fixture 验证两端结论一致。业务和编排 HTTP 均使用 `QNetworkAccessManager` 异步调用，以 `QTimer` 实现超时，不阻塞 GUI 线程。

### 以注册的可嵌入 QWidget 作为业务叶子边界

`Calculator`、`CalculationHistory`、`CalculationStats`、`Clock` 和 `NotePad` 均为普通 C++ QWidget 类。Registry 通过 `std::function<QWidget *(QWidget *parent)>` 暴露工厂，Service 依赖由工厂闭包注入。业务叶子必须在 GUI 线程构造、可嵌入且非顶层，支持 resize、reparent 和 QObject 所有权，并自行管理内部布局、信号槽、状态及 size hint/policy。

Renderer 将业务叶子视为不透明整体，不读取子控件、不创建业务连接，也不覆盖内部布局、样式或 `QSizePolicy`。不符合合同的遗留组件只能由项目维护 Adapter QWidget；Adapter 对 Agent 仍不透明。`Row/Column` 是 Renderer 拥有的布局节点，不是业务组件，其容器可以在提交时重建；稳定对象身份保证只针对注册的业务叶子。

### 使用能力边界明确的封闭 SurfaceSpec v0

完整目标文档继续使用协议版本 `0.1`、唯一 `surfaceId=main`、一个 root ID 和有序邻接表。允许的布局节点仅为递归 `Row/Column`。该模型覆盖正交切分布局，但明确不覆盖 Grid/跨行列、重叠、wrap、Splitter、Dock 和响应式断点；不可表达的请求返回 `unsupported_layout`，不得静默近似。

布局语义固定如下：

- `gap` 为 `none/small/medium/large`，内部映射 `0/4/8/16` 个逻辑像素；布局容器 margin 为 0。
- `weight` 范围为 0 到 10，默认 0；正值映射为父级 `QBoxLayout` stretch factor。
- 任一直接子项具有正 `weight` 时，父容器 `justify` 必须为 `start`，且不添加主轴分布 spacer。
- 所有权重为 0 时，`justify` 通过已记录的 stretch spacer 模式实现；空容器不添加 spacer，单子项的 `spaceBetween` 等同 start，`spaceAround/spaceEvenly` 等同 center。
- `align=stretch` 不设置交叉轴 alignment，但不覆盖叶子的 `QSizePolicy`。
- `HostShell` 使用固定 `QScrollArea` 承载 Surface；内容最小尺寸超出视口时滚动，不要求 Agent 生成响应式布局。

拒绝新增通用样式和像素几何字段，因为它们会扩大 Agent 权限而不增强本次机制验证。未来只有真实组件提供可复现失败案例后，才按具体场景新增白名单容器。

### 使用五组件计算记录闭环

Calculator 在本地按预定义按钮执行四则运算，不解释任意脚本；成功结果通过 `CalculationService` 调用 `POST /api/calculations` 持久化。CalculationHistory 启动时、每 2 秒和手动操作时通过 GET 查询最近 50 条记录，并通过 PATCH 修改备注、通过 DELETE 删除记录。CalculationStats 启动时及每 2 秒查询 summary。Clock 与 NotePad 分别验证计时状态和未提交文本状态。

业务合同固定为 `POST/GET /api/calculations`、`PATCH/DELETE /api/calculations/{id}` 和 `GET /api/calculations/summary`；记录包含 `id`、`expression`、`result`、`note`、`createdAt`、`updatedAt`。SQLite 由 Mock Business API 独占访问并使用文件持久化，测试使用临时数据库。组件之间不得直接连接信号、读取彼此对象或使用共享事件总线；History 和 Stats 最迟在两个轮询周期内反映成功写入。

### 分离概率式规划与确定性编译

LLM 接收用户指令、当前 Surface 和 Catalog，并返回 LayoutPlan；LayoutPlan 可以引用既有实例或申请创建新类型实例。Surface Compiler 解析引用、执行多实例规则、分配新 ID、复用有效旧 ID，并生成完整邻接表。Fixture 与 LLM 计划进入同一编译器。拒绝让 LLM 输出最终 ID，因为细微文本变化可能错误销毁有状态对象或错误处理同类型多实例。

### 前后端双重校验并在客户端事务式提交

后端在返回前校验编译结果；Qt 客户端仍独立验证协议、封闭字段、Catalog、ID/root、引用顺序、可达性、无环、单父节点、布局字段位置、`weight/justify` 组合、节点上限 32 和深度上限 8。Reconciler 先构建完整变更计划和暂存新增对象，确认成功后再提交；失败时释放暂存资源并保留旧树。

### 按 `(id, component type)` 协调业务叶子

ID 与类型均相同时，现有业务 QWidget 从旧布局安全脱离并复用；新 ID 通过 Registry 创建；旧 ID 对应不同类型时替换旧实例。成功提交后才销毁已移除节点，之后重新添加时使用新 ID 和全新状态。Renderer 可以重建 Row/Column 容器，但不得借此重建未变业务叶子。

### 隔离编排流量与业务流量

`POST /compose` 属于 Agent Service；`/api/calculations*` 属于独立 Mock Business API，只能由 C++ `CalculationService` 调用。两者可以共用 FastAPI 进程，但必须使用独立模块、Router、客户端和数据访问依赖；`/compose` 不得导入或调用计算记录数据访问层。业务 URL、method、超时及请求结构来自可信应用配置和编译代码，不进入任何 Agent 可控产物。

## Risks / Trade-offs

- [Row/Column 被误解为通用布局语言] → 在目标、错误合同和验收中明确能力矩阵，对不可表达请求返回 `unsupported_layout`。
- [Qt 5.12.8 已归档且存在安全与兼容限制] → 仅使用回环 HTTP，固定 API 子集，记录 ABI，并禁止外推生产结论。
- [手写 Qt 封闭校验与共享 Schema 漂移] → 前后端对同一组有效和非法 Fixture 运行合同测试。
- [轮询存在最多两个周期的可见延迟] → 固定 2 秒周期并把 4 秒内可见作为验收上限；不为 Demo 引入 WebSocket 或事件总线。
- [QWidget 重设父级或布局所有权可能导致意外销毁] → Reconciler 集中管理业务叶子所有权、提交前暂存，并测试对象身份与销毁信号。
- [Qt size hint 无法实现强制填充] → `align=stretch` 尊重组件自身 `QSizePolicy`，超出视口时由固定 QScrollArea 滚动。
- [SQLite 或业务 API 失败] → 组件保留最后有效显示或本地计算结果，显示错误状态，不伪造持久化成功。
- [LLM 输出不稳定或格式错误] → 输出限制为 LayoutPlan，严格校验，提供确定性 Fixture，并禁止绕过编译器。

## Migration Plan

1. 建立 Qt 5.12.8/C++14 构建入口，在 GCC 7.3.0 和 9.3.0 两端记录 Qt 包与 ABI 并完成最小构建。
2. 在不依赖动态编排的情况下交付五个传统 QWidget、`LegacyToolboxWindow`、`CalculationService`、业务 CRUD 和文件型 SQLite。
3. 用临时数据库验证 Calculator → API → History/Stats 的轮询联动和失败状态。
4. 引入共享 Catalog、Schema 和布局 Fixture，随后实现 Qt 等价校验、Registry、Row/Column Adapter、QScrollArea 和单次渲染。
5. 实现业务叶子 Reconciler 和生命周期保证，再连接 Agent Service、Surface Compiler 与 LLM。
6. 回滚时可禁用动态宿主和 Agent Service；五个业务组件、传统窗口和 Mock Business API 仍可独立运行。

## Open Questions

- 实施环境最终采用 CMake 还是 qmake 作为构建入口；无论选择哪一种，都必须固定 Qt 5.12.8 和 `-std=c++14`。
- 可选自然语言验收链路使用哪个 LLM Provider、Model 和凭据注入机制。
