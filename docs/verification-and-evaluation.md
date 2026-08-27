# 验证与评测报告

更新日期：2026-08-25  
当前审计批次：2026-08-21（Asia/Shanghai）

本文是当前项目测试数字、Provider 验收、问题与兜底的唯一完整信息源。

## 结论摘要

- 受控 GCC 7.3.0 和 9.3.0 环境中，frontend CTest 均为 12/12；backend unit
  37/37、integration 14/14，真实 uvicorn Surface 文档回环 5/5。
- Provider 七场景验收为 7/7：6 个受支持场景生成预期 Surface，1 个不支持场景
  正确返回 `unsupported_layout`。
- Provider 基线 50/50 完成，其中 40 个最终得到可校验 SurfaceSpec。`40/50`
  只是技术分类，不是产品通过率。
- 10 个没有得到可提交 Surface 的样本包含 1 次 Provider 传输失败和 9 次按封闭合同
  受控拒绝。`PB-048` 则是一份技术合法但未满足样式意图的语义 no-op。
- 已确认的产品风险集中在 Provider 长尾响应、前后端超时预算和意图判定；封闭
  Schema、服务端/Qt 双重校验与最后有效页面保护均保持有效。

这些结论证明 Demo 范围内的受控组件编排可行，不代表真实存量 QWidget、生产权限、
网络或供应链已完成认证。

## 用户看到的验证边界

| 部分 | 用户视角下的作用 | 本轮验证重点 |
|---|---|---|
| Qt 前端 | 显示控制区、动态 Surface 和诊断，运行五类业务组件 | 独立校验、原子应用、稳定 ID 复用、几何和生命周期 |
| 本地 FastAPI 后端 | 在后台编译布局、校验 Surface，并提供导入/导出与计算记录 API | 封闭合同、错误码、文档回环和数据隔离 |
| Provider | 将自然语言转成受限 LayoutPlan | 七场景意图断言、50 样本分类、响应时间与异常 |
| SQLite | 保存 Calculator 产生的计算记录，供 History/Stats 查询 | CRUD、重启持久化与三组件后端介导联动 |

Provider 或网络失败不会阻止用户继续使用已显示页面、计算器、DSL 导入/导出和
恢复默认。

## 术语与指标口径

- **测试通过**：自动化用例的全部断言满足。它只覆盖该用例明确声明的合同。
- **DSL 合法**：SurfaceSpec 通过封闭字段、Catalog、图结构、节点/深度和布局规则校验。
- **意图满足**：实际结构或错误码符合 Prompt 的可验收期望。DSL 合法是必要但不充分条件。
- **按设计拒绝**：请求越过 Row/Column、Catalog 或安全能力边界时，系统返回预期错误，
  不产生可提交 Surface，并保留最后有效页面。这是成功的安全行为，不是 Renderer 失败。
- **中位数 / p50**：排序后位于中间的响应时间；50 个样本时取第 25 和第 26 个值的平均。
- **最慢样本 / max / worst-case**：样本集中实测时间最大的单个样本，不能用 p50 代替。

## 基础测试和运行验证

| 范围 | 结果 | 覆盖与说明 |
|---|---:|---|
| GCC 7.3.0 frontend CTest | 12/12 | Qt 5.12.8、C++14、ABI=1，当前源码重新配置构建 |
| GCC 9.3.0 frontend CTest | 12/12 | 同上；不使用历史 Legacy 构建结果 |
| Backend unit | 37/37 | Validator、Compiler、Repository、runner 与共享合同 |
| Backend integration | 14/14 | `/compose`、Surface 文档、计算 API 和整体 app |
| 真实 uvicorn 文档回环 | 5/5 | default、import、export、错误结构和大小边界 |
| Surface 文档 | 通过 | 合法导入、规范化导出/重导入、恢复默认，非法 JSON/字段/图/超 64 KiB 拒绝 |
| 固定 seed DSL | 25/25 | seed `20260821`；服务端 Validator、`/surface/import` 和同一 HostShell 批量渲染 |
| 界面与缩放 | 通过 | 900×700；5 类代表 Surface × 100%/125%/150% = 15 张截图 |
| Ubuntu 生命周期 | 通过 | Wayland/XWayland 下 `xcb` 单 HostShell，关闭后只清理自有进程并释放端口 |

