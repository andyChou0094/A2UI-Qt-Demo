# A2UI Qt 受控组件编排 Demo

本项目验证一件事：能否在不改写既有 C++ QWidget 内部实现、状态和业务请求的前提下，
让 Agent 根据自然语言安全地重新组合一组已注册业务组件。

它是一个受控编排机制 Demo，不是任意前端生成器，也不是 Google A2UI 正式实现。

## 项目解决什么问题

存量 Qt 桌面软件往往已有大量成熟 QWidget，其中包含稳定样式、signal/slot、局部状态、Service
调用和后端合同。如果让 LLM 直接生成 C++、QML、QSS 或网络请求，结果难以审计，也容易破坏既有行为。

项目使用更窄的能力边界：

- 应用开发者通过 Component Registry 决定哪些完整 QWidget 可以被编排。
- Agent 只能在封闭 Catalog 中选择组件，请求新实例，并用 Row/Column 组织布局。
- 确定性 Compiler 分配或复用稳定 ID，服务端和 Qt 客户端双重校验完整 Surface。
- Renderer 先暂存目标，成功后原子提交；任何失败都保留最后有效页面。

## 用户能做什么

- 用自然语言把已有组件排成左右、上下、侧栏、受控 2×2 或权重突出布局。
- 添加同类独立实例，例如第二个 Calculator。
- 在合法重排时复用未变业务 QWidget，保留输入、选择、计时、草稿和 Qt 允许保留的焦点。
- 使用 Calculator、CalculationHistory、CalculationStats、Clock 和 NotePad 五类 Demo 组件。
- 导入、导出经校验的 SurfaceSpec JSON，或恢复受控默认排版。

当前不支持未注册组件、任意样式、业务 Action、脚本、Grid 跨列、Dock、Splitter、重叠、wrap
或响应式断点。越界请求应当得到明确拒绝，不会为了“看起来成功”而使用近似页面。

## 对本单位的价值

本方案允许存量 Qt 组件继续拥有自己的样式、控件树、signal/slot、业务逻辑、Service 和后端合同，
不需要为动态组合先把它们整体重写或迁移。自然语言和 DSL 可加快新页面组合的实验，封闭 Catalog、
双重校验和原子提交则使能力可审计、可拒绝、可回退。组件可以逐类认证和渐进注册，无需一次性接入全部资产。

边界必须与收益同时阅读：当前 Demo 只验证了 5 类示例 QWidget，并未认证本单位真实组件的
Adapter、GUI 线程、parent/reparent、QObject 生命周期、局部状态/焦点、尺寸策略、二进制 ABI、
第三方依赖和既有 Service/API 权限。

## 核心机制

```text
自然语言 + 当前 Surface + Component Catalog
                         │
                         ▼
               Provider 生成 LayoutPlan
                         │
                         ▼
           封闭校验 + Surface Compiler
              （复用/分配稳定 ID）
                         │
                         ▼
          服务端 SurfaceSpec Validator
                         │
                         ▼
              Qt 独立 Validator
                         │
                         ▼
        QWidget Reconciliation 暂存与原子提交
```

模型不生成最终 ID，不直接创建 QWidget，也不获得业务 API 的 URL、HTTP method 或请求体。
深入的合同、数据流和算法见[技术架构](docs/technical-architecture.md)。

## 快速启动

在 Ubuntu 图形桌面终端中，如果已存在合格的受控构建：

```bash
cd ~/桌面/A2UI-Qt-Demo
scripts/start_ubuntu_demo.sh
```

如需自然语言编排，首次先执行：

```bash
scripts/configure_demo_provider.sh
```

没有 API Key 时，默认界面、计算器、DSL 导入/导出和恢复默认仍可使用。如果启动脚本提示未找到合格 ELF
或构建早于当前源码，请不要绕过检查，按 [Ubuntu 启动指南](docs/getting-started.md) 完成受控构建、
预检、操作和故障处理。

## 当前验证结论

截至 2026-08-21 的审计证据：

- GCC 7.3.0 与 9.3.0 的 Qt 5.12.8/C++14 frontend CTest 均为 12/12。
- Backend unit 37/37、integration 14/14，真实 uvicorn Surface 文档回环 5/5。
- Provider 七场景 7/7，其中 6 个支持场景成功，1 个不支持场景正确拒绝。
- 50 样本基线完成 50/50，40 份最终得到可校验 SurfaceSpec；这是技术分类，不是产品通过率。
- 已确认 1 次 Provider 传输失败、1 份合法但未满足样式意图的 no-op，以及显著的长尾响应时间。
- 固定 seed 25 份合法 DSL、Surface 导入/导出、单窗口 XWayland/xcb 生命周期和
  5 类页面 × 3 档缩放均有通过记录。

数字口径、七场景表、50 样本中位数/最慢样本、问题与兜底统一收录在
[验证与评测报告](docs/verification-and-evaluation.md)。

## 文档地图

| 需求 | 文档 |
|---|---|
| 第一次启动和实际操作 | [Ubuntu 启动指南](docs/getting-started.md) |
| 本项目组件、DSL、核心算法与模块边界 | [技术架构](docs/technical-architecture.md) |
| 外部灵感、布局模型与跨方案比较 | [生成式 UI 布局模型调研](docs/research/generative-ui-layout-comparison.md) |
| 当前测试数字、Provider 样本、问题和复现 | [验证与评测报告](docs/verification-and-evaluation.md) |
| 可直接导入的评测 Surface 与被拒绝计划 | [评测数据索引](docs/evaluation-data/README.md) |
| Ubuntu 运行缓存、进程、Provider 命令和故障排查 | [Ubuntu 运行细节](docs/ubuntu-runtime.md) |
| Qt/GCC/C++/CMake 兼容基线 | [工具链矩阵](docs/toolchain-matrix.md) |
| 架构决策 | [ADR](docs/adr/) |
| 当前行为合同 | [OpenSpec 主规格](openspec/specs/) |
| 历史 change 与实施记录 | [OpenSpec 归档](openspec/changes/archive/) |
| 早期 A2UI 调研记录 | [A2UI 调研](A2UI调研.md) |

## 后续方向

近期先依据现有证据改善 Provider 长尾、超时口径和意图级 no-op 诊断；再用少量代表性旧 QWidget
验证直接注册或薄 Adapter，并针对可复现的排版问题利用 Qt 现有尺寸策略、stretch 与滚动能力。
只有持续测试或真实业务暴露当前机制无法处理的问题时，才扩展组件接入层、布局元数据或质量判定。
这些都是后续方向，不是当前 Demo 已实现能力；证据缺口见[验证与评测报告](docs/verification-and-evaluation.md)，
升级触发条件见[技术架构](docs/technical-architecture.md#渐进式展望)。
