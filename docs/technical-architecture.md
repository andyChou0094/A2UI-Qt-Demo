# 技术架构

更新日期：2026-08-27

## 业务目标与边界

本单位已有大量经过业务验证的 Qt/QWidget。它们不仅包含界面外观，还封装控件树、QSS/主题、
signal/slot、局部状态、Service 调用和后端 API 合同。整体重写会引入业务重新认证、行为偏差和
长期迁移成本。本项目因此只动态改变“哪些已批准 QWidget 在何处出现”，不改写组件内部实现。

应用开发者通过 Registry 决定可编排组件，Agent 只能在封闭 Catalog 中选择组件并用 Row/Column
组合。LLM 不生成 C++、QML、QSS、脚本、signal/slot 或 HTTP 请求；任何失败都保留最后有效页面。
这验证的是受控编排机制，不是任意前端生成器，也不是 Google A2UI 协议实现。

当前只支持单个 `main` Surface、五类 Demo QWidget、Row/Column 及枚举化布局属性；不支持 Grid、
Dock、Splitter、重叠、wrap、响应式断点、任意样式、多 Surface、Action 或流式增量更新。

## 整体架构

```text
自然语言 + 当前 Surface
         │
         ▼
┌──────────────── Backend Agent Service ────────────────┐
│ Effective Catalog → LLM Adapter → LayoutPlan          │
│                  Parser/Validator → Surface Compiler  │
│                                   → Surface Validator │
└──────────────────────────┬────────────────────────────┘
                           │ 完整 SurfaceSpec
                           ▼
┌────────────────── C++ Qt Host ────────────────────────┐
│ CompositionClient → Qt Validator → Reconciler        │
│                                      │                │
│                               WidgetRegistry          │
│                                      │                │
│             Calculator / History / Stats / Clock /   │
│                                      NotePad          │
└───────────────────────────────────────────────────────┘
```

- 前端使用 Qt 5.12.8 Core/Widgets/Network 与 ISO C++14；Row/Column 分别映射
  `QHBoxLayout`/`QVBoxLayout`。
- 后端使用 FastAPI；Agent Service 与计算记录业务 API 保持模块隔离。
- Catalog、JSON Schema、布局语义、默认 Surface 与跨语言 Fixture 统一放在 `shared/`。
- SQLite 只由 `CalculationRepository` 访问，编排 DSL 不携带业务请求参数。

外部灵感只概括为五点：Catalog 限制生成能力、声明式 IR 代替可执行代码、native Renderer 映射
本地组件、稳定 ID 支持差异更新、完整校验后再提交。外部项目机制、版本资料及跨方案比较见
[生成式 UI 布局模型调研](research/generative-ui-layout-comparison.md)。本项目如何落地由
[`ADR 0001`](adr/0001-agent-composes-only-registered-components.md) 和
[`ADR 0002`](adr/0002-surface-compiler-owns-stable-identity.md)约束。

## 关键组件及职责

| 组件 | 负责 | 明确不负责 | 必要性 |
|---|---|---|---|
| Qt Host / `HostShell` | 收集 Prompt、保存最后成功的 Surface、承载控制区和动态舞台 | 不解析 Provider 计划，不直接访问 SQLite | 给所有入口提供同一活动页面与失败回退边界 |
| 业务 QWidget / `WidgetRegistry` | Registry 将允许的 type 映射到 GUI 线程中的可信工厂；业务 QWidget 保有内部状态和请求 | Renderer 不读取内部控件树，模型不能注册新工厂 | 把成熟业务资产作为不透明叶复用，防止模型扩张执行能力 |
| Backend Agent Service | 接收 `/compose`，组织生成、校验、编译和错误收敛 | 不执行 QWidget 业务，不访问计算 Repository | 把概率性 Provider 隔离在确定性合同之前 |
| LLM Adapter | 将 Prompt、当前 Surface 与 Effective Catalog 转成受限生成上下文，取得 LayoutPlan | 不分配最终 ID，不把 Provider 输出视为可信结果 | 让自然语言只影响“意图计划”，不直接影响对象生命周期 |
| LayoutPlan Parser/Validator | 解析封闭节点形态，检查 Catalog、规模、布局语义和 existing 引用 | 不修补越界计划，不创建 Surface 节点 | 在编译前拒绝无法表达或不安全的意图 |
| Surface Compiler | 复用现有业务 ID，为新叶和容器确定性分配 ID，输出完整 SurfaceSpec | 不调用 LLM，不决定组件内部行为 | 将概率性计划变为可重放、可审计的渲染目标 |
| SurfaceSpec Validator | 在 Python 与 Qt 两侧独立校验字段、Catalog 与引用图 | 不根据“看起来合理”放宽合同 | 同时保护服务输出、本地导入和 Qt 对象边界 |
| Reconciler / Renderer | 按 `(id,type)` 分类复用、创建、替换、移除；暂存后原子提交 | 不在失败时留下半棵树，不把容器身份当业务状态 | 保持未变 QWidget 的状态，并保护最后有效页面 |

