"""Run the reproducible 50-sample Provider baseline through POST /compose."""

import argparse
import json
import os
import statistics
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from backend.agent_service.llm_adapter import adapter_from_environment, config_from_environment
from backend.agent_service.surface_validator import validate_surface_spec
from scripts.run_llm_acceptance import (
    REQUIRED_ENDPOINT,
    REQUIRED_MODEL,
    sensitive_report_reason,
    validate_acceptance_config,
)


CORPUS_PATH = ROOT / "shared" / "evaluation" / "provider-baseline-v1.json"
REGRESSION_CORPUS_PATH = (
    ROOT / "shared" / "evaluation" / "provider-regressions-v1.json"
)
FIXTURE_ROOT = ROOT / "shared" / "fixtures" / "surface-spec" / "valid"
FAILURE_STAGES = {
    "llm_provider_error": "transport",
    "invalid_layout_plan": "semantic",
    "unsupported_layout": "semantic",
    "invalid_compiled_surface": "compilation",
    "invalid_current_surface": "final_validation",
}


class TimedAdapter(object):
    def __init__(self, delegate):
        self.delegate = delegate
        self.last_seconds = None

    def generate_layout_plan(self, instruction, current_surface, effective_catalog):
        started = time.monotonic()
        try:
            return self.delegate.generate_layout_plan(
                instruction, current_surface, effective_catalog
            )
        finally:
            self.last_seconds = time.monotonic() - started


def load_corpus(path=CORPUS_PATH):
    corpus = json.loads(Path(path).read_text(encoding="utf-8"))
    samples = corpus.get("samples", [])
    ids = [sample.get("id") for sample in samples]
    if len(samples) != 50 or len(set(ids)) != 50:
        raise ValueError("Provider 基线语料必须包含 50 个唯一稳定样本 ID")
    return corpus


def load_regression_corpus(path=REGRESSION_CORPUS_PATH):
    corpus = json.loads(Path(path).read_text(encoding="utf-8"))
    cases = corpus.get("cases", [])
    ids = [case.get("id") for case in cases]
    if not cases or len(ids) != len(set(ids)) or any(not item for item in ids):
        raise ValueError("Provider 回归语料必须包含唯一且非空的稳定用例 ID")
    required = {"sourceSampleId", "fixture", "prompt", "expectedErrorCode", "guardrail"}
    if any(not required.issubset(case) for case in cases):
        raise ValueError("Provider 回归语料缺少最小复现或安全边界字段")
    return corpus


def classify_failure(code, diagnostics):
    text = " ".join(str(item) for item in diagnostics).lower()
    if code == "llm_provider_error" and "timed out" in text:
        return "timeout"
    if code == "invalid_layout_plan" and "invalid json" in text:
        return "parse"
    return FAILURE_STAGES.get(code, "transport")


def metric(values):
    if not values:
        return {"averageSeconds": None, "p50Seconds": None}
    return {
        "averageSeconds": sum(values) / len(values),
        "p50Seconds": statistics.median(values),
    }


def atomic_write(report, output_path, current_key, require_complete=True):
    reason = sensitive_report_reason(report, current_key)
    if reason:
        raise ValueError("基线报告未写入：{}".format(reason))
    if require_complete and (report.get("status") != "complete"
                             or report.get("completedSamples") != 50):
        raise ValueError("正式基线必须包含 50 个最终样本")
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", prefix=".provider-baseline-",
        suffix=".tmp", dir=str(output_path.parent), delete=False
    ) as temporary:
        temporary.write(payload)
        temporary.flush()
        temporary_path = Path(temporary.name)
    try:
        os.replace(str(temporary_path), str(output_path))
    finally:
        if temporary_path.exists():
            temporary_path.unlink()


