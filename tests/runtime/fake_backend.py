#!/usr/bin/env python3
"""仅供 Ubuntu 生命周期测试使用的零依赖回环探针。"""

import json
import os
import signal
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


records = []

capture_path = os.environ.get("A2UI_TEST_BACKEND_CONFIG_CAPTURE")
if capture_path:
    with open(capture_path, "w") as capture:
        json.dump({
            "endpoint": os.environ.get("A2UI_LLM_ENDPOINT"),
            "model": os.environ.get("A2UI_LLM_MODEL"),
            "timeout": os.environ.get("A2UI_LLM_TIMEOUT_SECONDS"),
            "keyConfigured": bool(os.environ.get("A2UI_LLM_API_KEY")),
        }, capture)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, _format, *_args):
        pass

    def respond(self, status, body):
        payload = json.dumps(body).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self):
        if self.path == "/health":
            return self.respond(200, {"status": "ok"})
        if self.path == "/api/calculations/summary":
            return self.respond(200, {
                "count": len(records),
                "latest": records[-1] if records else None,
            })
        if self.path.startswith("/api/calculations"):
            return self.respond(200, records)
        return self.respond(404, {})

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = json.loads(self.rfile.read(length) or b"{}")
        record = {
            "id": "runtime-1", "expression": body["expression"],
            "result": body["result"], "note": "",
            "createdAt": "2026-08-18T00:00:00Z",
            "updatedAt": "2026-08-18T00:00:00Z",
        }
        records.append(record)
        self.respond(201, record)


server = ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), Handler)
signal.signal(signal.SIGTERM, lambda *_args: sys.exit(0))
server.serve_forever()
