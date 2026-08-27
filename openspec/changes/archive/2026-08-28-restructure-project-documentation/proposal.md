## Why

首轮文档重构把外部灵感、方案比较、端到端处理步骤和远期设计集中写入核心文档，造成技术架构与评测报告过长、重点不清。需要重新划分文档边界，并以“先解释概念和必要性，再用实例串联协作关系”的方式提升可读性，同时控制后续计划与 task 数量。

## What Changes

- 将项目灵感来源和完整方案比较从 `docs/technical-architecture.md` 迁移到既有 `docs/research/generative-ui-layout-comparison.md`，技术架构只保留必要的设计来源摘要与链接；在根 `README.md` 中提供按需查阅入口。
- 系统重构 `docs/technical-architecture.md`：先解释关键组件、Effective Catalog、LayoutPlan、SurfaceSpec 和核心算法的含义、作用与必要性，再用一个具体请求串联数据变化和组件协作；删减流水账式步骤、重复背景和次要实现细节。
- 将技术架构中的复杂远期方案收敛为少量渐进式展望，遵循“先满足当前最小可用需求，再根据业务需求和测试证据升级”的原则，不预先展开完整框架或算法设计。
- 重构 `docs/verification-and-evaluation.md`：保留当前验证事实与问题分析，将冗长的“评测数据集如何设计”和“后续验证优先级”合并到文末的后续优化方向，按“当前覆盖—现有不足—后续设计”组织。
- 完成跨文档编辑复核，删除重复、啰嗦、晦涩和流水账式内容，核对信息边界、术语、链接与当前/未来能力表述。

## Capabilities

### New Capabilities

无。本 change 仍只调整文档与证据的组织和表达。

### Modified Capabilities

无。SurfaceSpec、LayoutPlan、Catalog、API、Renderer 和评测行为均不改变，继续使用 `skip_specs: true`。

## Impact

- 计划修改范围限定为根 `README.md`、`docs/technical-architecture.md`、`docs/verification-and-evaluation.md` 和既有 `docs/research/generative-ui-layout-comparison.md`。
- 不新增调研文档，不修改 C++、Python、构建配置、测试脚本、OpenSpec 主规格、Provider 结果或历史审计 JSON。
- 实施后需检查 Markdown 链接、术语一致性、跨文档重复以及当前能力与后续展望的边界。
