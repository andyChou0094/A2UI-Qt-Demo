# Provider 评测数据索引

更新日期：2026-08-24

本目录把 `docs/provider-baseline-results.json` 中的样本按协议层和可导入性分类。
原始审计 JSON 是权威证据；本目录中的文件是便于人工复现的派生副本。

## 哪些文件可以导入

| 数据类别 | 位置 | 数量 | 能否用“导入 DSL” |
|---|---|---:|---|
| Provider 合法 SurfaceSpec | [`provider-valid-surfaces/`](provider-valid-surfaces/) | 40 | 可以；每个文件是一份完整 SurfaceSpec |
| 被拒绝的 LayoutPlan | [`rejected-layout-plans/`](rejected-layout-plans/) | 9 | 不可以；这是 Compiler 之前的计划层数据 |
| 合法合同 Fixture | [`../../shared/fixtures/surface-spec/valid/`](../../shared/fixtures/surface-spec/valid/) | 7 | 可以 |
| 非法合同 Fixture | [`../../shared/fixtures/surface-spec/invalid/`](../../shared/fixtures/surface-spec/invalid/) | 11 | 不可以；用于验证拒绝边界 |

## 40 份合法 SurfaceSpec

`provider-valid-surfaces/PB-xxx.json` 只包含原始审计样本的 `surface`。可直接在工作台
中选择“导入 DSL”。样本为：

- `PB-001`–`PB-007`
- `PB-009`–`PB-040`
- `PB-048`

[`PB-048.json`](provider-valid-surfaces/PB-048.json) 通过 SurfaceSpec 技术校验，但原 Prompt
“给页面设置蓝色背景和自定义字体”没有得到实现或明确拒绝，因此它同时是一个
语义 no-op 证据。这说明“DSL 合法”不等于“意图满足”。

## 9 份被拒绝的 LayoutPlan

| 样本 | Prompt 要点 | 结果 | 首个失败边界 |
|---|---|---|---|
| [`PB-041`](rejected-layout-plans/PB-041.json) | Grid 三列 | `unsupported_layout` | LayoutPlan 封闭校验：Grid |
| [`PB-042`](rejected-layout-plans/PB-042.json) | 跨两列 | `unsupported_layout` | LayoutPlan 封闭校验：Grid |
| [`PB-043`](rejected-layout-plans/PB-043.json) | 响应式换行 | `unsupported_layout` | LayoutPlan 封闭校验：Wrap |
| [`PB-044`](rejected-layout-plans/PB-044.json) | Overlay | `unsupported_layout` | LayoutPlan 封闭校验：Overlay |
| [`PB-045`](rejected-layout-plans/PB-045.json) | Dock | `unsupported_layout` | LayoutPlan 封闭校验：Dock |
| [`PB-046`](rejected-layout-plans/PB-046.json) | Splitter | `unsupported_layout` | LayoutPlan 封闭校验：Splitter |
| [`PB-047`](rejected-layout-plans/PB-047.json) | Wrap | `unsupported_layout` | LayoutPlan 封闭校验：Wrap |
| [`PB-049`](rejected-layout-plans/PB-049.json) | 执行脚本的 Button | `invalid_layout_plan` | Catalog 校验：未知 `Button` |
| [`PB-050`](rejected-layout-plans/PB-050.json) | Grid、跨列、断点 | `unsupported_layout` | LayoutPlan 封闭校验：Grid |

这些 JSON 不是 SurfaceSpec，不能导入工作台。它们保留 Provider 返回的计划，用于复现
Parser/Compiler 的首个拒绝边界。

## 没有 JSON 输出的 PB-008

`PB-008` 的 Prompt 是“上方放计算器，下方放历史”。Provider 调用约 21.050 秒后发生
`connection refused`，返回 `llm_provider_error`；该样本没有 `rawLayoutPlan` 或 `surface`。
因此本索引只记录传输失败，不伪造“错误 DSL”。

## 现有 SurfaceSpec Fixture

合法 Fixture 覆盖深度边界、重复 Calculator、空 Surface、2×2、侧栏、上下和权重布局。
非法 Fixture 分别覆盖环、深度/节点超限、禁止布局、非法 JSON、多父节点、权重与
`justify` 冲突、root 权重、未知业务类型、未知字段和不可达节点。它们是合同测试的
单一信息源，本目录不复制这些 Fixture。

## 证据与完整性

- 权威原始文件：[`../provider-baseline-results.json`](../provider-baseline-results.json)。
- 语料定义：[`../../shared/evaluation/provider-baseline-v1.json`](../../shared/evaluation/provider-baseline-v1.json)。
- 完整结论与指标口径：[`../verification-and-evaluation.md`](../verification-and-evaluation.md)。
- 派生文件只能由原始审计数据重新提取；不应手工改写单个样本。