## 受控 DSL 与信任边界

### Catalog 与 Effective Catalog

Catalog 描述应用可能开放的组件能力，例如组件 type、是否允许多实例以及工厂映射。**Effective
Catalog** 是服务启动时实际加载并经过配置收敛后，本次运行真正允许生成使用的权威白名单。
它不是给模型的建议，Provider 也不能在输出中补充或改写它。

如果只在 Prompt 中列出组件而没有确定性校验，模型仍可能虚构 `Button`、Grid 或脚本能力。
因此 LayoutPlan Validator、SurfaceSpec Validator 和 Qt Registry 都必须以同一受控 Catalog 为界：
前两者拒绝越界描述，Registry 则保证即使描述抵达客户端也只能实例化已注册工厂。

下面是只保留一个组件的 Catalog 形态示例：

```json
{
  "catalogVersion": "0.1",
  "components": [
    {
      "type": "Calculator",
      "version": "1.0",
      "description": "使用预定义按钮完成本地四则运算并持久化成功结果。",
      "multiple": true,
      "allowedDisplayFields": [],
      "layoutHints": {"roles": ["input", "primary"], "preferredAxis": "either"}
    }
  ]
}
```

| 字段 | 当前含义 |
|---|---|
| `catalogVersion` | Catalog 合同版本，当前固定为 `0.1` |
| `components` | 本次运行允许使用的业务组件定义列表 |
| `type` | DSL 与 WidgetRegistry 共同识别的组件类型名，例如 `Calculator` |
| `version` / `description` | 组件版本和给生成上下文使用的人类可读说明，不决定 QWidget 身份 |
| `multiple` | 是否允许同一 `type` 在一个 Surface 中出现多个实例；Validator 会强制检查 |
| `allowedDisplayFields` | 可由 DSL 提供的受控展示字段；当前全部为空，因此模型不能注入业务 props |
| `layoutHints` | 给 LLM 的 `roles` 与 `preferredAxis` 生成提示；当前 Renderer 不把它当成强制布局规则 |

权威文件：[完整 Catalog](../shared/catalog/component-catalog.json)。服务端把它作为
`effectiveCatalog` 发送给 LLM Adapter；确定性校验当前实际使用 `type` 与 `multiple`，Qt 端最终还要求
对应 `type` 已在 WidgetRegistry 注册。

### LayoutPlan：受限意图计划

LayoutPlan 是 LLM Adapter 唯一要求模型产生的 DSL，版本为 `0.1`。节点只有三种：

- `existing`：引用当前 Surface 中已有的业务叶，并同时声明匹配的 `id` 与 `type`；
- `new`：请求一个 Catalog 允许的新业务组件，只声明 `type`，不能指定最终 ID；
- `layout`：用 Row/Column 和封闭的 `gap`、`align`、`justify`、`weight` 组合子节点。

LayoutPlan 表达“保留什么、新增什么、怎样排列”，但不是可直接渲染的对象图。它没有 `surfaceId`、
完整节点表或稳定身份分配权。保留这一中间层，可以明确区分 Provider 的意图理解错误与 Compiler、
Renderer 的确定性错误，也避免 LLM 用伪造 ID 控制 QWidget 复用或替换。

以下示例表示“横向保留当前 Calculator，并新增一个 NotePad”；新组件不带 `id`：

```json
{
  "version": "0.1",
  "root": {
    "kind": "layout",
    "type": "Row",
    "children": [
      {"kind": "existing", "id": "calculator-main", "type": "Calculator"},
      {"kind": "new", "type": "NotePad"}
    ]
  }
}
```

