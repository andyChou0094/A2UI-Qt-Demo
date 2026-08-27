> 任务 1–14 记录已经完成的实现；任务 15–17 是发现旧 Ubuntu 入口仍可启动过期 ELF 后追加的精简修正规划。新增任务尚未 apply。

## 1. Ubuntu 运行基础与路径隔离

- [x] 1.1 新增 Ubuntu 运行入口的参数解析与仓库根、受控 ELF 候选发现，支持仅预检和显式构建目录，并在缺少合格 ELF 时给出受控构建指引
- [x] 1.2 实现 XDG cache/data/state 目录解析，按 Qt/ICU 版本与来源摘要管理派生缓存，保证不写入源码树且不触碰仓库 Windows `.venv`
- [x] 1.3 实现离线 Qt 5.12.8/ICU 56 资源的校验、缓存命中和临时解压后原子更新，不调用额外 Qt `.run` 安装器
- [x] 1.4 实现启动前检查：ELF/架构、动态库、`platforms/libqxcb.so`、`DISPLAY`、中文字体、Linux Python 环境、回环端口和必需配置，失败时返回可操作的中文诊断
- [x] 1.5 为运行入口添加自动化 shell 测试或可替换探针，覆盖完整预检、缺少显示、无合格 ELF、损坏缓存和外部端口占用

## 2. 可移植测试资源与回归入口

- [x] 2.1 新增 Qt 测试共享源码根解析器，按 `A2UI_TEST_SOURCE_ROOT`、当前目录/可执行文件向上发现、编译路径回退的顺序验证仓库根
- [x] 2.2 将 Surface Fixture 和 `docs/llm-acceptance-results.json` 的 Qt 测试读取点迁移到共享解析器，并增加非 `/workspace` 路径测试
- [x] 2.3 新增 Ubuntu 回归入口，在当前仓库根执行 Qt 11 项、后端 31 项和源码策略检查且不依赖 `/workspace` 映射
- [x] 2.4 在 GCC 7.3.0 与 9.3.0 受控构建中验证 C++14、Qt 5.12.8、ABI、Qt 模块和既有测试覆盖未被放宽

## 3. 后端、Qt 启动与生命周期

- [x] 3.1 按 Python 版本与 requirements 摘要在 XDG cache 创建或复用独立 Linux venv，并验证仓库 `.venv` 在准备前后未变化
- [x] 3.2 实现统一 FastAPI 后端的回环绑定、XDG data SQLite 路径、XDG state 日志和 `/health` 就绪等待，健康检查失败时阻止 Qt 启动
- [x] 3.3 实现仅向子进程注入 `QT_QPA_PLATFORM=xcb`、插件路径、库路径和可信本地 API 配置，然后启动同时包含动态与传统窗口的既有 ELF
- [x] 3.4 实现 PID/启动标识校验、信号 trap、分级退出与端口释放检查，只清理入口拥有的进程
- [x] 3.5 增加集成验收，覆盖后端健康、SQLite CRUD/摘要、Qt 轮询联动、XWayland 可交互启动以及正常/中断退出后无遗留进程和端口

## 4. 宿主静态主题与业务组件视觉层级

- [x] 4.1 新增编译进应用的静态 QSS 资源和主题初始化模块，设置深海军蓝/青蓝视觉语言及 hover、focus、disabled、success、error 状态，加载失败时回退系统样式
- [x] 4.2 实现 Noto Sans CJK SC 优先、系统字体回退的字体选择，并加入中文渲染可用性检查而不打包外部字体
- [x] 4.3 为五类共享业务 QWidget 增加固定中文标题、可信 objectName/状态属性和一致卡片边界，确保动态与传统窗口继续复用同一类
- [x] 4.4 更新业务组件和 Reconciler 测试，验证标题/主题不改变业务请求、稳定 ID、QObject 身份、输入、选择、焦点、计时和笔记状态

## 5. HostShell 中文控制台与诊断体验

- [x] 5.1 重构 HostShell 顶部为固定“编排控制台”，统一就绪、请求中、成功、失败中文状态和防重复提交行为
- [x] 5.2 实现默认折叠的诊断区、最新中文摘要、限高完整历史和可键盘操作的展开/折叠控件
- [x] 5.3 建立宿主维护的错误码/已知英文诊断到中文说明的展示映射，在详情中保留原错误码和原始诊断文本
- [x] 5.4 更新传统窗口以应用相同基础主题和业务卡片，同时验证其固定布局且不出现编排入口
- [x] 5.5 扩展 HostShell Qt 测试，覆盖控制台状态、折叠诊断、错误详情、最后有效 Surface 保留和滚动宿主行为
- [x] 5.6 在 900×700 及 100%、125%、150% 缩放下执行几何/截图冒烟，检查关键中文无方框、乱码、裁剪或重叠且溢出内容可滚动

## 6. DeepSeek 安全配置与七场景验收

