# AI 动态编排 Qt 组件 Demo 设计方案 v2

## 1. 验证目标

验证以下闭环是否可行：

```text
自然语言
→ 受控布局计划
→ 确定性 SurfaceSpec
→ 动态编排预先注册、可嵌入的 C++ QWidget
→ QWidget 保持原有界面、状态、逻辑和预定义业务请求
```

Demo 使用玩具组件，不接入公司真实项目。因此成功结论仅限于“受控编排机制可行”，不代表真实项目已经完成工具链、依赖、生命周期、网络安全和集成兼容验证，也不代表 SurfaceSpec 能表达任意前端界面。

## 2. 核心边界与评估结论

- 前端使用 C++ 与 Qt Widgets，不使用 PySide6 重写组件。
- 注册的、可嵌入 QWidget 是最小业务叶子单元；`Row`、`Column` 是 Renderer 管理的布局节点，不属于业务组件。
- Renderer 将业务 QWidget 视为不透明整体，不读取或修改其内部控件树。
- AI 只能选择组件、创建实例和描述受控布局，不能生成 URL、请求参数、Action、数据绑定、信号槽、脚本或 QSS。
- 业务请求由 QWidget 依赖的预定义 C++ Service 发起；组件间不得直接连接信号或共享事件总线实现联动。
- 固定 `HostShell` 提供自然语言输入、可滚动动态区域和错误状态；AI 只控制一个 `main` Surface。
- 使用项目自有的 `Qt UI Composition Protocol v0`，仅借鉴 A2UI 思想，不承诺 A2UI 兼容。
- HTTP 一次返回完整目标规格；暂不实现 Streaming、WebSocket、MCP 或 AG-UI。

递归 `Row/Column` 可以表达侧栏、上下分区、嵌套面板、2×2 区域和常见 Dashboard 等正交切分布局，但不能表达跨行列对齐、Grid 跨列、重叠、换行、Dock、拖拽 Splitter 或响应式断点。v0 的目标是覆盖 Demo 所需的常见嵌套布局，而不是成为通用前端布局语言。该结论与[生成式 UI 布局模型调研](docs/research/generative-ui-layout-comparison.md)一致。

## 3. 技术栈与兼容约束

前端固定使用以下基线：

| 项目 | 版本或范围 | 约束 |
| --- | --- | --- |
| Qt | `5.12.8` | 仅使用该版本存在的 Qt Core、Widgets、Network、Test API |
| GCC | `7.3.0` 至 `9.3.0` | 支持范围两端均需完成构建验证 |
| C++ | ISO C++14 | 构建配置显式指定 `-std=c++14` |
| Qt 模块 | Core、Widgets、Network、Test | SQLite 仅由 Python 后端访问，不引入 QtSql |

