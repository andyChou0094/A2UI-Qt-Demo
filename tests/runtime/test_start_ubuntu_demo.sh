#!/usr/bin/env bash
set -Eeuo pipefail

readonly ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
readonly TMP="$(mktemp -d)"
trap 'rm -rf -- "$TMP"' EXIT

fake_runtime="${TMP}/runtime"
fake_build="${TMP}/build-gcc-9.3.0"
mkdir -p "$fake_runtime/lib" "$fake_runtime/plugins/platforms" \
    "$fake_build/frontend/app" "$fake_build/CMakeFiles/3.16.9"
for file in libQt5Core.so.5.12.8 libicuuc.so.56.1; do printf x >"$fake_runtime/lib/$file"; done
printf x >"$fake_runtime/plugins/platforms/libqxcb.so"
cp /bin/true "$fake_build/frontend/app/a2ui_qt_demo"
printf '%s\n' 'set(CMAKE_CXX_COMPILER_VERSION "9.3.0")' \
    >"$fake_build/CMakeFiles/3.16.9/CMakeCXXCompiler.cmake"

common=(env HOME="$TMP/home" XDG_CACHE_HOME="$TMP/cache" XDG_DATA_HOME="$TMP/data" \
    XDG_STATE_HOME="$TMP/state" A2UI_RUNTIME_ROOT="$fake_runtime" \
    A2UI_PROVIDER_CONFIG="$TMP/missing-provider.env" \
    A2UI_SKIP_VENV_PREPARE=1 A2UI_PROBE_PYTHON=ok A2UI_PROBE_ELF=ok A2UI_PROBE_FONT=ok \
    A2UI_PROBE_DISPLAY=ok A2UI_PROBE_PORT=free)
mkdir -p "$TMP/home"

"${common[@]}" "$ROOT/scripts/start_ubuntu_demo.sh" --preflight-only --build-dir "$fake_build"

touch -d '2000-01-01 00:00:00 UTC' "$fake_build/frontend/app/a2ui_qt_demo"
if "${common[@]}" "$ROOT/scripts/start_ubuntu_demo.sh" --preflight-only --build-dir "$fake_build" \
    >"$TMP/stale-elf.log" 2>&1; then
    printf '旧 ELF 用例本应失败\n' >&2; exit 1
fi
grep -q '目标 ELF 早于当前前端源码或资源' "$TMP/stale-elf.log"
grep -q '受控构建' "$TMP/stale-elf.log"
touch "$fake_build/frontend/app/a2ui_qt_demo"

if env -u DISPLAY HOME="$TMP/home" XDG_CACHE_HOME="$TMP/cache" XDG_DATA_HOME="$TMP/data" \
    XDG_STATE_HOME="$TMP/state" A2UI_RUNTIME_ROOT="$fake_runtime" \
    A2UI_SKIP_VENV_PREPARE=1 A2UI_PROBE_PYTHON=ok A2UI_PROBE_ELF=ok A2UI_PROBE_FONT=ok A2UI_PROBE_PORT=free \
    "$ROOT/scripts/start_ubuntu_demo.sh" --preflight-only --build-dir "$fake_build" \
    >"$TMP/no-display.log" 2>&1; then
    printf '缺少显示用例本应失败\n' >&2; exit 1
fi
grep -q '缺少 DISPLAY' "$TMP/no-display.log"

if "${common[@]}" "$ROOT/scripts/start_ubuntu_demo.sh" --preflight-only \
    --build-dir "$TMP/missing" >"$TMP/no-elf.log" 2>&1; then
    printf '无 ELF 用例本应失败\n' >&2; exit 1
fi
grep -q '构建目录不存在' "$TMP/no-elf.log"

: >"$fake_runtime/lib/libQt5Core.so.5.12.8"
if "${common[@]}" "$ROOT/scripts/start_ubuntu_demo.sh" --preflight-only --build-dir "$fake_build" \
    >"$TMP/cache.log" 2>&1; then
    printf '损坏缓存用例本应失败\n' >&2; exit 1
fi
grep -q '动态库不完整' "$TMP/cache.log"
printf valid >"$fake_runtime/lib/libQt5Core.so.5.12.8"

if env HOME="$TMP/home" XDG_CACHE_HOME="$TMP/cache" XDG_DATA_HOME="$TMP/data" \
    XDG_STATE_HOME="$TMP/state" A2UI_RUNTIME_ROOT="$fake_runtime" \
    A2UI_SKIP_VENV_PREPARE=1 A2UI_PROBE_PYTHON=ok A2UI_PROBE_ELF=ok A2UI_PROBE_FONT=ok \
    A2UI_PROBE_DISPLAY=ok A2UI_PROBE_PORT=busy \
    "$ROOT/scripts/start_ubuntu_demo.sh" --preflight-only --build-dir "$fake_build" \
    >"$TMP/port.log" 2>&1; then
    printf '端口占用用例本应失败\n' >&2; exit 1
fi
grep -q '外部进程占用' "$TMP/port.log"

printf 'fake-demo-key\n' | "$ROOT/scripts/configure_demo_provider.sh" \
    "$TMP/generated-provider.env" >"$TMP/configure.log" 2>&1
[[ "$(stat -c '%a' "$TMP/generated-provider.env")" == 600 ]]
grep -q '^A2UI_LLM_TIMEOUT_SECONDS=90$' "$TMP/generated-provider.env"
grep -q '^A2UI_COMPOSITION_TIMEOUT_MS=120000$' "$TMP/generated-provider.env"
! grep -q 'fake-demo-key' "$TMP/configure.log"

cp "$TMP/generated-provider.env" "$TMP/permissive-provider.env"
chmod 644 "$TMP/permissive-provider.env"
if "${common[@]}" A2UI_PROVIDER_CONFIG="$TMP/permissive-provider.env" \
    "$ROOT/scripts/start_ubuntu_demo.sh" --preflight-only --build-dir "$fake_build" \
    >"$TMP/provider-permission.log" 2>&1; then
    printf '宽权限 Provider 配置用例本应失败\n' >&2; exit 1
fi
grep -q 'Provider 配置权限过宽' "$TMP/provider-permission.log"

cp "$TMP/generated-provider.env" "$TMP/unknown-provider.env"
printf 'UNSUPPORTED_FIELD=value\n' >>"$TMP/unknown-provider.env"
chmod 600 "$TMP/unknown-provider.env"
if "${common[@]}" A2UI_PROVIDER_CONFIG="$TMP/unknown-provider.env" \
    "$ROOT/scripts/start_ubuntu_demo.sh" --preflight-only --build-dir "$fake_build" \
    >"$TMP/provider-field.log" 2>&1; then
    printf '未知 Provider 字段用例本应失败\n' >&2; exit 1
fi
grep -q 'Provider 配置包含不支持的字段' "$TMP/provider-field.log"
printf 'Ubuntu 运行入口探针测试通过\n'