| 字段 | 适用节点 | 当前含义 |
|---|---|---|
| `version` / `root` | 顶层 | 固定合同版本和唯一的递归计划根；顶层只能有这两个字段 |
| `kind` | 所有计划节点 | 决定节点是 `layout`、`existing` 还是 `new`，并决定允许出现哪些其他字段 |
| `type` | 所有计划节点 | layout 仅允许 `Row/Column`；业务节点必须是 Effective Catalog 中的 type |
| `children` | `layout` | 按声明顺序保存子计划；子项仍可继续是 layout，因而可以递归嵌套 |
| `id` | `existing` | 必须匹配当前 Surface 中同 type 的业务 ID；`new` 不允许携带此字段 |
| `gap` / `align` / `justify` | `layout` | 可选的容器布局语义；缺省值和取值差异在“QWidget 几何映射”统一解释 |
| `weight` | 非根计划节点 | 可选的 0–10 整数，表示该节点作为父容器直接子项时的相对伸展系数 |

权威文件：[LayoutPlan JSON Schema](../shared/schema/layout-plan-v0.schema.json)、
[合法 Fixture](../shared/fixtures/layout-plan/valid/)和
[Parser/Compiler](../backend/agent_service/layout_plan.py)。

### SurfaceSpec：完整渲染目标

SurfaceSpec 是 Compiler 产生、Renderer 消费的提交合同。它包含固定版本、`surfaceId: "main"`、
`root` ID 和平铺 `nodes`；layout 节点通过有序 children ID 引用其他节点，所有节点都有稳定且唯一的
ID。它描述完整目标而非 patch，也不保存 QWidget 内部状态、业务数据、Provider 配置或主题。

完整目标比增量 patch 传输更多数据，却让每次更新都能在独立、有限的引用图上验证，并让失败回退
只需保留旧 Surface。LayoutPlan 与 SurfaceSpec 两层 DSL 将概率性的意图表达和确定性的渲染合同分开；
模型选择允许的结构，Compiler 掌握身份，Renderer 只提交已验证目标。

上面的 LayoutPlan 经 Compiler 后会得到下面这种平铺引用图；`layout-1` 和 `note-pad-1` 由确定性
代码分配，而不是来自 LLM：

```json
{
  "version": "0.1",
  "surfaceId": "main",
  "root": "layout-1",
  "nodes": [
    {"id": "layout-1", "type": "Row", "children": ["calculator-main", "note-pad-1"]},
    {"id": "calculator-main", "type": "Calculator"},
    {"id": "note-pad-1", "type": "NotePad"}
  ]
}
```

| 字段 | 当前含义 |
|---|---|
| `version` | SurfaceSpec 合同版本，当前固定为 `0.1` |
| `surfaceId` | Surface 身份，当前只允许单一的 `main` |
| `root` | 布局树入口节点的 ID；它必须出现在 `nodes` 中且不得声明 `weight` |
| `nodes` | 最多 32 个平铺节点；顺序不是层级，层级由 layout 节点的 `children` 引用建立 |
| `id` | 全局唯一稳定 ID，以字母开头，最长 64 个字符；Renderer 用它参与 QWidget 复用判断 |
| `type` | `Row/Column` 或 Catalog 业务 type；它与 `id` 一起组成复用身份 `(id,type)` |
| `children` | 仅 layout 节点使用的有序子 ID 列表；业务节点没有 children |
| `gap` / `align` / `justify` | layout 节点的可选容器属性，控制直接子项之间的几何关系 |
| `weight` | layout 或业务节点都可使用的可选子项属性，但 root 禁止使用 |

权威文件：[SurfaceSpec JSON Schema](../shared/schema/surface-spec-v0.schema.json)、
[跨字段布局语义](../shared/protocol/layout-semantics.md)、
[默认 Surface](../shared/default-surface.json)和
[合法/非法 Fixture](../shared/fixtures/surface-spec/)。

## 核心算法及存在理由

### 1. 分层格式与能力校验

校验分层是因为每层面对的输入和信任边界不同，不能合并成一次“JSON 合法”检查：

