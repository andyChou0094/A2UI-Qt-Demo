"""Run and record the seven real-provider natural-language acceptance scenarios."""

import argparse
import json
import os
import re
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REQUIRED_ENDPOINT = "https://api.deepseek.com/chat/completions"
REQUIRED_MODEL = "deepseek-v4-flash"
SENSITIVE_KEYS = {
    "authorization", "a2ui_llm_api_key", "deepseek_api", "api_key",
    "access_token", "refresh_token", "password", "secret",
}
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from backend.agent_service.layout_plan import CompositionError, SurfaceCompiler
from backend.agent_service.llm_adapter import adapter_from_environment, config_from_environment


BUSINESS_TYPES = {
    "Calculator",
    "CalculationHistory",
    "CalculationStats",
    "Clock",
    "NotePad",
}

SCENARIOS = (
    {
        "name": "左右",
        "prompt": "把计算器放左边，历史记录放右边",
        "fixture": "top-bottom.json",
        "expected": "Row(Calculator,CalculationHistory)",
    },
    {
        "name": "上下",
        "prompt": "计算器在上，历史记录在下",
        "fixture": "top-bottom.json",
        "expected": "Column(Calculator,CalculationHistory)",
    },
    {
        "name": "侧栏",
        "prompt": "统计和时钟做左侧栏，其余放右侧",
        "fixture": "sidebar.json",
        "expected": "Row(Column(CalculationStats,Clock),Column(Calculator,CalculationHistory))",
    },
    {
        "name": "2x2",
        "prompt": "只用 Row 和 Column 做成上下两行、每行两个区域的 2×2 面板",
        "fixture": "nested-2x2.json",
        "expected": "Column(Row(Calculator,CalculationStats),Row(CalculationHistory,NotePad))",
    },
    {
        "name": "权重突出",
        "prompt": "计算器宽度约为历史记录两倍",
        "fixture": "weighted-highlight.json",
        "expected": "weight Calculator:CalculationHistory = 2:1, justify=start",
    },
    {
        "name": "重复组件",
        "prompt": "再添加一个独立计算器",
        "fixture": "top-bottom.json",
        "expected": "two Calculator leaves; preserve the existing Calculator ID",
    },
    {
        "name": "不支持布局",
        "prompt": "用 Grid，让历史记录跨两列并支持响应式断点",
        "fixture": "top-bottom.json",
        "expected": "unsupported_layout",
    },
)


def load_surface(name):
    path = ROOT / "shared" / "fixtures" / "surface-spec" / "valid" / name
    return json.loads(path.read_text(encoding="utf-8"))


def business_ids(surface):
    return {
        node["id"]: node["type"]
        for node in surface["nodes"]
        if node["type"] in BUSINESS_TYPES
    }


def structure(surface):
    by_id = {node["id"]: node for node in surface["nodes"]}

    def visit(node_id):
        node = by_id[node_id]
        if node["type"] in BUSINESS_TYPES:
            return node["type"]
        return "{}({})".format(
            node["type"], ",".join(visit(child) for child in node["children"])
        )

    return visit(surface["root"])


def evaluate(scenario, current, surface, error_code):
    checks = []
    before = business_ids(current)
    if scenario["name"] == "不支持布局":
        checks.append(error_code == "unsupported_layout")
        return checks, before, before, {}

    if surface is None:
        return [False], before, {}, {}

    after = business_ids(surface)
    checks.append(error_code is None)
    checks.append(all(after.get(node_id) == node_type for node_id, node_type in before.items()))

    if scenario["name"] in {"左右", "上下", "侧栏"}:
        checks.append(structure(surface) == scenario["expected"])
    elif scenario["name"] == "2x2":
        by_id = {node["id"]: node for node in surface["nodes"]}
        root = by_id[surface["root"]]
        children = [by_id[node_id] for node_id in root.get("children", [])]
        checks.extend((
            root["type"] == "Column",
            len(children) == 2,
            all(node["type"] == "Row" and len(node.get("children", [])) == 2 for node in children),
        ))
    elif scenario["name"] == "权重突出":
        by_type = {node["type"]: node for node in surface["nodes"]}
        root = next(node for node in surface["nodes"] if node["id"] == surface["root"])
        checks.extend((
            root["type"] == "Row",
            root.get("justify", "start") == "start",
            by_type.get("Calculator", {}).get("weight") == 2,
            by_type.get("CalculationHistory", {}).get("weight") == 1,
        ))
    elif scenario["name"] == "重复组件":
        calculator_ids = [node_id for node_id, kind in after.items() if kind == "Calculator"]
        new_calculators = [node_id for node_id in calculator_ids if node_id not in before]
        checks.extend((len(calculator_ids) == 2, len(new_calculators) == 1))

    new_ids = {node_id: kind for node_id, kind in after.items() if node_id not in before}
    return checks, before, after, new_ids


def validate_acceptance_config(config):
    if config is None:
        raise SystemExit(
            "请设置 A2UI_LLM_ENDPOINT、A2UI_LLM_MODEL 和 A2UI_LLM_API_KEY"
        )
    if config.endpoint != REQUIRED_ENDPOINT or config.model != REQUIRED_MODEL:
        raise SystemExit(
            "真实验收只允许固定 endpoint {} 和模型 {}".format(
                REQUIRED_ENDPOINT, REQUIRED_MODEL
            )
        )
    return config


