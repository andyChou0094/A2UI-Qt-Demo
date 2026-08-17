## Why

我们需要验证：自然语言指令能否在不重写既有 C++ Qt Widgets 的界面、状态、行为和业务请求的前提下，安全地重新编排这些组件。当前方案还需要明确 `Row/Column` 的能力边界、可注册 QWidget 合同、Qt 5.12.8/GCC 兼容基线，以及只经后端 API 联动的业务闭环，才能形成可实施且不过度外推的 Demo。

## What Changes

- 新增一个 C++/Qt Widgets 宿主应用，包含固定的 `HostShell`、自然语言输入区、由 `QScrollArea` 承载的单一动态 `main` 表面、状态展示，以及用于对照的传统固定布局窗口。
- 新增可复用的 `Calculator`、`CalculationHistory`、`CalculationStats`、`Clock` 和 `NotePad` QWidget 类；固定版与动态版使用完全相同的实现。
- 定义项目自有的组件目录（Component Catalog）和 `Qt UI Composition Protocol v0` SurfaceSpec Schema，仅允许使用已注册业务叶子以及受控、可递归嵌套的 `Row`/`Column` 布局节点；明确拒绝 Grid、跨行列、重叠、Splitter、Dock、wrap 和响应式断点。
- 明确可注册业务叶子的 QWidget 合同与 `std::function<QWidget *(QWidget *parent)>` 工厂接口；不符合合同的遗留控件只能通过项目维护的 Adapter 注册。
- 新增客户端校验、基于 Registry 的组件创建、确定性 Qt 布局映射、事务式协调更新、生命周期管理和稳定实例状态保持。
- 新增 FastAPI 编排端点：将受约束的 LLM LayoutPlan 交给拥有稳定组件 ID 的确定性表面编译器（Surface Compiler），生成完整 SurfaceSpec。
- 新增相互独立的计算记录业务 API、文件型 SQLite 数据库和预定义 C++ `CalculationService`；Calculator、History 与 Stats 只通过该后端状态联动，请求细节不得进入提示词、Catalog、LayoutPlan 或 SurfaceSpec。
- 固定前端基线为 Qt 5.12.8、GCC 7.3.0 至 9.3.0、ISO C++14，并限制 Qt 模块为 Core、Widgets、Network、Test。
- 新增 Fixture 与验收测试，覆盖确定性嵌套布局、同类型多实例、保持状态的重排、移除与重建、业务 CRUD/轮询联动、SQLite 持久化，以及非法和不支持更新的拒绝与回滚。
- 明确排除任意样式、AI 生成请求或信号槽、多 Surface、流式传输，以及对公司真实组件兼容性的结论。

## Capabilities

### New Capabilities

- `reusable-demo-widgets`：固定版和动态版共享符合嵌入合同的有状态 C++ QWidget 组件，并通过预定义计算记录业务请求形成后端联动。
- `controlled-surface-protocol`：由 Catalog 管控的 SurfaceSpec v0 结构、明确的 Row/Column 能力边界及受限、确定性的布局语义。
- `safe-surface-rendering`：客户端结构和语义校验、原子提交，以及可诊断的失败处理。
- `stable-widget-reconciliation`：基于稳定 ID 的 QWidget 复用、移动、销毁及状态与生命周期保证。
- `agent-surface-composition`：从自然语言和当前 Surface 出发，经 LayoutPlan 和拥有稳定 ID 的编译器生成 SurfaceSpec 的 HTTP 编排链路。

### Modified Capabilities

无。

## Impact

- 新增 Qt 5.12.8/C++14 Widgets 前端，按宿主应用、编排运行时、可复用组件和 C++ Service 分层组织，并在 GCC 7.3.0 与 9.3.0 两端验证构建。
- 新增后端 Agent Service 与 SQLite-backed Mock Business API 模块；Demo 中二者可以共用一个 FastAPI 进程，但 Router、客户端、数据访问和依赖方向必须分离。
- 新增供前后端共同使用的 Catalog 和 JSON Schema 合同。
- Qt 客户端使用 `QJsonDocument` 等 Qt 5.12 API 实现与共享 Schema 等价的封闭校验，不引入 QtSql 或依赖较新 C++ ABI 的预编译插件。
- 新增覆盖完整受控编排、业务 CRUD/轮询和 SQLite 持久化闭环的 Fixture、单元测试、集成测试和验收测试。
- 引入项目自有协议，不承诺正式 A2UI、MCP、AG-UI、Streaming 或 WebSocket 兼容性。
