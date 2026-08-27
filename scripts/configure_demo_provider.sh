#!/usr/bin/env bash
# 为本机 demo 创建 Git 忽略的 Provider 配置；密钥通过隐藏输入读取。
set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"
readonly TARGET="${1:-${A2UI_PROVIDER_CONFIG:-${REPO_ROOT}/config/provider.local.env}}"

[[ "$TARGET" != "${REPO_ROOT}/config/provider.local.env.example" ]] \
    || { printf '错误：不能覆盖示例配置。\n' >&2; exit 2; }

printf '请输入 A2UI-Qt demo API Key：' >&2
IFS= read -r -s api_key
printf '\n' >&2
[[ -n "$api_key" && "$api_key" != *[[:space:]]* ]] \
    || { printf '错误：API Key 不能为空或包含空白字符。\n' >&2; exit 2; }

target_parent="$(dirname -- "$TARGET")"
mkdir -p -- "$target_parent"
temporary="$(mktemp "${target_parent}/.provider.local.XXXXXX")"
cleanup() { [[ ! -e "$temporary" ]] || rm -f -- "$temporary"; }
trap cleanup EXIT
chmod 600 "$temporary"
printf '%s\n' \
    'A2UI_LLM_ENDPOINT=https://api.deepseek.com/chat/completions' \
    'A2UI_LLM_MODEL=deepseek-v4-flash' \
    "A2UI_LLM_API_KEY=${api_key}" \
    'A2UI_LLM_TIMEOUT_SECONDS=90' \
    'A2UI_COMPOSITION_TIMEOUT_MS=120000' >"$temporary"
mv -f -- "$temporary" "$TARGET"
chmod 600 "$TARGET"
printf '已写入本地 demo 配置：%s（权限 0600）\n' "$TARGET"