- [x] 6.1 在 Ubuntu 子进程边界实现 `deepseek_api` 到 `A2UI_LLM_API_KEY` 的非回显临时映射，保持应用三环境变量合同且不读取 `.env` 或持久化密钥
- [x] 6.2 加固 `scripts/run_llm_acceptance.py` 的报告生成，加入当前密钥、Authorization/Bearer 模式和敏感字段扫描，并使用临时文件通过门禁后原子写入目标
- [x] 6.3 扩展验收 runner 单元测试，覆盖配置缺失、报告疑似含凭据、任一场景失败、`unsupported_layout` 和 7/7 汇总判定
- [x] 6.4 在操作者提供轮换后的密钥后，以固定 endpoint 和 `deepseek-v4-flash` 重新运行七场景，保存不含凭据的审计结果；没有新密钥时明确记录未验收且不复用旧结果
- [x] 6.5 使用新审计结果重跑 Qt Reconciliation 身份测试，确认六个有效更新保持对象状态且不支持布局保留最后有效界面

## 7. 最终验证与文档

- [x] 7.1 运行源码策略检查并核对 Catalog、Schema、协议字段、业务 API、Qt 模块和核心依赖版本没有变化
- [x] 7.2 执行 Ubuntu 预检、完整回归、生命周期集成、三档缩放冒烟和 Provider 验收门禁，汇总每项通过/未执行原因且不记录凭据
- [x] 7.3 更新 Ubuntu 运行、缓存失效、故障排查、退出清理和安全 Provider 重验文档，所有示例仅使用密钥占位符

## 8. Demo 配置简化与超时修复

- [x] 8.1 新增 Git 忽略、`0600` 权限的本地 Provider 配置和隐藏输入配置脚本，运行入口仅解析白名单字段且环境变量优先
- [x] 8.2 将 Provider 默认超时和 Qt CompositionClient 请求预算调整为 90/120 秒并增加配置、权限、优先级与超时回归测试
- [x] 8.3 验证固定 API 的最小请求与完整 `/compose` 链路，确认超时修复后前端成功应用，并更新 Demo 操作文档与验证结论

## 9. SurfaceSpec 文档接口与默认排版

- [x] 9.1 将硬编码初始 Surface 提取为单一版本控制 JSON 源，并同时接入 Qt 只读 resource 与后端默认 Surface 加载路径
- [x] 9.2 复用服务端封闭校验实现 `POST /surface/import`，限制 64 KiB UTF-8 JSON，并统一返回 `invalid_surface_document` 结构化诊断
- [x] 9.3 实现 `POST /surface/export`，对当前 Surface 复核后返回带建议文件名、稳定缩进和末尾换行的 `application/json` 响应
- [x] 9.4 实现 `GET /surface/default`，启动时校验默认文档并保证接口结果与 Qt resource 中的默认 Surface 一致
- [x] 9.5 增加后端接口测试，覆盖合法文档、非法 JSON、未知字段/类型、节点与深度边界、图结构错误、超限文档、导入导出往返和默认文档一致性
- [x] 9.6 新增 Qt Surface 文档客户端与导入/导出/恢复默认操作，导入只在服务端校验和 Renderer 暂存均成功后更新 `currentSurfaceJson_`
- [x] 9.7 使用 `QSaveFile` 实现原子导出，并增加用户取消、网络失败、校验失败、写入失败和恢复默认失败时页面不变的 Qt 测试

## 10. 单窗口受控界面工作台

- [x] 10.1 使用可调 `QSplitter` 将 HostShell 重构为编排/文档控制栏与动态 Surface 舞台，加入来源、surfaceId、节点数和最近应用状态元数据
- [x] 10.2 将主题 token 更新为冷灰画布、墨黑控制栏、纸白舞台、指令蓝、成功绿和错误红，并实现标题、正文、数据三种可信字体角色与中文回退
- [x] 10.3 为编排、导入、导出、恢复默认、诊断展开和请求状态补齐一致的 hover、focus、disabled、busy、success、error 反馈及键盘顺序
- [x] 10.4 优化五类业务 QWidget 的 minimumSizeHint、size policy、内部 stretch、窄宽度排列和内容对齐，确保不改变业务 API、稳定 ID、QObject 身份和本地状态
- [x] 10.5 扩展 HostShell 和业务组件测试，覆盖双区分隔、Surface 元数据、文档按钮状态、组件窄列/嵌套几何、滚动可达和最后有效页面保留

## 11. 移除传统示例窗口

- [x] 11.1 从 Qt 主程序、生产 CMake source 和应用头文件依赖中移除 LegacyToolboxWindow，正常运行只创建一个 HostShell
- [x] 11.2 删除或迁移只验证传统窗口的测试，将仍有价值的默认组件组合断言迁移到默认 Surface 与动态 Renderer 测试
- [x] 11.3 更新截图冒烟、Ubuntu 生命周期测试和运行文档，只等待并捕获单一动态工作台，不再生成 `legacy-toolbox.png`
- [x] 11.4 验证关闭唯一 HostShell 后 Qt、后端子进程和端口均按原所有权规则清理

