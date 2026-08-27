# host-ui-presentation Specification

## Purpose

定义单窗口动态 HostShell 在不扩大 Agent 权限、不改变业务 QWidget 身份语义的前提下，由宿主应用统一提供的中文表达、SurfaceSpec 文档流转、静态视觉主题、工作区分隔、交互反馈、诊断呈现和缩放可用性合同。

## Requirements

### Requirement: 面向用户的界面使用统一中文
所有面向用户的窗口标题、控件标题、占位提示、运行状态和错误说明 SHALL 使用自然、统一的中文；SurfaceSpec、LayoutPlan、错误码、协议字段以及被测试或客户端依赖的内部英文诊断 SHALL 保持原合同。宿主展示内部英文诊断时 SHALL 同时提供对应的中文说明，而不是改写原始诊断值。

#### Scenario: 展示不支持布局错误
- **WHEN** 编排服务返回 `unsupported_layout` 和既有英文诊断
- **THEN** 界面显示清晰的中文能力限制说明，同时保留原错误码与可展开查看的原始诊断

#### Scenario: 展示后端连接失败
- **WHEN** 本地业务 API 无法连接或请求超时
- **THEN** 相关状态以一致中文说明失败及最后有效数据是否保留，不出现无上下文的英文错误

### Requirement: 主题完全由宿主应用控制
应用 SHALL 使用随程序发布的静态可信主题，由宿主代码决定颜色、字体回退、边框、留白和控件状态；LLM、Component Catalog、LayoutPlan 和 SurfaceSpec SHALL NOT 携带或选择颜色、字体、QSS、像素几何或任意样式。主题 SHALL 兼容 Qt 5.12.8，且 SHALL NOT 依赖透明模糊、重动画、外部字体包或新增图形模块。

#### Scenario: 动态编排改变布局
- **WHEN** 有效 SurfaceSpec 重排业务组件
- **THEN** 新布局继续使用宿主静态主题，且 SurfaceSpec 中不存在任何主题或样式字段

#### Scenario: 收到任意样式字段
- **WHEN** 编排产物尝试携带 QSS、颜色、字体或像素尺寸
- **THEN** 既有封闭校验拒绝该产物，当前已应用界面和宿主主题保持不变

### Requirement: 动态 HostShell 呈现清晰的编排控制台
动态 HostShell SHALL 将自然语言输入、主要编排操作、Surface 文档操作和当前编排状态组织为视觉明确的控制区，并 SHALL 将该控制区与动态 Surface 舞台分隔；请求期间 SHALL 防止重复提交，完成或失败后 SHALL 恢复可继续操作的状态。Surface 舞台 SHALL 显示由宿主计算的当前来源、节点数量和最近应用状态，不得从 DSL 读取装饰或样式。

#### Scenario: 提交编排请求
- **WHEN** 用户输入自然语言指令并开始编排
- **THEN** 控制台立即显示请求中状态、禁用重复提交，并在请求结束后显示成功或失败反馈

#### Scenario: 没有初始 Surface
- **WHEN** 用户在尚无有效初始 Surface 时尝试编排
- **THEN** 控制台显示中文前置条件说明，且不发送无效请求

#### Scenario: Provider 响应超过十秒
- **WHEN** 后端 Provider 调用在其允许预算内完成，但耗时超过旧的固定 10 秒前端阈值
- **THEN** CompositionClient 继续等待并应用最终有效结果，不显示虚假的本地超时；请求预算可由宿主配置且大于 Provider 超时预算

#### Scenario: 区分控制区与动态 Surface
- **WHEN** 单窗口 HostShell 在 900×700 下显示
- **THEN** 编排输入、文档操作与诊断位于独立控制区，动态生成内容位于带来源和状态信息的 Surface 舞台，两者不会被误认为同一内容区域

