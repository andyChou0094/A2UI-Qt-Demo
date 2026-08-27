# provider-runtime-acceptance Specification

## Purpose

定义 DeepSeek 真实 Provider 在本机运行、验收和测评中的固定配置、临时密钥映射、凭据防泄漏、七场景冒烟、50 样本合法率与平均值/p50 延迟、批量 DSL 渲染回归、问题处置和可审计记录合同，确保重验不会放宽受控编排边界。

## Requirements

### Requirement: Provider 使用固定应用配置合同
真实 Provider 运行 SHALL 使用 endpoint `https://api.deepseek.com/chat/completions` 和模型 `deepseek-v4-flash`；应用 SHALL 继续只从 `A2UI_LLM_ENDPOINT`、`A2UI_LLM_MODEL` 和 `A2UI_LLM_API_KEY` 读取 endpoint、模型和密钥，SHALL NOT 新增从源码、配置文件或编排产物读取凭据的路径。

#### Scenario: 配置完整
- **WHEN** 三个应用环境变量在验收进程中均有效
- **THEN** 七场景验收调用指定 endpoint 和模型，并继续经过既有受限 LayoutPlan 适配器与编译链路

#### Scenario: 配置缺失
- **WHEN** endpoint、模型或密钥任一缺失
- **THEN** 验收在真实调用前失败并给出中文配置说明，不把该次运行记为通过

### Requirement: 外部密钥只在进程环境中映射
若操作者提供的外部变量名为 `deepseek_api`，运行入口 SHALL 只在其启动的后端或验收子进程环境中将值映射到 `A2UI_LLM_API_KEY`；密钥 SHALL NOT 写入源码、脚本、日志、文档、命令回显、运行数据库或验收结果。已经暴露的旧密钥 SHALL NOT 用于新的真实 Provider 验收，操作者必须先提供轮换后的密钥。

#### Scenario: 使用外部变量启动验收
- **WHEN** 当前进程具有轮换后的 `deepseek_api` 且未显式设置应用密钥变量
- **THEN** 子进程可通过 `A2UI_LLM_API_KEY` 完成调用，而父级持久配置和任何输出文件中不出现密钥值

#### Scenario: 没有轮换后的可用密钥
- **WHEN** 只有已暴露旧密钥或没有可用密钥
- **THEN** 系统不发起新的真实 Provider 调用，也不复用旧验收结果冒充本次通过

### Requirement: Demo 可使用未纳入版本控制的本地 Provider 配置
Ubuntu Demo 运行入口 MAY 从 `config/provider.local.env` 读取固定白名单字段，以减少每次启动时重复设置环境变量；该文件 SHALL 被 Git 忽略、权限 SHALL 不宽于 `0600`，仓库 SHALL 只提交不含凭据的示例。进程环境中的同名变量 SHALL 优先于本地配置。应用和验收 runner 的三环境变量合同保持不变。

#### Scenario: 使用本地 Demo 配置启动
- **WHEN** 操作者通过隐藏输入创建权限为 `0600` 的本地 Provider 配置
- **THEN** Ubuntu 入口将配置仅注入自有子进程，动态 HostShell 可调用固定 Provider，源码和审计结果不包含密钥

#### Scenario: 本地配置权限过宽
- **WHEN** Provider 本地配置允许组用户或其他用户读取
- **THEN** 运行入口在启动前拒绝加载并给出修复权限的中文诊断

### Requirement: 七个自然语言场景全部重新验收
真实 Provider 验收 SHALL 依次验证左右、上下、侧栏、2×2、权重突出、重复组件和不支持布局七个自然语言场景；六个受支持场景 SHALL 产生满足结构和稳定 ID 断言的有效结果，不支持布局场景 SHALL 返回 `unsupported_layout` 且不产生可提交目标。仅当七个场景全部通过时整次验收才 SHALL 标记为通过。

#### Scenario: 七场景全部满足合同
- **WHEN** Provider 对六个支持场景产生有效 LayoutPlan，且对 Grid/跨列/响应式请求触发 `unsupported_layout`
- **THEN** 验收结果标记为 7/7 通过，并记录每个场景的独立断言结果

#### Scenario: 任一场景不满足合同
- **WHEN** 任一结果结构错误、丢失既有稳定 ID、错误地近似不支持布局或 Provider 调用失败
- **THEN** 整次验收标记为失败，并保留不含凭据的失败诊断

### Requirement: 验收使用既有封闭编排链路
Provider 原始响应 SHALL 经过与 Fixture 相同的 LayoutPlan Parser、Surface Compiler 和服务端 Validator，SHALL NOT 通过手写 SurfaceSpec、跳过校验或放宽 Schema 来获得通过；不支持布局或其他失败 SHALL 保留最后有效 Qt 界面。

#### Scenario: Provider 返回近似或越权字段
- **WHEN** 模型输出任意样式、业务请求、Action、脚本或协议外布局
- **THEN** 既有 Parser 或 Validator 拒绝结果，验收失败且不修改当前 Surface

#### Scenario: 重复组件场景
- **WHEN** 模型请求新增一个 Calculator 并保留现有 Calculator
- **THEN** Surface Compiler 复用原稳定 ID、为新增实例分配独立 ID，且模型本身不分配最终 ID

### Requirement: 生成不含凭据的可追溯审计结果
每次真实验收 SHALL 生成带运行时间、endpoint、模型、场景输入、原始模型内容、解析计划、编译 Surface、结构断言、稳定 ID 变化、错误码和通过状态的审计结果；记录 SHALL 明确标注未保存 API Key，并 SHALL 在写入前对潜在认证信息执行防泄漏检查。