## 12. 五十样本 Provider 合法率与延迟基线

- [x] 12.1 建立版本化测评语料和 50 个稳定样本 ID，覆盖左右、上下、侧栏、嵌套、权重、重复组件、空 Surface 与不支持布局意图
- [x] 12.2 实现串行 Provider 基线 runner，使用单调时钟记录 Provider 与 `/compose` 端到端耗时，并分类传输、超时、解析、语义、编译和最终校验失败
- [x] 12.3 以 50 为固定分母计算最终 DSL 合法率，分别输出全部样本及成功样本的平均响应时间和 p50，本轮只输出这两个延迟指标
- [x] 12.4 在不足 50 个最终样本时标记基线未完成，保存已完成数量和原因但不覆盖正式基线；运行前明确显示请求次数与成本提示
- [x] 12.5 复用凭据扫描与原子写入生成样本级审计报告，并增加合法率、平均值、p50、失败阶段、外部中断和敏感字段的单元测试

## 13. 合法 DSL 批量导入与动态渲染回归

- [x] 13.1 实现固定随机种子且无新增依赖的合法 SurfaceSpec 生成器，输出 seed 与覆盖矩阵并由服务端 Validator 自检全部样本
- [x] 13.2 覆盖根叶子/空容器、Row/Column 嵌套、节点数与深度边界、五类组件、允许重复组件及 gap、align、justify、weight 合法组合
- [x] 13.3 编写后端批量脚本逐个调用 `/surface/import`，断言所有生成样本返回完整合法 Surface 且不产生服务端错误
- [x] 13.4 编写 Qt 批量测试在同一 HostShell 依次应用导入结果，断言组件类型/数量、有效几何、无主要控件重叠、滚动可达、无崩溃和关键 Qt 告警
- [x] 13.5 为结构类别和边界条件选择代表性 DSL，在 900×700 及 100%、125%、150% 缩放下生成单窗口截图并检查控制区隔离、中文、裁剪和操作可达性

## 14. 问题闭环与最终验证

- [x] 14.1 在轮换密钥和固定运行参数下先重跑七场景门禁，再执行正式 50 样本基线并保存不含凭据的合法率、平均值、p50 和失败明细
- [x] 14.2 汇总 Provider、接口、Renderer、组件几何和视觉问题，记录频率、影响、最小复现、证据、责任边界、下一步处理和当前兜底
- [x] 14.3 将已确认缺陷的最小复现 DSL 或 Prompt 纳入固定回归语料，禁止通过放宽 Schema、跳过校验或直接应用未验证 DSL 获得通过
- [x] 14.4 执行两套 GCC 构建、后端/Qt 完整回归、源码策略、Ubuntu 单窗口生命周期、文档往返、批量 DSL 渲染和三档缩放检查
- [x] 14.5 更新 Demo 操作、Surface 文档格式、评测复现、指标口径、问题清单和兜底文档，并明确当前 change 已从 35/35 重新进入待实现状态

## 15. 替换启动入口

- [x] 15.1 以现有 `scripts/run_ubuntu.sh` 为基础新增 `scripts/start_ubuntu_demo.sh`，保留现有预检、XDG、Qt/ICU、Provider、后端生命周期和参数行为，不新增公共框架或自动构建逻辑
- [x] 15.2 在新入口中检查目标 ELF 是否早于 `frontend/app`、`frontend/composition`、`frontend/widgets`、默认 Surface 或相关 CMake 文件；发现旧构建时停止并提示执行既有受控构建
- [x] 15.3 将两项 runtime 测试切换到新入口并覆盖“当前构建可启动、前端文件更新后旧 ELF 被拒绝”，随后删除 `scripts/run_ubuntu.sh`，不保留兼容包装

## 16. 删除确认无用的旧内容

- [x] 16.1 再次搜索前端源码、CMake 和测试中的 `LegacyToolboxWindow`/旧入口引用；当前 HostShell、Theme、SurfaceDocumentClient、Renderer 和业务 QWidget 保持不动
- [x] 16.2 只删除搜索确认无调用的旧窗口源码、专用测试或 CMake source；若 Legacy 仅存在于旧 ELF、CTest 或 `legacy-toolbox.png`，只清理这些旧生成物
- [x] 16.3 使用当前源码完成一次受控构建，通过新入口启动并确认单一 HostShell、导入/导出/恢复默认按钮、新主题和退出清理正常

## 17. 更新启动文档

- [x] 17.1 更新 `docs/getting-started.md` 和 `docs/README.md`，只使用 `scripts/start_ubuntu_demo.sh`，写清旧构建被拒绝后的受控重建与启动步骤
- [x] 17.2 更新仍直接引用 `scripts/run_ubuntu.sh` 的 `docs/ubuntu-runtime.md`、`docs/toolchain-matrix.md`；其他文档只在搜索到相同错误说明时修改
- [x] 17.3 全仓搜索非归档旧命令、双窗口当前态和错误启动说明，确认没有残留引用，并将真实界面验收结果写入现有验证文档