def sensitive_report_reason(value, current_key):
    def walk(item):
        if isinstance(item, dict):
            for key, nested in item.items():
                if str(key).strip().lower() in SENSITIVE_KEYS:
                    return "报告包含敏感字段 {}".format(key)
                reason = walk(nested)
                if reason:
                    return reason
        elif isinstance(item, list):
            for nested in item:
                reason = walk(nested)
                if reason:
                    return reason
        return None

    reason = walk(value)
    if reason:
        return reason
    serialized = json.dumps(value, ensure_ascii=False, sort_keys=True)
    if current_key and current_key in serialized:
        return "报告疑似包含当前 API Key"
    if re.search(r"(?i)\bauthorization\s*[:=]", serialized):
        return "报告疑似包含 Authorization header"
    if re.search(r"(?i)\bbearer\s+[A-Za-z0-9._~+/=-]{8,}", serialized):
        return "报告疑似包含 Bearer 凭据"
    return None


def report_is_complete(report):
    scenarios = report.get("scenarios", [])
    return (len(scenarios) == 7
            and all(item.get("passed") is True for item in scenarios)
            and report.get("passed") is True)


def write_report_safely(report, output_path, current_key, state_home=None):
    reason = sensitive_report_reason(report, current_key)
    if reason:
        raise ValueError("验收报告未写入：{}".format(reason))
    if not report_is_complete(report):
        raise ValueError("验收报告未写入：七场景门禁未达到 7/7")

    state_root = Path(state_home or os.environ.get(
        "XDG_STATE_HOME", Path.home() / ".local" / "state"
    )) / "a2ui-qt-demo" / "acceptance"
    state_root.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", prefix="report-", suffix=".json",
        dir=str(state_root), delete=False
    ) as staging:
        staging.write(payload)
        staging.flush()
        staging_path = Path(staging.name)
    try:
        staged = json.loads(staging_path.read_text(encoding="utf-8"))
        reason = sensitive_report_reason(staged, current_key)
        if reason:
            raise ValueError("验收报告未写入：{}".format(reason))
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", prefix=".llm-acceptance-",
            suffix=".tmp", dir=str(output_path.parent), delete=False
        ) as target_tmp:
            target_tmp.write(payload)
            target_tmp.flush()
            target_tmp_path = Path(target_tmp.name)
        os.replace(str(target_tmp_path), str(output_path))
    finally:
        if staging_path.exists():
            staging_path.unlink()


def run(output_path):
    config = validate_acceptance_config(config_from_environment())
    adapter = adapter_from_environment()
    compiler = SurfaceCompiler()
    results = []

    for scenario in SCENARIOS:
        current = load_surface(scenario["fixture"])
        plan = None
        surface = None
        error_code = None
        diagnostics = []
        raw_content = None
        try:
            plan = adapter.generate_layout_plan(
                scenario["prompt"],
                current,
                json.loads(
                    (ROOT / "shared" / "catalog" / "component-catalog.json").read_text(
                        encoding="utf-8"
                    )
                ),
            )
            raw_content = adapter.last_response_content
            surface = compiler.compile(plan, current)
        except CompositionError as error:
            raw_content = adapter.last_response_content
            error_code = error.code
            diagnostics = error.diagnostics

        checks, before, after, new_ids = evaluate(
            scenario, current, surface, error_code
        )
        results.append({
            "name": scenario["name"],
            "prompt": scenario["prompt"],
            "currentFixture": scenario["fixture"],
            "expected": scenario["expected"],
            "passed": all(checks),
            "checks": checks,
            "rawLayoutPlan": raw_content,
            "layoutPlan": plan,
            "compiledSurface": surface,
            "actualStructure": structure(surface) if surface else None,
            "stableBusinessIdsBefore": before,
            "stableBusinessIdsAfter": after,
            "newBusinessIds": new_ids,
            "errorCode": error_code,
            "diagnostics": diagnostics,
        })

    report = {
        "runAt": datetime.now(timezone.utc).isoformat(),
        "providerEndpoint": config.endpoint,
        "model": config.model,
        "apiKeyRecorded": False,
        "passed": all(item["passed"] for item in results),
        "scenarios": results,
    }
    summary = {
        "output": str(output_path) if report_is_complete(report) else None,
        "persisted": False,
        "passed": report["passed"],
        "scenarioResults": [
            {"name": item["name"], "passed": item["passed"], "errorCode": item["errorCode"]}
            for item in results
        ],
    }
    if not report_is_complete(report):
        reason = sensitive_report_reason(report, config.api_key)
        if reason:
            raise ValueError("验收报告未写入：{}".format(reason))
        print(json.dumps(summary, ensure_ascii=False, indent=2))
        return 1
    write_report_safely(report, output_path, config.api_key)
    summary["persisted"] = True
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0 if report["passed"] else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "docs" / "llm-acceptance-results.json",
    )
    args = parser.parse_args()
    return run(args.output.resolve())


if __name__ == "__main__":
    sys.exit(main())
