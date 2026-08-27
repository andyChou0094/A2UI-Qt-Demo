#!/usr/bin/env bash
# Ubuntu 桌面上的唯一运行入口。构建仍由受控容器完成；本脚本只准备和运行当前 ELF。
set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"
readonly QT_VERSION="5.12.8"
readonly ICU_VERSION="56"

preflight_only=0
build_dir=""
port="${A2UI_BACKEND_PORT:-8000}"
backend_pid=""
backend_start=""
qt_pid=""
qt_start=""
runtime_tmp=""
venv_tmp=""
provider_config="${A2UI_PROVIDER_CONFIG:-${REPO_ROOT}/config/provider.local.env}"
config_endpoint=""
config_model=""
config_key=""
config_llm_timeout=""
config_composition_timeout=""

usage() {
    printf '%s\n' \
        "用法: scripts/start_ubuntu_demo.sh [--preflight-only] [--build-dir DIR] [--port PORT]" \
        "  --preflight-only  完成全部准备和预检，但不启动后端与 Qt" \
        "  --build-dir DIR   显式选择 build-gcc-9.3.0 或 build-gcc-7.3.0"
}

die() { printf '错误：%s\n' "$*" >&2; exit 2; }
info() { printf '• %s\n' "$*"; }
cleanup_preflight_temps() {
    [[ -z "$runtime_tmp" || ! -d "$runtime_tmp" ]] || rm -rf -- "$runtime_tmp"
    [[ -z "$venv_tmp" || ! -d "$venv_tmp" ]] || rm -rf -- "$venv_tmp"
}
trap cleanup_preflight_temps EXIT

while (($#)); do
    case "$1" in
        --preflight-only) preflight_only=1 ;;
        --build-dir) shift; (($#)) || die "--build-dir 缺少目录"; build_dir="$1" ;;
        --port) shift; (($#)) || die "--port 缺少端口"; port="$1" ;;
        -h|--help) usage; exit 0 ;;
        *) die "未知参数：$1" ;;
    esac
    shift
done

[[ -f "${REPO_ROOT}/CMakeLists.txt" && -d "${REPO_ROOT}/shared/schema" ]] \
    || die "无法确认仓库根：${REPO_ROOT}"
[[ "$port" =~ ^[0-9]+$ ]] && ((port >= 1024 && port <= 65535)) \
    || die "端口必须是 1024–65535 的整数"

load_provider_config() {
    [[ -e "$provider_config" ]] || return 0
    [[ -f "$provider_config" && ! -L "$provider_config" ]] \
        || die "Provider 配置必须是普通文件且不能是符号链接：${provider_config}"
    local config_mode permission line key value
    config_mode="$(stat -c '%a' "$provider_config")"
    [[ "$config_mode" =~ ^[0-7]{3,4}$ ]] || die "无法确认 Provider 配置权限"
    permission=$((8#$config_mode))
    (( (permission & 077) == 0 )) \
        || die "Provider 配置权限过宽（当前 ${config_mode}，要求 0600 或更严格）：${provider_config}"
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%$'\r'}"
        [[ -z "$line" || "$line" == \#* ]] && continue
        [[ "$line" == *=* ]] || die "Provider 配置存在无效行：${provider_config}"
        key="${line%%=*}"
        value="${line#*=}"
        [[ -n "$value" ]] || die "Provider 配置字段 ${key} 不能为空"
        case "$key" in
            A2UI_LLM_ENDPOINT) config_endpoint="$value" ;;
            A2UI_LLM_MODEL) config_model="$value" ;;
            A2UI_LLM_API_KEY) config_key="$value" ;;
            A2UI_LLM_TIMEOUT_SECONDS) config_llm_timeout="$value" ;;
            A2UI_COMPOSITION_TIMEOUT_MS) config_composition_timeout="$value" ;;
            *) die "Provider 配置包含不支持的字段：${key}" ;;
        esac
    done <"$provider_config"
    info "已加载本地 Provider 配置：${provider_config}"
}

