#!/usr/bin/env bash
set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"
build_dir="${1:-${REPO_ROOT}/build-gcc-9.3.0}"
[[ -d "$build_dir" ]] || { printf '错误：构建目录不存在：%s\n' "$build_dir" >&2; exit 2; }

export A2UI_TEST_SOURCE_ROOT="$REPO_ROOT"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
ctest --test-dir "$build_dir" --output-on-failure
python3 -m unittest discover -s "${REPO_ROOT}/backend/tests/unit" -p 'test_*.py'
python3 -m unittest discover -s "${REPO_ROOT}/backend/tests/integration" -p 'test_*.py'
python3 "${REPO_ROOT}/scripts/check_source_policy.py" "$REPO_ROOT"

