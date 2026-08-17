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