1. LLM Adapter/Parser 先确认 Provider 响应可解析，LayoutPlan 版本和节点字段封闭。
2. LayoutPlan Validator 检查 Row/Column、枚举属性、0–10 的整数 weight、32 节点、8 层深度、
   Catalog 多实例限制，以及 `existing` 是否在当前 Surface 中存在且 type 一致。
3. 可识别的 Grid、Dock、Splitter、Wrap、Overlay、Responsive 等越界能力返回
   `unsupported_layout`；普通字段或引用错误返回 `invalid_layout_plan`，不会静默近似。
4. Compiler 产物由 Python SurfaceSpec Validator 复核，防止编译缺陷越过服务边界。
5. Qt Validator 对 HTTP 结果、本地导入和默认资源执行独立等价检查，保护最终对象边界。

Prompt 约束只降低错误概率，不能替代确定性校验；服务端通过也不能替代 Qt 对网络、版本漂移和本地
文件的防御。错误码按首次失败阶段收敛，使审计能区分传输、计划、编译、图或客户端问题。

### 2. 稳定 ID 编译

稳定 ID 是跨次编排识别同一业务 QWidget 的身份键。若 ID 由 LLM 随机生成，同一 Calculator 即使
只是从左移到右，也可能被误判为新对象，丢失输入、焦点和局部状态；恶意或错误 ID 还可能替换另一
组件。

Compiler 因此以前序深度优先顺序处理计划：`existing` 复制已验证的业务 ID；新业务叶按 type 的
kebab-case 前缀选择未占用的最小正整数后缀；容器选择未占用的最小 `layout-N`。相同计划、当前
Surface、Catalog 和限制会得到相同结果。ID 规则集中在确定性代码中，便于重放、测试和审计。

### 3. SurfaceSpec 图校验

平铺 `nodes` 本质上构成从 layout 指向 children 的有向引用图。Validator 先建立 ID 索引和父计数，
再从 root 深度优先遍历，验证：

- ID 唯一且引用存在；root 存在、无父且不带 weight；
- 非 root 节点恰有一个父，避免共享同一 QWidget 或出现 ownership 歧义；
- 图无环，全部节点从 root 可达，节点数和深度有限；
- Row/Column 的 children 顺序、gap/align/justify/weight 语义有效。

只验证 JSON Schema 无法证明无环、单父或可达。显式图校验确保 Renderer 接收的是一棵明确、有限、
可递归构建的布局树，而不是可能导致无限递归、孤儿对象或多重 parent 的任意图。

### 4. 事务式 Reconciliation

Validator 通过后，Renderer 以 `(id,type)` 比较活动叶和目标叶：两者都相同则复用，ID 相同而 type
变化则替换，新 ID 创建，目标中消失的 ID 移除。容器由 Renderer 拥有并可重建，不承载业务状态。

Renderer 先在未挂入 Host 的 `stagingOwner` 中构造新容器和新叶；复用叶的位置暂放占位 QWidget。
若工厂、ownership 或布局构建失败，整个暂存树被丢弃，活动树从未被移动。只有暂存完整成功，才把
复用叶迁入新树、交换 `activeSurface_`，最后销毁旧容器和移除/替换的叶，并尽量恢复原焦点。

这相当于页面级事务：提交前没有破坏性动作，提交后目标一次生效。它保护 Demo 的对象状态，但不证明
任意存量 QWidget 都可安全 reparent；真实组件仍需单独认证。

## 实例：计算器在左、历史记录在右，宽度 2:1

假设当前 Surface 已有 `calculator-main:Calculator` 和
`history-main:CalculationHistory`。用户输入：“把计算器放左边、历史记录放右边，并按 2:1 分配
宽度。”Qt 发送该 Prompt 与最后成功的 Surface；LLM Adapter 同时提供 Effective Catalog。模型只能
生成类似下面的 LayoutPlan，而不能提供新 ID、像素宽度或 QSS：

```json
{
  "version": "0.1",
  "root": {
    "kind": "layout",
    "type": "Row",
    "justify": "start",
    "children": [
      {"kind": "existing", "id": "calculator-main", "type": "Calculator", "weight": 2},
      {"kind": "existing", "id": "history-main", "type": "CalculationHistory", "weight": 1}
    ]
  }
}
```