纯工具链矩阵中，CompositionClient 依赖外部真实回环服务的条件 case 被跳过；它已在独立真实
`/compose` 回环门禁通过。受限 sandbox 不支持 TestClient 需要的进程内通信：系统 Python
可表现为 1 项执行、13 项按条件 skip，完整依赖环境可在首个 HTTP 用例等待。这些是
环境结果，不计入产品通过或失败。受控环境的 14/14 是 integration 结论。

## Provider 七场景验收

审计时间为 2026-08-21 16:10（Asia/Shanghai），endpoint 为 DeepSeek Chat Completions，
model 为 `deepseek-v4-flash`。`apiKeyRecorded=false`，敏感信息扫描通过。

| # | Prompt | 期望 | 实际结构/错误码 | 稳定 ID 行为 | 通过依据 |
|---:|---|---|---|---|---|
| 1 | 把计算器放左边，历史记录放右边 | `Row(Calculator,History)` | `Row(Calculator,CalculationHistory)` | 复用 `calculator-main`、`history-main` | 结构、类型、ID 断言全部通过 |
| 2 | 计算器在上，历史记录在下 | `Column(Calculator,History)` | `Column(Calculator,CalculationHistory)` | 复用两个原 ID | 结构、类型、ID 断言全部通过 |
| 3 | 统计和时钟做左侧栏，其余放右侧 | 两列嵌套 Column | `Row(Column(Stats,Clock),Column(Calculator,History))` | 复用 4 个原 ID | 结构与业务 ID 集一致 |
| 4 | 只用 Row 和 Column 做成上下两行、每行两个区域的 2×2 面板 | `Column(Row(...),Row(...))` | `Column(Row(Calculator,Stats),Row(History,NotePad))` | 复用 4 个原 ID | Row/Column 子集、顺序和 ID 均符合 |
| 5 | 计算器宽度约为历史记录两倍 | 权重 2:1，`justify=start` | `Row(Calculator,History)`，权重 2:1 | 复用两个原 ID | 权重、主轴约束和 ID 断言通过 |
| 6 | 再添加一个独立计算器 | 两个 Calculator，保留原 Calculator | `Column(Calculator,History,Calculator)` | 复用原 ID，Compiler 分配 `calculator-1` | 叶子数、类型、复用与新 ID 均符合 |
| 7 | 用 Grid，让历史记录跨两列并支持响应式断点 | `unsupported_layout` | `unsupported_layout`：Grid 不可表达 | 未生成 Surface，原 ID 集不变 | 预期拒绝码和页面保护断言通过 |

因此 `7/7 = 6 个受支持场景成功 + 1 个不支持场景正确拒绝`，不是 7 份任意
Prompt 都产生 Surface。详细原始响应、编译结果和 checks 位于
[`llm-acceptance-results.json`](llm-acceptance-results.json)。

## Provider 50 样本基线

审计时间为 2026-08-21 16:21（Asia/Shanghai）；temperature 0，Provider 超时 90 秒，
串行执行。以原始 JSON 重算得到：

| 范围 | 样本数 | Provider 平均 | Provider 中位数 | Provider 最慢 | `/compose` 平均 | `/compose` 中位数 | `/compose` 最慢 |
|---|---:|---:|---:|---|---:|---:|---|
| 全部样本 | 50 | 11.499s | 5.342s | `PB-049` 68.481s | 11.500s | 5.343s | `PB-049` 68.483s |
| 合法 Surface | 40 | 8.088s | 5.009s | `PB-018` 41.189s | 8.090s | 5.011s | `PB-018` 41.191s |

计数和分类：

- 请求 50，最终样本 50，审计状态 `complete`。
- 40 份最终通过 SurfaceSpec 校验；它们的单文件证据见
  [`evaluation-data/provider-valid-surfaces/`](evaluation-data/provider-valid-surfaces/)。
- 失败阶段为 transport 1、semantic 9、timeout 0、parse 0、compilation 0、final validation 0。
- 该 runner 没有为每个受支持 Prompt 执行完整意图断言，因此不把 40/50 重新命名为
  自动化产品通过率。

### 异常和边界样本逐例说明

