可以重新把这件事定下来。经过这次重新调研，我认为你这个项目最容易踩的坑，是把 **A2UI、MCP Apps、AG-UI、Tool Calling、CopilotKit** 当成几条互斥的技术路线。

实际上它们解决的是不同层的问题。

# 一、先给最终结论

对于你的 **存量 C++ + Qt 项目**，我最推荐的不是直接“接入某个 Generative UI 框架”，而是：

> **现有 Qt 组件不动 → Component Registry 封装 → 自定义声明式 UI Schema/IR → LLM 根据自然语言生成 UI Schema → Qt Renderer 将 Schema 映射回原有 Qt 组件 → 原组件继续执行原来的 signal/slot 和后端请求。**

在这个基础上：

> **第一阶段：Tool Calling + Component Registry**
> **第二阶段：把内部 UI Schema 设计成 A2UI-compatible**
> **第三阶段：如果 Agent 独立部署，再引入 AG-UI 做流式通信和状态同步**
> **MCP 用于暴露业务能力，但暂时不要用 MCP Apps 做主 Qt UI。**

也就是：

```text
                       ┌──────────────┐
用户自然语言 ──────────→│   LLM/Agent  │
                       └──────┬───────┘
                              │
                     生成声明式 UI Spec
                              │
                              ▼
                    ┌───────────────────┐
                    │ UI Policy/Validator│
                    └─────────┬─────────┘
                              │
                              ▼
                    ┌───────────────────┐
                    │ Qt UI Orchestrator│
                    └─────────┬─────────┘
                              │
                     Component Registry
                              │
                ┌─────────────┼─────────────┐
                ▼             ▼             ▼
          原有 QWidget     原有 QWidget    原有 QML
          DevicePanel      AlarmTable      ChartView
                │             │             │
                └─────────────┼─────────────┘
                              │
                     原 signal / slot
                              │
                              ▼
                        原有业务逻辑
                              │
                              ▼
                        原有后端接口
```

**这是我认为最符合你要求的架构。**

A2UI 可以成为你的“UI 描述协议”；AG-UI 可以成为“Agent ↔ Qt 的通信协议”；MCP 可以成为“业务能力调用协议”。三者可以同时存在，但不应该让它们取代你的 Qt 组件层。

---

# 二、先把这几个概念彻底分清楚

| 技术                      | 本质解决什么                 | 是否负责 UI 描述 | 是否负责 Agent 通信 | 是否适合直接渲染 Qt |
| ----------------------- | ---------------------- | ---------: | ------------: | ----------: |
| Tool Calling            | LLM 调用确定性能力            |         部分 |             否 |       ★★★★★ |
| A2UI                    | Agent 如何描述 UI          |      **是** |             否 |       ★★★★☆ |
| AG-UI                   | Agent 如何和前端实时交互        |         部分 |         **是** |       ★★★★☆ |
| MCP                     | Agent 如何访问工具/资源        |          否 |           工具层 |       ★★★★☆ |
| MCP Apps                | MCP Tool 返回交互式 HTML UI |          是 |             是 |       ★★☆☆☆ |
| CopilotKit              | Web Agent UI 框架        |          是 |             是 |       ★★☆☆☆ |
| json-render             | JSON → UI Component    |          是 |             否 |       ★★☆☆☆ |
| Qt QUiLoader/QML Loader | Qt 动态实例化 UI            |      Qt 内部 |             否 |       ★★★★★ |

所以你真正需要解决的是四层：

```text
自然语言理解
      ↓
UI 决策
      ↓
UI 描述
      ↓
Qt 原生组件实例化
```

而不是“找一个框架把四件事情全包掉”。

---

# 三、方案一：Tool Calling → Qt Component Registry

这是我建议**第一步真正落地**的方案。

## 3.1 核心思想

不要让 LLM：

```text
"给我生成 QWidget"
```

也不要让 LLM：

```text
"给我写 QML"
```

而应该让它说：

```json
{
  "type": "AlarmTable",
  "props": {
    "level": "critical",
    "autoRefresh": true
  }
}
```

然后 C++：

```cpp
registry.create("AlarmTable", props);
```

最终实际上创建的是你公司已有的：

```cpp
new AlarmTableWidget(...);
```

于是：

```text
样式没变
布局没变
signal/slot 没变
业务 Controller 没变
网络请求没变
后端 API 没变
```

只改变了：

> **“谁决定这个组件什么时候出现”。**

以前是程序员：

```cpp
layout->addWidget(new AlarmTableWidget());
```

以后变成：

```text
LLM
 ↓
AlarmTable
 ↓
Registry
 ↓
new AlarmTableWidget()
```

这正好命中你的核心需求。

Qt 本身的 Meta-Object System 已经提供运行时类型信息、属性系统以及 signal/slot 机制，因此 Qt 非常适合在原组件外面增加这样一层适配，而不是把现有逻辑搬到新的 Web 前端。([Qt 文档][1])

---

# 四、Component Registry 到底是什么

可以把它理解为：

> **一本给 AI 使用的“Qt 组件菜单”。**

例如公司现在有：

```text
DeviceStatusWidget
AlarmTableWidget
TrendChartWidget
UserFilterWidget
NetworkTopologyWidget
TaskListWidget
```

不要把这些 C++ class 名直接暴露给 AI。

注册成语义化描述：

```cpp
registry.registerComponent({
    .name = "device_status",
    .description = "Display current device status",
    .factory = [](const Props& props) {
        return new DeviceStatusWidget(props.deviceId);
    }
});
```

同时提供参数 Schema：

```json
{
  "name": "device_status",
  "description": "显示设备实时运行状态",
  "props": {
    "deviceId": {
      "type": "string",
      "required": true
    },
    "showMetrics": {
      "type": "boolean",
      "default": true
    }
  }
}
```

这样 LLM 只能使用：

```text
device_status
```

而不能乱生成：

```text
SuperCoolDeviceWidgetV2
```

这实际上与 A2UI、json-render 等项目采用的 **catalog / registry + declarative UI** 思想高度一致：模型只能从开发者批准的组件目录中选择，而不能生成任意可执行代码。A2UI 就把这一原则明确作为安全设计的一部分；json-render 也是“catalog + schema + renderer”的结构。([GitHub][2])

---

# 五、非常重要：不要“一个 Qt 组件 = 一个 Tool”

这是我这次重新考虑以后特别建议你改掉的地方。

比如你有 300 个 Qt 组件。

不要给模型暴露：

```text
show_alarm_table()
show_device_panel()
show_chart()
show_network()
show_task_table()
...
300 tools
```

这会导致 Tool 数量爆炸。

更好的方式是：

```text
search_components
compose_ui
update_ui
invoke_action
```

例如：

```text
用户：
“给我看一下设备 A01 最近的告警和运行状态。”
```

Agent 第一步：

```json
search_components({
    "query": "device alarms and status"
})
```

得到：

```json
[
  "device_status",
  "alarm_table",
  "trend_chart"
]
```

再调用：

```json
compose_ui({
  "layout": {
    "type": "column",
    "children": [
      {
        "component": "device_status",
        "props": {
          "deviceId": "A01"
        }
      },
      {
        "component": "alarm_table",
        "props": {
          "deviceId": "A01"
        }
      }
    ]
  }
})
```

这样 Component 是 Component，Tool 是 Tool。

这两个概念不要混在一起。

---

# 六、方案二：A2UI

A2UI 是这次调研后我认为**与你思想最接近的开源标准**。

它不是某一个 UI 框架，而是：

> **Agent-to-User Interface Protocol**

Agent 不生成 HTML，不生成 QML，也不生成 JavaScript，而是生成：

```text
声明式 JSON UI
```

客户端拥有：

```text
Component Catalog
```

然后：

```text
A2UI JSON
     ↓
A2UI Renderer
     ↓
native component
```

官方明确把流程分成：

```text
Agent generation
      ↓
transport
      ↓
client resolution
      ↓
native rendering
```

而客户端负责把抽象组件映射成自己的 React、Flutter、SwiftUI 或其他 native widget。([GitHub][2])

这和你的需求几乎天然一致：

```text
A2UI Button
      ↓
Qt Renderer
      ↓
MyCompanyButton

A2UI DeviceStatus
      ↓
Qt Renderer
      ↓
DeviceStatusWidget

A2UI AlarmTable
      ↓
Qt Renderer
      ↓
AlarmTableWidget
```

---

# 七、为什么我没有直接建议“全面采用 A2UI”

因为截至 **2026 年 8 月**，A2UI 官方仍把项目标为 **early-stage public preview**；当前 production release 是 v0.9.1，v1.0 仍处于 release candidate 阶段。更关键的是，官方目前列出的 host framework 主要还是 Web / Flutter，没有现成 Qt Renderer。([GitHub][2])

所以如果你决定：

> “公司内部协议直接100%绑定 A2UI。”

风险在于 A2UI 规范继续变化时，你的 Qt 代码会跟着变化。

因此我的建议是：

```text
           Agent
             │
          A2UI JSON
             │
        A2UI Adapter
             │
             ▼
      Company UI IR
             │
        Qt Renderer
```

而不是：

```text
A2UI JSON
   ↓
Qt 所有代码都直接理解 A2UI
```

你的 Qt 核心只认自己的稳定 IR。

例如内部协议：

```json
{
  "version": "1.0",
  "surface": "main_workspace",

  "layout": {
    "type": "row",
    "children": [
      {
        "component": "device_status",
        "id": "status",
        "props": {
          "deviceId": "A01"
        }
      },

      {
        "component": "alarm_table",
        "id": "alarms",
        "props": {
          "deviceId": "A01",
          "severity": "critical"
        }
      }
    ]
  }
}
```

以后：

```text
A2UI → Company UI IR
OpenJSON → Company UI IR
Tool Call → Company UI IR
```

Qt Renderer 完全不用改。

这叫：

> **Anti-Corruption Layer / Adapter Layer**

非常适合公司存量项目。

---

# 八、方案三：AG-UI

AG-UI 很容易和 A2UI 搞混。

A2UI 的核心问题是：

> **“界面长什么样？”**

AG-UI 的核心问题是：

> **“Agent 和前端怎么持续交流？”**

AG-UI 是 event-based protocol，支持实时流式输出、双向 state synchronization、frontend tools、Generative UI 和 Human-in-the-loop。官方把它定位为 Agent 与用户侧应用之间的协议层。([GitHub][3])

例如：

```text
Qt Client
   │
   │ 用户输入
   ▼
AG-UI
   │
   ▼
Python Agent
   │
   ├── TEXT_MESSAGE
   ├── TOOL_CALL
   ├── STATE_DELTA
   └── UI_SPEC
   │
   ▼
Qt
```

更有意思的是，现在 AG-UI 已经存在 **community C++ SDK**，提供 C++17 实现、SSE、HTTP、事件处理、state management、middleware 和 subscriber pattern。([GitHub][4])

