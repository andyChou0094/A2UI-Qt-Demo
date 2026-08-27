# ADR 0001：Agent 只编排已注册的完整组件

- 状态：已接受
- 决策日期：2026-08-14
- 最近复核：2026-08-21

## 背景

项目需要验证自然语言是否能够改变既有 Qt 界面组合，同时保留业务 QWidget 的内部控件、状态、
信号槽、样式和网络行为。如果允许模型直接创建任意控件、脚本、请求或 QSS，Renderer 就必须
解释不可控行为，也无法证明原组件仍然保持原实现。

## 决策

Agent 只能选择、申请实例和排列 Component Catalog 中注册的完整业务叶子。

- 业务叶子是可嵌入、非顶层的 C++ QWidget。
- Renderer 把业务叶子视为不透明整体，不读取其子控件。
- Agent 不能修改组件内部布局、样式、信号槽、Service 或业务请求。
- `Row` 和 `Column` 是 Renderer 自有布局节点，不是业务组件。
- 不符合嵌入合同的遗留组件只能通过项目维护的固定 Adapter 接入。
- Catalog、LayoutPlan 和 SurfaceSpec 都禁止 URL、HTTP method、Action、数据绑定、脚本和任意样式。

## 后果

收益：

- 组件继续拥有其原有状态和业务行为。
- Agent 权限可以用封闭 Schema 与 Catalog 审计。
- 同一 QWidget 类可以在默认 Surface 和任意合法动态布局中复用。
- Renderer 不需要理解具体业务控件树。

代价：

- Agent 不能生成任意页面或修改组件内部细节。
- 新业务能力必须先由开发者实现、认证并注册。
- QMainWindow、Dock、模态流程、QML 和不符合嵌入合同的组件不能直接作为叶子。

## 边界

本决策不说明所有 QWidget 都天然安全或可嵌入。真实组件仍需验证 GUI 线程构造、reparent、
QObject 所有权、尺寸策略、ABI、失败恢复和业务权限。

只有真实用例证明当前边界不足后，才可以通过新的 ADR 和 OpenSpec change 扩展 Catalog 或布局
容器；不能让模型绕过 Registry 直接创建控件。