Validator 确认两个 existing 引用与当前 Surface 匹配，正 weight 配合 `justify=start`。Compiler 为
容器分配可用的 `layout-N`，保留两个业务 ID，生成的 SurfaceSpec 图等价于
`layout-N → [calculator-main, history-main]`。Python 与 Qt 两侧分别验证唯一性、引用、单父、无环和
可达性。

Reconciler 看到两个 `(id,type)` 都未变化，因此复用两个 QWidget，只暂存新的 Row 容器。提交时
QHBoxLayout 将 weight 映射为 stretch 2:1，同时继续服从组件的最小/首选/最大尺寸和 `QSizePolicy`。
新 Row 成为活动页面后旧容器才销毁；若生成、校验或暂存任一步失败，原页面及两个 QWidget 的状态
均保持不变。

这个例子展示了各层只依赖上一层建立的不变量：Provider 只表达受限意图，Compiler 决定身份，
SurfaceSpec 固化完整目标，Validator 证明图可渲染，Qt 才复用对象并原子提交。

## QWidget 几何映射

SurfaceSpec 不传每个 QWidget 的像素矩形。Renderer 只把有限的布局语义翻译成 Qt
`QBoxLayout`，最终尺寸仍由 Qt 根据可用空间和组件自身约束计算。当前字段的归属是：`type`、
`children`、`gap`、`align`、`justify` 属于父 layout 节点；`weight` 写在直接子节点上。

### Row、Column、children 与嵌套

每个 layout 节点都是一个容器，`children` 数组从前到后决定直接子项在主轴上的顺序。在当前从左到右
的界面方向中：

| `type` | Qt 映射 | 主轴与 children 顺序 | 交叉轴 |
|---|---|---|---|
| `Row` | `QHBoxLayout` | 水平；第一个 child 在左，后续依次向右 | 垂直方向 |
| `Column` | `QVBoxLayout` | 垂直；第一个 child 在上，后续依次向下 | 水平方向 |

layout 也可以成为另一个 layout 的 child。父容器先把整个子 layout 当作一个矩形区域放置，子 layout
再在自己的区域内排列 children，因此多个 Row/Column 的相对位置由“父容器方向 + children 顺序”
递归决定。例如：

```json
[
  {"id": "root", "type": "Column", "children": ["top", "bottom"]},
  {"id": "top", "type": "Row", "children": ["calculator-main", "stats-main"]},
  {"id": "bottom", "type": "Row", "children": ["history-main", "notes-main"]}
]
```

可视化后是：

```text
Column(root)
├─ Row(top):    Calculator | CalculationStats
└─ Row(bottom): History    | NotePad
```

即 root 先把两个 Row 排成上下两行，再由每个 Row 把自己的两个子项排成左右两列。完整可导入示例见
[nested-2x2 SurfaceSpec](../shared/fixtures/surface-spec/valid/nested-2x2.json)。嵌套层级当前最多 8 层。

### gap：直接子项之间的固定间距

`gap` 只影响同一个 Row/Column 内相邻直接子项之间的间距，不是容器外边距，也不会跨层继承。
每个嵌套容器可以声明自己的 gap；未声明时使用 `medium`。Renderer 将容器 margin 固定为 0。

| `gap` | Qt `setSpacing()` | 视觉含义 |
|---|---:|---|
| `none` | 0 逻辑像素 | 相邻区域不额外留缝 |
| `small` | 4 逻辑像素 | 紧凑间距 |
| `medium` | 8 逻辑像素 | 默认间距 |
| `large` | 16 逻辑像素 | 更明显的区域分隔 |

“逻辑像素”是 Qt 布局使用的设备无关坐标单位；在高 DPI 缩放下，Qt 会把它换算为相应物理像素，
因此 8 个逻辑像素不一定等于屏幕上的 8 个物理像素。gap 是固定基础间距，justify 产生的可伸缩空白
会在此基础上额外分配。

### align：交叉轴上的对齐

`align` 由父 layout 声明，并统一作用于它的直接 children。它不改变 children 顺序，只决定每个 child
在垂直于排列方向的剩余空间中靠哪一侧；缺省值是 `stretch`。

