## Purpose

定义与当前源码一致的受控 Qt 构建物在 Ubuntu 图形桌面上的可重复启动、依赖校验、后端隔离、运行数据管理和退出清理合同，使本机演示与测试不依赖容器内固定路径，也不改变受支持工具链基线。

## ADDED Requirements

### Requirement: Ubuntu 运行入口执行完整预检
系统 SHALL 以 `scripts/start_ubuntu_demo.sh` 提供单一、可重复执行的 Ubuntu Demo 运行入口，并在启动任何应用进程前验证图形显示可用、`xcb` 平台插件可加载、Qt 5.12.8 与 ICU 56 动态库可解析、目标 ELF 可执行且不早于当前前端源码和资源、后端端口可用、Linux Python 环境有效且至少存在一种可渲染中文的系统字体；预检失败时 SHALL 给出可操作的中文诊断并停止启动。

#### Scenario: 缺少显示会话时拒绝启动
- **WHEN** 当前会话没有可供 `xcb` 使用的 `DISPLAY`
- **THEN** 运行入口在启动后端或 Qt 进程前失败，并明确提示需要 X11 或 Wayland 下的 XWayland 显示会话

#### Scenario: 依赖与当前构建完整时通过预检
- **WHEN** 不早于当前前端源码和资源的受控 ELF、Qt 5.12.8、ICU 56、`xcb` 插件、回环端口、Linux Python 环境和中文字体均可用
- **THEN** 运行入口报告预检通过并继续启动，不要求安装 Qt Wayland 模块

#### Scenario: 旧入口不再可用
- **WHEN** 操作者或自动化仍尝试调用 `scripts/run_ubuntu.sh`
- **THEN** 仓库不提供该命令、兼容包装或别名，并明确要求迁移到唯一的新入口

### Requirement: 仅运行受控基线构建物
Ubuntu 运行入口 SHALL 仅运行由受控 GCC 7.3.0 或 9.3.0、Qt 5.12.8、CMake 3.16.9、ISO C++14 和 `_GLIBCXX_USE_CXX11_ABI=1` 基线产生，且不早于当前前端源码、Qt 资源、默认 Surface 和相关 CMake 文件的 ELF。入口 SHALL NOT 仅凭文件存在或目录名称接受过期候选，SHALL NOT 使用 Ubuntu 系统 GCC 11 临时重建或放宽 `A2uiBuildPolicy.cmake`；运行时 Qt 模块仍 SHALL 限于 Core、Widgets、Network 和 Test。

#### Scenario: 发现系统 GCC 11
- **WHEN** Ubuntu 主机默认编译器为 GCC 11.4.0 但存在合格的既有 ELF
- **THEN** 运行入口直接使用合格 ELF，且不调用系统 GCC 重新配置或构建项目

#### Scenario: 没有合格 ELF
- **WHEN** 候选构建物不存在或无法证明来自支持的构建配置
- **THEN** 运行入口停止并提示先执行受控构建流程，而不是自动绕过工具链策略

#### Scenario: 候选 ELF 落后于当前源码或资源
- **WHEN** 当前前端源码、QSS/QRC、默认 Surface 或相关 CMake 文件中至少一个晚于候选 ELF
- **THEN** 运行入口在启动后端和 Qt 前失败，并指出需要重新执行受控构建，不得回退到该过期 ELF

### Requirement: 后端使用独立 Linux 环境和可信回环地址
后端 SHALL 使用专用于 Ubuntu 运行的 Linux Python 虚拟环境，SHALL NOT 复用、覆盖或修改仓库中的 Windows `.venv`；Calculation API 与 Composition API SHALL 只绑定可信本地回环地址，Qt 启动前 SHALL 等待健康检查成功。端口已被非本次运行拥有的进程占用时，运行入口 SHALL 报错且不得终止该进程。

#### Scenario: 仓库存在 Windows 虚拟环境
- **WHEN** 项目根目录已有 Windows Python 3.13 `.venv`
- **THEN** Ubuntu 运行入口在独立 Linux 路径创建或复用环境，且 `.venv` 内容保持不变

#### Scenario: 后端端口被其他进程占用
- **WHEN** 目标回环端口在启动前已被其他进程监听
- **THEN** 运行入口停止、说明端口冲突，并保留占用端口的原进程

### Requirement: 运行数据与缓存不污染源码树
运行数据库、日志、进程标记、临时解压内容、Linux 虚拟环境及派生缓存 SHALL 默认写入 Git 管理范围之外的用户级数据、状态或缓存目录；运行入口 SHALL 优先复用仓库已有的离线 Qt、ICU 和 CMake 资源，并在资源可用时 SHALL NOT 运行额外 Qt `.run` 安装器。

#### Scenario: 首次准备离线运行依赖
- **WHEN** 用户缓存尚未准备但仓库离线归档完整可用
- **THEN** 运行入口将经完整性验证的派生内容写入用户缓存，并保持源码工作区没有新增运行产物

#### Scenario: 缓存内容不再匹配来源
- **WHEN** 已缓存内容的版本或完整性标记与仓库离线资源不一致
- **THEN** 运行入口拒绝使用旧缓存并重新准备对应版本，不静默混用不同 Qt 或 ICU 内容

### Requirement: 单一动态 Qt 工作台通过 xcb 可交互运行
运行入口 SHALL 设置 Qt 使用 `xcb`，并在 Ubuntu 的 X11 或 Wayland/XWayland 桌面上只显示一个可交互的动态 HostShell；应用 SHALL NOT 再创建传统固定排版示例窗口。关闭 HostShell 后 SHALL 不遗留本次启动的后端进程或端口占用。

#### Scenario: Wayland 桌面通过 XWayland 启动
- **WHEN** 用户处于具备 XWayland 和有效 `DISPLAY` 的 Wayland 会话
- **THEN** 单一 HostShell 通过 `xcb` 创建并可接受鼠标和键盘输入，其控制区、动态 Surface 及恢复默认操作均可用

#### Scenario: 用户正常关闭应用
- **WHEN** 用户关闭 Qt 应用或运行入口收到终止信号
- **THEN** 入口只清理本次启动的子进程、临时状态和端口占用，并以可判断的退出码结束

### Requirement: 测试资源定位不依赖固定容器路径
Qt 测试和验收入口 SHALL 从当前仓库或显式、经过验证的源码根定位 Fixture 与审计文件，SHALL NOT 要求主机存在 `/workspace` 路径映射；路径修正后 SHALL 保持 Qt 前端测试、后端单元与集成测试及源码策略检查的既有覆盖。

#### Scenario: 仓库位于任意用户目录
- **WHEN** 既有测试 ELF 在不含 `/workspace` 的 Ubuntu 仓库路径中运行
- **THEN** 测试从当前仓库读取共享 Fixture 和验收结果，不因编译时绝对路径而失败

#### Scenario: 运行完整回归
- **WHEN** 使用受支持构建物执行 Ubuntu 回归入口
- **THEN** 当前注册的全部 Qt 前端测试、后端测试和源码策略检查均执行且无未说明的 skip，结果不依赖历史测试数量