所以假设以后你的系统是：

```text
C++/Qt Desktop
        │
        │ AG-UI
        ▼
Python LangGraph Agent
        │
        ▼
LLM
```

这条路线是相当自然的。

但是：

> **第一版完全没必要强行接 AG-UI。**

开始可以：

```text
Qt
 ↓ HTTP
Agent API
 ↓
JSON UI Spec
```

等你需要：

```text
token streaming
agent progress
UI streaming
state synchronization
interrupt
human approval
```

再升级到 AG-UI。

---

# 九、方案四：MCP Apps / MCP-UI

这条路线我现在反而**不建议作为你的主方案**。

MCP Apps 的核心机制是：

```text
MCP Tool
   ↓
ui:// resource
   ↓
HTML + CSS + JS
   ↓
sandboxed iframe
```

Host 与 iframe 再通过 JSON-RPC / `postMessage` 进行双向交互。官方文档明确说明 MCP Apps 的 UI resource 本质上是 HTML 页面，Web host 通常通过 sandboxed iframe 渲染。([Model Context Protocol][5])

这意味着如果放到你的 Qt 项目：

```text
Qt
 ↓
QWebEngineView
 ↓
HTML
 ↓
React / Vue / JS component
```

结果是什么？

你原来是：

```text
DeviceStatusWidget : QWidget
AlarmTableWidget : QWidget
```

现在变：

```text
DeviceStatus.jsx
AlarmTable.jsx
```

那你就不得不重新实现：

```text
样式
布局
状态
事件
业务调用
```

这恰恰违背：

> **“保留原有 Qt 排版、样式、逻辑。”**

所以：

### MCP 很值得用。

但：

### MCP Apps 不适合做你的主 Qt 渲染方案。

MCP Apps 更适合：

```text
ChatGPT
Claude
IDE
Web Agent Host
```

里面动态嵌入：

```text
dashboard
form
viewer
configuration UI
```

而不是给大型已有 QWidget 应用做 native UI orchestration。([Model Context Protocol][5])

`mcp-ui` 目前则可以理解为 MCP Apps 生态中的一个 SDK 实现，提供 TypeScript/Python/Ruby server 以及 Web client renderer。([GitHub][6])

---

# 十、MCP 在你的项目里应该放哪

实际上它很适合放在另一个地方：

```text
                         ┌── search_device
                         ├── restart_device
LLM ── MCP Client ───────┼── query_alarm
                         ├── generate_report
                         └── export_data
```

也就是说：

> MCP = **业务能力层**

而：

```text
Component Registry
+
UI Schema
+
Qt Renderer
```

负责：

> **表现层**

以后：

```text
                Agent
             /         \
           MCP          A2UI
           │             │
     Business Tools    UI Spec
           │             │
           ▼             ▼
       Backend       Qt Renderer
```

这个边界会非常干净。

---

# 十一、方案五：json-render

这个项目也非常值得你研究，但主要是**学设计思想**。

它的设计几乎就是：

```text
Component Catalog
       +
Action Catalog
       ↓
LLM JSON
       ↓
Renderer
       ↓
Real Component
```

比如：

```text
Card
Metric
Button
```

先注册 schema，再注册对应真实实现，然后 AI 只能生成 catalog 中存在的组件。项目支持 React、Vue、Svelte、Solid、React Native 等 renderer，但目前并没有 Qt renderer。([GitHub][7])

所以：

> json-render 不适合直接塞进你的 C++ 项目。

但它的：

```text
Catalog
Schema
Registry
Renderer
Action
Spec
```

这六个抽象，我建议你认真借鉴。

事实上我建议你们内部 Qt Generative UI Framework 就按照这个模型设计。

---

# 十二、方案六：CopilotKit

CopilotKit 现在已经是一套比较完整的 Agent frontend stack，支持 Generative UI、backend tool rendering、shared state、Human-in-the-loop，并且可以连接 AG-UI，也支持 A2UI、MCP Apps 等 Generative UI 模式。([GitHub][8])

它非常适合：

```text
React
Angular
Vue
React Native
Web frontend
```

但对于你：

```text
C++ + QWidget/QML
```

仍然面临：

> 没有 native Qt renderer。

所以除非公司准备：

```text
Qt → Web frontend
```

或者：

```text
Qt + QWebEngine + React
```

否则我不会让 CopilotKit 成为你的生产基础。

但它非常适合做一个：

> **Generative UI 技术验证 Demo。**

因为 CopilotKit 本身同时展示了：

```text
Tool Rendering
A2UI
MCP Apps
Own Components
```

你可以快速理解这几种 paradigm 的区别。([CopilotKit Docs][9])

---

# 十三、assistant-ui / Tambo / Vercel AI SDK 怎么看

这些基本属于同一类：

```text
Web Generative UI SDK
```

例如 assistant-ui 支持将 tool calls / JSON 渲染成 React components；Tambo 则主打将已有 React components 接给 Agent。([GitHub][10])

它们证明了一件事：

> **“LLM 不生成页面代码，而是选择开发者提供的组件”已经成为 Generative UI 中非常主流的工程模式。**

但对于你的 Native Qt：

> 可以学习模式，不建议直接引入 Runtime。

---

# 十四、Qt 自己其实已经提供了最重要的一半能力

这点非常关键。

你并不需要重新发明：

> “动态创建 UI”。

Qt 本来就能做。

## QWidget 项目

如果大量页面来自：

```text
.ui
```

Qt 官方 `QUiLoader` 可以运行时读取 `.ui` 文件并动态创建 Widget tree。([Qt 文档][11])

但对于你公司的已有项目，我更建议：

```cpp
factory -> new ExistingBusinessWidget()
```

而不是简单：

```cpp
QUiLoader -> .ui
```

因为真正重要的通常不只是 `.ui`：

```text
Widget subclass
Controller
signals
slots
network logic
model
business state
```

你的目标是复用**整个业务组件**。

---

# 十五、如果项目是 QML，会更加容易

如果原项目主要使用：

```text
Qt Quick / QML
```

就更适合这套方案。

Qt 官方支持：

```javascript
Qt.createComponent("DevicePanel.qml")
```

然后：

```javascript
component.createObject(parent, {
    deviceId: "A01"
})
```

动态实例化已有组件。([Qt 文档][12])

非常适合：

```text
AI JSON
 ↓
QML component name
 ↓
Qt.createComponent
```

而且 Qt 官方还特别提醒：

> 不建议运行时拼 QML 字符串，因为每次都需要编译，容易生成非法 QML。

所以你更加不应该：

```text
LLM → QML source code
```

而应该：

```text
LLM
 ↓
DevicePanel + props
 ↓
已有 DevicePanel.qml
```

这正是 Component Registry 思路。([Qt 文档][12])

---

# 十六、我建议你们真正设计这 7 个模块

| 模块                 | 作用                      |
| ------------------ | ----------------------- |
| Agent Service      | 理解自然语言                  |
| Component Catalog  | 告诉 Agent 有哪些组件          |
| UI Planner         | 决定展示什么组件                |
| UI Schema / IR     | 描述界面结构                  |
| UI Validator       | 防止非法组件/参数/操作            |
| Qt Renderer        | UI Schema → QWidget/QML |
| State/Event Bridge | Qt ↔ Agent 状态和事件同步      |

其中最核心的是：

```text
Component Registry
+
UI IR
+
Qt Renderer
```

---

# 十七、Component Registry 建议长这样

不要只保存：

```text
name → QWidget factory
```

应该保存：

```cpp
struct ComponentDescriptor {

    QString id;

    QString description;

    JsonSchema propsSchema;

    std::vector<QString> events;

    std::vector<QString> capabilities;

    SecurityPolicy policy;

    std::function<QWidget*(
        QWidget* parent,
        const QJsonObject& props
    )> factory;
};
```

例如：

```text
component:
    alarm_table

description:
    显示设备告警记录

props:
    deviceId
    severity
    startTime
    endTime

events:
    alarmSelected
    refreshRequested

permissions:
    read_only

factory:
    AlarmTableWidget
```

这样 Component Registry 就同时成为：

```text
AI Component Catalog
+
Qt Component Factory
+
Schema Registry
+
Permission Registry
```

---

# 十八、布局怎么解决

这是你的要求里非常关键的一点：

> “保留原有排版、布局、样式。”

我不建议让 AI 控制：

```text
x = 123
y = 456
width = 782
margin = 13
font-size = 17
```

AI 不应该做设计师级像素控制。

应该只允许有限布局 primitive：

```text
Row
Column
Grid
Tabs
Stack
Splitter
```

例如：

```json
{
  "type": "Row",
  "children": [
    {
      "component": "device_status"
    },
    {
      "component": "alarm_table"
    }
  ]
}
```

对应：

```cpp
QHBoxLayout
```

而：

```text
DeviceStatusWidget 内部布局
AlarmTableWidget 内部布局
```

完全不动。

于是：

```text
AI 控制宏观组合

Qt Component 控制内部 UI
```

这是一个非常重要的边界。

---

# 十九、什么粒度的组件最合适

不要封装成：

```text
Button
Label
LineEdit
ComboBox
```

然后让 AI 拼页面。

对于企业存量系统，我建议组件主要是：

```text
业务级 Widget
+
页面 Section
+
少量 Layout Primitive
```

例如：

```text
AlarmFilterPanel
AlarmTable
DeviceOverview
DeviceMetricChart
NetworkTopology
TaskExecutionPanel
ReportPreview
```

而不是：

```text
QPushButton
QLabel
QLineEdit
```

否则：

```text
300 个业务界面
```

最后会变成：

```text
3000 个原子组件
```

Agent 很难选择，布局也容易失控。

---

# 二十、已有后端调用怎么保证完全不变

假设现在：

```cpp
AlarmTableWidget
   │
   │ signal
   ▼
AlarmController
   │
   ▼
AlarmService
   │
   ▼
HTTP API
```

千万不要改成：

```text
AI
 ↓
MCP
 ↓
Alarm API
```

来替代它。

应该保持：

```text
AI
 ↓
创建 AlarmTableWidget
 ↓
AlarmTableWidget
 ↓
AlarmController
 ↓
AlarmService
 ↓
原 API
```

这样 Agent 只是：

> **控制 UI 的出现和参数。**

它不接管业务逻辑。

这会极大降低改造风险。

---

# 二十一、一个完整示例

用户说：

> “打开 A01 设备的监控，左边显示状态和趋势，右边显示严重告警。”

Agent 先寻找组件：

```text
search_components(
    "device status trend alarm"
)
```

Catalog：

```text
device_status
device_trend
alarm_table
```

LLM 输出：

```json
{
  "layout": {
    "type": "row",

    "children": [
      {
        "type": "column",

        "children": [
          {
            "component": "device_status",
            "props": {
              "deviceId": "A01"
            }
          },

          {
            "component": "device_trend",
            "props": {
              "deviceId": "A01"
            }
          }
        ]
      },

      {
        "component": "alarm_table",

        "props": {
          "deviceId": "A01",
          "severity": "critical"
        }
      }
    ]
  }
}
```