| 样本 | Prompt | 结果与边界 | 证据 |
|---|---|---|---|
| `PB-008` | 上方放计算器，下方放历史 | 21.050s 后 Provider `connection refused`，`llm_provider_error`；无 LayoutPlan | [数据索引](evaluation-data/README.md#没有-json-输出的-pb-008) |
| `PB-041` | 使用 Grid 排成三列 | `unsupported_layout`：Grid | [LayoutPlan](evaluation-data/rejected-layout-plans/PB-041.json) |
| `PB-042` | 让历史记录跨两列 | `unsupported_layout`：Grid | [LayoutPlan](evaluation-data/rejected-layout-plans/PB-042.json) |
| `PB-043` | 按窗口宽度自动响应式换行 | `unsupported_layout`：Wrap | [LayoutPlan](evaluation-data/rejected-layout-plans/PB-043.json) |
| `PB-044` | 把计算器浮在历史记录上方 | `unsupported_layout`：Overlay | [LayoutPlan](evaluation-data/rejected-layout-plans/PB-044.json) |
| `PB-045` | 用可拖动 Dock 停靠统计面板 | `unsupported_layout`：Dock | [LayoutPlan](evaluation-data/rejected-layout-plans/PB-045.json) |
| `PB-046` | 用 Splitter 让模型控制分隔条 | `unsupported_layout`：Splitter | [LayoutPlan](evaluation-data/rejected-layout-plans/PB-046.json) |
| `PB-047` | 让组件自动 Wrap 到下一行 | `unsupported_layout`：Wrap | [LayoutPlan](evaluation-data/rejected-layout-plans/PB-047.json) |
| `PB-048` | 给页面设置蓝色背景和自定义字体 | 合法 Column，但未实现或拒绝样式意图；语义 no-op | [SurfaceSpec](evaluation-data/provider-valid-surfaces/PB-048.json) |
| `PB-049` | 添加一个点击后执行脚本的按钮 | `invalid_layout_plan`：Catalog 无 `Button`；同时是全样本最慢 | [LayoutPlan](evaluation-data/rejected-layout-plans/PB-049.json) |
| `PB-050` | 使用 Grid、跨列和响应式断点 | `unsupported_layout`：Grid | [LayoutPlan](evaluation-data/rejected-layout-plans/PB-050.json) |

`PB-041`–`PB-047`、`PB-049`、`PB-050` 是受控拒绝，不是“错误 DSL”或 Renderer 缺陷。
`PB-018`（“右侧再分成上下两个区域”）是合法 Surface 中最慢样本，证据为
[`PB-018.json`](evaluation-data/provider-valid-surfaces/PB-018.json)。

## 已确认问题、责任边界与处置

### EVAL-01：Provider 传输失败

- **状态**：观察中。
- **频率**：1/50。
- **复现输入**：`PB-008`，“上方放计算器，下方放历史”。
- **原因/责任边界**：Provider 或网络传输在约 21.050 秒后 `connection refused`；尚未进入
  LayoutPlan Parser、Compiler、Validator 或 Renderer。
- **影响**：该请求不产生新 Surface，用户本次编排未完成；其他页面和本地功能不受影响。
- **当前兜底**：返回结构化 `llm_provider_error`，无效结果不提交，保留最后有效页面；
  不自动重试可能仍在执行的请求。
- **后续建议**：继续按阶段记录网络、限流和 Provider 5xx，根据幂等性与请求状态再设计
  用户主动重试，不盲目自动重放。

### EVAL-02：样式指令被处理为语义 no-op

- **状态**：待优化。
- **频率**：1/10 个明确不支持意图，即 `PB-048`。
- **复现输入**：“给页面设置蓝色背景和自定义字体”。
- **原因/责任边界**：Provider Prompt/意图判定没有将样式请求转成明确拒绝；Schema 和
  Validator 正确禁止样式字段，所以产生了安全但未满足意图的 Column。
- **影响**：页面不会被注入颜色、字体或 QSS，但用户可能误以为请求成功。
- **当前兜底**：宿主主题不变；样本单独标注为 no-op；固定回归语料中保留 `PR-001`。
- **后续建议**：在封闭 DSL 技术校验之上增加 Prompt 分类和意图级断言，对不支持样式返回
  明确错误；不向 DSL 开放颜色、字体或 QSS。

### EVAL-03：Provider 长尾响应与超时预算

- **状态**：已确认长尾，待系统性优化。
- **频率**：本轮全样本最慢 68.481 秒，成功样本最慢 41.189 秒；当前 90 秒 Provider
  预算内没有 timeout 分类。
- **复现输入**：`PB-049` 是全样本最慢；`PB-018` 是成功样本最慢。
- **原因/责任边界**：Provider 时间与 `/compose` 总时间几乎一致，本地 Parser/Compiler/
  Validator 不是主要耗时。
- **影响**：长时间等待可能使用户误判卡死；若 Qt `CompositionClient` 预算小于后端/
  Provider 预算，前端会在后端仍可完成时提前中止。
- **当前兜底**：请求期间显示等待状态；失败或超时时保留最后有效页面；不盲目重试。
- **后续建议**：同时观察平均、中位数、最慢样本和阶段耗时；使 Qt 预算严格大于后端
  `/compose` 预算，后者再覆盖 Provider 预算与本地处理余量；超时后返回可区分状态。

## 按设计工作的拒绝和未发现回归

9 份被拒绝 LayoutPlan 停在 Parser/Compiler 前后的受控边界，没有进入 Qt 提交。不应通过
近似 Row/Column、添加未知组件、放宽 Schema 或跳过 Validator 来提高表面成功数。

本轮在以下范围未确认产品失败：Surface 导入/规范化导出/恢复默认、Renderer 暂存与
原子提交、固定 seed 合法 DSL 批量渲染、主要业务控件几何与滚动可达性、三档缩放以及
单窗口 XWayland/xcb 生命周期。

## 统一失败保护策略

1. Provider、Parser、Compiler、服务端 Validator 或 Qt Validator 任一失败，无效结果不提交。
2. Renderer 先完成校验和暂存，只在目标可提交时更换活动树；失败保留最后有效页面与
   已复用 QWidget 状态。
3. 慢请求显示明确状态；客户端不盲目重试可能仍在后端执行的请求。
4. 用户始终可导出当前 Surface、导入已知合法 DSL 或恢复受控默认排版。
5. 新的确定性缺陷保存最小 Prompt 或 DSL、错误阶段和无敏感信息的诊断，再加入固定回归。

## 后续优化方向

### 当前数据集覆盖

测试输入来自仓库中版本化的脚本或 JSON，不由 Provider 在运行时改写目标和判定规则。当前四类数据
各自回答不同问题：

- **七场景意图验收**：[`scripts/run_llm_acceptance.py`](../scripts/run_llm_acceptance.py) 固定 Prompt、
  起始 Surface 和预期结构/错误码，直接断言左右、上下、侧栏、2×2、2:1 权重、重复 Calculator、
  稳定 ID 与越界拒绝；它是当前最强的 Provider 意图证据，但场景数量有限。
- **50 样本 Provider 基线**：
  [`provider-baseline-v1.json`](../shared/evaluation/provider-baseline-v1.json) 用稳定 `PB-xxx` ID 覆盖
  8 类受支持 Prompt（每类 5 条）和 10 条越界 Prompt，记录原文、第一失败阶段、错误码、最终 Surface
  合法性和耗时；多数受支持样本没有完整的逐条意图断言。
- **固定 seed SurfaceSpec**：[`generate_legal_surfaces.py`](../scripts/generate_legal_surfaces.py) 以 seed
  `20260821` 生成 25 份合法 DSL，覆盖 Row/Column、五种业务类型、枚举属性、重复/空容器、根叶、
  32 节点和 8 层深度。它证明确定性合同可校验、导入和批量渲染，不证明自然语言生成稳定性。
- **合同 Fixture**：[`shared/fixtures/`](../shared/fixtures/) 固定合法 LayoutPlan、合法 SurfaceSpec 和
  至少 11 类非法 SurfaceSpec，供 Parser、Compiler、Python/Qt Validator 和 Renderer 重复验证。

因此必须分别判断合同合法性、Catalog 能力边界、业务意图、稳定身份、提交/回退和性能；“得到合法
JSON”不能替代其他结论。语料、Fixture、预期结果或生成规则改变时，应提升 `corpusVersion` 或建立
新版本，不能覆盖已用于审计的语义。

### 主要不足

- 50 样本对组件顺序、嵌套、权重、增删和稳定 ID 的意图断言不足；`PB-048` 已证明技术合法可能
  仍是语义 no-op。
- Prompt 以中文短句为主，缺少噪声、歧义、冲突约束、中英混合、多语言、注入、伪造 Catalog、
  脚本/URL/样式诱导等 Provider 层反例。
- 状态迁移未系统覆盖“成功→失败→恢复→再成功”、重复组件多次增删和不同前置 Surface；节点数、
  深度、weight 0/10、64 KiB 等边界也未形成合法/非法成对追踪。
- temperature 0 下每条只采样一次，无法估计概率稳定性或把网络波动与模型推理波动分离。
- 尚无代表性真实存量 QWidget 数据；生命周期、ABI、第三方依赖和 Service/API 权限仍未认证。
- 视觉质量主要依靠代表性截图和人工观察，尚无对不可读尺寸、极端比例、溢出和留白失衡的可执行判定。

这些缺口限制结论范围，但不会把当前已经通过的确定性合同测试改写为失败，也不能用样本总数包装成
覆盖率或产品通过率。

### 后续数据集设计

优先扩充能回归已知风险的最小集合，再由新业务和失败证据决定是否引入系统化追踪或门禁：

1. 为 50 样本中的支持意图补齐结构、顺序、权重、增删和稳定 ID 断言；保存
   [`PR-001/PB-048`](../shared/evaluation/provider-regressions-v1.json) 等最小缺陷回归。
2. 为 Parser、Compiler、Python Validator、Qt Validator 和 Renderer 成对增加字段、图不变量及
   上下边界 Fixture；补充成功/失败/恢复状态迁移，验证失败后仍可继续提交。
3. 在固定语料版本上加入同义变形、噪声、歧义、多语言与安全反例；对 Provider 重复采样并报告
   意图满足、受控拒绝、非法输出、语义 no-op、响应时间分位数和失败阶段。
4. 选择少量代表性旧 QWidget，记录 GUI 线程、parent/reparent、QObject ownership、状态/焦点、
   尺寸、ABI、依赖和业务权限前提；针对真实缺陷建立组件兼容回归，而非预设完整分层框架。
5. 为标准窗口和缩放组合保存几何测量与最小可执行视觉阈值；只有持续缺陷证明必要时，再扩展布局
   元数据或质量判定。

当核心支持意图已有语义断言、已知缺陷已有回归、安全边界已有反例后，再考虑需求—风险—用例追踪
与 CI 门禁。本 change 不新增 runner、不修改语料、不重跑 Provider，也不把 2026-08-21 历史审计
包装为新的通过率。

## 复现命令和证据限制

不调用 Provider 的本地回归：

```bash
pwsh ./tools/build-matrix.ps1
python3 -m unittest discover -s backend/tests/unit -v
python3 -m unittest discover -s backend/tests/integration -v
python3 scripts/generate_legal_surfaces.py --seed 20260821 --output /tmp/legal-surfaces.json
python3 scripts/run_surface_import_batch.py --seed 20260821
scripts/run_ubuntu_regression.sh build-gcc-9.3.0
tests/runtime/test_start_ubuntu_demo.sh
tests/runtime/test_runtime_lifecycle.sh
```

Provider 命令会发生真实外部请求，必须使用已轮换密钥并明确承担次数/费用：

```bash
deepseek_api='<rotated-provider-key>' \
A2UI_KEY_ROTATED=1 \
scripts/run_provider_acceptance.sh

A2UI_LLM_ENDPOINT='https://api.deepseek.com/chat/completions' \
A2UI_LLM_MODEL='deepseek-v4-flash' \
A2UI_LLM_API_KEY='<rotated-provider-key>' \
python3 scripts/run_provider_baseline.py --confirm-50-requests
```

长期保留的证据与语料：

- 七场景审计：[`llm-acceptance-results.json`](llm-acceptance-results.json)。
- 50 样本审计：[`provider-baseline-results.json`](provider-baseline-results.json)。
- 单样本派生证据：[`evaluation-data/`](evaluation-data/README.md)。
- Provider 语料和回归：[`../shared/evaluation/`](../shared/evaluation/)。
- SurfaceSpec/LayoutPlan 合同 Fixture：[`../shared/fixtures/`](../shared/fixtures/)。

两份审计 JSON 均记录 `apiKeyRecorded=false`，不包含可用密钥。Provider 输出具有概率性，新一次
运行必须重新满足断言，不能用旧结果代替实际调用。`/tmp` 中的构建、日志和截图是临时证据，
重启或清理后不可依赖。需要正式留档时，应在仓库外审计位置记录源码版本、工具链、命令、
时间和输出位置。