| `align` | Row 中的效果（垂直） | Column 中的效果（水平） |
|---|---|---|
| `start` | 靠上 | 当前左到右界面中靠左 |
| `center` | 垂直居中 | 水平居中 |
| `end` | 靠下 | 当前左到右界面中靠右 |
| `stretch` | 不设置额外 Qt alignment，让组件按自身纵向 `QSizePolicy` 使用单元格 | 不设置额外 Qt alignment，让组件按自身横向 `QSizePolicy` 使用单元格 |

`stretch` 不会改写业务 QWidget 的 `QSizePolicy`，所以 Fixed 组件不会仅因 align=stretch 就被强制拉满。

### justify：主轴上的整体分布

`justify` 由父 layout 声明，在所有直接 children 的 `weight` 都为 0 时，通过可伸缩 spacer 分配主轴
剩余空间；缺省值是 `start`。下表中的“起点/终点”对 Row 分别是左/右，对 Column 分别是上/下：

| `justify` | 主轴效果 | 两个子项的示意（`·` 表示可伸缩空白） |
|---|---|---|
| `start` | children 靠起点，剩余空间放在末尾 | `A B ···` |
| `center` | children 作为整体居中 | `·· A B ··` |
| `end` | children 靠终点，剩余空间放在开头 | `··· A B` |
| `spaceBetween` | 首尾贴近两端，剩余空间只放在相邻项之间 | `A ··· B` |
| `spaceAround` | 每项两侧获得同等份额，因此外侧空白约为项间空白的一半 | `· A ·· B ·` |
| `spaceEvenly` | 首尾和相邻项之间的可伸缩空白相等 | `· A · B ·` |

空容器不添加 spacer。单个 child 使用 `spaceBetween` 时等同 `start`；使用 `spaceAround` 或
`spaceEvenly` 时等同 `center`。这些规则避免单项布局产生无法解释的特殊空白。

### weight：直接子项的主轴伸展比例

`weight` 是 0–10 的整数，写在 child 节点上，表示该 child 相对于同一父容器兄弟节点的 Qt stretch
factor。它只作用于父容器的主轴：Row 中影响横向伸展，Column 中影响纵向伸展。嵌套 layout 也可以
携带 weight，此时比例分配给整个子树。root 没有父容器，因此禁止声明 weight。

当任一直接 child 的 `weight > 0` 时，父容器必须使用 `justify=start`，Renderer 不再添加 justify
spacer，而是把各 child 的 weight 传给 `QBoxLayout::addWidget()`。例如 weight 2:1 表示两个区域按
2:1 分享可伸展的剩余空间；weight 0 的兄弟主要按自身尺寸需求占用空间。它不是绝对像素比例，也不
表示第一个组件最终宽度必然严格等于第二个的两倍。

### Qt 如何得到最终尺寸

可以把当前 Renderer 的计算理解为以下顺序：

1. 父容器取得 Qt 分配给它的矩形，外 margin 固定为 0。
2. 按 children 数量扣除相邻项的固定 gap。
3. Qt 先尊重每个 child 的最小尺寸、`sizeHint()`（首选尺寸）、最大尺寸和 `QSizePolicy`。
4. 若存在正 weight，按 stretch factor 分配可伸展空间；否则按 `justify` 添加 spacer，并结合各
   QWidget 的 `QSizePolicy` 分配空间。
5. 最后按 `align` 决定 child 在交叉轴单元格中的位置或伸展方式。

常见 `QSizePolicy` 可简化理解为：`Fixed` 倾向保持首选尺寸，`Preferred` 可以伸缩但不主动争取
额外空间，`Expanding` 表示能够有效利用额外空间。Qt 仍会同时受显式 minimum/maximum 和嵌套子树
约束，因此 DSL 只表达布局意图，不能越过 QWidget 的硬尺寸限制。

Host 最外层使用 `QScrollArea(widgetResizable=true)`；当整棵子树的最小尺寸超过 viewport 时，滚动
保证内容可达。当前 DSL 没有 `width`、`height`、margin、padding、像素坐标、Grid 或 breakpoint
字段，这符合 MVP 的受控边界。权威实现与规则见
[SurfaceRenderer.cpp](../frontend/composition/SurfaceRenderer.cpp)和
[layout-semantics.md](../shared/protocol/layout-semantics.md)。

## 辅助业务流