Qt Renderer：

```text
Row
 ↓
QHBoxLayout

Column
 ↓
QVBoxLayout

device_status
 ↓
new DeviceStatusWidget("A01")

device_trend
 ↓
new DeviceTrendWidget("A01")

alarm_table
 ↓
new AlarmTableWidget("A01", Critical)
```

到这里 AI 的工作结束。

后面的：

```text
点击告警
刷新
查询
跳转
HTTP
缓存
数据库
权限
```

继续走旧代码。

这就是你真正想要的：

> **Generative Composition，而不是 Generative Code。**

---

# 二十二、最后我对所有方案的评价

| 方案                   | 保留 Qt 原组件 |  动态组合 |  开发成本 |  技术风险 | 我的评价                |
| -------------------- | --------: | ----: | ----: | ----: | ------------------- |
| Tool + Qt Registry   |     ★★★★★ | ★★★★☆ | ★★☆☆☆ | ★☆☆☆☆ | **第一阶段首选**          |
| A2UI + Qt Renderer   |     ★★★★★ | ★★★★★ | ★★★★☆ | ★★★☆☆ | **长期方向**            |
| AG-UI + Qt           |     ★★★★★ |     — | ★★★☆☆ | ★★☆☆☆ | **通信层推荐**           |
| MCP Tools            |     ★★★★★ |     — | ★★★☆☆ | ★★☆☆☆ | **业务能力层可用**         |
| MCP Apps             |     ★★☆☆☆ | ★★★★★ | ★★★★☆ | ★★★☆☆ | **不建议主方案**          |
| json-render          |     ★☆☆☆☆ | ★★★★★ | ★★★★☆ | ★★☆☆☆ | **学习架构**            |
| CopilotKit           |     ★☆☆☆☆ | ★★★★★ | ★★★☆☆ | ★★☆☆☆ | **Web 项目很好，Qt 不合适** |
| assistant-ui / Tambo |     ★☆☆☆☆ | ★★★★☆ | ★★★☆☆ | ★★☆☆☆ | **Web 参考**          |

---

# 二十三、我给你确定的最终架构

如果这个项目现在交给我做，我会采用：

```text
                    ┌────────────────────┐
                    │    Natural Language │
                    └─────────┬──────────┘
                              │
                              ▼
                    ┌────────────────────┐
                    │    Agent / LLM      │
                    │ LangGraph 可选      │
                    └───────┬────────────┘
                            │
                 search_components
                            │
                            ▼
                   Component Catalog
                            │
                            ▼
                    generate UI Spec
                            │
                            ▼
                  ┌───────────────────┐
                  │ Company UI IR     │
                  │ A2UI-compatible   │
                  └─────────┬─────────┘
                            │
                      Schema Validator
                            │
                            ▼
                  ┌───────────────────┐
                  │ Qt UI Orchestrator│
                  └─────────┬─────────┘
                            │
                  ┌─────────▼─────────┐
                  │ Component Registry│
                  └─────────┬─────────┘
                            │
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
 Existing QWidget     Existing QWidget      Existing QML
        │                   │                   │
        └───────────────────┼───────────────────┘
                            ▼
                     Existing Controller
                            ▼
                      Existing Backend
```

以后再加：

```text
Agent ←──── AG-UI ────→ Qt Client
```

以及：

```text
Agent ── MCP ──→ additional business tools
```

---

# 二十四、实施顺序也不要搞反

### 第 1 阶段：不要碰 A2UI、MCP Apps

挑三个已有 Qt 业务组件：

```text
DeviceStatus
AlarmTable
TrendChart
```

做：

```text
ComponentDescriptor
ComponentRegistry
QtRenderer
JSON UI Spec
```

实现：

> 自然语言 → JSON → 原 Qt Widget 动态出现

成功以后，整个技术路线就验证了。

### 第 2 阶段：增加动态组合

只实现：

```text
Row
Column
Grid
Tabs
Stack
Splitter
```

并增加：

```text
mount
unmount
updateProps
replace
```

实现：

> “把趋势图放左边，把告警放右边。”

### 第 3 阶段：增加 Component Search

组件越来越多以后，不让全部 Catalog 塞进 Prompt。

变成：

```text
用户请求
 ↓
Component Retrieval
 ↓
Top 5~10 relevant components
 ↓
LLM compose
```

### 第 4 阶段：接 AG-UI

当你真正需要：

```text
streaming
state synchronization
agent progress
human approval
```

再使用 AG-UI。其社区 C++ SDK 已经具备 SSE、事件模型、state management 等能力，可以作为 Qt/C++ 客户端的参考实现。([GitHub][4])

### 第 5 阶段：接 A2UI Adapter

内部：

```text
Company UI IR
```

保持不动。

增加：

```text
A2UI → UI IR Adapter
```

等 A2UI v1.0 真正稳定以后，再决定是否直接成为你们的标准协议。A2UI 自己也明确支持“抽象 UI 描述 → 客户端 native component mapping”这一模式，因此长期兼容它非常合理。([GitHub][2])

---

# 二十五、如果只允许我用一句话告诉你“到底选哪个”

答案是：

> **不要选择“A2UI 或 MCP Apps 或 Tool Calling”中的某一个；以 `Tool Calling + Component Registry + Qt Native Renderer` 作为工程核心，以 `A2UI` 作为未来 UI 协议兼容方向，以 `AG-UI` 作为未来 Agent↔Qt 通信协议，以 `MCP` 只负责业务工具能力，不以 MCP Apps 作为主 Qt 界面方案。**

尤其是你这个项目最重要的设计原则，我建议写进架构文档第一页：

> **LLM decides *what* to show; Qt decides *how* it looks and *how* it works.**

也就是：

```text
AI：
选择什么
组合什么
传什么参数

Qt：
如何布局
如何绘制
如何交互
如何请求后端
如何执行业务逻辑
```

这样才能真正做到你最开始要求的：

**“自然语言动态 UI” + “不重写公司原有 Qt 前端”。**

[1]: https://doc.qt.io/qt-6/qmetaobject.html?utm_source=chatgpt.com "QMetaObject Struct | Qt Core | Qt 6.11.1"
[2]: https://github.com/google/a2ui "GitHub - a2ui-project/a2ui · GitHub"
[3]: https://github.com/ag-ui-protocol/ag-ui "GitHub - ag-ui-protocol/ag-ui: AG-UI: the Agent-User Interaction Protocol. Bring Agents into Frontend Applications. · GitHub"
[4]: https://github.com/ag-ui-protocol/ag-ui/tree/main/sdks/community/c%2B%2B "ag-ui/sdks/community/c++ at main · ag-ui-protocol/ag-ui · GitHub"
[5]: https://modelcontextprotocol.io/extensions/apps/overview "MCP Apps - Model Context Protocol"
[6]: https://github.com/MCP-UI-Org/mcp-ui?utm_source=chatgpt.com "MCP-UI-Org/mcp-ui: UI over MCP ..."
[7]: https://github.com/vercel-labs/json-render "GitHub - vercel-labs/json-render: The Generative UI framework · GitHub"
[8]: https://github.com/copilotkit/copilotkit "GitHub - CopilotKit/CopilotKit: The Frontend Stack for Agents & Generative UI. React, Angular, Mobile, Slack, and more. Makers of the AG-UI Protocol · GitHub"
[9]: https://docs.copilotkit.ai/langgraph-python/generative-ui?utm_source=chatgpt.com "Generative UI"
[10]: https://github.com/assistant-ui/assistant-ui?utm_source=chatgpt.com "assistant-ui/assistant-ui: Typescript/React Library for AI Chat"
[11]: https://doc.qt.io/qt-6/quiloader.html?utm_source=chatgpt.com "QUiLoader Class | Qt UI Tools | Qt 6.11.1"
[12]: https://doc.qt.io/qt-6/qtqml-javascript-dynamicobjectcreation.html?utm_source=chatgpt.com "Dynamic QML Object Creation from JavaScript"








======================================================








你可以先用一个最简单的类比理解：

> **Vue 是“浏览器里的前端框架”，Qt 是“桌面应用开发框架”。**
>
> Vue 的核心运行环境是浏览器 DOM；Qt 的核心运行环境是 Qt 自己的对象树和窗口系统。

这会直接导致我们前面讨论的“AI 动态生成界面”方案，在 Qt 里不能简单照搬 Vue 的思路。

## 1. 先把 Vue 和 Qt 一一对应起来

你熟悉 Vue，可以先这么看：

| Vue 世界            | Qt Widgets 世界                          | Qt Quick/QML 世界      |
| ----------------- | -------------------------------------- | -------------------- |
| HTML              | `.ui` / QWidget 对象树                    | `.qml`               |
| CSS               | QSS / QPalette / Style                 | QML style / property |
| JavaScript        | C++ 业务逻辑 / signal-slot                 | JS + C++             |
| Vue Component     | QWidget 子类                             | QML Component        |
| props             | constructor 参数 / setter / `Q_PROPERTY` | property             |
| emit              | signal                                 | signal               |
| event handler     | slot / lambda                          | signal handler       |
| DOM Tree          | QObject / QWidget Tree                 | QML Object Tree      |
| Vue Router        | QStackedWidget / 页面管理器                 | StackView / Loader   |
| `v-if`            | show/hide / 创建销毁 Widget                | Loader / visible     |
| `v-for`           | C++ 动态创建 Widget                        | Repeater             |
| dynamic component | Widget Factory                         | Loader / Component   |

所以如果你以前写 Vue：

```vue
<DevicePanel
  :device-id="deviceId"
  @refresh="handleRefresh"
/>
```

在 Qt Widgets 里，大概等价于：

```cpp
auto *panel = new DevicePanelWidget(parent);

panel->setDeviceId(deviceId);

connect(
    panel,
    &DevicePanelWidget::refreshRequested,
    controller,
    &DeviceController::refresh
);
```

本质其实非常像。

只是 Vue 把很多东西替你自动化了。

---

# 2. Vue 的组件本质上是什么

比如：

```vue
<template>
  <div class="device-card">
    <span>{{ deviceName }}</span>
    <button @click="refresh">
      Refresh
    </button>
  </div>
</template>

<script setup>
defineProps({
  deviceName: String
})

const emit = defineEmits(["refresh"])
</script>

<style>
.device-card {
  ...
}
</style>
```

这是一个完整组件。

它同时包含：

```text
结构
+
样式
+
数据绑定
+
交互
```

父组件只需要：

```vue
<DeviceCard
   deviceName="A01"
   @refresh="onRefresh"
/>
```

所以 Vue 天生非常适合做：

```text
JSON
 ↓
component name
 ↓
Vue dynamic component
```

例如：