GCC 官方资料确认 C++14 在该编译器范围内完整支持；选择 C++14 可以避免 GCC 7.3 上部分 C++17 标准库实现和 ABI 差异。[GCC C++ 支持说明](https://gcc.gnu.org/projects/cxx-status.html#cxx14)

Qt 5.12.8 是已归档的历史 LTS 补丁版本。本 Demo 的 HTTP 通信仅允许连接配置的本地回环地址，不能据此作生产网络安全结论。[Qt 5.12.8 发布说明](https://www.qt.io/blog/qt-5-12-8-released)、[Qt 历史版本归档](https://download.qt.io/archive/qt/5.12/5.12.8/)

版本约束对实现产生以下影响：

- 禁止使用 Qt 5.15、Qt 6 或 C++17 才提供的 API，不引入依赖更新 libstdc++ ABI 的预编译插件。
- 所有 C++ 依赖必须与实际 Qt 包、目标 GCC 和目标 libstdc++ ABI 匹配；“支持 GCC 7.3.0 至 9.3.0”不等于两个环境间可直接混用二进制产物。
- Qt 5.12 没有内建 JSON Schema Validator。共享 JSON Schema 是协议权威；Qt 客户端使用 `QJsonDocument`、`QJsonObject`、`QJsonArray` 实现等价的封闭协议校验，并用合同测试保证前后端规则一致。
- HTTP 使用 `QNetworkAccessManager` 异步调用，并用 `QTimer` 实现请求超时；不得阻塞 GUI 线程。

## 4. 技术架构

```text
┌────────────── C++ Qt HostShell ──────────────┐
│ PromptBar → Composition Client               │
│                    │                         │
│                    │ SurfaceSpec             │
│                    ▼                         │
│ Validator → Reconciler → Qt Layout Adapter   │
│                    │                         │
│            QScrollArea: main                 │
│                    │                         │
│ Registered QWidget → CalculationService ─────┼──→ Mock Business API → SQLite
│ StatusPanel                                  │
└────────────────────┬─────────────────────────┘
                     │ POST /compose
                     ▼
┌────────────── Agent Service ─────────────────┐
│ Catalog → LLM LayoutPlan                     │
│              → Surface Compiler              │
│              → Server Validator              │
└──────────────────────────────────────────────┘
```

Agent 编排链路与业务请求链路相互独立：

```text
编排：HostShell → /compose → SurfaceSpec
业务：Calculator / History / Stats → CalculationService → /api/calculations* → SQLite
```

Agent Service 与 Mock Business API 可以在 Demo 中共用一个 FastAPI 进程，但模块、Router、客户端和依赖方向必须分离。`/compose` 不得导入或调用计算记录的数据访问层。

## 5. Demo 组件与业务联动

组件均先在固定页面 `LegacyToolboxWindow` 中正常运行，再将同一批 QWidget 类注册到动态 Renderer，禁止为动态版复制实现。

| 类型 | 行为 | 多实例 |
| --- | --- | --- |
| `Calculator` | 按钮输入并在本地完成四则计算；成功后通过 `CalculationService` 创建记录；除零和非法输入不入库 | 是 |
| `CalculationHistory` | 启动时、每 2 秒及手动刷新时查询最近 50 条记录；支持修改备注和删除记录 | 是 |
| `CalculationStats` | 启动时及每 2 秒查询记录总数与最新结果 | 是 |
| `Clock` | 显示当前时间并持续计时，用于验证重排后的运行状态 | 是 |
| `NotePad` | 保留未提交文本，用于验证对象身份、输入和焦点保持 | 是 |
| `Row` | Renderer 拥有的水平布局节点 | 不适用 |
| `Column` | Renderer 拥有的垂直布局节点 | 不适用 |

`Calculator` 只能使用预定义按钮和运算逻辑，不执行任意表达式脚本。一次成功计算先在本地得到结果，再调用业务 API；写入失败时保留本地结果并显示错误状态，但不得伪装成已持久化。

三个计算组件通过后端持久化状态形成可见联动：

```text
Calculator ──POST──▶ SQLite
                         ▲
History ─────GET/PATCH/DELETE
Stats ───────GET summary
```

它们之间不得直接连接信号、读取彼此对象或使用共享事件总线。History 和 Stats 最迟应在两个轮询周期内反映 Calculator 成功写入的记录。

### 5.1 业务 API

| 方法与路径 | 用途 |
| --- | --- |
| `POST /api/calculations` | 创建计算记录 |
| `GET /api/calculations?limit=50` | 按创建时间倒序查询最近记录，`limit` 上限为 50 |
| `PATCH /api/calculations/{id}` | 仅修改记录 `note` |
| `DELETE /api/calculations/{id}` | 删除指定记录 |
| `GET /api/calculations/summary` | 查询记录总数和最新一条记录 |

`CalculationRecord` 固定包含：

```json
{
  "id": 1,
  "expression": "12 + 30",
  "result": "42",
  "note": "",
  "createdAt": "2026-08-17T10:00:00Z",
  "updatedAt": "2026-08-17T10:00:00Z"
}
```

SQLite 由 Mock Business API 独占访问并使用文件持久化；自动化测试使用临时数据库。API 地址、HTTP method、超时和请求响应结构来自应用配置及 C++ `CalculationService`，不进入 Catalog、提示词、LayoutPlan 或 SurfaceSpec。

## 6. Component Catalog 与 QWidget 编排合同

Catalog 是 Agent 可编排业务能力的唯一清单，由项目维护并同时供 Agent 与 C++ Registry 校验。每个业务组件声明：

```text
type、version、description、multiple、允许的展示参数、布局提示说明
```

Registry 的业务组件工厂接口固定为：

```cpp
using WidgetFactory = std::function<QWidget *(QWidget *parent)>;
```

Service 等依赖由项目代码通过工厂闭包注入，不由 SurfaceSpec 提供。可注册 QWidget 必须满足：

- 在 GUI 线程构造，是可嵌入的非顶层 QWidget，不依赖 `Qt::Window` 标志。
- 支持正常的 resize、reparent 和 QObject 父子所有权。
- 自己管理内部布局、控件、信号槽和业务状态，并提供有效的 `minimumSizeHint`、`sizeHint` 与 `QSizePolicy`。
- Renderer 无需读取子控件或建立业务连接即可使用。

`QMainWindow`、`QDockWidget` 顶层用法、模态 `QDialog` 工作流、菜单栏/状态栏宿主能力、QML、Graphics View、非视觉服务和任意跨组件行为不属于 v0 可注册叶子。遗留组件若不符合合同，必须由项目维护固定 Adapter QWidget；Adapter 对 Agent 仍是不透明叶子。

第一阶段不做用户权限过滤；`Effective Catalog` 与渲染时鉴权作为未来扩展。

## 7. 生成与编译

LLM 不直接拥有最终组件 ID：

```text
用户指令 + 当前 Surface + Catalog
→ LLM LayoutPlan
→ Surface Compiler 分配或复用稳定 ID
→ Schema 与语义校验
→ 完整目标 SurfaceSpec
```

LayoutPlan 只能引用已有实例或声明新实例。Surface Compiler 负责处理重复组件、稳定身份和最终邻接表，避免模型改写 ID 导致 QWidget 被误销毁。

若用户请求 Grid、跨行列、重叠、拖拽分栏、Dock、wrap 或响应式断点等 v0 无法表达的布局，Agent Service 必须返回结构化诊断 `unsupported_layout`，不得静默近似或修改当前 Surface。

## 8. SurfaceSpec v0

协议版本保持 `0.1`。由于尚未进入实现，不需要迁移旧运行时数据。

```json
{
  "protocolVersion": "0.1",
  "surfaceId": "main",
  "root": "root",
  "components": [
    {
      "id": "root",
      "component": "Row",
      "children": ["left", "right"],
      "justify": "start",
      "align": "stretch",
      "gap": "medium"
    },
    {
      "id": "left",
      "component": "Column",
      "children": ["clock", "calculator"],
      "gap": "small",
      "weight": 1
    },
    {
      "id": "right",
      "component": "Column",
      "children": ["history", "stats", "note"],
      "gap": "medium",
      "weight": 2
    },
    { "id": "clock", "component": "Clock" },
    { "id": "calculator", "component": "Calculator" },
    { "id": "history", "component": "CalculationHistory" },
    { "id": "stats", "component": "CalculationStats" },
    { "id": "note", "component": "NotePad" }
  ]
}
```

### 8.1 布局字段

| 字段 | 约束 | Qt 映射 |
| --- | --- | --- |
| `justify` | `start/center/end/spaceBetween/spaceAround/spaceEvenly`，默认 `start` | 主轴 stretch spacer 的确定性组合 |
| `align` | `start/center/end/stretch`，默认 `stretch` | `Qt::Alignment`；`stretch` 不覆盖叶子原有 `QSizePolicy` |
| `weight` | 非 root 节点可用，作为父布局直接子项的属性，整数 `0..10`，默认 `0` | `QBoxLayout` stretch factor |
| `gap` | `none/small/medium/large`，默认 `medium` | 分别映射为 `0/4/8/16` 个逻辑像素 |

Renderer 管理的 Row/Column 容器 margin 固定为 0；`HostShell` 负责动态区外层留白。协议不开放像素宽高、min/max 尺寸、绝对坐标、任意 margin/padding、颜色、字体或 QSS。

### 8.2 `weight` 与 `justify`

- 当所有直接子项的 `weight` 都为 0 时，`justify` 通过 spacer 分配主轴剩余空间。
- 当任一直接子项 `weight > 0` 时，容器的 `justify` 必须为 `start`，且 Renderer 不添加主轴分布 spacer；正权重子项按比例获得剩余空间，零权重子项遵循自身 size hint/policy。
- 其他组合属于语义错误，必须在修改活动界面前拒绝。

无正权重时，`justify` 的确定性规则如下：

| 值 | spacer 规则 |
| --- | --- |
| `start` | 尾部一个 stretch spacer |
| `center` | 首尾各一个等权 stretch spacer |
| `end` | 首部一个 stretch spacer |
| `spaceBetween` | 仅相邻子项之间放等权 stretch spacer；单子项等同 `start` |
| `spaceAround` | 首尾 spacer 权重为 1，相邻子项间 spacer 权重为 2；单子项居中 |
| `spaceEvenly` | 首尾及相邻子项间均放等权 stretch spacer；单子项居中 |

空 Row/Column 合法且不添加 spacer。固定 `gap` 只作用于相邻业务或布局子项，不替代上述可伸缩 spacer。

### 8.3 `align` 与溢出

- `start/center/end` 按 Row 或 Column 的交叉轴映射到相应 `Qt::Alignment`。
- `stretch` 表示不增加交叉轴 alignment 限制，但不改写业务 QWidget 的 `QSizePolicy`；组件是否填满可用空间由自身策略决定。
- 组件自身的 `minimumSizeHint`、`sizeHint`、最小/最大尺寸和 `QSizePolicy` 始终有效。
- `HostShell` 使用固定 `QScrollArea` 承载 `main` Surface，并启用可调整内容；当组件最小尺寸超过视口时出现滚动条，而不是裁剪或要求 Agent 生成响应式布局。

## 9. 校验与提交

Renderer 不信任模型或服务端输出。提交前必须校验：

- 协议版本、唯一 `surfaceId` 与唯一 root。
- 类型必须存在于 Catalog，字段必须符合共享 Schema 和 Qt 侧等价封闭校验。
- ID 唯一，children 引用有效且有序。
- 无环、无多父节点、无不可达节点。
- 业务叶子不得包含 children 或容器布局字段；`weight` 只描述父布局中的直接子项。
- `weight` 与 `justify` 组合符合第 8.2 节规则。
- 默认上限为 32 个节点、8 层深度，可通过可信应用配置调整。

新规格先完整校验和预构建 Reconciliation Plan，再一次性提交。任何失败均释放暂存资源、保留旧界面，并在 `StatusPanel` 显示可诊断错误。

## 10. QWidget 生命周期

Reconciler 按稳定 ID 和业务组件类型处理完整目标规格：

- ID 与类型均不变：复用原业务 QWidget，只调整所在布局。
- 新 ID：从 Registry 创建实例。
- 同 ID 但类型变化：成功提交后销毁旧实例并创建新实例。
- 明确移除：更新成功后销毁并释放资源。
- 移除后再次添加：使用新 ID 创建新实例，不恢复旧状态。
- Renderer 拥有的 Row/Column 布局容器可以在提交时重建；稳定对象身份与状态保持保证只针对注册的业务叶子。

Renderer 永远不创建组件间业务连接。业务组件原有内部连接保持不变，Calculator、History 与 Stats 的联动只能通过各自的 `CalculationService` 请求和后端 SQLite 状态完成。

## 11. 代码结构

```text
a2ui-qt-demo/
├─ frontend/
│  ├─ app/                 # HostShell、QScrollArea、LegacyToolboxWindow
│  ├─ composition/         # Catalog、Compiler client、Validator、Reconciler
│  ├─ widgets/             # 5 个独立业务 QWidget
│  └─ services/            # CalculationService 等预定义业务请求
├─ backend/
│  ├─ agent_service/       # /compose、LLM、Surface Compiler
│  └─ mock_business_api/   # /api/calculations*、SQLite 数据访问
├─ shared/
│  ├─ catalog.json
│  └─ surface-spec.schema.json
└─ tests/
   └─ fixtures/
```

## 12. 实施顺序

1. 建立 Qt 5.12.8、C++14 构建，并分别验证 GCC 7.3.0 与 9.3.0；记录实际 Qt 包和 libstdc++ ABI。
2. 实现固定版五个 C++ QWidget、`LegacyToolboxWindow`、`CalculationService` 和文件型 SQLite Mock Business API。
3. 实现业务 API CRUD、轮询联动及临时测试数据库。
4. 实现 Catalog、SurfaceSpec Schema、LayoutPlan Fixture 和 SurfaceSpec Fixture。
5. 实现 Qt 侧等价封闭校验、Registry、Row/Column Layout Adapter、QScrollArea 和单次完整渲染。
6. 实现稳定业务叶子 ID、差异更新、状态保持、销毁和失败回滚。
7. 实现 FastAPI `/compose`、LLM LayoutPlan、确定性 Surface Compiler 和 `unsupported_layout` 诊断。
8. 运行完整验收；先验证 Fixture 链路，再验证自然语言链路。

## 13. 验收标准

- 固定版与动态版直接复用相同的五个业务 QWidget 类。
- 左右、上下、侧栏、2×2 嵌套、权重突出、空 Surface 和同类型多实例 Fixture 均产生预期结构。
- `gap`、`justify`、`align` 与 `weight` 严格遵循第 8 节映射；非法组合不改变当前界面。
- Grid、跨行列、重叠、拖拽分栏和响应式请求返回 `unsupported_layout`，不得静默近似。
- Calculator 成功写入后，History 与 Stats 在两个轮询周期内仅通过后端 API 反映变化。
- 业务 API 覆盖创建、查询、修改备注和删除；SQLite 重启后保留数据，测试使用临时数据库。
- SurfaceSpec、LayoutPlan 和 Catalog 中不存在 URL、请求体、Action、数据绑定或业务信号槽。
- 重排后未变业务组件的对象身份、Calculator 输入、Clock 计时、NotePad 文本、History 选择状态和支持情况下的焦点保持不变。
- 移除会销毁业务实例，重新添加会创建新实例。
- 非法 JSON、未知组件、循环引用、布局字段错误和超限结构不破坏当前界面。
- 前后端 Validator 对共享 Fixture 给出一致结论。
- Qt 5.12.8 + C++14 在 GCC 7.3.0 和 9.3.0 两端均构建通过。

建议自然语言用例：

```text
1. 左边放计算器，右边放计算历史和统计。
2. 上面放时钟，下面放计算器和历史。
3. 左边放时钟和计算器，右边放一个大便签。
4. 两个计算器左右并排，下面放历史和统计。
5. 把便签移到左边，其他内容不变。        # 验证业务实例复用
6. 删除便签，再添加一个新的便签。        # 验证销毁与新建
7. 把历史做成跨两列的网格。              # 返回 unsupported_layout
```

## 14. 暂不实现

- 真实公司项目组件接入及其兼容性结论。
- Grid、跨行列、重叠、wrap、Splitter、Dock、动态标签页和响应式断点。
- `QMainWindow`、模态窗口、QML、Graphics View 与非视觉对象编排。
- 多 Surface、多窗口和 Surface 间通信。
- 权限过滤与渲染时二次鉴权。
- AI 生成业务请求、Action、DataModel、信号槽、组件联动或任意样式。
- 正式 A2UI、MCP、AG-UI、Streaming 或 WebSocket 兼容。

只有真实业务组件提供可复现证据证明 `Row/Column + weight + QSizePolicy` 不足时，才按具体场景设计新的白名单容器；不得在 v0 中提前加入通用布局或样式能力。
