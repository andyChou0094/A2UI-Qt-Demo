# 开发状态

更新日期：2026-08-24

## 当前完成度

两个实现 change 均已完成并归档：

- `build-controlled-qt-composition-demo`：58/58，归档于
  `openspec/changes/archive/2026-08-18-build-controlled-qt-composition-demo/`。
- `enable-ubuntu-runtime-and-polish-frontend`：66/66，归档于
  `openspec/changes/archive/2026-08-24-enable-ubuntu-runtime-and-polish-frontend/`。

当前应用为单一动态 HostShell，包含自然语言编排、SurfaceSpec 导入/导出/恢复默认、五类业务
QWidget、稳定 ID Reconciliation 以及后端介导的计算记录闭环。

## 验证摘要

- 双 GCC frontend CTest 均为 12/12。
- Backend unit 37/37、integration 14/14，真实 uvicorn Surface 文档回环 5/5。
- Provider 七场景 7/7，即 6 个支持场景成功 + 1 个不支持场景正确拒绝。
- 50 样本基线完成 50/50，40 份得到可校验 SurfaceSpec；该分类不是产品通过率。

完整数字、指标定义、七场景、50 样本、问题和兜底只在
[验证与评测报告](verification-and-evaluation.md) 维护。

## 当前限制

- SurfaceSpec v0 只支持单 `main` Surface 和递归 Row/Column。
- Grid、跨行列、重叠、wrap、Splitter、Dock、响应式断点、任意样式和脚本被拒绝。
- 合法 DSL 不等于意图满足；`PB-048` 是当前已知语义 no-op。
- Provider 存在传输失败和长尾响应，需继续协调前后端超时预算。
- Qt 5.12.8 及相关基础系统已停止常规支持，当前基线只用于 Demo 兼容与可重现性验证。
- 未认证本单位真实 QWidget、第三方二进制插件、生产权限/网络和供应链。

业务背景、设计来源、数据流、模块边界和后续两条主线见[技术架构](technical-architecture.md)。