```vue
<component
  :is="spec.component"
  v-bind="spec.props"
/>
```

这就是为什么 Web Generative UI 看起来特别自然。

---

# 3. Qt Widgets 没有 Vue 那么“声明式”

这是最大的区别。

假设 Qt 里已经有：

```cpp
class DevicePanel : public QWidget
{
    Q_OBJECT

public:
    DevicePanel(QWidget* parent = nullptr);

    void setDeviceId(QString id);

signals:
    void refreshRequested();

private slots:
    void onButtonClicked();
};
```

UI 可能来自：

```text
DevicePanel.ui
```

样式可能来自：

```text
style.qss
```

逻辑在：

```text
DevicePanel.cpp
```

所以 Qt Widget 的“组件”其实是：

```text
.ui
+
.h
+
.cpp
+
QSS
+
Controller / Model
```

共同组成。

而不是像 `.vue` 一个文件就能比较完整地表达。

这对你的项目非常重要。

---

# 4. 因此在 Qt 里，我们不能直接做 Vue 那种动态组件

Vue 里面：

```javascript
const components = {
    alarm: AlarmTable,
    device: DevicePanel,
    chart: TrendChart
}
```

然后：

```vue
<component :is="components[type]" />
```

基本就完事了。

Qt Widgets 没有一个天然的：

```cpp
<component :is="xxx">
```

所以我们需要自己补一层：

# Component Registry

例如：

```cpp
registry.registerComponent(
    "device_status",
    [](QWidget* parent, const QJsonObject& props) {

        auto* w = new DeviceStatusWidget(parent);

        w->setDeviceId(
            props["deviceId"].toString()
        );

        return w;
    }
);
```

然后：

```cpp
QWidget* widget =
    registry.create(
        "device_status",
        props
    );
```

这个 Registry，实际上就是我们人为给 Qt 增加了一套类似 Vue：

```text
component name
      ↓
Vue component
```

的机制。

所以：

> **Vue 原生就具备动态组件体系；Qt Widgets 需要你自己建立这层 Component Registry。**

这就是具体实施方案上的第一个重大变化。

---

# 5. 第二个重大变化：Vue 的 props 很标准，Qt 没那么统一

Vue 所有组件大概都可以理解为：

```text
Component
   +
props
   +
events
```

所以 AI 非常容易理解。

例如：

```json
{
  "component": "AlarmTable",
  "props": {
    "deviceId": "A01",
    "level": "critical"
  }
}
```

Vue：

```vue
<AlarmTable
  v-bind="props"
/>
```

很自然。

但 Qt 可能出现这种情况：

```cpp
DeviceWidget(QString deviceId);

AlarmWidget();
alarm->setDevice(device);

ChartWidget(Context* context, int mode);

NetworkWidget(Session* session);
network->initialize();
```

也就是说，每个 Widget 初始化方式可能完全不统一。

这是存量 Qt 项目非常常见的问题。

因此你们需要增加：

# Adapter Layer

例如原组件：

```cpp
AlarmWidget* alarm =
    new AlarmWidget(controller);

alarm->setDevice(
    deviceManager->get(deviceId)
);

alarm->setFilter(
    AlarmLevel::Critical
);
```

AI 不需要知道这些。

AI 只知道：

```json
{
  "component": "alarm_table",
  "props": {
    "deviceId": "A01",
    "severity": "critical"
  }
}
```

然后 Adapter：

```text
AI Props
   ↓
Component Adapter
   ↓
公司的复杂初始化逻辑
```

也就是说：

```text
            Vue

JSON props
    ↓
Vue component
```

而 Qt 很可能需要：

```text
             Qt

JSON props
    ↓
Component Adapter
    ↓
Existing QWidget
```

这是第二个明显不同。

---

# 6. 第三个重大区别：Vue 的布局是 DOM，Qt Widgets 是 Layout 对象

Vue：

```html
<div class="row">

    <DevicePanel />

    <AlarmTable />

</div>
```

CSS：

```css
.row {
    display: flex;
}
```

Qt：

```cpp
auto* layout =
    new QHBoxLayout(parent);

layout->addWidget(
    devicePanel
);

layout->addWidget(
    alarmTable
);
```

所以 AI 输出：

```json
{
  "type": "row",
  "children": [
    {
      "component": "device_status"
    },
    {
      "component": "alarm_table"
    }
  ]
}
```

在 Vue Renderer 里可能变：

```html
<div style="display:flex">
```

而 Qt Renderer 则需要映射：

```text
row
 ↓
QHBoxLayout

column
 ↓
QVBoxLayout

grid
 ↓
QGridLayout

tabs
 ↓
QTabWidget

splitter
 ↓
QSplitter
```

所以你需要专门写一个：

# Qt Layout Renderer

大概：

```cpp
QWidget* renderNode(
    const UiNode& node,
    QWidget* parent
)
{
    if (node.type == "component") {
        return registry.create(
            node.component,
            node.props,
            parent
        );
    }

    if (node.type == "row") {

        auto* container =
            new QWidget(parent);

        auto* layout =
            new QHBoxLayout(container);

        for (auto& child : node.children) {
            layout->addWidget(
                renderNode(
                    child,
                    container
                )
            );
        }

        return container;
    }
}
```

这就是 Qt 版的：

```text
Virtual DOM Renderer
```

你完全可以用 Vue 的思维理解它。

---

# 7. 甚至可以把我们的 Qt 方案理解成“自己造一个极简 Vue Runtime”

这其实是最有帮助的理解方法。

Vue 内部大概是：

```text
Template
   ↓
Virtual DOM
   ↓
Renderer
   ↓
DOM
```

你们的方案：

```text
Natural Language
       ↓
      LLM
       ↓
    UI JSON
       ↓
   Qt Renderer
       ↓
 QWidget Tree
```

非常像。

更进一步：

```text
Vue Component Registry
        ≈
Qt Component Registry

Vue props
        ≈
Qt component props schema

Vue emits
        ≈
Qt signal

Vue methods
        ≈
Qt slot / command

Vue DOM renderer
        ≈
Qt Widget renderer

Vue virtual DOM
        ≈
我们的 UI IR / UI Spec
```

所以你之前学过 Vue，其实对于理解这个架构是很有帮助的。

---

# 8. 但有一个非常大的区别：Qt 的组件往往更“重”

Vue 的：

```vue
<DeviceCard />
```

很多时候只是一个显示组件。

而公司的 Qt Widget 很可能：

```text
DevicePanelWidget
     │
     ├── 自己持有 Model
     ├── 自己连接 Controller
     ├── 自己发网络请求
     ├── 自己监听设备状态
     ├── 自己做线程处理
     └── 自己保存状态
```

也就是说它是：

> **UI + 一部分业务逻辑的组合体。**

这其实反而对你的项目是好事。

因为你的目标就是：

> 保留原有逻辑。

所以 AI 完全不要拆它。

例如：

```text
          AI

“展示设备监控”
       ↓
DeviceMonitorWidget
       ↓
┌────────────────────┐
│ 原来的 UI          │
│ 原来的信号槽       │
│ 原来的网络请求     │
│ 原来的 Controller  │
└────────────────────┘
```

AI 只负责把它拿出来。

---

# 9. 所以组件粒度和 Vue 项目会明显不同

如果是 Vue Generative UI，你可能会想封装：

```text
Button
Input
Card
Table
Badge
Select
Dialog
```

然后让 AI：

```text
拼 UI
```

但是你的 Qt 项目，我强烈建议不要这么干。

应该封装：

```text
设备信息面板

告警列表

网络拓扑图

实时趋势图

配置面板

设备控制面板
```

也就是：

> **业务级组件。**

AI 做：

```text
业务组件之间的组合
```

而不是：

```text
Button + Label + Input
```

组合成整个页面。

否则你会把已有 UI 全部重新实现一遍。

---

# 10. 举一个你非常容易理解的 Vue → Qt 对比

假设 Vue 有：

```vue
<template>

  <div class="page">

    <DeviceStatus
      :device-id="deviceId"
    />

    <AlarmTable
      :device-id="deviceId"
      severity="critical"
    />

  </div>

</template>
```

如果让 AI 动态生成：

```json
{
  "type": "Column",
  "children": [
    {
      "component": "DeviceStatus",
      "props": {
        "deviceId": "A01"
      }
    },
    {
      "component": "AlarmTable",
      "props": {
        "deviceId": "A01",
        "severity": "critical"
      }
    }
  ]
}
```

Vue renderer：

```text
Column
 ↓
<div>

DeviceStatus
 ↓
<DeviceStatus />

AlarmTable
 ↓
<AlarmTable />
```

Qt renderer：

```text
Column
 ↓
QVBoxLayout

DeviceStatus
 ↓
DeviceStatusWidget

AlarmTable
 ↓
AlarmTableWidget
```

最终效果完全一样。

只不过底层 renderer 不一样。

---

# 11. Qt 还有一个关键概念：signal / slot

你可以把它简单理解成 Vue 的：

```text
emit + event listener
```

Vue：

```vue
<AlarmTable
    @alarm-clicked="handleAlarm"
/>
```

子组件：

```javascript
emit(
    "alarm-clicked",
    alarm
)
```

Qt：

```cpp
connect(
    alarmTable,
    &AlarmTableWidget::alarmClicked,

    controller,
    &AlarmController::handleAlarmClicked
);
```

所以你们 Component Descriptor 最好也定义：

```json
{
  "component": "alarm_table",

  "props": {
    "deviceId": "string"
  },

  "events": [
    "alarmClicked",
    "refreshRequested"
  ]
}
```

于是你的 AI UI Framework 就非常像 Vue：

```text
props down
events up
```

---

# 12. Qt 还有一个 Vue 没有那么明显的问题：生命周期

Vue 生命周期：

```text
mounted
updated
unmounted
```

非常统一。

Qt Widget 生命周期则是：

```text
constructor
showEvent
hideEvent
closeEvent
destructor
deleteLater
```

再加 QObject parent-child ownership。

所以动态 UI 时，你必须特别注意：

```text
谁创建 Widget

谁作为 parent

什么时候销毁

signal 是否断开

网络请求是否停止

timer 是否停止
```

否则可能发生：

```text
页面已经换了

Widget 没释放

Timer 还在运行

请求还在继续

Signal 还在发
```

这也是 Qt 动态 UI 比 Vue 更需要工程约束的地方。

因此 Renderer 不能只有：

```cpp
create()
```

还应该有：

```text
mount

update

unmount

destroy
```

例如：

```cpp
class ComponentInstance
{
public:

    QWidget* widget;

    void updateProps(
        QJsonObject props
    );

    void unmount();

    void destroy();
};
```

---

# 13. QObject 的 parent 机制非常重要

Qt 有一种很方便的内存管理方式。

比如：

```cpp
QWidget* child =
    new QWidget(parent);
```

当：

```text
parent 被销毁
```

