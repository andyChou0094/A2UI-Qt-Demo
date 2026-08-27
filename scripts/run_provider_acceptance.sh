#!/usr/bin/env bash
set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"
output="${1:-${REPO_ROOT}/docs/llm-acceptance-results.json}"
key="${A2UI_LLM_API_KEY:-${deepseek_api:-}}"

[[ "${A2UI_KEY_ROTATED:-0}" == 1 ]] || {
    printf '未验收：必须由操作者确认 A2UI_KEY_ROTATED=1，旧密钥不得复用。\n' >&2
    exit 3
}
[[ -n "$key" ]] || {
    printf '未验收：请仅在当前进程环境提供轮换后的 deepseek_api 或 A2UI_LLM_API_KEY。\n' >&2
    exit 3
}

env A2UI_LLM_ENDPOINT=https://api.deepseek.com/chat/completions \
    A2UI_LLM_MODEL=deepseek-v4-flash \
    A2UI_LLM_API_KEY="$key" \
    python3 "${REPO_ROOT}/scripts/run_llm_acceptance.py" --output "$output"