- Surface 导入：本地 JSON（最多 64 KiB）经 `/surface/import` 与 Qt 双重校验后提交。
- Surface 导出：当前 Surface 经 `/surface/export` 复核、规范化，再由 `QSaveFile` 原子写入。
- 恢复默认：`shared/default-surface.json` 通过同一 Qt 校验和提交链；不会绕过 Renderer。
- 计算记录：Calculator 写入本地 FastAPI/SQLite，History 与 Stats 各自轮询；组件之间不直接查找对象
  或共享事件总线，布局编排不会改变其业务 API。
- Ubuntu 启停：脚本校验图形环境、受控 ELF、依赖和端口，只管理本轮启动的 FastAPI 与 HostShell。

## 模块索引

| 模块 | 核心职责 | 主要位置 |
|---|---|---|
| Host 与主题 | 唯一窗口、控制区、Surface 舞台 | `frontend/app/` |
| 编排客户端 | 网络、Qt Validator、Registry、Renderer/Reconciler | `frontend/composition/` |
| 业务组件与服务 | 五类 QWidget、本地计算 API 客户端 | `frontend/widgets/`、`frontend/services/` |
| Agent Service | `/compose`、LLM Adapter、LayoutPlan、Compiler、Surface 文档 | `backend/agent_service/` |
| 计算业务 API | 记录 CRUD、摘要、SQLite Repository | `backend/mock_business_api/` |
| 共享合同 | Catalog、Schema、语义、默认 Surface、Fixture、评测语料 | `shared/` |
| 验证入口 | C++/Python/运行测试与 Provider runner | `tests/`、`backend/tests/`、`scripts/` |

## 安全、故障恢复与当前局限

- Catalog、LayoutPlan 和 SurfaceSpec 均为封闭 allowlist；DSL 不能提供 URL、method、Action、脚本、
  signal/slot、QSS、颜色、字体或像素几何。
- HTTP 只连接可信本地回环；Provider Key 只进入后端 Authorization header，不进入 Prompt、DSL、Qt
  或审计结果。
- Provider、Parser、Compiler、任一 Validator 或暂存失败时，无效目标不提交；用户仍可导出当前
  Surface、导入合法文档或恢复默认。
- 当前未认证真实 QMainWindow/Dock、QML、Graphics View、二进制插件、复杂 QObject 生命周期、
  生产权限/多租户和供应链边界。
- `PB-048` 证明合法 DSL 可能仍未满足意图；Provider 长尾证明平均值不能代表最坏等待。证据见
  [验证与评测报告](verification-and-evaluation.md)。

## 渐进式展望

后续能力只在现有证据不足时逐步扩展，不预设完整兼容框架或布局求解器：

| 当前证据/问题 | 最小下一步 | 升级触发条件 |
|---|---|---|
| Provider 存在长尾、传输失败和样式语义 no-op | 统一 Provider/后端/Qt 超时口径，补充阶段诊断和意图级断言，保留用户主动重试 | 重复采样仍显示特定失败模式时，再扩展生成约束或专用判定层 |
| Demo 只验证五类项目内 QWidget | 选择少量代表性旧组件，先尝试直接注册或薄 Adapter，验证 GUI 线程、parent/reparent、状态、焦点和依赖 | 真实组件持续暴露生命周期、ABI、隔离或权限问题时，再设计更复杂的接入层 |
| Qt 原生 stretch/size policy/滚动可达，但视觉质量只经代表性检查 | 先针对可复现缺陷调整可信组件尺寸策略、stretch、对齐和滚动行为 | 现有 Qt 机制无法解决可重复、可测量的视觉缺陷时，再引入受控元数据或质量判定 |

任何升级都必须保持 Catalog 权限、稳定 ID、业务状态和失败不提交。未通过验证的旧组件保留原静态
页面或独立入口；没有可审计等价结果的排版请求应拒绝，而不是静默改成另一种布局。

## 依赖版本策略

当前受控基线为 Qt 5.12.8（Core/Widgets/Network/Test）、GCC 7.3.0 与 9.3.0、ISO C++14、
CMake 3.16.9、`_GLIBCXX_USE_CXX11_ABI=1`。真实存量组件与二进制依赖可能和它强绑定，因此不把
大版本升级作为落地前提。若安全、操作系统或供应链要求必须升级，应另立 OpenSpec change，先验证
真实组件兼容、双基线和可执行回退。
