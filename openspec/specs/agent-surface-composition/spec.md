# agent-surface-composition Specification

## Purpose

定义 Agent Service 将自然语言意图安全、确定性地编译为完整目标 SurfaceSpec 的职责边界。

## Requirements

### Requirement: 编排 HTTP 合同
Agent Service SHALL 提供 `POST /compose`，接收用户自然语言指令和当前 Surface 上下文，并返回一个完整且经过校验的目标 SurfaceSpec，或结构化诊断错误。

#### Scenario: 成功编排返回完整目标
- **WHEN** Prompt 与当前 Surface 产生有效编排结果
- **THEN** 响应包含完整的 `main` SurfaceSpec，而不是命令式或局部 UI 变更

### Requirement: LLM 只能输出受限 LayoutPlan
LLM SHALL 接收 Effective Catalog 和当前 Surface，并输出只能引用既有业务叶子、申请新的 Catalog 实例以及描述递归 Row/Column 层级与允许布局意图的 LayoutPlan；LLM SHALL NOT 分配最终组件 ID，或生成可执行行为、业务请求、数据绑定和任意样式。

#### Scenario: 新增同类型实例时不由模型分配最终 ID
- **WHEN** 用户要求添加第二个 Calculator
- **THEN** LLM 的 LayoutPlan 表达新增 Calculator 的意图，最终独立 ID 由 Surface Compiler 分配

### Requirement: 不可表达布局返回明确诊断
当用户请求 Grid、跨行列、重叠、wrap、Splitter、Dock 或响应式断点等 SurfaceSpec v0 无法表达的布局时，Agent Service SHALL 返回错误码 `unsupported_layout` 和可读诊断，SHALL NOT 生成近似 LayoutPlan 或 SurfaceSpec。

#### Scenario: Grid 跨列请求不被近似
- **WHEN** 用户要求 CalculationHistory 跨越网格两列
- **THEN** `/compose` 返回 `unsupported_layout`，且 Qt 客户端保留最后一个有效 Surface

### Requirement: Surface Compiler 拥有稳定身份
确定性的 Surface Compiler SHALL 解析既有实例引用、执行 Catalog 多实例规则、为新实例分配 ID、复用符合条件的既有 ID，并生成最终有序 SurfaceSpec 邻接表。

#### Scenario: 未变化内容保留 ID
- **WHEN** LayoutPlan 重排一个逻辑上未变化的 NotePad
- **THEN** Surface Compiler 在新的图位置输出该 NotePad 的既有 ID

### Requirement: Fixture 与 LLM 共用编译链路
LayoutPlan Fixture 与 LLM 生成的 LayoutPlan SHALL 经过同一个 Surface Compiler；每个编译出的 SurfaceSpec SHALL 经过同一个服务端 Validator 后才可返回。

#### Scenario: Fixture 独立验证 Compiler
- **WHEN** LLM 集成不可用
- **THEN** 确定性 LayoutPlan Fixture 仍可验证稳定 ID、同类型多实例、校验及完整 SurfaceSpec 生成

### Requirement: 编排 API 与业务 API 隔离
即使运行在同一个 FastAPI 进程中，编排端点与 SQLite-backed Mock Calculation Business API 也 SHALL 使用相互独立的模块、Router、客户端和数据访问依赖；`/compose` SHALL NOT 导入或调用计算记录数据访问层。

#### Scenario: 编排链路没有计算记录请求权限
- **WHEN** `/compose` 处理任意 Prompt
- **THEN** 它既不生成也不执行 `/api/calculations*` endpoint、HTTP method 或 request body
