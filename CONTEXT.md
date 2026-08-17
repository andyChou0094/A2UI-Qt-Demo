# Qt Dynamic UI Composition

本上下文描述自然语言驱动的受控 UI 编排，以及宿主应用与既有界面模块之间的边界。

## Language

**Host Shell**:
始终由应用控制的固定界面区域，承载用户输入、动态区域和运行状态。
_Avoid_: Generated shell, dynamic main window

**Dynamic Surface**:
允许根据用户指令改变组件选择与排列的受控界面区域。
_Avoid_: Dynamic page, generated window

**Registered Component**:
被批准用于动态编排的自包含界面模块；其内部行为不属于编排范围。
_Avoid_: Generated component, atomic control

**Component Catalog**:
当前系统允许编排的组件类型及其受控参数的唯一清单。
_Avoid_: Component list, prompt component description

**Layout Plan**:
模型表达的组件选择与排列意图，尚未具有最终组件身份。
_Avoid_: SurfaceSpec, generated JSON

**Surface Specification**:
通过确定性编译和校验后，可用于更新一个 Dynamic Surface 的完整目标结构。
_Avoid_: Layout Plan, A2UI payload

**Surface Compiler**:
将 Layout Plan 转换为 Surface Specification，并拥有组件稳定身份的确定性边界。
_Avoid_: LLM renderer, ID generator prompt

**Composition Request**:
用户要求改变 Dynamic Surface 中组件选择或排列的自然语言指令。
_Avoid_: Business request, action

**Business Request**:
由 Registered Component 按预定义行为发起的业务服务调用，与界面编排无关。
_Avoid_: Composition Request, generated API call
