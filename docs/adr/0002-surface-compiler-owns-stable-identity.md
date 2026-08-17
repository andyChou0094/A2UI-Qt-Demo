# Surface Compiler 拥有稳定组件身份

LLM 只生成 Layout Plan，确定性的 Surface Compiler 负责分配和复用最终组件 ID；同 ID 且同类型的组件在重排时复用原实例。该设计增加一个编译阶段，以换取可预测的差异更新、状态保持和重复组件处理。