def build_report(corpus, config, results, interruption=None):
    successful = [item for item in results if item["validSurface"]]
    all_provider = [item["providerSeconds"] for item in results
                    if item["providerSeconds"] is not None]
    all_compose = [item["composeSeconds"] for item in results]
    successful_provider = [item["providerSeconds"] for item in successful]
    successful_compose = [item["composeSeconds"] for item in successful]
    complete = len(results) == 50 and interruption is None
    return {
        "baselineVersion": "provider-baseline-report-v1",
        "corpusVersion": corpus["corpusVersion"],
        "runAt": datetime.now(timezone.utc).isoformat(),
        "providerEndpoint": config.endpoint,
        "model": config.model,
        "temperature": 0,
        "timeoutSeconds": config.timeout_seconds,
        "execution": "serial",
        "requestedSamples": 50,
        "completedSamples": len(results),
        "status": "complete" if complete else "incomplete",
        "incompleteReason": interruption,
        "apiKeyRecorded": False,
        "validSurfaceCount": len(successful),
        "validSurfaceRate": len(successful) / 50.0,
        "allSamplesProvider": metric(all_provider),
        "allSamplesCompose": metric(all_compose),
        "successfulSamplesProvider": metric(successful_provider),
        "successfulSamplesCompose": metric(successful_compose),
        "failureCounts": {
            stage: sum(item.get("failureStage") == stage for item in results)
            for stage in ("transport", "timeout", "parse", "semantic", "compilation", "final_validation")
        },
        "samples": results,
    }


def run(output_path, incomplete_output_path=None):
    try:
        from fastapi.testclient import TestClient
        from backend.agent_service.app import create_agent_app
    except ImportError as error:
        raise SystemExit("缺少 FastAPI TestClient 运行环境：{}".format(error))
    config = validate_acceptance_config(config_from_environment())
    corpus = load_corpus()
    timed_adapter = TimedAdapter(adapter_from_environment())
    application = create_agent_app(timed_adapter)
    results = []
    interruption = None
    try:
        with TestClient(application) as client:
            for sample in corpus["samples"]:
                current = json.loads(
                    (FIXTURE_ROOT / sample["fixture"]).read_text(encoding="utf-8")
                )
                timed_adapter.last_seconds = None
                started = time.monotonic()
                response = client.post(
                    "/compose", json={"prompt": sample["prompt"], "currentSurface": current}
                )
                compose_seconds = time.monotonic() - started
                code = None
                diagnostics = []
                surface = None
                if response.status_code == 200:
                    surface = response.json().get("surface")
                    diagnostics = validate_surface_spec(surface)
                    if diagnostics:
                        code = "invalid_final_surface"
                else:
                    error = response.json().get("error", {})
                    code = error.get("code", "transport_error")
                    diagnostics = error.get("diagnostics", [])
                valid = surface is not None and not diagnostics
                results.append({
                    "id": sample["id"],
                    "category": sample["category"],
                    "prompt": sample["prompt"],
                    "currentFixture": sample["fixture"],
                    "providerSeconds": timed_adapter.last_seconds,
                    "composeSeconds": compose_seconds,
                    "validSurface": valid,
                    "failureStage": None if valid else (
                        "final_validation" if code == "invalid_final_surface"
                        else classify_failure(code, diagnostics)
                    ),
                    "errorCode": code,
                    "diagnostics": diagnostics,
                    "rawLayoutPlan": getattr(timed_adapter.delegate, "last_response_content", None),
                    "surface": surface,
                })
    except KeyboardInterrupt:
        interruption = "operator_interrupt"
    except Exception as error:
        interruption = "external_runtime_error: {}".format(type(error).__name__)

    report = build_report(corpus, config, results, interruption)
    if report["status"] == "complete":
        atomic_write(report, output_path, config.api_key)
        return 0
    incomplete_path = Path(incomplete_output_path) if incomplete_output_path else (
        Path(os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state"))
        / "a2ui-qt-demo" / "baseline" / "provider-baseline-incomplete.json"
    )
    atomic_write(report, incomplete_path, config.api_key, require_complete=False)
    return 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path,
                        default=ROOT / "docs" / "provider-baseline-results.json")
    parser.add_argument("--incomplete-output", type=Path)
    parser.add_argument("--confirm-50-requests", action="store_true")
    args = parser.parse_args()
    print("成本提示：本次将串行发起恰好 50 次真实 Provider 请求。")
    if not args.confirm_50_requests:
        print("未执行：请确认轮换密钥与成本后添加 --confirm-50-requests。")
        return 2
    return run(args.output.resolve(), args.incomplete_output)


if __name__ == "__main__":
    sys.exit(main())
