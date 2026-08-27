#!/usr/bin/env bash
set -Eeuo pipefail

readonly ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
readonly TMP="$(mktemp -d)"
runtime_pid=""
cleanup() {
    [[ -z "$runtime_pid" ]] || kill -TERM "$runtime_pid" 2>/dev/null || true
    rm -rf -- "$TMP"
}
trap cleanup EXIT

fake_runtime="$TMP/runtime"
fake_build="$TMP/build-gcc-9.3.0"
mkdir -p "$fake_runtime/lib" "$fake_runtime/plugins/platforms" \
    "$fake_build/frontend/app" "$fake_build/CMakeFiles/3.16.9" "$TMP/home"
printf valid >"$fake_runtime/lib/libQt5Core.so.5.12.8"
printf valid >"$fake_runtime/lib/libicuuc.so.56.1"
printf valid >"$fake_runtime/plugins/platforms/libqxcb.so"
printf '%s\n' 'set(CMAKE_CXX_COMPILER_VERSION "9.3.0")' \
    >"$fake_build/CMakeFiles/3.16.9/CMakeCXXCompiler.cmake"
cp "$ROOT/tests/runtime/fake_qt.sh" "$fake_build/frontend/app/a2ui_qt_demo"
cat >"$TMP/provider.env" <<'EOF'
A2UI_LLM_ENDPOINT=https://configured.example/v1/chat/completions
A2UI_LLM_MODEL=configured-model
A2UI_LLM_API_KEY=fake-test-key
A2UI_LLM_TIMEOUT_SECONDS=91
A2UI_COMPOSITION_TIMEOUT_MS=122000
EOF
chmod 600 "$TMP/provider.env"
port="$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')"

env HOME="$TMP/home" XDG_CACHE_HOME="$TMP/cache" XDG_DATA_HOME="$TMP/data" \
    XDG_STATE_HOME="$TMP/state" A2UI_RUNTIME_ROOT="$fake_runtime" \
    A2UI_PROVIDER_CONFIG="$TMP/provider.env" \
    A2UI_LLM_ENDPOINT=https://override.example/v1/chat/completions \
    A2UI_SKIP_VENV_PREPARE=1 A2UI_PROBE_PYTHON=ok A2UI_PROBE_ELF=ok \
    A2UI_PROBE_FONT=ok A2UI_PROBE_DISPLAY=ok \
    A2UI_TEST_BACKEND_CONFIG_CAPTURE="$TMP/backend-config.json" \
    A2UI_TEST_QT_CONFIG_CAPTURE="$TMP/qt-config.json" \
    A2UI_BACKEND_LAUNCHER="$ROOT/tests/runtime/fake_backend.py" \
    "$ROOT/scripts/start_ubuntu_demo.sh" --build-dir "$fake_build" --port "$port" &
runtime_pid=$!

python3 - "$port" <<'PY'
import json, sys, time
from urllib.request import Request, urlopen
base = "http://127.0.0.1:{}".format(sys.argv[1])
for _ in range(50):
    try:
        if json.load(urlopen(base + "/health", timeout=.2))["status"] == "ok":
            break
    except Exception:
        time.sleep(.05)
else:
    raise SystemExit("健康检查未就绪")
request = Request(base + "/api/calculations",
                  data=b'{"expression":"1+2","result":3}',
                  headers={"Content-Type": "application/json"}, method="POST")
assert json.load(urlopen(request))["id"] == "runtime-1"
summary = json.load(urlopen(base + "/api/calculations/summary"))
assert summary["count"] == 1 and summary["latest"]["result"] == 3
PY
wait "$runtime_pid"
runtime_pid=""
python3 - "$TMP/backend-config.json" "$TMP/qt-config.json" <<'PY'
import json, sys
backend = json.load(open(sys.argv[1]))
qt = json.load(open(sys.argv[2]))
assert backend == {
    "endpoint": "https://override.example/v1/chat/completions",
    "model": "configured-model",
    "timeout": "91",
    "keyConfigured": True,
}
assert qt == {"compositionTimeout": "122000", "llmKeyPresent": False}
PY
python3 - "$port" <<'PY'
import socket, sys
s = socket.socket()
s.settimeout(.2)
assert s.connect_ex(("127.0.0.1", int(sys.argv[1]))) != 0
s.close()
PY
printf 'Provider 配置优先级、密钥边界、后端健康、CRUD 与退出清理测试通过\n'
