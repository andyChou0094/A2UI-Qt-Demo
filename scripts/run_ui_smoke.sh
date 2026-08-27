#!/usr/bin/env bash
set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"
build_dir="${1:-${REPO_ROOT}/build-gcc-9.3.0}"
runtime_root="${2:-${A2UI_RUNTIME_ROOT:-}}"
output_root="${3:-${XDG_STATE_HOME:-${HOME}/.local/state}/a2ui-qt-demo/ui-smoke}"

[[ -x "${build_dir}/frontend/app/a2ui_qt_demo" ]] \
    || { printf '错误：缺少 Qt Demo ELF：%s\n' "$build_dir" >&2; exit 2; }
[[ -s "${runtime_root}/plugins/platforms/libqoffscreen.so" ]] \
    || { printf '错误：请提供包含 offscreen 插件的 Qt 5.12.8 runtime root\n' >&2; exit 2; }
mkdir -p -- "$output_root"

representatives=(sidebar nested-2x2 depth-boundary duplicate-calculators empty-surface)
for factor in 1.00 1.25 1.50; do
    for representative in "${representatives[@]}"; do
        destination="${output_root}/scale-${factor}/${representative}"
        mkdir -p -- "$destination"
        env QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR="$factor" \
            QT_PLUGIN_PATH="${runtime_root}/plugins" \
            LD_LIBRARY_PATH="${runtime_root}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
            A2UI_DEMO_SCREENSHOT_DIR="$destination" \
            A2UI_DEMO_SCREENSHOT_SURFACE_PATH="${REPO_ROOT}/shared/fixtures/surface-spec/valid/${representative}.json" \
            "${build_dir}/frontend/app/a2ui_qt_demo"
        [[ -s "${destination}/dynamic-host.png" ]] \
            || { printf '错误：缩放 %s 的 %s 未生成单窗口工作台截图\n' "$factor" "$representative" >&2; exit 1; }
    done
    env QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR="$factor" \
        QT_PLUGIN_PATH="${runtime_root}/plugins" \
        LD_LIBRARY_PATH="${runtime_root}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
        "${build_dir}/tests/a2ui_test_host_shell" \
        controllerFitsNineHundredBySevenHundredAndSurfaceRemainsScrollable
done
printf '三档缩放、五类代表 DSL 与单窗口截图冒烟通过：%s\n' "$output_root"