Qt 会把 child 一起销毁。

所以动态 UI Renderer 应该严格建立：

```text
UI Tree
       ↓
QObject Parent Tree
```

例如：

```text
GeneratedPage
   │
   ├── StatusPanel
   ├── AlarmTable
   └── Chart
```

对应：

```text
GeneratedPage QWidget
     │
     ├── DeviceStatusWidget
     ├── AlarmTableWidget
     └── TrendChartWidget
```

这样生命周期管理会简单很多。

---

# 14. 如果公司用的是 QML，情况又完全不同

这是你接下来一定要先搞清楚的一件事：

> **公司项目到底主要是 QWidget，还是 QML/Qt Quick？**

虽然都是 Qt，但实现路线差别不小。

如果是 QWidget：

```text
.ui
+
QWidget
+
C++
+
signal / slot
```

你需要：

```text
Component Registry
+
Factory
+
Qt Renderer
```

自己做得多一些。

但是如果项目是 QML：

```qml
DevicePanel {
    deviceId: "A01"
}
```

它本身就特别像 Vue。

例如：

```qml
Column {
    DeviceStatus {
        deviceId: "A01"
    }

    AlarmTable {
        severity: "critical"
    }
}
```

是不是已经很像：

```vue
<div>
    <DeviceStatus />
    <AlarmTable />
</div>
```

所以：

> **QML 可以理解成 Qt 世界里的“Vue template + CSS + 一部分 JS”。**

---

# 15. QML 和 Vue 的相似程度非常高

例如 Vue：

```vue
<MyButton
    :text="buttonText"
    @click="submit"
/>
```

QML：

```qml
MyButton {
    text: buttonText

    onClicked: {
        submit()
    }
}
```

Vue：

```vue
<div v-if="visible">
```

QML：

```qml
visible: condition
```

Vue：

```vue
<div v-for="item in list">
```

QML：

```qml
Repeater {
    model: items
}
```

Vue：

```text
computed
```

QML：

```text
property binding
```

所以如果公司是 QML：

> 你这个 Generative UI 项目会明显更容易。

---

# 16. QWidget 和 QML 的实施方案分别是什么

### 如果公司是 QWidget

我建议：

```text
Existing QWidget
       ↓
Component Adapter
       ↓
Component Registry
       ↓
UI JSON
       ↓
Qt Widget Renderer
```

例如：

```cpp
registry.registerComponent(
    "alarm_table",
    AlarmTableAdapter
);
```

Renderer：

```text
row
→ QHBoxLayout

column
→ QVBoxLayout

tabs
→ QTabWidget

split
→ QSplitter
```

这是比较典型的路线。

---

### 如果公司是 QML

可以变成：

```text
Existing QML Component
        ↓
Component Registry
        ↓
UI JSON
        ↓
QML Loader
```

甚至：

```json
{
  "component": "DevicePanel",
  "props": {
    "deviceId": "A01"
  }
}
```

可以非常直接映射：

```text
DevicePanel.qml

deviceId = A01
```

因此 QML 的实现会更加接近你熟悉的：

```text
Vue Dynamic Component
```

---

# 17. 还有一个很大的不同：Web CSS 和 Qt 样式机制

Vue：

```text
HTML
+
CSS
```

DOM 是结构，CSS 控制样式。

Qt Widgets 有：

```text
QSS
```

看起来类似 CSS：

```css
QPushButton {
    background: #333;
}
```

但不要认为它就是 CSS。

Qt 的视觉可能同时来自：

```text
QSS
+
QStyle
+
QPalette
+
Widget 自绘 paintEvent
+
资源文件 qrc
```

因此你更不能让 AI：

```text
重新生成 style
```

否则非常容易破坏公司原来的视觉系统。

正确的做法是：

```text
AI
 ↓
选择 Existing Widget
 ↓
Existing Widget 自动继承公司 Style
```

也就是说：

> **Style 根本不要进入 LLM 控制范围。**

这和 Vue Generative UI 最大的工程策略差异之一。

---

# 18. 你可以把整个项目理解成“Qt 版的低代码 Vue”

以后你们的系统内部其实相当于：

```text
                Vue

JSON Spec
   ↓
Vue Renderer
   ↓
Vue Components
   ↓
DOM


                你的系统

JSON Spec
   ↓
Qt Renderer
   ↓
Qt Business Components
   ↓
QWidget Tree
```

只是这个 Qt Renderer，需要你们自己做。

---

# 19. 我建议你特别注意“不要让 LLM 知道太多 Qt”

这是非常重要的设计原则。

不要给 LLM：

```json
{
  "class": "QWidget",
  "layout": "QHBoxLayout",
  "stretch": [1,2],
  "minimumWidth": 250,
  "sizePolicy": "Expanding"
}
```

不要。

应该：

```json
{
  "type": "row",
  "children": [...]
}
```

Renderer 自己决定：

```text
row
→ QHBoxLayout
```

同样：

不要：

```json
{
  "qtClass": "DeviceStatusWidget"
}
```

而是：

```json
{
  "component": "device_status"
}
```

为什么？

因为：

```text
AI 协议
```

不应该绑定：

```text
C++ class name
```

以后你把：

```text
DeviceStatusWidget
```

换成：

```text
NewDeviceOverview
```

AI 一点都不用知道。

---

# 20. 最适合你的架构现在可以进一步细化

结合你熟悉 Vue 的背景，可以这样理解：

```text
┌──────────────────────────────────────┐
│             Agent / LLM              │
│                                      │
│ 类似：Server-side UI Composer        │
└────────────────┬─────────────────────┘
                 │
                 │ JSON
                 ▼
┌──────────────────────────────────────┐
│              UI Spec                 │
│                                      │
│ 类似：Virtual DOM                    │
└────────────────┬─────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────┐
│          Qt UI Renderer              │
│                                      │
│ 类似：Vue Renderer                   │
└────────────────┬─────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────┐
│        Component Registry            │
│                                      │
│ 类似：Vue components{}               │
└────────────────┬─────────────────────┘
                 │
         ┌───────┼───────┐
         ▼       ▼       ▼
      Widget   Widget   Widget
         │       │       │
         └───────┼───────┘
                 ▼
          原 Controller
                 ▼
             原 Backend
```

你完全可以把：

> ┼───────┐
> ▼       ▼       ▼
> Widget   Widget   Widget
> │       │       │
> └────── **UI Spec 理解成 Virtual DOM。**

这是你从 Vue 迁移思维时最有用的类比。

---

# 21. 最终对具体实施方案产生的 5 个关键变化

相比“如果这是 Vue 项目”，Qt 项目需要额外做五件事情：

**第一，自己建设 Component Registry。**

Vue 已经天然有组件动态挂载机制；Qt Widgets 需要 factory。

**第二，为已有 Widget 做 Adapter。**

因为不同 Widget 的 constructor、setter、Controller 初始化往往不统一。

**第三，自己建设 Qt Renderer。**

把：

```text
Row / Column / Tabs / Grid
```

映射到：

```text
QHBoxLayout
QVBoxLayout
QTabWidget
QGridLayout
```

**第四，严格处理 QObject 生命周期。**

动态挂载、更新、卸载、销毁必须规范。

**第五，LLM 控制的粒度要明显高于 Vue 原子组件。**

最好：

```text
AI → Business Widget
```

而不是：

```text
AI → QPushButton / QLabel
```

---

## 所以如果用一句你熟悉 Vue 的语言描述我们接下来要做什么

其实就是：

> **我们要在现有 Qt 项目外面补一个类似 Vue Runtime 的薄层，让现有 QWidget/QML 变成“可被 JSON 动态调用的组件”，然后让 LLM 负责生成这个 JSON。**

而绝对不是：

> LLM 生成 Qt 页面。

这两个方向的工程风险差一个数量级。

接下来你们项目最值得先确认的是 **现有项目究竟以 QWidget + `.ui` 为主，还是 QML/Qt Quick 为主**。一旦确定这个，我就可以直接继续给你画出一版非常具体的 **“从一个现有 Qt 页面改造成 AI 可调用组件”代码级架构**，包括 `ComponentRegistry`、`ComponentAdapter`、`UI Schema`、`QtRenderer` 各自的 C++ 类应该怎么写。





=====================================================================


可以。你可以把 A2UI 理解成：

> **一套“AI 描述界面”的标准语言 + Agent 侧生成工具 + Client 侧 Renderer。**

它本身不是 React、Vue、Qt 这样的 UI 框架，而更像是夹在 **Agent 和真正 UI 框架之间的一层协议**。

截至目前，A2UI 仓库仍把 **v0.9.1 作为当前 production/stable release，v1.0 是 release candidate**。下面我主要按最新 v1.0 的架构讲，因为它的模块边界更清楚；你真正落地时仍应注意版本兼容。([GitHub][1])

---

# 一、先用一张图看懂 A2UI

整个 A2UI 可以先压缩成：

```text
                   ┌──────────────────┐
                   │      User        │
                   │   自然语言请求    │
                   └────────┬─────────┘
                            │
                            ▼
                   ┌──────────────────┐
                   │    A2UI Agent    │
                   │      + LLM       │
                   └────────┬─────────┘
                            │
                     读取 Catalog
                            │
                            ▼
                   ┌──────────────────┐
                   │ A2UI JSON Message│
                   │                  │
                   │ createSurface    │
                   │ updateComponents │
                   │ updateDataModel  │
                   └────────┬─────────┘
                            │
                      Transport
                 A2A / AG-UI / WS...
                            │
                            ▼
              ┌───────────────────────────┐
              │       A2UI Renderer       │
              │                           │
              │  Parser / Validator       │
              │  Surface Manager          │
              │  Data Model               │
              │  Component Resolver       │
              │  Action Handler           │
              └─────────────┬─────────────┘
                            │
                   Component Catalog
                            │
                     名称 → 真组件
                            │
           ┌────────────────┼────────────────┐
           ▼                ▼                ▼
        Button           Table          DevicePanel
           │                │                │
           ▼                ▼                ▼
       React组件         Flutter         Qt Widget
```

最核心的一句话就是：

> **Agent 只负责说“我要什么 UI”；Renderer 决定“这个 UI 在当前平台具体怎么实现”。**

这也是 A2UI 最大的设计思想。A2UI 官方明确把生成、传输、解析和 native rendering 分开；同一份声明式 A2UI JSON 可以被不同客户端映射到自己的 native components。([GitHub][1])

---

# 二、A2UI GitHub 项目本身有哪些主要模块

你打开现在的 A2UI 仓库，会看到类似：

```text
a2ui/
├── agent_sdks/
├── renderers/
├── specification/
├── samples/
├── conformance/
├── docs/
├── eval/
├── blueprints/
└── ...
```

其中你真正需要关注的是下面几个。