### Requirement: SurfaceSpec JSON 导入必须先校验再提交
系统 SHALL 提供 `POST /surface/import` 后端接口和前端“导入 DSL”操作，接收 UTF-8 JSON SurfaceSpec 文档；后端 SHALL 对文档大小、JSON 语法、封闭 Schema、Catalog、节点数量、深度和图结构语义执行与编排链路一致的校验。只有后端校验和 Qt Renderer 暂存构建均成功后，前端才 SHALL 更新当前 Surface；任何失败 SHALL 显示结构化中文诊断并保留最后有效页面和当前 Surface JSON。

#### Scenario: 导入合法 SurfaceSpec
- **WHEN** 用户选择未超过 64 KiB 且满足项目全部 SurfaceSpec 合同的 UTF-8 JSON 文件
- **THEN** 后端返回经校验的完整 Surface，前端事务式应用该 Surface，并将其标记为当前可导出页面

#### Scenario: 导入非法或超限文档
- **WHEN** 文件不是合法 JSON、超过 64 KiB、包含未知字段或类型、违反节点/深度/图结构限制，或 Renderer 无法暂存目标组件
- **THEN** 导入返回 `invalid_surface_document` 或等效结构化错误，前端显示失败阶段和诊断，页面、组件状态及当前 Surface JSON 均保持不变

### Requirement: 当前 Surface 可导出为规范化 JSON
系统 SHALL 提供 `POST /surface/export` 后端接口和前端“导出 DSL”操作；前端 SHALL 提交其当前已应用 Surface，后端 SHALL 在导出前再次执行完整校验，并返回 MIME 类型为 `application/json`、UTF-8 编码、带建议文件名且以换行结尾的规范化 JSON。导出 SHALL NOT 包含 QWidget 状态、业务数据、Provider 凭据、诊断日志或宿主主题信息。

#### Scenario: 导出当前动态页面
- **WHEN** 用户在至少存在一个已应用 Surface 时选择“导出 DSL”并确认目标文件
- **THEN** 保存的 JSON 能再次通过 `/surface/import` 并还原相同 SurfaceSpec 结构，且不包含运行时或敏感数据

#### Scenario: 导出校验或文件写入失败
- **WHEN** 当前 Surface 未通过服务端复核、用户取消保存或目标文件无法写入
- **THEN** 界面给出明确结果且不修改当前 Surface，不产生被误认为成功的残缺文件

### Requirement: 用户可恢复可信默认排版
系统 SHALL 以单一受控默认 SurfaceSpec 来源提供 `GET /surface/default` 接口、前端恢复方法和“恢复默认排版”按钮；默认文档 SHALL 经过与导入相同的校验和事务式应用。恢复默认 SHALL 只改变 Surface 布局和实例集合，不重置无关业务数据；失败时 SHALL 保留当前页面。

#### Scenario: 从模型或导入布局恢复默认
- **WHEN** 当前页面已被编排或导入改变且用户确认“恢复默认排版”
- **THEN** HostShell 应用受控默认 Surface、更新当前可导出 JSON 和来源状态，并继续保持单窗口运行

#### Scenario: 默认 Surface 不可用
- **WHEN** 默认文档缺失、无效或 Renderer 无法完成暂存构建
- **THEN** 系统显示恢复失败诊断并保留当前页面，不回退到传统示例窗口或空白窗口

### Requirement: 五类业务组件具有一致且可辨识的视觉层级
`Calculator`、`CalculationHistory`、`CalculationStats`、`Clock` 和 `NotePad` SHALL 在动态 Surface 中具有应用维护的中文标题、清晰边界、一致内边距和层级，并 SHALL 在合法 Row/Column 组合下使用宿主定义的合理最小尺寸、伸缩策略和内容对齐；这些展示信息 SHALL NOT 来自 Catalog 或编排产物，也 SHALL NOT 改变组件职责、业务请求、稳定 ID、QObject 身份或本地状态。