load_provider_config
provider_endpoint="${A2UI_LLM_ENDPOINT:-${config_endpoint:-https://api.deepseek.com/chat/completions}}"
provider_model="${A2UI_LLM_MODEL:-${config_model:-deepseek-v4-flash}}"
provider_key="${A2UI_LLM_API_KEY:-${deepseek_api:-${config_key:-}}}"
provider_timeout="${A2UI_LLM_TIMEOUT_SECONDS:-${config_llm_timeout:-90}}"
composition_timeout_ms="${A2UI_COMPOSITION_TIMEOUT_MS:-${config_composition_timeout:-120000}}"
[[ "$provider_timeout" =~ ^[0-9]+$ ]] && ((provider_timeout > 0)) \
    || die "A2UI_LLM_TIMEOUT_SECONDS 必须是正整数"
[[ "$composition_timeout_ms" =~ ^[0-9]+$ ]] && ((composition_timeout_ms > 0)) \
    || die "A2UI_COMPOSITION_TIMEOUT_MS 必须是正整数"
((composition_timeout_ms > provider_timeout * 1000)) \
    || die "前端编排超时必须大于 Provider 超时（当前 ${composition_timeout_ms}ms / ${provider_timeout}s）"

xdg_cache="${XDG_CACHE_HOME:-${HOME}/.cache}/a2ui-qt-demo"
xdg_data="${XDG_DATA_HOME:-${HOME}/.local/share}/a2ui-qt-demo"
xdg_state="${XDG_STATE_HOME:-${HOME}/.local/state}/a2ui-qt-demo"
for target in "$xdg_cache" "$xdg_data" "$xdg_state"; do
    [[ "$target" != "${REPO_ROOT}" && "$target" != "${REPO_ROOT}/"* ]] \
        || die "XDG 运行目录不得位于源码树：${target}"
    mkdir -p -- "$target"
done

if [[ -n "$build_dir" ]]; then
    build_dir="$(cd -- "$build_dir" 2>/dev/null && pwd -P)" \
        || die "构建目录不存在：${build_dir}"
else
    for candidate in "${REPO_ROOT}/build-gcc-9.3.0" "${REPO_ROOT}/build-gcc-7.3.0"; do
        if [[ -x "${candidate}/frontend/app/a2ui_qt_demo" ]]; then build_dir="$candidate"; break; fi
    done
