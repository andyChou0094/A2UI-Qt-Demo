# Ubuntu 运行细节

更新时间：2026-08-24

第一次启动请优先阅读[项目启动说明](getting-started.md)。本文记录缓存、进程、测试和 Provider
评测等进阶细节。

## 运行模型

`scripts/start_ubuntu_demo.sh` 是 Ubuntu 桌面的唯一启动入口。它只运行 GCC 7.3.0 或 9.3.0 受控
构建产生的 ELF，不调用系统编译器，也不自动修改构建策略。

未指定 `--build-dir` 时，入口只按顺序发现仓库内的 `build-gcc-9.3.0` 和
`build-gcc-7.3.0`。它不会自动搜索 `/tmp` 或其他目录。使用临时构建时需要每次执行：

```bash
scripts/start_ubuntu_demo.sh --build-dir /tmp/a2ui-build930
```

仓库内标准构建存在且未过期时，后续启动可以简化为：

```bash
scripts/start_ubuntu_demo.sh
```

启动分为四个阶段：

```text
配置与依赖预检
      ↓
准备或复用 XDG Qt/ICU 与 Linux venv
      ↓
启动 127.0.0.1 FastAPI 并等待 /health
      ↓
设置 xcb 环境并启动唯一 HostShell
```

在准备运行缓存或启动进程前，入口会比较目标 ELF 与 `frontend/app`、`frontend/composition`、
`frontend/widgets`、`shared/default-surface.json`、顶层/前端 CMake 文件及构建策略的时间。任一
输入较新时，入口拒绝旧 ELF，并提示使用 `tools/build-matrix.ps1` 或
`tools/container-build.sh` 重新执行受控构建；不会调用系统 GCC 11、自动构建或选择更旧候选。

## Provider 本地配置

```bash
scripts/configure_demo_provider.sh
```

脚本创建 `config/provider.local.env`，权限为 `0600`，Git 会忽略该文件。运行入口不使用
`source`，只按数据解析五个白名单字段；未知字段、空值、符号链接或过宽权限都会在启动前被
拒绝。

配置优先级从高到低为：

1. 当前进程环境变量；
2. `config/provider.local.env`；
3. endpoint、model 和超时的安全默认值。

API Key 只传给后端子进程，不传给 Qt，不进入 Prompt、Catalog、LayoutPlan、SurfaceSpec、
日志或评测结果。

## XDG 隔离

- Qt/ICU 派生缓存和 Linux venv：`${XDG_CACHE_HOME:-$HOME/.cache}/a2ui-qt-demo`
- SQLite：`${XDG_DATA_HOME:-$HOME/.local/share}/a2ui-qt-demo/calculations.sqlite3`
- 后端日志、PID 和运行状态：`${XDG_STATE_HOME:-$HOME/.local/state}/a2ui-qt-demo`

Qt/ICU 缓存键包含版本和来源归档摘要。Linux venv 缓存键包含 Python 主次版本与
`backend/requirements.txt` 摘要。缓存不匹配时，入口会在同一父目录中准备临时内容，校验
后原子替换，不会混用不同版本。

需要手工失效缓存时，只删除 XDG cache 下对应的 `runtime/qt-5.12.8-icu-56-*` 或
`python/py-*-req-*` 子目录。不要删除仓库 `.cache` 中的离线来源归档。

## 进程所有权与退出

入口在端口空闲后才创建后端，并记录实际 PID 与 `/proc` 启动标识。退出时：

1. 先终止身份仍匹配的自有 Qt 与后端子进程；
2. 超时后才升级处理；
3. 最后验证回环端口释放。

入口不会根据端口号杀死未知进程。启动前已经占用端口的进程会被保留，并产生中文冲突诊断。

## Surface 文档操作

工作台提供“导入 DSL”“导出 DSL”和“恢复默认排版”。

### 导入

