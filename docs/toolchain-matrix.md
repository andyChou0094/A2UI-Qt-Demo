# Qt 双 GCC 工具链

更新时间：2026-08-21

## 支持基线

| 项目 | 固定值 | 说明 |
|---|---|---|
| Qt | 5.12.8 | 只使用 Core、Widgets、Network、Test |
| GCC | 7.3.0、9.3.0 | 两端都必须构建和测试 |
| C++ | ISO C++14 | 显式 `-std=c++14` |
| CMake | 3.16.9 | 受控工具链版本 |
| libstdc++ ABI | `_GLIBCXX_USE_CXX11_ABI=1` | 两端均检查 |
| ICU | 56 | 与 Qt 归档包配套 |

`cmake/A2uiBuildPolicy.cmake` 会拒绝 Qt 5.15、Qt 6、不支持的 GCC、QtSql、C++17 专属能力和
未经认证的运行时二进制插件。Ubuntu 系统 GCC 11 不属于支持矩阵。

## 工具链来源

- GCC 基础镜像：`gcc:7.3.0` 与 `gcc:9.3.0`。
- Qt：官方归档 `qtbase-Linux-RHEL_7_4-GCC-Linux-RHEL_7_4-X86_64.7z`。
- ICU：官方归档 `icu-linux-Rhel7.2-x64.7z`。
- Linux 图形运行库：Debian Stretch 归档中的 Mesa 与 X11/XCB/DRM 依赖。
- CMake：仓库缓存的 3.16.9 归档。

离线包进入工具链前会校验摘要。项目不混用不同工具链生成的插件，也不链接 QtSql。

## 验证证明

受控矩阵必须证明 Qt 5.12.8、`__cplusplus=201402L`、
`_GLIBCXX_USE_CXX11_ABI=1`、编译/链接和 frontend CTest。当前通过数、覆盖、条件 skip 和
后端测试口径统一见[验证与评测报告](verification-and-evaluation.md)，本文不复制日期化结果。

## 标准构建入口

在具备 PowerShell、Docker/Podman 兼容容器运行时和仓库离线缓存的环境中执行：

```powershell
.\tools\build-matrix.ps1
```

脚本分别创建或更新：

- `build-gcc-7.3.0`
- `build-gcc-9.3.0`

不要把旧构建目录中的 2026-08-18 `LastTest.log` 当作当前源码结果；那批日志仍包含已经删除的
LegacyToolboxWindow。当前验证证据来自 2026-08-21 重新配置和构建的临时受控目录。

## Ubuntu 运行关系

`scripts/start_ubuntu_demo.sh` 只消费上述受控构建生成的 ELF，不执行构建。它会读取构建目录中的
CMake 编译器证明，确认 GCC 是 7.3.0 或 9.3.0，并拒绝早于当前前端源码、默认 Surface 或相关
CMake 文件的 ELF，再准备 Qt/ICU 运行缓存并通过 `xcb` 启动。出现旧构建诊断时，重新执行上面的
矩阵构建；不要恢复旧入口或用系统 GCC 11 临时重建。

启动步骤见[项目启动说明](getting-started.md)，依赖与缓存细节见[Ubuntu 运行细节](ubuntu-runtime.md)。

## 安全边界

Qt 5.12.8、Debian Stretch/Buster 和相关依赖已经停止常规支持。本矩阵证明的是 Demo 的兼容性
和可复现性，不代表生产漏洞修复、供应链安全或长期维护能力。接入真实组件前必须重新执行
Adapter、ABI、生命周期和安全认证。
