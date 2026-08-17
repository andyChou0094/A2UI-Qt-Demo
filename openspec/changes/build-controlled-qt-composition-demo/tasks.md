## 1. 工具链与项目骨架

- [ ] 1.1 建立 Qt 5.12.8、Core/Widgets/Network/Test 和显式 `-std=c++14` 的前端构建入口
- [ ] 1.2 分别使用 GCC 7.3.0 与 9.3.0 完成最小构建，记录实际 Qt 包、编译器、libstdc++ ABI 和构建系统
- [ ] 1.3 增加检查以拒绝 Qt 5.15/Qt 6、C++17 专属 API、QtSql 和依赖不兼容 ABI 的预编译插件
- [ ] 1.4 创建前端 app、composition、widgets、services 分层及后端 agent_service、mock_business_api 分层
- [ ] 1.5 新增可重复执行的前后端单元测试、集成测试和双工具链构建入口

## 2. 可嵌入业务组件与传统窗口

- [ ] 2.1 定义 Registry 的 `std::function<QWidget *(QWidget *parent)>` 工厂及 GUI 线程、非顶层、resize/reparent、QObject 所有权和 size policy 合同
- [ ] 2.2 实现有状态的 `Calculator`、`CalculationHistory`、`CalculationStats`、`Clock` 和 `NotePad` QWidget 类
- [ ] 2.3 让 Calculator 仅通过预定义按钮执行本地四则计算，并确保除零、非法输入和任意脚本不会创建记录
- [ ] 2.4 实现不读取内部控件的 Registry 检查，并为不合规遗留组件保留项目维护 Adapter 的注册路径
- [ ] 2.5 使用计划注册到动态 Renderer 的同一批 QWidget 类实现 `LegacyToolboxWindow`
- [ ] 2.6 新增组件本地行为、多实例、嵌入合同和固定版/动态版同类实现测试

## 3. CalculationService、业务 API 与 SQLite

- [ ] 3.1 实现 C++14 `CalculationService`，从可信应用配置读取本地回环 API 地址，并使用 `QNetworkAccessManager` 与 `QTimer` 提供异步请求和超时
- [ ] 3.2 定义 `CalculationRecord` 的 id、expression、result、note、createdAt、updatedAt 请求响应合同
- [ ] 3.3 在 Mock Business API 中实现文件型 SQLite 初始化和独占数据访问，测试时注入独立临时数据库
- [ ] 3.4 实现 `POST /api/calculations` 与 `GET /api/calculations?limit=50`，限制查询上限为 50 并按创建时间倒序
- [ ] 3.5 实现仅修改 note 的 `PATCH /api/calculations/{id}`、`DELETE /api/calculations/{id}` 和 `GET /api/calculations/summary`
- [ ] 3.6 让 Calculator 在本地计算成功后创建记录，并在请求失败时保留结果、显示错误且不伪造持久化成功
- [ ] 3.7 让 History 在启动时、每 2 秒及手动刷新时查询，支持修改备注和删除，并在失败时保留最后有效显示
- [ ] 3.8 让 Stats 在启动时及每 2 秒查询总数和最新记录，并在失败时标记最后摘要过期
- [ ] 3.9 新增 CRUD、4 秒内可见联动、SQLite 重启持久化、临时库隔离、超时和恢复测试，证明组件间没有直接信号或事件总线

## 4. 共享编排合同

- [ ] 4.1 创建共享、机器可读的业务 Component Catalog，覆盖五类业务叶子的版本、描述、多实例规则、允许展示字段和布局提示
- [ ] 4.2 创建 SurfaceSpec v0 JSON Schema，覆盖协议 `0.1`、Surface `main`、有序邻接表、递归 Row/Column 和封闭字段
- [ ] 4.3 固定 gap token 到 `0/4/8/16` 逻辑像素、容器 margin 为 0、align 默认 stretch、justify 默认 start、weight 范围 `0..10` 且默认 0
- [ ] 4.4 在 Schema 与语义规则中拒绝正 weight 与非 start justify 的组合，并记录空容器及单子项 justify 行为
- [ ] 4.5 为侧栏、上下、2×2 嵌套、权重突出、空 Surface、重复 Calculator 和深度边界新增有效 SurfaceSpec 与 LayoutPlan Fixture
- [ ] 4.6 新增非法 Fixture，覆盖格式错误 JSON、未知或禁止字段和类型、环、多父、不可达、weight 位置或组合错误及复杂度超限
- [ ] 4.7 新增合同测试，证明 URL、method、request body、Action、数据绑定、信号槽、脚本、QSS、任意样式和像素几何不能进入 Catalog、LayoutPlan 或 SurfaceSpec

## 5. Qt 5.12 校验与受控布局渲染

