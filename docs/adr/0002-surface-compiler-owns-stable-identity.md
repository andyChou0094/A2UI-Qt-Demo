# ADR 0002：Surface Compiler 拥有稳定组件身份

- 状态：已接受
- 决策日期：2026-08-14
- 最近复核：2026-08-21

## 背景

动态重排必须保留 Calculator 输入、Clock 运行状态、NotePad 草稿、History 选择和焦点。若让
LLM 直接分配最终组件 ID，同一个逻辑组件可能因为措辞变化获得新 ID，导致 QWidget 被错误
销毁；同类型多实例也难以确定哪些是既有实例、哪些是新增实例。

## 决策

LLM 只生成 LayoutPlan，确定性的 Surface Compiler 负责：

- 解析对当前业务实例的引用；
- 执行 Catalog 的 `multiple` 规则；
- 为新实例分配最终 ID；
- 为逻辑上未变化的实例复用既有 ID；
- 生成完整、有序的 SurfaceSpec 邻接表；
- 在返回前调用服务端 Validator。

Qt Reconciler 再以 `(stable ID, component type)` 为键应用目标：

- ID 与类型相同则复用 QWidget；
- 新 ID 创建新对象；
- 同 ID 不同类型在可提交后替换；
- 已移除对象在提交成功后销毁；
- 失败更新不执行破坏性生命周期操作。

## 后果

收益：

- 文本提示变化不会直接决定对象生命周期。
- 重排可以保持本地状态和 QObject 身份。
- 重复组件有明确、可测试的新旧实例语义。
- Fixture 和真实 Provider 结果使用同一编译链路。

代价：

- 系统增加 LayoutPlan、Compiler 和 SurfaceSpec 三层合同。
- Compiler 必须持有当前 Surface 上下文并维护稳定分配规则。
- ID 语义变化需要同时更新编译器、Qt Reconciler 和合同测试。

## 边界

稳定身份只保证注册的业务叶子。Renderer 拥有的 `Row/Column` 容器可以在提交时重建。

当前后端不保存用户会话或“当前页面”；Qt HostShell 显式把当前 Surface 随 `/compose` 请求发送，
因此身份计算仍以客户端提供且经过校验的当前 Surface 为上下文。