| 项目目录                 | 作用                                         | 你可以理解成    |
| -------------------- | ------------------------------------------ | --------- |
| `specification/`     | 定义 A2UI 协议、JSON Schema、Catalog 规范、不同协议版本   | “语言标准”    |
| `agent_sdks/python/` | Agent 侧帮助生成/处理 A2UI                        | “服务端 SDK” |
| `renderers/`         | 将 A2UI 转为 React/Lit/Angular/Flutter 等真实 UI | “浏览器引擎”   |
| `samples/`           | 完整端到端 Demo                                 | “参考项目”    |
| `conformance/`       | 协议兼容性相关内容                                  | “标准测试”    |

当前 `renderers/` 目录中已经能看到 Angular、Flutter、Lit、React、`web_core` 等实现；`agent_sdks/python/` 又分为 `a2ui_agent` 和 `a2ui_core`。([GitHub][2])

而 `specification/` 同时保存 v0.8、v0.9、v0.9.1、v1.0 等不同协议版本。([GitHub][3])

但这些只是**仓库怎么组织**。

对于你真正开发 Qt 项目，更重要的是下面的**运行时组件划分**。

---

# 三、真正的 A2UI 运行时有哪几个核心组件

我建议你记住九个东西：

```text
Agent
Catalog
A2UI Message
Transport
Renderer Core
Framework Adapter
Catalog Implementation
Surface + Data Model
Action / Function
```

其中最重要的是：

```text
Agent
   ↓
A2UI Message
   ↓
Renderer
   ↓
Catalog Implementation
   ↓
真实 UI Component
```

下面一个个讲。

---

# 四、第一部分：Agent

## Agent 的责任非常单纯

Agent 负责：

> **根据用户意图决定“应该展示什么 UI”。**

例如用户：

> 查看 A01 设备的状态和严重告警。

Agent 不能随便生成：

```text
QWidget
HTML
JavaScript
QML
```

而是先知道客户端允许什么。

例如客户端告诉它：

```text
我支持：

Row
Column
DeviceStatus
AlarmTable
TrendChart
```

然后 Agent 才生成：

```text
Column
 ├─ DeviceStatus
 └─ AlarmTable
```

官方定义的 Agent/Renderer 交互就是：Renderer 提供 Catalog 及使用说明，Agent 根据这些能力生成 UI，接收用户输入，再继续更新 UI/data。([A2UI][4])

所以：

```text
Agent = UI Planner
```

不是 UI Renderer。

---

# 五、第二部分：Catalog

这是理解 A2UI 最重要的概念之一。

## Catalog 是什么？

你可以把它理解成：

> **客户端给 AI 的“可使用组件说明书”。**

例如：

```json
{
  "components": {
    "DeviceStatus": {
      "description": "显示设备状态"
    },

    "AlarmTable": {
      "description": "显示告警列表"
    },

    "Row": {
      "description": "横向排列子组件"
    }
  }
}
```

Catalog 不只是告诉 AI：

```text
有哪些组件
```

还会告诉：

```text
组件有哪些属性
属性类型是什么
哪些属性必须提供
可以放在哪些父组件下
允许哪些子组件
有哪些函数
如何使用这些组件
```

v1 Catalog 顶层正式定义了诸如：

```text
catalogId
instructions
components
functions
```

这样的结构。([A2UI][5])

---

# 六、Catalog 和 Component Registry 有什么区别？

这点对你尤其重要。

假设：

```text
Catalog：
告诉 AI，“存在一个叫 AlarmTable 的东西。”
```

而：

```text
Registry：
告诉程序，“AlarmTable 对应 AlarmTableWidget。”
```

所以：

```text
                   Catalog
                      │
            给 LLM 看：“你能用什么”
                      │
                      ▼
                    Agent
                      │
              "AlarmTable"
                      │
                      ▼
                  Renderer
                      │
                      ▼
             Component Registry
                      │
                      ▼
          AlarmTableWidget(C++)
```

在 A2UI 官方术语里，Renderer 需要实现 Catalog，将 Schema 中的抽象组件映射为当前框架里的真实组件。([A2UI][4])

---

# 七、Catalog Transformer

这是稍微高级一点，但是以后你的公司项目非常有价值。

假设公司有：

```text
300 个 Qt 业务组件
```

如果每次全部告诉 LLM：

```text
300 components
+ 300 schemas
+ descriptions
```

Prompt 会非常大。

所以 A2UI 现在还引入了：

> **Catalog Transformer**

作用是：

```text
完整 Catalog
     ↓
Transformer
     ↓
当前任务需要的 Catalog
```

例如用户说：

> 查看设备告警。

只提供：

```text
DeviceStatus
AlarmTable
TrendChart
Row
Column
Tabs
```

而不是把：

```text
UserManagement
ReportEditor
NetworkConfig
SystemSettings
...
```

全部扔进去。

官方 glossary 把它定义为对原 Catalog 进行过滤、适配或修改的一组规则，也明确提出了 token 优化、权限限制等用途。([A2UI][4])

这个东西和我们之前讨论的：

```text
search_components
```

其实思想高度一致。

---

# 八、第三部分：A2UI Message

这才是真正“在网络上传输”的东西。

A2UI 不是把：

```text
整个页面 JSON
```

一次生成完就结束。

它采用的是：

> **一连串增量消息。**

v1.0 envelope 当前定义了六类 Agent → Renderer 消息：

```text
createSurface

updateComponents

updateDataModel

deleteSurface

callRendererFunction

agentFunctionResponse
```

([A2UI][5])

你现阶段最应该理解前三个。

---

# 九、createSurface

Surface 可以理解为：

> **AI 可以控制的一块 UI 区域。**

例如你的 Qt 程序：

```text
┌───────────────────────────────────┐
│ 工具栏                            │
├───────────┬───────────────────────┤
│           │                       │
│ 导航栏    │     AI Workspace      │
│           │                       │
│           │     Surface           │
│           │                       │
└───────────┴───────────────────────┘
```

那么：

```text
AI Workspace
```

就是一个 Surface。

Agent：

```json
{
  "version": "v1.0",

  "createSurface": {
    "surfaceId": "device_workspace"
  }
}
```

Renderer 收到：

```text
创建一个：
device_workspace
```

A2UI v1 规定 Surface 是顶层容器，并隐式指向 ID 为 `root` 的根组件。([A2UI][5])

---

# 十、updateComponents

这是真正描述：

> **界面结构**

的消息。

例如：

```json
{
  "updateComponents": {

    "surfaceId": "device_workspace",

    "components": [

      {
        "id": "root",
        "component": "Column",
        "children": [
          "status",
          "alarms"
        ]
      },

      {
        "id": "status",
        "component": "DeviceStatus"
      },

      {
        "id": "alarms",
        "component": "AlarmTable"
      }

    ]
  }
}
```

注意这里一个非常重要的设计：

# A2UI 不是嵌套 JSON Tree。

而是：

> **Flat List + ID Reference**

也就是：

```text
root
 ├─ status
 └─ alarms
```

在 JSON 里却是：

```text
root
status
alarms
```

平铺。

官方称为：

> adjacency list model

这样做的原因是更容易让 LLM 生成、更容易流式发送，也可以直接通过 ID 更新任意组件，而不用重新生成整个深层 JSON tree。([A2UI][6])

---

# 十一、为什么 A2UI 特别强调 Flat List

假设传统 Vue 风格：

```json
{
  "Column": {
    "children": [
      {
        "Row": {
          "children": [
            {
              "Card": {
                ...
              }
            }
          ]
        }
      }
    ]
  }
}
```

如果 AI 要修改最里面 Card：

```text
很麻烦。
```

但 A2UI：

```text
root
row1
card1
card2
button1
```

AI 可以直接：

```text
update card1
```

所以非常适合：

```text
Streaming UI
```

例如：

```text
0 ms：
创建 Surface

100 ms：
创建标题

200 ms：
创建状态卡

400 ms：
创建告警表

800 ms：
补充数据
```

而不是必须等完整页面生成完。

官方 Renderer 也明确要求处理 incremental messages、progressive rendering 和 server-initiated updates。([A2UI][7])

---

# 十二、第四部分：Data Model

A2UI 有一个非常漂亮的设计：

> **UI Structure 和 Data 分离。**

也就是说：

```text
Component Tree
```

负责：

```text
界面长什么结构
```

而：

```text
Data Model
```

负责：

```text
界面当前有什么数据
```

例如 UI：

```text
DeviceStatus
```

绑定：

```text
/device/status
```

Data Model：

```json
{
  "device": {
    "status": "Running",
    "temperature": 62
  }
}
```

于是：

```text
DeviceStatus
        │
        ▼
 /device/status
        │
        ▼
    "Running"
```

A2UI 的 Data Model 是每个 Surface 独立维护的、JSON-like 的可观察状态；组件可以通过 path 绑定其中的数据，Agent 和 Renderer 两侧都可以推动状态更新。([A2UI][4])

---

# 十三、updateDataModel 为什么非常重要

假设：

```text
AlarmTable UI
```

已经渲染出来。

过了 5 秒，后台出现新告警。

你没有必要重新：

```text
updateComponents
```

只需要：

```json
{
  "updateDataModel": {
    "surfaceId": "device_workspace",

    "path": "/alarms",

    "value": [...]
  }
}
```

Renderer：

```text
更新 Data Model
      ↓
AlarmTable 自动刷新
```

v1.0 明确把 `updateComponents` 和 `updateDataModel` 分开，因此可以只修改数据而无需重新发送整个 UI 结构。([A2UI][5])

这其实非常像你熟悉的 Vue：

```text
Template
  +
Reactive State
```

---

# 十四、第五部分：Transport

Transport 只干一件事情：

> **把 Agent 的 A2UI Message 送到 Renderer，再把用户事件送回 Agent。**

A2UI 本身：

> **不强制规定你一定用 HTTP、WebSocket 还是 AG-UI。**

官方明确把 A2UI 设计成 transport-agnostic；仓库 README 当前直接列出了 A2A、AG-UI 等搭配方式，而协议文档也讨论了 WebSocket/gRPC 等双向流式场景。([GitHub][1])

所以：

```text
A2UI ≠ 网络协议
```

例如完全可以：

```text
A2UI + HTTP
```

也可以：

```text
A2UI + WebSocket
```

也可以：

```text
A2UI + AG-UI
```

---

# 十五、这也解释了为什么 AG-UI 和 A2UI 不冲突

现在你应该可以很清楚地看出来：

```text
A2UI
=
传什么 UI 内容
```

而：

```text
AG-UI
=
Agent 和 Client 怎么实时通信
```

所以：

```text
Agent
   │
   │ A2UI JSON
   │
   │ 通过 AG-UI 发送
   ▼
Client
```

A2UI 官方也明确提供了“A2UI with AG-UI”的集成路径。([A2UI][8])

---

# 十六、第六部分：Renderer Core

这是客户端最重要的部分。

如果你最后要做 Qt：

> **这是你真正需要重点开发的东西。**