- [ ] 5.1 使用 `QJsonDocument`、`QJsonObject`、`QJsonArray` 实现与共享 Schema 等价的 Qt 封闭字段和类型校验
- [ ] 5.2 实现 version、surface、root、唯一 ID、引用顺序、可达性、无环、单父、布局字段位置、多实例及节点/深度限制的语义校验
- [ ] 5.3 使用相同共享 Fixture 证明服务端 Validator 与 Qt Validator 的接受或拒绝结论一致
- [ ] 5.4 实现 Renderer 拥有的递归 Row/Column 容器、0 margin、gap token 和 start/center/end/spaceBetween/spaceAround/spaceEvenly spacer 映射
- [ ] 5.5 实现正 weight 时仅使用 `QBoxLayout` stretch factor 且不添加主轴 spacer，并确保 align=stretch 不改写业务叶子 `QSizePolicy`
- [ ] 5.6 使用有效 Fixture 实现单次完整渲染，并测试每个非法 Fixture 均不会改变当前界面
- [ ] 5.7 在 HostShell 中用固定且可调整内容的 `QScrollArea` 承载 `main`，验证最小尺寸溢出时出现滚动而非裁剪

## 6. 稳定业务叶子 Reconciliation 与宿主体验

- [ ] 6.1 实现以稳定业务组件 ID 和类型为键的 Reconciliation Plan，覆盖有序移动、新建、类型替换和移除
- [ ] 6.2 允许提交时重建 Renderer 拥有的 Row/Column 容器，同时保证未变业务叶子不被重建
- [ ] 6.3 实现暂存资源创建和原子提交，确保校验或构造失败时释放暂存资源且不修改活动树
- [ ] 6.4 实现已移除或替换业务 QWidget 的提交后销毁，以及重新添加时的新 ID 和新状态
- [ ] 6.5 实现固定 HostShell，包含 Prompt 输入、编排进度、QScrollArea `main` Surface 和诊断 StatusPanel
- [ ] 6.6 使用对象身份和销毁观察新增 Qt 测试，验证 Calculator 输入、Clock 计时、NotePad 文本、History 选择、焦点、多实例、移动、替换、移除和回滚

## 7. 确定性 Surface Compiler

- [ ] 7.1 定义并校验受限 LayoutPlan 合同，覆盖既有业务叶子引用、新 Catalog 实例、递归 Row/Column 层级和允许布局意图，但不包含最终 ID
- [ ] 7.2 实现确定性的稳定 ID 复用与分配、同类型多实例、multiple 规则及完整有序 SurfaceSpec 编译
- [ ] 7.3 使用同一份 Catalog、Schema、布局规则、图不变量和复杂度限制实现后端 SurfaceSpec Validator
- [ ] 7.4 让每个 LayoutPlan Fixture 经过 Surface Compiler 与服务端 Validator，并测试确定性输出、未变 ID 复用、新 ID 分配和非法计划诊断
- [ ] 7.5 对 Grid、跨行列、重叠、wrap、Splitter、Dock 和响应式断点返回 `unsupported_layout`，不得生成近似 SurfaceSpec

## 8. Agent 编排服务与链路隔离

- [ ] 8.1 实现 `POST /compose` 的 request、完整目标成功 response、`unsupported_layout` 和其他结构化 error 合同
- [ ] 8.2 实现 LLM Adapter 和受限 Prompt，只使用用户指令、当前 SurfaceSpec 和 Effective Catalog，并通过配置注入 Provider 凭据
- [ ] 8.3 让 LLM LayoutPlan 经过与 Fixture 完全相同的 Parser、Surface Compiler 和服务端 Validator
- [ ] 8.4 实现 Qt Composition Client，使用 Qt 5.12 异步网络和 QTimer 超时连接本地校验及事务式 Reconciliation
- [ ] 8.5 保持 Agent Service 与 Mock Business API 的模块、Router、客户端和数据访问依赖分离，禁止 `/compose` 导入计算记录数据层
- [ ] 8.6 新增集成测试，证明 `/compose` 不能生成或执行 `/api/calculations*` 请求细节，并验证 `unsupported_layout` 不破坏当前界面

## 9. 端到端验收与文档

- [ ] 9.1 优先使用 Fixture 验收左右、上下、侧栏、2×2、权重突出、空 Surface、重复组件和深度边界
- [ ] 9.2 验证固定版与动态版使用相同五类业务 QWidget，且 Calculator → API → History/Stats 只经后端 SQLite 联动
- [ ] 9.3 验证创建、查询、修改备注、删除、SQLite 重启持久化、临时测试库、网络失败和恢复
- [ ] 9.4 验证格式错误 JSON、未知组件、环、禁止字段、非法 weight/justify、超限结构和不支持布局均产生诊断且保留最后有效界面
- [ ] 9.5 通过已配置 LLM 运行七条推荐自然语言场景，记录实际结构、预期结构、稳定业务实例和 `unsupported_layout` 结果
- [ ] 9.6 在 GCC 7.3.0 与 9.3.0 上完成 Qt 5.12.8/C++14 构建与测试，并记录 ABI、build、run、test、Catalog、Schema、LLM 和本地回环前置条件
- [ ] 9.7 明确最终结论只证明受控机制可行，不证明任意前端布局、生产网络安全或真实公司组件兼容
