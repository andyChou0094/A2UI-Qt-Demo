# 生成式 UI 框架的布局模型比较与 Qt Demo 建议

调研日期：2026-08-14

## 结论先行

对当前 Qt Demo，最合适的第一阶段布局边界不是任意坐标、任意尺寸或任意 QSS，而是：

- 仅开放 `Row`、`Column` 两种编排容器；如确有滚动需求，再加入语义明确的 `List/Scroll`。
- `Row/Column` 只接收 `children`、`justify`、`align`；直接子节点可接收相对 `weight`。
- 可增加一个受控的 `gap` 间距档位，例如 `none | small | medium | large`，由 Qt 主题映射为固定 spacing；这属于项目自定义扩展，并非 A2UI Basic Catalog 的现成字段。
- 不向模型开放 `width/height`、`minWidth/minHeight`、绝对坐标、任意 margin/padding、QSS、颜色或字体。组件自身的 `sizeHint`、`minimumSizeHint`、`QSizePolicy`、内部布局和样式继续生效。
- “右边放一个大便签”优先用兄弟节点的 `weight: 1` 与 `weight: 2` 表达，不需要先引入像素尺寸。
- 窄窗口适配第一阶段由 Qt 布局和组件固有尺寸策略负责；不要为 Demo 发明 breakpoint DSL。后续真实组件证明有需求后，再新增白名单化的响应式容器或语义尺寸档位。
- 稳定 `id` 用于寻址和差异更新；项目还必须额外规定：`id` 与组件类型都不变时复用同一个 QWidget 实例。这是 Qt Demo 的生命周期保证，不是 A2UI 协议自动保证的行为。

这一方向与 A2UI Basic Catalog 的 `Row/Column + justify/align/weight` 最接近，也与 json-render 的“小型布局组件 + 枚举属性 + Catalog 校验”一致。

## 比较总览

| 方案 | 它是否定义内部布局 | 布局与尺寸模型 | 增量更新/身份 | 白名单与样式边界 | 对 Qt Demo 的意义 |
| --- | --- | --- | --- | --- | --- |
| Google A2UI | 核心协议不固定布局；所选 Catalog 定义布局组件 | 官方 Basic Catalog 提供 `Row`、`Column`、`List`、`Card` 等；`Row/Column` 有 `justify`、`align`，直接子节点有相对 `weight`；无通用像素尺寸或 breakpoint | 扁平组件表、稳定组件 `id`、`updateComponents` 添加或更新；`updateDataModel` 单独更新数据 | Catalog 是组件、函数和属性的允许列表；Basic Catalog 倾向语义 variant，不开放任意样式 | 最值得直接借鉴的协议骨架；Qt 的 QWidget 实例复用需要自行加强 |
| MCP Apps / OpenAI Apps SDK | **不定义 iframe 内部布局** | 服务端返回完整 HTML/JS/CSS View；宿主只协商 iframe 显示模式和容器尺寸 | 每次渲染的是 UI resource/View 实例；UI 内状态由 View 自己管理，工具结果通过消息更新 | iframe sandbox、CSP 和域名 allowlist；但 View 内可使用完整 Web 布局能力 | 适合“把完整微前端嵌入聊天”，不适合作为 QWidget 组件树布局协议 |
| AG-UI | **不定义 UI 组件或布局** | 事件与状态传输协议；布局由前端应用或叠加的 A2UI/json-render 等协议决定 | 消息有 `messageId`；状态支持 snapshot + RFC 6902 delta | `Custom/Raw` 可承载扩展，但语义和安全由应用负责 | 可作为流式传输层，不能回答 Row/Column、尺寸或样式问题 |
| Vercel AI SDK Generative UI | **不定义通用布局协议** | 模型调用预定义 tool，应用把 tool 结果映射到开发者编写的 React 组件；布局是 React 代码的职责 | 聊天消息和 tool parts 有 ID/状态；没有跨平台组件树 diff 合约 | 模型只调用已提供的 tools，组件代码由开发者控制 | 可借鉴“模型只选择预定义能力”；不能直接借鉴布局 schema |
| Vercel Labs json-render | 定义可配置的受控组件树；具体布局由 Catalog 决定 | 扁平 `{root,elements}`；官方 shadcn Catalog 有 `Stack(direction/gap/align/justify)`、`Grid(columns 1-6/gap)`；也允许自定义 schema | 元素键即稳定 ID；支持流式 spec，并提供 patch/merge/diff 编辑模式 | 模型只能使用 Catalog 中的组件和 schema 属性；是否开放 raw style 取决于开发者是否把它写进 Catalog | 与 Qt 自有协议非常接近；适合借鉴受控 gap/枚举和原子校验 |