文件必须是不超过 64 KiB 的 UTF-8 JSON。后端 `POST /surface/import` 先检查大小、JSON、
封闭 Schema、Catalog、节点、深度和图结构；Qt 再执行独立校验和 Renderer 暂存。两层均成功
后才替换当前页面。

### 导出

前端把当前已应用 Surface 发送到 `POST /surface/export` 复核，再通过 `QSaveFile` 原子写入
规范化 JSON。导出不包含 QWidget 状态、业务数据、主题、日志或 Provider 配置。

### 恢复默认

默认文档的单一源是 `shared/default-surface.json`。同一文件进入 Qt resource，并由后端
`GET /surface/default` 加载。恢复流程仍使用校验、暂存和原子提交。

接口示例：

```bash
curl --fail http://127.0.0.1:8000/surface/default

curl --fail -H 'Content-Type: application/json' \
  --data-binary @shared/default-surface.json \
  http://127.0.0.1:8000/surface/import

curl --fail -H 'Content-Type: application/json' \
  --data-binary @shared/default-surface.json \
  http://127.0.0.1:8000/surface/export \
  --output surface-main.json
```

## 回归与截图

当前通过数、缩放/截图结论和环境 skip 口径统一见
[验证与评测报告](verification-and-evaluation.md)。本节只保留复现入口。

```bash
scripts/run_ubuntu_regression.sh build-gcc-9.3.0
scripts/run_ui_smoke.sh build-gcc-9.3.0 /path/to/qt-5.12.8/gcc_64
tests/runtime/test_start_ubuntu_demo.sh
tests/runtime/test_runtime_lifecycle.sh
```

回归入口设置 `A2UI_TEST_SOURCE_ROOT`，不依赖 `/workspace`。缩放冒烟在 900×700 下检查
100%、125%、150% 并生成单窗口截图。

## Provider 测评与确定性 DSL 回归

当前 Provider 结果、中位数、最慢样本、失败分类和证据限制见
[验证与评测报告](verification-and-evaluation.md)；单文件样本见[评测数据索引](evaluation-data/README.md)。

### 七场景门禁

```bash
deepseek_api='<rotated-provider-key>' \
A2UI_KEY_ROTATED=1 \
scripts/run_provider_acceptance.sh
```

只有 7/7 场景和敏感信息扫描都通过，runner 才会更新正式结果。

### 50 样本基线

```bash
A2UI_LLM_ENDPOINT='https://api.deepseek.com/chat/completions' \
A2UI_LLM_MODEL='deepseek-v4-flash' \
A2UI_LLM_API_KEY='<rotated-provider-key>' \
python3 scripts/run_provider_baseline.py --confirm-50-requests
```

脚本会先提示请求次数和成本风险。不足 50 个最终样本时只写未完成报告，不覆盖正式基线。

### 不调用 Provider 的 DSL 回归

```bash
python3 scripts/generate_legal_surfaces.py \
  --seed 20260821 \
  --output /tmp/legal-surfaces.json

python3 scripts/run_surface_import_batch.py --seed 20260821
```

生成器输出 seed、覆盖矩阵和样本。所有样本先经过服务端 Validator 自检，再进入 Surface 导入
和 Qt 批量 Renderer 测试。

## 故障排查

- 缺少 `DISPLAY`：确认处于真实图形会话，Wayland 下确认 XWayland 已启动。
- 缺少 `libqxcb.so` 或动态库：恢复离线归档，失效对应 runtime cache 后重试。
- Linux venv 准备失败：安装 `python3-venv`，或提供可用的 `uv`/pip 缓存。
- `/health` 未就绪：检查 XDG state 下的 `backend.log`。
- Provider 配置权限过宽：执行 `chmod 600 config/provider.local.env`。
- 编排预算错误：确保 Qt 毫秒预算严格大于 Provider 秒预算换算后的值。
- 中文方框：安装 Noto CJK 或其他 `fontconfig` 可识别的中文字体。
- 集成测试在受限 sandbox 中等待：改在允许进程内线程通信和本地回环的受控环境执行。
