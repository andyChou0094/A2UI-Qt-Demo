## 1. 文档边界与索引

- [x] 1.1 将项目灵感来源和完整方案比较迁移到既有 `docs/research/generative-ui-layout-comparison.md`，从技术架构删除重复的外部原理与比较，只保留必要摘要和链接。
- [x] 1.2 更新根 `README.md` 的文档索引，并压缩其中与技术架构重复的后续方案说明。

## 2. 技术架构重构

- [x] 2.1 按组件、DSL 和核心算法重组 `docs/technical-architecture.md`，讲清 Effective Catalog、LayoutPlan、SurfaceSpec、分层校验、稳定 ID、图校验和原子提交的含义、作用与必要性。
- [x] 2.2 用一个具体 Prompt 串联受限生成、计划编译、SurfaceSpec、QWidget 复用与提交，删除同等篇幅逐阶段展开的流水账和非关键实现细节。
- [x] 2.3 将复杂的旧组件兼容与布局质量方案收敛为由当前证据、最小下一步和升级触发条件组成的渐进式展望。

## 3. 评测报告重构

- [x] 3.1 将“评测数据集如何设计”和“后续验证优先级”合并至 `docs/verification-and-evaluation.md` 文末，按当前覆盖、主要不足、后续设计的顺序精简表述。

## 4. 编辑复核

- [x] 4.1 逐段检查 README、技术架构、方案调研和评测报告的重复、冗长、晦涩及流水账内容，并验证术语、当前/未来能力边界、标题层级和 Markdown 相对链接。

## 5. 技术说明补充

- [x] 5.1 在 Catalog、LayoutPlan、SurfaceSpec 对应子标题下补充当前合同的最小 JSON 示例和权威文件索引，并检查是否遗漏影响业务理解的重要 DSL 字段。
- [x] 5.2 在 `QWidget 几何映射` 中集中解释 Row/Column、嵌套与 children 顺序、gap、align、justify、weight 及 Qt 尺寸约束，使用最小嵌套示例说明相对位置并避免跨章节重复。