## 1. Google A2UI

### 1.1 核心协议与 Catalog 必须分开理解

A2UI 核心协议定义 Surface、组件 `id`、扁平邻接表、`createSurface`、`updateComponents`、`updateDataModel` 和 `deleteSurface`。具体有哪些组件和属性不属于核心协议，而由当前 `catalogId` 对应的 Catalog 决定。官方明确说明，自定义 Catalog 可以限制 Agent 只能使用应用真实存在的组件和视觉语言。[A2UI v0.9 协议](https://a2ui.org/specification/v0.9-a2ui/)，[定义自有 Catalog](https://a2ui.org/guides/defining-your-own-catalog/)

因此，“A2UI 使用 Row/Column”更准确的说法是：“A2UI 官方 Basic Catalog 选择了 Row/Column”；自定义 Catalog 仍可定义 `Splitter`、`DockArea` 或业务容器。

### 1.2 Basic Catalog 的布局能力

官方 Basic Catalog 的布局组件包括：

- `Row`：水平排列直接子项。
- `Column`：垂直排列直接子项。
- `List`：水平或垂直滚动列表，支持静态 children 或数据模板。
- `Card`：单 child 容器；多个内容需要先包在 Row/Column 中。
- `Tabs`、`Modal`：通过组件 ID 引用内容。

`Row/Column` 支持主轴 `justify` 与交叉轴 `align`；直接子节点可带 `weight`，语义类似 flex-grow，用相对比例分配剩余空间。官方组件参考展示了这些属性，官方 Renderer 清单也要求实现 child `weight`。[组件参考](https://a2ui.org/reference/components/)，[Renderer Development](https://a2ui.org/guides/renderer-development/)，[v0.9.1 Basic Catalog Schema](https://github.com/a2ui-project/a2ui/blob/main/specification/v0_9_1/catalogs/basic/catalog.json)

Basic Catalog 没有通用的 `width`、`height`、min/max 尺寸、绝对坐标、任意 margin/padding、CSS Grid 列定义或 breakpoint/media query 语言。布局适应主要依赖 Row/Column、`weight` 以及各平台 Renderer 的原生布局行为。v1.0 Candidate 的 Basic Catalog 仍延续这一模型，而没有引入通用 breakpoint DSL。[v1.0 Candidate Basic Catalog Schema](https://github.com/a2ui-project/a2ui/blob/main/specification/v1_0/catalogs/basic/catalog.json)

### 1.3 样式、安全和响应式

A2UI 的安全边界是 Catalog：客户端只注册受信任组件，消息按对应 JSON Schema 校验。Basic Catalog 更倾向 `variant: "h1"`、`variant: "primary"` 这类语义提示，实际字体、颜色、间距和控件实现由客户端主题掌控。自定义 Catalog 技术上可以开放更多样式字段，所以“禁止任意样式”应由 Qt Catalog schema 明确落实，而不能只依赖 A2UI 名称。[Theming & Styling](https://a2ui.org/guides/theming/)，[A2UI 官方仓库的 Security-first 原则](https://github.com/a2ui-project/a2ui)

响应式方面应区分两种含义：A2UI 支持流式、渐进式渲染，用户感知上“响应快”；但 Basic Catalog 没有按窗口宽度切换布局的 breakpoint 表达。若以后需要“小屏时 Row 自动转 Column”，应把它做成受控的自定义容器或 Renderer 策略，而不是允许模型输出平台 CSS/QSS。

### 1.4 增量更新和稳定 ID

A2UI 将组件存入按 `id` 索引的表中。`updateComponents` 可添加新组件或更新已有 ID；父组件通过 children ID 顺序组织树；`updateDataModel` 可以只更新内容而不重发结构。这个模型天然适合流式补全和局部更新。[A2UI v0.9：updateComponents 与邻接表](https://a2ui.org/specification/v0.9-a2ui/)，[Components & Structure](https://a2ui.org/concepts/components/)

但协议只约定组件定义按 ID 更新，没有承诺原生 Renderer 必须保留同一个对象实例。对当前目标，“重排但保留待办内容、焦点、未提交编辑和网络状态”必须成为 Qt Renderer 的额外规则：

1. 同 `id` 且同组件类型：复用 QWidget，仅从旧布局移出并插入新位置。
2. 同 `id` 但类型变化：明确销毁旧实例并新建。
3. 新 `id`：创建。
4. 从新树中移除：在整次更新提交后销毁或按策略缓存。
5. 先完整校验新规格，再一次性提交；失败时保留旧树。

## 2. MCP Apps 与 OpenAI Apps SDK

MCP Apps 把 UI 定义为 `ui://` HTML resource，由宿主放进 sandboxed iframe。Host 与 View 通过 JSON-RPC over `postMessage` 通信，并可向 View 提供 `inline/fullscreen/pip` 显示模式和固定或最大容器宽高。规范同时要求 CSP、外部域 allowlist 和权限声明。[MCP Apps Overview](https://github.com/modelcontextprotocol/ext-apps/blob/main/docs/overview.md)，[MCP Apps Specification](https://github.com/modelcontextprotocol/ext-apps/blob/main/specification/2026-01-26/apps.mdx)

OpenAI 的当前官方文档同样建议新 UI 采用 MCP Apps：工具用 `_meta.ui.resourceUri` 关联 UI resource，View 在 iframe 中运行；ChatGPT 特有能力才使用额外扩展。文档也要求声明 `connectDomains`、`resourceDomains`、`frameDomains`，并把 tool result 当作不可信输入处理。[OpenAI：Add UI to your MCP server](https://developers.openai.com/plugins/build/chatgpt-ui)

这一路线没有规定 iframe 内用 Flex、Grid 还是绝对定位，应用可以自行包含任意 HTML/CSS/JS。它的安全来自“完整 View 的隔离和 CSP”，不是“Agent 只能生成有限布局节点”。所以它对 Qt Demo 的启示主要是职责隔离、资源生命周期和后端能力 allowlist，而不是布局字段。

## 3. AG-UI

AG-UI 是 Agent 与前端之间的事件协议，主要定义运行生命周期、文本、工具调用、状态和自定义事件。状态同步采用 snapshot 与 RFC 6902 JSON Patch delta；`Raw` 和 `Custom` 可以承载外部或应用自定义结构。[AG-UI 官方仓库](https://github.com/ag-ui-protocol/ag-ui)，[AG-UI Events](https://github.com/ag-ui-protocol/ag-ui/blob/main/docs/concepts/events.mdx)

它没有组件 Catalog、Row/Column、尺寸、样式或 Renderer 合约。AG-UI 官方生态把 A2UI 作为可叠加的声明式 UI 层，也说明两者职责不同：AG-UI 可运送或触发 UI 更新，A2UI 才定义组件树。[AG-UI 仓库中的 A2UI integration 指引](https://github.com/ag-ui-protocol/ag-ui/blob/main/CLAUDE.md)

因此，若 Demo 后续需要 SSE/WebSocket 事件、工具进度或状态 delta，可以借鉴 AG-UI；当前布局决定不能引用 AG-UI 作为依据。

## 4. Vercel AI SDK Generative UI

Vercel AI SDK UI 的官方模式是：开发者向模型提供预定义 tools；模型选择 tool 并生成经 schema 校验的参数；tool 结果再由应用代码映射到具体 React 组件。官方示例按 `tool-${toolName}` 的 message part 与执行状态渲染 Weather/Stock 等组件。[AI SDK UI：Generative User Interfaces](https://ai-sdk.dev/docs/ai-sdk-ui/generative-user-interfaces)

这是一种“模型选择预定义能力”的安全模式，但不是跨平台布局协议：组件内部及多个组件之间如何布局，仍由 React 代码决定。旧的 AI SDK RSC 能流式返回 React 组件，但目前是 experimental，官方建议生产使用 AI SDK UI；其文档也记录了 `.done()` 时组件 remount 等限制。[AI SDK RSC Overview](https://ai-sdk.dev/docs/ai-sdk-rsc/overview)，[Migrating from RSC to UI](https://ai-sdk.dev/docs/ai-sdk-rsc/migrating-to-ui)

对 Qt Demo 可借鉴的是：模型只决定“使用哪个预注册组件、传哪些白名单参数”，而业务请求和组件实现留在 C++。它不能替代 SurfaceSpec、稳定节点 ID 或 Qt 差异更新设计。

## 5. Vercel Labs json-render

json-render 用扁平的 `{ root, elements }` 规格表示树，每个 `elements` key 都是节点身份；Catalog 定义可用组件及 Zod/JSON Schema 属性，Registry 把这些类型映射为实际组件。官方明确以“只能使用 Catalog 中的组件”和 schema 匹配作为 guardrail。[json-render 官方仓库](https://github.com/vercel-labs/json-render)，[Specs](https://json-render.dev/docs/specs)

官方 shadcn Catalog 提供小而受控的布局词汇：

- `Stack`：`direction`、`gap`、`align`、`justify`。
- `Grid`：`columns` 限制为 1–6，并提供 `gap`。
- `Card`：可选标题、说明、最大宽度、居中。

这些是 Catalog 暴露的受控属性，不是允许模型传入任意 CSS。[shadcn API](https://json-render.dev/docs/api/shadcn)

框架同时支持流式规格，以及 patch（RFC 6902）、merge（RFC 7396）、diff 三类多轮编辑模式；这说明稳定元素 ID 和小范围结构修改是成熟实现的共同方向。[json-render Core Changelog](https://github.com/vercel-labs/json-render/blob/main/packages/core/CHANGELOG.md)，[Core API](https://json-render.dev/docs/api/core)

json-render 也强调 schema-agnostic：开发者可以定义任意结构。如果自定义 Catalog 暴露 `style: string`，安全边界同样会被开发者自己扩大。对 Qt 最合适的借鉴是 Catalog/Registry、扁平 ID 树、枚举 gap、严格 schema 和可审计的更新操作，而不是照搬 Web CSS 能力。

## 6. 建议写入 Qt Demo 方案的布局契约

### 6.1 MVP schema

推荐布局节点：

```json
{
  "id": "main",
  "component": "Row",
  "children": ["todo", "notes"],
  "justify": "start",
  "align": "stretch",
  "gap": "medium"
}
```

推荐业务组件节点：

```json
{
  "id": "todo",
  "component": "TodoListWidget",
  "weight": 1
}
```

```json
{
  "id": "notes",
  "component": "NotesWidget",
  "weight": 2
}
```

字段约束建议：

- `component`: 只能来自 Catalog。
- `children`: 有序、唯一、同 Surface 内存在的 ID；禁止环和多父节点。
- `justify`: `start | center | end | spaceBetween | spaceAround | spaceEvenly`。
- `align`: `start | center | end | stretch`。
- `weight`: 仅在 Row/Column 直接子节点上有效；建议限制为整数 `0..10`，默认 `0` 或 `1` 需在协议中选定并固定。
- `gap`: `none | small | medium | large`，Renderer 映射到主题 spacing；不接受任意数字。
- 树深、节点数、重复 ID 和无效引用均设硬上限并在渲染前校验。

### 6.2 Qt 映射

- `Row` → `QHBoxLayout`。
- `Column` → `QVBoxLayout`。
- `justify` → 前后/中间 spacer 或布局 stretch；需要为每个枚举写确定性映射。
- `align` → `Qt::Alignment` 和/或子控件 `QSizePolicy` 的受控组合。
- `weight` → `QBoxLayout::setStretch(index, weight)`。
- `gap` → `QLayout::setSpacing(themeTokenValue)`。
- 业务 QWidget 的内部 layout、QSS、signals/slots、Service 和网络请求代码保持不变。

### 6.3 暂不实现

- Grid、Splitter、Dock、Tabs 的动态组合。
- 任意宽高、min/max 尺寸、绝对位置。
- 任意 margin/padding/QSS/颜色/字体。
- breakpoint、条件布局、自动 Row↔Column 切换。
- 模型生成 URL、HTTP 方法、请求体模板或信号槽连接。

### 6.4 升级触发条件

只有下列验收用例不能由 `Row/Column + weight + 组件自身 QSizePolicy` 满足时，才扩展布局协议：

1. 真实业务组件有已知不可压缩最小尺寸，宿主无法从其 `minimumSizeHint` 正确处理。
2. 用户必须拖拽调整面板比例，此时新增受控 `Splitter` 比开放 width 更合适。
3. 窄窗口必须结构性换行，此时新增专用 `ResponsiveRow` 或 Renderer 断点策略。
4. 仪表盘确实需要二维布局，此时新增列数有限的 `Grid`，而不是绝对坐标。

## 7. 对当前问题的建议答案

建议接受“Row/Column + 白名单布局提示”，但把提示调整为：

```text
Row / Column
+ justify
+ align
+ child weight
+ gap token（项目自定义，可选）
```

第一阶段不加入 `minWidth`、`minHeight`。A2UI Basic Catalog 本身也没有这类通用字段；“大一点”优先由相对 `weight` 表达，控件真实的最低可用尺寸继续由 QWidget 自己声明。这样既足以验证自然语言编排，也最能保护原组件排版和样式不被 Renderer 接管。