Renderer Core 负责：

```text
接收 A2UI Message
        ↓
解析
        ↓
校验
        ↓
维护 Surface
        ↓
维护 Component Tree
        ↓
维护 Data Model
        ↓
触发 Framework Adapter
```

官方对 Renderer 的职责定义得很明确：

> buffer/handle A2UI messages、实现生命周期、渲染 widgets、处理 data binding、处理 incremental updates，并把用户 action 路由回 Agent。([A2UI][7])

可以理解成：

```text
                 Qt Renderer Core

┌──────────────────────────────────────┐
│ Message Parser                       │
│                                      │
│ Validator                            │
│                                      │
│ Surface Store                        │
│                                      │
│ Component Tree                       │
│                                      │
│ Data Model Store                     │
│                                      │
│ Binding Engine                       │
│                                      │
│ Action Dispatcher                    │
└──────────────────────────────────────┘
```

---

# 十七、Framework Adapter

Renderer Core 还是：

```text
平台无关
```

Framework Adapter 才知道：

```text
React
Angular
Flutter
Qt
```

例如：

```text
A2UI Row
```

React Adapter：

```html
<div class="row">
```

Flutter：

```text
Row(...)
```

Qt：

```cpp
new QHBoxLayout()
```

所以：

```text
           A2UI Core

              Row
               │
       ┌───────┼───────┐
       ▼       ▼       ▼
     React   Flutter    Qt
       │       │        │
     div      Row   QHBoxLayout
```

官方把 Renderer stack 明确拆成：

```text
Core Library
Catalog Schema
Framework Adapter
Catalog Implementation
```

这四层。([A2UI][4])

---

# 十八、Catalog Implementation

这个名字特别重要。

Catalog Schema 只是：

```text
“AlarmTable 是什么”
```

Catalog Implementation 才是：

```text
“AlarmTable 真正如何创建”
```

对于你的 Qt：

```text
Catalog Schema

AlarmTable:
    deviceId: string
    severity: string
```

对应：

```text
Qt Catalog Implementation

AlarmTable
      ↓
AlarmTableAdapter
      ↓
AlarmTableWidget
```

例如：

```cpp
registry.registerComponent(
    "AlarmTable",

    [](const Props& props) {

        auto* widget =
            new AlarmTableWidget();

        widget->setDeviceId(
            props["deviceId"]
        );

        return widget;
    }
);
```

所以：

> **Catalog Implementation 基本就是我们之前说的 Component Registry + Component Adapter。**

---

# 十九、第七部分：Component

现在再来看：

> A2UI Component 到底是什么？

它不是：

```text
React Component
```

也不是：

```text
QWidget
```

而是：

> **一个抽象的 UI 类型。**

例如：

```json
{
  "id": "submit_button",
  "component": "Button"
}
```

A2UI 的 Basic Catalog 提供了一组基本 UI component 类型，例如：

```text
Text
Button
TextField
Row
Column
Card
...
```

但 A2UI 允许你自定义 Catalog，因此你的公司完全可以定义领域组件：

```text
DeviceStatus
AlarmTable
TrendChart
NetworkTopology
DeviceController
```

官方明确提出 Catalog 可以既包含基础 Button/Label/Row，也可以包含 HotelCheckout、FlightSelector 这样的领域级组件。([A2UI][4])

这就是为什么 A2UI 很适合你的场景。

---

# 二十、第八部分：Action

界面不是只看，还需要：

```text
点击
提交
选择
刷新
```

A2UI 把它抽象成：

> Action

目前主要有两种思路。

## 第一种：发回 Agent

例如：

```json
{
  "action": {
    "event": {
      "name": "refresh_device",

      "context": {
        "deviceId": "A01"
      }
    }
  }
}
```

流程：

```text
用户点 Refresh
      ↓
Renderer
      ↓
refresh_device
      ↓
Agent
      ↓
Agent 决定下一步
```

---

## 第二种：在客户端执行本地 Function

例如：

```json
{
  "action": {

    "functionCall": {
      "call": "openDeviceDetail",

      "args": {
        "deviceId": "A01"
      }
    }
  }
}
```

流程：

```text
Button
  ↓
Renderer
  ↓
本地 function
  ↓
Qt Controller
```

A2UI v1 明确区分发送给 Agent 的 event 和在 Renderer 侧执行的 `functionCall`。([A2UI][5])

---

# 二十一、这个设计对你的 Qt 项目非常关键

因为你希望：

> 保留现有后端请求。

那么完全可以：

```text
                AlarmTableWidget

用户点击“Refresh”
        │
        ▼
原来的 signal
        │
        ▼
原 Controller
        │
        ▼
原 Backend
```

不一定所有 Action 都发 Agent。

也就是说：

```text
简单业务操作
→ Renderer Local Function / 原 Qt signal-slot

需要 AI 决策的操作
→ Agent Event
```

这个边界非常合理。

---

# 二十二、Validator

Validator 负责：

> **AI 生成的 UI 合不合法。**

例如 Catalog 说：

```text
MenuItem
只能放在 Menu
```

AI 却生成：

```text
Column
 └─ MenuItem
```

Validator：

```text
拒绝。
```

A2UI v1 Catalog 可以通过 `allowedParents` / `allowedChildren` 等规则限制组件结构，协议也定义了对应的 validation error；另外 Catalog 的 child references 需要使用指定的 ComponentId/ChildList schema，以便验证器检查组件引用。([A2UI][5])

对于公司项目，这是非常重要的安全层。

---

# 二十三、现在把所有组件放到一起

最终的责任分工就是：

| 模块                     | 核心职责                    | 不应该负责       |
| ---------------------- | ----------------------- | ----------- |
| Agent                  | 理解用户、规划 UI              | 真正绘制 UI     |
| Catalog                | 告诉 Agent 有什么组件          | 创建真实 Widget |
| Catalog Transformer    | 筛选当前可用组件                | UI 渲染       |
| A2UI Message           | 描述 UI/数据变化              | 网络通信        |
| Transport              | 传输消息                    | 理解 UI       |
| Renderer Core          | 管理 A2UI 生命周期和状态         | 具体 Qt 样式    |
| Framework Adapter      | A2UI → Qt/React/Flutter | Agent 推理    |
| Catalog Implementation | Component → 真实组件        | 自然语言理解      |
| Surface                | 管理一个 AI UI 区域           | 业务逻辑        |
| Data Model             | 保存 UI 状态                | UI 绘制       |
| Action Handler         | 用户事件/函数调用               | UI 规划       |
| Validator              | 检查 AI 输出是否合法            | 业务执行        |

---

# 二十四、完整工作流：从用户说一句话开始

用你的项目举例。

用户：

> “打开 A01 的状态和告警页面，状态放上面，严重告警放下面。”

---

## Step 1：Renderer 告诉 Agent 自己有什么能力

Qt Client：

```text
Catalog

Column
Row
DeviceStatus
AlarmTable
TrendChart
```

其中：

```text
DeviceStatus(deviceId)

AlarmTable(
    deviceId,
    severity
)
```

Catalog 被提供给 Agent，使 Agent 只能生成客户端支持的组件。([A2UI][4])

---

## Step 2：LLM 理解自然语言

Agent 理解成：

```text
需要：

Column
 ├─ DeviceStatus
 └─ AlarmTable

deviceId = A01
severity = critical
```

---

## Step 3：Agent 创建 Surface

```json
{
  "version": "v1.0",

  "createSurface": {
    "surfaceId": "device_page"
  }
}
```

---

## Step 4：Agent 生成 Component Structure

```json
{
  "version": "v1.0",

  "updateComponents": {

    "surfaceId": "device_page",

    "components": [

      {
        "id": "root",
        "component": "Column",
        "children": [
          "status",
          "alarms"
        ]
      },

      {
        "id": "status",
        "component": "DeviceStatus",
        "deviceId": "A01"
      },

      {
        "id": "alarms",
        "component": "AlarmTable",
        "deviceId": "A01",
        "severity": "critical"
      }

    ]
  }
}
```

这种 flat-list + ID reference 正是 A2UI 用于增量 UI 的核心结构。([A2UI][5])

---

# 二十五、Step 5：Transport 发给 Qt

例如：

```text
Agent
   │
   │ AG-UI / WebSocket / HTTP
   ▼
Qt Client
```

Transport：

```text
完全不理解：
DeviceStatus 是什么。

只是传 JSON。
```

---

# 二十六、Step 6：Qt Renderer Core 接收

Qt：

```text
JSON
 ↓
A2UI Parser
 ↓
Validator
 ↓
Surface Manager
 ↓
Component Tree
```

得到：

```text
device_page

root
 ├─ status
 └─ alarms
```

---

# 二十七、Step 7：Qt Framework Adapter 开始工作

看到：

```text
Column
```

Qt Adapter：

```text
Column
 ↓
QVBoxLayout
```

看到：

```text
DeviceStatus
```

去 Registry：

```text
DeviceStatus
      ↓
DeviceStatusAdapter
      ↓
DeviceStatusWidget
```

看到：

```text
AlarmTable
```

变成：

```text
AlarmTable
      ↓
AlarmTableAdapter
      ↓
AlarmTableWidget
```

最终：

```text
QWidget

┌────────────────────────┐
│ DeviceStatusWidget     │
├────────────────────────┤
│                        │
│ AlarmTableWidget       │
│                        │
└────────────────────────┘
```

Renderer 将 A2UI 抽象组件映射成当前框架 native widgets，正是官方对 Renderer 的核心定义。([A2UI][7])

---

# 二十八、Step 8：原 Qt Widget 继续自己的业务逻辑

这一部分 A2UI 根本不用知道。

```text
AlarmTableWidget
        │
        │ signal
        ▼
AlarmController
        │
        ▼
AlarmService
        │
        ▼
公司原 backend
```

所以：

> **A2UI 不需要替换你的后端逻辑。**

它只是改变：

```text
谁决定哪个 Widget 出现。
```

---

# 二十九、Step 9：用户点击界面

例如用户：

> 点击某条告警。

如果这是原 Qt Widget 的业务逻辑：

```text
AlarmTableWidget
      ↓
signal
      ↓
Controller
```

直接继续执行。

如果这是一个 AI Action：

```text
“分析这个告警”
```

则：

```text
Qt
 ↓
Action
 ↓
Transport
 ↓
Agent
```

---

# 三十、Step 10：Agent 增量修改 UI

Agent 分析完：

> 这个告警可能是温度异常。

不需要重新生成整个页面。

只发送新的：

```text
updateComponents
```

例如增加：

```text
DiagnosisPanel
```

原来：

```text
root
 ├─ status
 └─ alarms
```

变：

```text
root
 ├─ status
 ├─ alarms
 └─ diagnosis
```

这就是：

> **Incremental Generative UI**

而不是传统：

```text
LLM 每次重新生成整个页面。
```

---

