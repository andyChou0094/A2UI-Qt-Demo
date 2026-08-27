## Why

现有受控工具链已能产出可在 Ubuntu 图形桌面运行的 Qt 5.12.8 ELF，但缺少可重复的本机启动、依赖检查、Linux 后端环境和路径可移植性保障，导致已验证的前后端闭环难以稳定复现。同时，当前前端的信息层级、动态区域边界和不同 DSL 下的自适应表现仍不足，且缺少 SurfaceSpec 文档导入/导出、可重复的大模型质量与延迟测评以及批量渲染回归，无法系统判断“模型输出是否合法、接口是否够快、动态页面是否稳定好看”。后续验证还发现，既有 Ubuntu 入口只凭 ELF 存在、构建目录名和 GCC 版本判断候选合格，可能在源码已经加入导入/导出、单窗口工作台和新主题后仍启动旧二进制；因此本 change 还必须修正启动合同并淘汰相关过时入口、实现证据和文档。

## What Changes

- 新增可重复执行的 Ubuntu 桌面运行入口，自动定位仓库内既有 Qt、ICU 和 ELF，以 `xcb` 启动单一动态 HostShell，并对显示会话、动态库、平台插件、中文字体、后端端口和独立 Linux Python 环境进行启动前检查。
- 隔离运行数据、缓存、临时目录和日志，修复测试产物对 `/workspace` 的固定路径依赖，并提供干净的进程及端口退出语义；继续由受控 GCC 7.3/9.3 工具链负责构建，不把系统 GCC 11 纳入支持矩阵。
- 增加安全的 DeepSeek 运行配置和七场景真实 Provider 验收流程；仅在进程环境中映射密钥，审计产物不记录凭据，已暴露密钥必须轮换后方可重验。
- 为本地 Demo 增加 Git 忽略、权限受限的 Provider 配置文件入口，减少重复环境变量操作；环境变量仍可覆盖本地配置，仓库只提交无密钥示例。
- 将 Qt 编排请求预算与 Provider 调用预算对齐，避免后端最终成功但前端因固定 10 秒超时提前放弃结果。
- 统一面向用户的中文标题、提示、状态和错误说明，同时保持 SurfaceSpec、LayoutPlan、错误码、协议字段及既有英文诊断合同不变。
- 为动态 HostShell 增加宿主自有的静态主题、清晰的编排与文档控制区、一致的交互状态、业务组件视觉边界和紧凑可折叠的诊断区域。
- 将 HostShell 重构为单窗口“受控界面工作台”：以明确分区隔离编排/文档控制与动态 Surface 舞台，优化业务组件在不同合法 Row/Column DSL 下的尺寸、留白、滚动与层级表现。
- 新增前后端 SurfaceSpec JSON 导入与导出接口。导入必须先完成大小、JSON、Schema、Catalog 和语义校验，再事务式应用；任一环节失败均保留最后有效页面。导出必须对当前 Surface 再校验并生成规范化 UTF-8 JSON 文件。
- 新增可信默认 Surface 获取/恢复接口与前端按钮，删除运行时传统固定排版示例窗口；恢复默认也沿用校验后提交和失败不变原则。
- 将真实 Provider 验收扩展为可重复测评体系：首轮固定执行 50 个样本，统计最终 SurfaceSpec 合法率、平均响应时间和 p50，并按传输、超时、解析、语义、编译和校验阶段分类失败。
- 新增固定随机种子的合法 DSL 批量生成和导入渲染回归，覆盖结构边界、组件组合与布局属性，并对代表性动态页面执行几何、缩放和截图检查。
- 生成不含凭据的测评报告和问题清单，按频率、影响、可复现性明确下一步修复与立即可用的保留最后有效页面、恢复默认和错误诊断兜底。
- 移除传统固定窗口及其生产启动路径、截图和测试合同，应用运行后只显示动态 HostShell。
- **BREAKING**：仓库当前没有能保证启动最新前端的正确脚本；删除会默认选择过期 ELF 的 `scripts/run_ubuntu.sh`，新增 `scripts/start_ubuntu_demo.sh` 作为唯一 Ubuntu Demo 入口，不保留兼容包装。
- 新入口复用现有预检、运行环境和退出清理逻辑，只增加简单的构建新旧检查：若当前前端源码、主题资源、默认 Surface 或相关 CMake 文件晚于目标 ELF，则拒绝启动并提示执行既有受控构建；不引入构建清单、来源证明服务或新的工具链层。
- 审查旧入口直接关联的前端/CMake/测试文件；已经确认当前生产入口只创建 HostShell，不存在 LegacyToolboxWindow 源文件，因此不做前端重构，只删除审计后确实无引用的旧代码、旧测试和旧构建/截图产物。
- 更新 `docs/getting-started.md`、`docs/README.md` 及仍直接引用旧命令的 Ubuntu 运行文档，确保启动步骤、重新构建条件和当前单窗口界面一致。
- 保持 Qt 5.12.8、GCC 7.3/9.3、CMake 3.16.9、ISO C++14、既有 Qt 模块与 ABI 基线，不新增布局能力、图形依赖或 LLM 可控的样式与行为权限。

## Capabilities

### New Capabilities

- `ubuntu-desktop-runtime`: 定义 Ubuntu 图形桌面上的依赖发现、启动前检查、独立后端环境、运行数据隔离、可移植测试入口及进程清理行为。
- `host-ui-presentation`: 定义宿主控制的静态主题、单窗口工作台、SurfaceSpec 文档导入/导出与恢复默认、业务组件动态布局呈现、诊断区域和缩放可用性要求。
- `provider-runtime-acceptance`: 定义 DeepSeek 配置、密钥安全映射、50 样本合法率与平均值/p50 延迟测评、批量 DSL 渲染回归、问题分类及无凭据审计要求。

### Modified Capabilities

- 无。既有编排协议、业务 QWidget、Renderer 校验与稳定身份合同保持不变；本 change 通过新增宿主运行和呈现能力约束其集成方式。

## Impact

- 影响 Ubuntu 启动/验收脚本、运行时环境配置、缓存与日志管理、后端 Linux 虚拟环境和测试 Fixture 路径解析。
- 影响 Ubuntu 启动入口及其生命周期测试；`scripts/run_ubuntu.sh` 将被 `scripts/start_ubuntu_demo.sh` 取代，不新增构建系统或公共验证框架。
- 影响动态 HostShell、静态主题、StatusPanel/状态文案、业务 QWidget 的宿主视觉包装、Surface 文档客户端及相关 Qt 前端测试；移除 LegacyToolboxWindow 的生产入口和对照窗口合同。
- 影响 DeepSeek Provider 的运行配置与验收记录生成，但不改变应用读取的 `A2UI_LLM_ENDPOINT`、`A2UI_LLM_MODEL`、`A2UI_LLM_API_KEY` 合同。
- 影响本地运行配置加载、CompositionClient 超时设置和 FastAPI Surface 文档路由；`/compose` 请求/响应、错误码及 Provider endpoint/model 合同保持不变。
- 新增测试语料、固定种子 DSL 生成器、50 样本测评与批量导入渲染报告；报告不得包含凭据，并需明确样本定义、失败分类及后续处置。
- 影响旧入口引用、确认无用的 Legacy 构建/截图产物，以及 `docs/getting-started.md`、`docs/README.md`、`docs/ubuntu-runtime.md`、`docs/toolchain-matrix.md` 中的相关说明；不扩展到无关文档或归档 change。
- 不改变 Catalog、SurfaceSpec/Schema、LayoutPlan、业务 API、五类业务 QWidget 职责、稳定 ID/对象身份语义或核心依赖版本。
