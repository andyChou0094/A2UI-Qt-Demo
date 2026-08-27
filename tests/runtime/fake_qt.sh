#!/usr/bin/env bash
if [[ -n "${A2UI_TEST_QT_CONFIG_CAPTURE:-}" ]]; then
    key_present=false
    [[ -z "${A2UI_LLM_API_KEY:-}" ]] || key_present=true
    printf '{"compositionTimeout":"%s","llmKeyPresent":%s}\n' \
        "${A2UI_COMPOSITION_TIMEOUT_MS:-}" "$key_present" \
        >"$A2UI_TEST_QT_CONFIG_CAPTURE"
fi
sleep 2