# 三十一、所以最终的 A2UI 工作流可以压缩成这张图

```text
① Renderer / Qt
        │
        │ Catalog
        │
        ▼
② Agent + LLM
        │
        │ 用户自然语言
        ▼
③ UI Planner
        │
        │
        ▼
④ A2UI Messages
   createSurface
   updateComponents
   updateDataModel
        │
        ▼
⑤ Transport
   AG-UI / A2A / WS
        │
        ▼
⑥ Qt Renderer Core
   Parser
   Validator
   Surface Manager
   Data Model
        │
        ▼
⑦ Qt Framework Adapter
        │
        ▼
⑧ Component Registry
        │
        ├── Column
        │     ↓
        │   QVBoxLayout
        │
        ├── AlarmTable
        │     ↓
        │   AlarmTableWidget
        │
        └── DeviceStatus
              ↓
            DeviceStatusWidget
        │
        ▼
⑨ Existing Qt Logic
   signal / slot
   Controller
   Backend
        │
        ▼
⑩ User Action
        │
        ├── Local Qt Logic
        │
        └── Agent Event
                ↓
             Agent
                ↓
         updateComponents
```

---

# 三十二、你现在最应该抓住的三个模块

如果我们接下来真要把 A2UI 思想用到你公司的 Qt 项目里，其实先不用把整个 A2UI 都实现。

你只需要先抓：

```text
Catalog
     ↓
Renderer
     ↓
Component Registry
```

换成你的项目：

```text
Company Qt Catalog

DeviceStatus
AlarmTable
TrendChart
Row
Column
       ↓

Qt A2UI Renderer
       ↓

Company Component Registry
       ↓

现有 QWidget / QML
```

其中真正开发量最大的其实不是 Agent。

而是：

> **Qt Renderer + Catalog Implementation。**

Agent 这一端，A2UI 已经把问题转化成了：

```text
给 LLM Catalog
+
让 LLM 按 Schema 生成 A2UI
```

而 Qt 这一端才需要解决：

```text
A2UI Component
        ↓
如何对应已有 Qt Widget

A2UI Props
        ↓
如何对应 Widget 初始化参数

A2UI Layout
        ↓
如何对应 Qt Layout

A2UI Action
        ↓
如何对应原 signal / slot
```

这也正是我们这个项目下一步最应该继续深入的地方。

[1]: https://github.com/a2ui-project/a2ui "GitHub - a2ui-project/a2ui · GitHub"
[2]: https://github.com/a2ui-project/a2ui/tree/main/renderers "a2ui/renderers at main · a2ui-project/a2ui · GitHub"
[3]: https://github.com/a2ui-project/a2ui/tree/main/specification "a2ui/specification at main · a2ui-project/a2ui · GitHub"
[4]: https://a2ui.org/concepts/glossary/ "Glossary - A2UI"
[5]: https://a2ui.org/specification/v1.0-a2ui/ "A2UI Protocol - A2UI"
[6]: https://a2ui.org/concepts/components/ "Components & Structure - A2UI"
[7]: https://a2ui.org/reference/renderers/ "Renderers (Clients) - A2UI"
[8]: https://a2ui.org/guides/a2ui-with-any-agent-framework/?utm_source=chatgpt.com "Use A2UI with Any Agent Framework (AG-UI)"












===============================================
可以把业务场景直接换成一个任何人都能看懂的 **“AI 动态桌面工具箱”**。这样 Demo 的重点完全落在“自然语言 → 动态组合 Qt 组件”，不会被设备、告警、监控等业务概念干扰。

# Demo：AI 动态桌面工具箱

## 1. 技术选型

| 部分       | 技术                                          |
| -------- | ------------------------------------------- |
| Agent 后端 | Python + FastAPI                            |
| LLM      | 任意支持 JSON Structured Output 的模型             |
| 前端       | Python + PySide6 / Qt Widgets               |
| 通信       | HTTP + JSON                                 |
| UI 协议    | 精简版 A2UI：`createSurface + updateComponents` |

暂时不实现：

```text
MCP / AG-UI / Action / DataModel / Streaming / WebSocket
```

---

# 2. 示例组件

只做 4 个真实 Qt Widget：

| Component    | 功能        |
| ------------ | --------- |
| `Clock`      | 显示当前时间    |
| `Calculator` | 简单计算器     |
| `TodoList`   | 添加、删除待办事项 |
| `NotePad`    | 简易文本便签    |

再提供两个布局组件：

```text
Row     → QHBoxLayout
Column  → QVBoxLayout
```

每个 Widget 自己包含自己的界面和逻辑。

例如：

```text
CalculatorWidget
├─ 输入框
├─ 数字按钮
├─ 计算逻辑
└─ 结果显示
```

Renderer 完全不关心计算器内部怎么工作。

---

# 3. 项目架构

```text
用户自然语言
     │
     ▼
┌─────────────────────┐
│ FastAPI Backend     │
│                     │
│ Catalog             │
│    ↓                │
│ LLM Agent           │
│    ↓                │
│ A2UI JSON           │
└──────────┬──────────┘
           │ HTTP
           ▼
┌─────────────────────┐
│ PySide6 Frontend    │
│                     │
│ Renderer            │
│    ↓                │
│ Qt Adapter          │
│    ↓                │
│ Component Registry  │
│    ↓                │
│ Qt Widgets          │
└─────────────────────┘
```

---

# 4. Catalog

Agent 只知道这些组件：

```json
{
  "components": {
    "Row": {
      "description": "横向排列子组件"
    },
    "Column": {
      "description": "纵向排列子组件"
    },
    "Clock": {
      "description": "显示当前时间"
    },
    "Calculator": {
      "description": "计算器"
    },
    "TodoList": {
      "description": "待办事项列表"
    },
    "NotePad": {
      "description": "文本便签"
    }
  }
}
```

Agent 只能使用 Catalog 中出现的组件。

---

# 5. 一个实际例子

用户输入：

> 左边放一个计算器，右边放便签，下面放待办列表。

Agent 输出：

```json
{
  "createSurface": {
    "surfaceId": "main"
  }
}
```

以及：

```json
{
  "updateComponents": {
    "surfaceId": "main",
    "components": [
      {
        "id": "root",
        "component": "Column",
        "children": ["top", "todo"]
      },
      {
        "id": "top",
        "component": "Row",
        "children": ["calculator", "note"]
      },
      {
        "id": "calculator",
        "component": "Calculator"
      },
      {
        "id": "note",
        "component": "NotePad"
      },
      {
        "id": "todo",
        "component": "TodoList"
      }
    ]
  }
}
```

Qt 最终动态创建：

```text
┌────────────────┬────────────────┐
│                │                │
│   Calculator   │    NotePad     │
│                │                │
├────────────────┴────────────────┤
│                                 │
│            TodoList             │
│                                 │
└─────────────────────────────────┘
```

这个页面**不是提前写好的**。

是 Agent 根据自然语言组合出来的。

---

# 6. 前端核心模块

项目只需要三个核心模块。

### `renderer.py`

负责解析 A2UI JSON：

```text
JSON
 ↓
找到 root
 ↓
解析组件树
 ↓
交给 Qt Adapter
```

不包含 Qt 组件实现。

### `qt_adapter.py`

负责：

```text
Row
→ QHBoxLayout

Column
→ QVBoxLayout

Calculator
→ Registry.create("Calculator")
```

### `registry.py`

保存映射：

```python
COMPONENTS = {
    "Clock": ClockWidget,
    "Calculator": CalculatorWidget,
    "TodoList": TodoListWidget,
    "NotePad": NotePadWidget
}
```

---

# 7. 项目目录

```text
a2ui-qt-demo/
│
├── backend/
│   ├── main.py
│   ├── agent.py
│   └── catalog.json
│
├── frontend/
│   ├── main.py
│   ├── api_client.py
│   ├── renderer.py
│   ├── qt_adapter.py
│   ├── registry.py
│   │
│   └── widgets/
│       ├── clock.py
│       ├── calculator.py
│       ├── todo_list.py
│       └── note_pad.py
│
└── requirements.txt
```

---

# 8. 最终工作流

```text
① 用户输入自然语言

“左边计算器，右边便签，下面待办”

        ↓

② Qt 调用 FastAPI

POST /render

        ↓

③ Agent 读取 Catalog

        ↓

④ LLM 生成 A2UI JSON

        ↓

⑤ Renderer 解析组件树

        ↓

⑥ Qt Adapter

Row → QHBoxLayout
Column → QVBoxLayout

        ↓

⑦ Component Registry

Calculator → CalculatorWidget
NotePad    → NotePadWidget
TodoList   → TodoListWidget

        ↓

⑧ Qt 动态渲染完整界面
```

---

# 9. Demo 测试用例

只测试这 4 条就足够证明方案：

**测试 1**

> 左边计算器，右边便签。

```text
Row
├ Calculator
└ NotePad
```

**测试 2**

> 上面时钟，下面待办列表。

```text
Column
├ Clock
└ TodoList
```

**测试 3**

> 左边放时钟和计算器，右边放一个大便签。

```text
Row
├ Column
│  ├ Clock
│  └ Calculator
└ NotePad
```

**测试 4**

> 两个计算器左右并排，下面放待办。

```text
Column
├ Row
│  ├ Calculator
│  └ Calculator
└ TodoList
```

第四条尤其重要，因为它证明：

> **同一个组件可以被 Agent 任意实例化和组合，而不是“识别意图后打开固定页面”。**

---

# 10. 实施顺序

```text
Step 1
实现 4 个独立 PySide6 Widget

        ↓

Step 2
实现 Component Registry

        ↓

Step 3
实现 Row / Column Qt Adapter

        ↓

Step 4
手写一份 A2UI JSON
验证 JSON → Qt 动态界面

        ↓

Step 5
实现 FastAPI /render

        ↓

Step 6
加入 Catalog + LLM
实现自然语言 → A2UI JSON

        ↓

Step 7
测试不同自然语言能否产生不同组件树
```

**最关键的是 Step 4。**

先完全不接 LLM，用手写 JSON 验证：

```text
A2UI JSON
→ Renderer
→ Qt Adapter
→ Existing Widget
```

这条链路跑通，再接 Agent。这样如果出现问题，你能明确知道是 **Qt Renderer 的问题还是 LLM 生成的问题**。

这个 Demo 做完，真正能够回答的核心问题就是：

> **“公司已有 Qt Widget 能否被封装成 Catalog 中的组件，并让 AI 根据自然语言真正动态组合，而不是打开固定页面？”**

如果答案是可以，下一阶段再考虑把 PySide6 Widget 换成公司的 C++ QWidget，以及逐步增加更完整的 A2UI 能力。

这个精简方案已经把验证范围收敛到“自然语言动态组合 Qt 组件”，下一步可直接按 7 步实施顺序落地并用 4 条测试用例验收，先验证 JSON 到 Qt 的渲染链路再接入 LLM。