#### Scenario: 初始动态界面包含五类组件
- **WHEN** 初始 Surface 在 900×700 动态窗口中应用
- **THEN** 五类组件可通过中文标题和视觉边界快速区分，主要控件保持清晰可用

#### Scenario: 组件被动态重排
- **WHEN** 有效目标仅改变既有业务组件的位置
- **THEN** 组件标题和视觉容器随组件移动，且原 QWidget 的对象身份、输入、选择、焦点、计时和笔记状态保持不变

#### Scenario: 合法 DSL 产生窄列或深层嵌套
- **WHEN** 批量生成的合法 Row/Column Surface 使业务组件可用宽度变窄或内容超出舞台视口
- **THEN** 组件内部控件仍具有有效几何和可辨识层级，无法同时容纳的内容由 Surface 滚动承接，不出现重叠、负尺寸或不可访问的主要操作

### Requirement: 诊断区域默认紧凑且信息完整
动态 HostShell SHALL 以默认折叠或等效的紧凑方式呈现诊断区域，并在折叠状态保留最新结果摘要；用户 SHALL 能展开查看完整诊断历史。诊断区 SHALL 有上限，SHALL NOT 持续挤占主 Surface 的主要空间。

#### Scenario: 正常编排成功
- **WHEN** 编排目标完成校验并应用
- **THEN** 折叠状态显示简洁成功摘要，主 Surface 不因完整日志内容显著缩小

#### Scenario: 多条诊断需要检查
- **WHEN** 用户展开诊断区域
- **THEN** 界面提供完整的已保留诊断文本和滚动能力，并可再次折叠回紧凑状态

### Requirement: 交互控件具有一致状态反馈
按钮、输入框、列表、滚动区域和状态提示 SHALL 对 hover、focus、disabled、success 与 error 使用一致且可辨识的表现；状态表达 SHALL 不只依赖颜色，并保持足够的文字或边界线索。

#### Scenario: 键盘焦点移动到输入框
- **WHEN** 用户通过键盘将焦点移到自然语言输入框或业务输入控件
- **THEN** 焦点状态具有清晰边界且不依赖动画或透明模糊

#### Scenario: 操作失败
- **WHEN** 编排或业务请求进入失败状态
- **THEN** 界面同时使用中文文本和一致的错误视觉标记，成功与失败不会仅靠相近颜色区分

### Requirement: 中文字体与缩放保持可用
应用 SHALL 优先使用可用的 Noto Sans CJK SC，并在其缺失时使用系统字体回退；在 900×700 窗口及 100%、125%、150% 缩放下，主要控件 SHALL 无明显重叠、关键文本裁剪、方框或乱码，内容超出视口时 SHALL 通过既有滚动宿主访问。

#### Scenario: 主机未安装首选字体
- **WHEN** Noto Sans CJK SC 不可用但系统存在其他中文字体
- **THEN** 应用使用系统回退正常显示中文，而不是阻止窗口启动或显示方框

#### Scenario: 150% 缩放下显示动态窗口
- **WHEN** 900×700 动态窗口在 150% 缩放下显示初始 Surface
- **THEN** 编排控制台和主要业务控件不重叠，超出空间的内容可通过滚动访问

### Requirement: 应用只显示单一动态工作台
正常运行和截图冒烟 SHALL 只创建并显示动态 HostShell，不再创建传统固定排版示例窗口。原固定窗口的默认布局价值 SHALL 由受控默认 SurfaceSpec 与“恢复默认排版”操作替代。

#### Scenario: 启动桌面应用
- **WHEN** Ubuntu 运行入口完成预检并启动 Qt 应用
- **THEN** 用户只看到一个包含控制区和动态 Surface 舞台的 HostShell，不出现第二个 LegacyToolboxWindow

#### Scenario: 执行截图冒烟
- **WHEN** 测试启用非交互截图模式
- **THEN** 只生成动态工作台及所选代表性 DSL 的截图，不再要求传统窗口截图产物