fi
[[ -n "$build_dir" ]] || die "未找到合格 ELF。请先运行 tools/build-matrix.ps1 的 GCC 9.3.0 或 7.3.0 受控构建。"
compiler_contract="${build_dir}/CMakeFiles/3.16.9/CMakeCXXCompiler.cmake"
[[ -f "$compiler_contract" ]] || die "构建目录缺少 CMake 3.16.9 编译器证明：${build_dir}"
compiler_version="$(sed -n 's/^set(CMAKE_CXX_COMPILER_VERSION "\([^"]*\)")/\1/p' "$compiler_contract" | head -n 1)"
[[ "$compiler_version" == "7.3.0" || "$compiler_version" == "9.3.0" ]] \
    || die "构建物不是受支持的 GCC 7.3.0/9.3.0 基线（检测到 ${compiler_version:-未知}）"
elf="${build_dir}/frontend/app/a2ui_qt_demo"
[[ -x "$elf" ]] || die "受控构建目录缺少可执行 ELF：${elf}"

check_build_freshness() {
    local input newer_input
    local freshness_inputs=(
        "${REPO_ROOT}/frontend/app"
        "${REPO_ROOT}/frontend/composition"
        "${REPO_ROOT}/frontend/widgets"
        "${REPO_ROOT}/shared/default-surface.json"
        "${REPO_ROOT}/CMakeLists.txt"
        "${REPO_ROOT}/frontend/CMakeLists.txt"
        "${REPO_ROOT}/frontend/services/CMakeLists.txt"
        "${REPO_ROOT}/cmake/A2uiBuildPolicy.cmake"
    )
    for input in "${freshness_inputs[@]}"; do
        [[ -e "$input" ]] || die "无法检查构建新旧：缺少前端输入 ${input}"
    done
    newer_input="$(find "${freshness_inputs[@]}" -type f -newer "$elf" -print -quit)"
    [[ -z "$newer_input" ]] || die \
        "目标 ELF 早于当前前端源码或资源（较新的输入：${newer_input}）。请使用 tools/build-matrix.ps1 或 tools/container-build.sh 重新执行 GCC 9.3.0/7.3.0 受控构建后再启动。"
}

check_build_freshness

prepare_runtime_cache() {
    if [[ -n "${A2UI_RUNTIME_ROOT:-}" ]]; then
        runtime_root="$(cd -- "$A2UI_RUNTIME_ROOT" && pwd -P)"
        return
    fi
    local qt_archive="${REPO_ROOT}/.cache/qt-archives/qtbase-Linux-RHEL_7_4-GCC-Linux-RHEL_7_4-X86_64.7z"
    local icu_archive="${REPO_ROOT}/.cache/qt-archives/icu-linux-Rhel7.2-x64.7z"
    local seven_zip="${REPO_ROOT}/.cache/tools/linux/7zzs"
    [[ -s "$qt_archive" && -s "$icu_archive" && -x "$seven_zip" ]] \
        || die "离线 Qt 5.12.8/ICU 56 归档或 7zzs 不完整；请恢复仓库 .cache 离线资源。"
    local digest cache_root marker
    digest="$(sha256sum "$qt_archive" "$icu_archive" | sha256sum | cut -c1-20)"
    cache_root="${xdg_cache}/runtime/qt-${QT_VERSION}-icu-${ICU_VERSION}-${digest}"
    marker="${cache_root}/.a2ui-source-digest"
    if [[ -r "$marker" ]] && [[ "$(<"$marker")" == "$digest" ]] \
        && [[ -s "${cache_root}/${QT_VERSION}/gcc_64/lib/libQt5Core.so.5.12.8" ]] \
        && [[ -s "${cache_root}/${QT_VERSION}/gcc_64/lib/libicuuc.so.56.1" ]] \
        && [[ -s "${cache_root}/${QT_VERSION}/gcc_64/plugins/platforms/libqxcb.so" ]]; then
        runtime_root="${cache_root}/${QT_VERSION}/gcc_64"
        info "命中 Qt/ICU 派生缓存：${cache_root}"
        return
    fi
    mkdir -p -- "${xdg_cache}/runtime"
    runtime_tmp="$(mktemp -d "${xdg_cache}/runtime/.prepare.XXXXXX")"
    "$seven_zip" x -y "-o${runtime_tmp}" "$qt_archive" >/dev/null \
        || die "Qt 5.12.8 离线归档解压失败"
    "$seven_zip" x -y "-o${runtime_tmp}" "$icu_archive" >/dev/null \
        || die "ICU 56 离线归档解压失败"
    [[ -s "${runtime_tmp}/${QT_VERSION}/gcc_64/lib/libQt5Core.so.5.12.8" \
       && -s "${runtime_tmp}/${QT_VERSION}/gcc_64/lib/libicuuc.so.56.1" \
       && -s "${runtime_tmp}/${QT_VERSION}/gcc_64/plugins/platforms/libqxcb.so" ]] \
        || die "离线归档内容未通过 Qt/ICU/xcb 完整性检查"
    printf '%s\n' "$digest" >"${runtime_tmp}/.a2ui-source-digest"
    if [[ -e "$cache_root" ]]; then
        stale="${cache_root}.stale.$$"
        mv -- "$cache_root" "$stale"
        mv -- "$runtime_tmp" "$cache_root"
        runtime_tmp="$stale"
        rm -rf -- "$stale"
    else
        mv -- "$runtime_tmp" "$cache_root"
    fi
    runtime_tmp=""
    runtime_root="${cache_root}/${QT_VERSION}/gcc_64"
    info "已从校验过的离线归档原子准备 Qt/ICU 缓存"
}

venv_snapshot() {
    if [[ -d "${REPO_ROOT}/.venv" ]]; then
        find "${REPO_ROOT}/.venv" -printf '%P %s %T@\n' | LC_ALL=C sort | sha256sum | cut -d' ' -f1
    else printf '%s\n' absent; fi
}

prepare_linux_venv() {
    local python_bin="${A2UI_PYTHON:-python3}"
    command -v "$python_bin" >/dev/null || die "缺少 Linux Python 3"
    local py_version requirements_digest key before after
    py_version="$($python_bin -c 'import sys; print("{}.{}".format(*sys.version_info[:2]))')"
    requirements_digest="$(sha256sum "${REPO_ROOT}/backend/requirements.txt" | cut -c1-20)"
    key="py-${py_version}-req-${requirements_digest}"
    linux_venv="${xdg_cache}/python/${key}"
    before="$(venv_snapshot)"
    if [[ "${A2UI_SKIP_VENV_PREPARE:-0}" == 1 ]]; then
        venv_python="$python_bin"
    else
        if [[ ! -x "${linux_venv}/bin/python" ]]; then
            mkdir -p -- "${xdg_cache}/python"
            venv_tmp="$(mktemp -d "${xdg_cache}/python/.prepare.XXXXXX")"
            if "$python_bin" -c 'import ensurepip' >/dev/null 2>&1; then
                "$python_bin" -m venv "$venv_tmp" \
                    || die "无法创建独立 Linux venv（请安装 python3-venv）"
                "$venv_tmp/bin/python" -m pip install --disable-pip-version-check \
                    -r "${REPO_ROOT}/backend/requirements.txt" \
                    || die "Linux venv 依赖准备失败；请检查可用的 pip 源或预热缓存"
            elif command -v uv >/dev/null 2>&1; then
                uv venv --python "$python_bin" --clear "$venv_tmp" >/dev/null \
                    || die "uv 无法创建独立 Linux venv"
                uv pip install --python "$venv_tmp/bin/python" \
                    -r "${REPO_ROOT}/backend/requirements.txt" \
                    || die "uv 无法准备 Linux venv 依赖；请检查包源或 uv 缓存"
            else
                die "无法创建独立 Linux venv；请安装 python3-venv 或 uv"
            fi
            mv -- "$venv_tmp" "$linux_venv"
            venv_tmp=""
        fi
        venv_python="${linux_venv}/bin/python"
    fi
    if [[ "${A2UI_PROBE_PYTHON:-}" != ok ]]; then
        "$venv_python" -c 'import fastapi, httpx2, uvicorn' >/dev/null \
            || die "独立 Linux Python 环境缺少 FastAPI/httpx2/uvicorn"
    fi
    after="$(venv_snapshot)"
    [[ "$before" == "$after" ]] || die "仓库 Windows .venv 在准备期间发生变化，已停止启动"
}

port_is_free() {
    if [[ -n "${A2UI_PROBE_PORT:-}" ]]; then [[ "$A2UI_PROBE_PORT" == free ]]; return; fi
    "$venv_python" - "$port" <<'PY'
import socket, sys
s = socket.socket()
try:
    s.bind(("127.0.0.1", int(sys.argv[1])))
except OSError:
    raise SystemExit(1)
finally:
    s.close()
PY
}

port_is_listening() {
    "$venv_python" - "$port" <<'PY'
import socket, sys
s = socket.socket()
s.settimeout(.1)
try:
    raise SystemExit(0 if s.connect_ex(("127.0.0.1", int(sys.argv[1]))) == 0 else 1)
finally:
    s.close()
PY
}

prepare_runtime_cache
prepare_linux_venv

if [[ "${A2UI_PROBE_ELF:-}" != ok ]]; then
    file "$elf" | grep -q 'ELF 64-bit.*x86-64' \
        || die "目标不是受支持的 x86-64 ELF：${elf}"
    readelf -d "$elf" | grep -q 'libQt5Core.so.5' \
        || die "ELF 未链接受控 Qt 5 运行时"
fi
[[ -s "${runtime_root}/plugins/platforms/libqxcb.so" ]] \
    || die "缺少 Qt xcb 平台插件：${runtime_root}/plugins/platforms/libqxcb.so"
[[ -s "${runtime_root}/lib/libQt5Core.so.5.12.8" && -s "${runtime_root}/lib/libicuuc.so.56.1" ]] \
    || die "Qt 5.12.8 或 ICU 56 动态库不完整"
if [[ "${A2UI_PROBE_DISPLAY:-}" != ok ]]; then
    [[ -n "${DISPLAY:-}" ]] || die "缺少 DISPLAY；请在 X11 或带 XWayland 的 Wayland 图形会话中运行。"
fi
if [[ "${A2UI_PROBE_FONT:-}" != ok ]]; then
    command -v fc-match >/dev/null || die "缺少 fontconfig，无法验证中文字体"
    [[ -n "$(fc-match ':lang=zh-cn' 2>/dev/null)" ]] || die "系统未发现可渲染中文的字体（建议安装 Noto Sans CJK SC）"
fi
port_is_free || die "回环端口 127.0.0.1:${port} 已被外部进程占用；不会终止该进程。"

export A2UI_RUNTIME_CACHE="$runtime_root"
info "预检通过：GCC ${compiler_version} / Qt ${QT_VERSION} / ICU ${ICU_VERSION} / xcb / Linux Python"
if ((preflight_only)); then exit 0; fi

process_start() { awk '{print $22}' "/proc/$1/stat" 2>/dev/null || true; }
owned_process() { [[ -n "$1" && -n "$2" && -r "/proc/$1/stat" && "$(process_start "$1")" == "$2" ]]; }
stop_owned() {
    local pid="$1" start="$2"
    owned_process "$pid" "$start" || return 0
    kill -TERM "$pid" 2>/dev/null || true
    for _ in {1..30}; do owned_process "$pid" "$start" || return 0; sleep 0.1; done
    owned_process "$pid" "$start" && kill -KILL "$pid" 2>/dev/null || true
}
cleanup() {
    local status=$?
    trap - EXIT INT TERM
    stop_owned "$qt_pid" "$qt_start"
    stop_owned "$backend_pid" "$backend_start"
    [[ -z "$runtime_tmp" || ! -d "$runtime_tmp" ]] || rm -rf -- "$runtime_tmp"
    for _ in {1..30}; do
        port_is_listening || break
        sleep 0.1
    done
    port_is_listening && printf '警告：端口 %s 尚未释放。\n' "$port" >&2 || true
    exit "$status"
}
trap cleanup EXIT INT TERM

backend_log="${xdg_state}/backend.log"
database="${xdg_data}/calculations.sqlite3"
child_api="http://127.0.0.1:${port}"
if [[ -n "${A2UI_BACKEND_LAUNCHER:-}" ]]; then
    env A2UI_LLM_ENDPOINT="$provider_endpoint" \
        A2UI_LLM_MODEL="$provider_model" \
        A2UI_LLM_API_KEY="$provider_key" \
        A2UI_LLM_TIMEOUT_SECONDS="$provider_timeout" \
        "$A2UI_BACKEND_LAUNCHER" "$port" "$database" >>"$backend_log" 2>&1 &
else
    env A2UI_CALCULATION_DB_PATH="$database" \
        A2UI_LLM_ENDPOINT="$provider_endpoint" \
        A2UI_LLM_MODEL="$provider_model" \
        A2UI_LLM_API_KEY="$provider_key" \
        A2UI_LLM_TIMEOUT_SECONDS="$provider_timeout" \
        "$venv_python" -m uvicorn backend.demo_app:app --host 127.0.0.1 --port "$port" \
        >>"$backend_log" 2>&1 &
fi
backend_pid=$!
backend_start="$(process_start "$backend_pid")"

healthy=0
for _ in {1..100}; do
    if "$venv_python" - "$child_api/health" <<'PY' >/dev/null 2>&1
import json, sys
from urllib.request import urlopen
with urlopen(sys.argv[1], timeout=.3) as response:
    assert json.load(response)["status"] == "ok"
PY
    then healthy=1; break; fi
    owned_process "$backend_pid" "$backend_start" || break
    sleep 0.1
done
((healthy)) || die "后端 /health 未就绪，Qt 未启动；请查看 ${backend_log}"

env QT_QPA_PLATFORM=xcb \
    QT_PLUGIN_PATH="${runtime_root}/plugins" \
    QT_QPA_PLATFORM_PLUGIN_PATH="${runtime_root}/plugins/platforms" \
    LD_LIBRARY_PATH="${runtime_root}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    A2UI_CALCULATION_API_BASE_URL="$child_api" \
    A2UI_COMPOSITION_API_BASE_URL="$child_api" \
    A2UI_COMPOSITION_TIMEOUT_MS="$composition_timeout_ms" \
    "$elf" &
qt_pid=$!
qt_start="$(process_start "$qt_pid")"
wait "$qt_pid"