#### Scenario: 验收完成并写入结果
- **WHEN** 七个场景执行完毕
- **THEN** 审计文件足以复核每个判定，且不包含 API Key、Authorization header 或环境变量值

#### Scenario: 输出疑似包含凭据
- **WHEN** 写入前的报告内容匹配当前密钥或认证头模式
- **THEN** 验收入口拒绝持久化不安全报告、返回失败并仅输出不含敏感值的中文诊断

### Requirement: 首轮 Provider 基线使用五十个样本测量合法率和延迟
系统 SHALL 提供独立于七场景门禁的 Provider 基线脚本，首轮 SHALL 对版本化 Prompt 语料执行恰好 50 次完整请求。每个样本 SHALL 从发起请求到获得最终结果使用单调时钟记录 Provider 响应时间和 `/compose` 端到端时间，并记录是否依次通过传输、LayoutPlan 解析、语义检查、Surface Compiler 和最终 SurfaceSpec Validator。报告 SHALL 计算 50 个样本的最终 DSL 合法率、平均响应时间和 p50。

#### Scenario: 完成五十样本基线
- **WHEN** 50 个独立样本均获得成功或可分类的最终结果
- **THEN** 报告以 50 为合法率分母，分别给出全部样本与成功样本的平均值和 p50、各阶段耗时、失败数量及样本级明细

#### Scenario: 样本因外部条件未完成
- **WHEN** 运行被中断、密钥失效、Provider 限流或其他外部条件导致不足 50 个最终样本
- **THEN** 报告明确标记“基线未完成”、保留已完成数量和失败原因，不将部分样本冒充正式 50 样本结论

#### Scenario: 模型响应无法形成合法 DSL
- **WHEN** Provider 返回内容但在 LayoutPlan 解析、语义检查、Surface 编译或最终 Surface 校验任一阶段失败
- **THEN** 该样本计入 50 个样本和总延迟统计、计为 DSL 不合法，并记录唯一的首个失败阶段及无凭据诊断

### Requirement: 测评语料和运行参数可复现
50 样本测评 SHALL 记录语料版本、Prompt 标识、endpoint、模型、温度、超时、执行顺序和运行时间；语料 SHALL 覆盖左右、上下、侧栏、嵌套、权重、重复组件、空 Surface 以及明确不支持的布局意图。脚本 SHALL 默认串行执行以贴近单用户 HostShell 请求链路，并 SHALL 提供显式的成本与预计请求次数提示。

#### Scenario: 重复执行同一基线
- **WHEN** 操作者使用相同语料版本、模型和运行参数再次执行 50 样本测评
- **THEN** 两次报告具有可直接对比的样本标识、合法率、平均值、p50 和失败分类，同时明确 Provider 输出仍具有概率性

#### Scenario: 语料或参数发生变化
- **WHEN** Prompt 语料、模型、温度、超时或执行方式任一改变
- **THEN** 报告使用新的基线标识并列出变化，不与旧基线聚合成同一组指标

### Requirement: 合法 DSL 批量导入和 Qt 渲染必须可重复验证
系统 SHALL 使用固定随机种子的生成器批量产生符合 SurfaceSpec Schema 与 Catalog 的合法 DSL，覆盖 Row/Column 嵌套、全部业务组件、允许的重复组件、节点数与深度边界以及 gap、align、justify、weight 合法组合。每个样本 SHALL 经过 `/surface/import` 后端接口和真实 Qt Renderer；测试 SHALL 记录导入结果、渲染结果、组件清单、几何检查、Qt 关键告警和截图证据，而不依赖 Provider 生成这些合法样本。

#### Scenario: 批量合法 DSL 全部可渲染
- **WHEN** 固定种子生成的一批合法 DSL 依次调用导入接口并在同一 HostShell 中应用
- **THEN** 每个样本均通过后端校验和 Renderer 暂存提交，业务组件类型与数量匹配，主要控件无重叠或负尺寸，测试过程中无崩溃且前一样本状态不会污染后一样本

#### Scenario: 代表性动态页面执行视觉回归
- **WHEN** 从结构类别和边界条件中选取代表性 DSL
- **THEN** 测试在 900×700 及 100%、125%、150% 缩放下生成单窗口截图，并检查控制区/Surface 隔离、中文、裁剪、滚动和主要操作可达性

### Requirement: 测评问题形成可执行的处置和兜底清单
每次正式测评 SHALL 将问题按 Provider 传输、超时、输出解析、布局语义、Surface 编译/校验、接口导入导出、Renderer、组件几何和视觉呈现分类，并 SHALL 记录影响、频率、复现输入、证据、责任边界、下一步处理和当前兜底。任何测评失败 SHALL NOT 通过放宽封闭 Schema、跳过校验或直接应用未验证 DSL 解决。

#### Scenario: 发现高频模型输出不合法
- **WHEN** 多个样本在相同解析或语义阶段失败
- **THEN** 问题清单优先安排语料、Prompt、Parser 诊断或受控重试方案评估，当前产品继续拒绝无效结果并保留最后有效页面

#### Scenario: 发现长响应或超时
- **WHEN** 平均值、p50 或样本明细表明 Provider 响应较慢或出现超时
- **THEN** 问题清单区分 Provider 时间与本地处理时间，评估超时预算和交互反馈，不自动重复可能仍在执行的请求；用户仍可导入 DSL、导出当前页面或恢复默认排版

#### Scenario: 发现特定 DSL 渲染异常
- **WHEN** 合法 DSL 通过后端导入但在 Qt 渲染、几何或视觉检查中失败
- **THEN** 报告保存最小复现 DSL 和无敏感信息的诊断/截图，将其加入回归语料，并确保交互应用保留最后有效页面
