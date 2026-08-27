"""Validate every deterministic legal DSL through the real import route."""

import argparse
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.generate_legal_surfaces import DEFAULT_SEED, generate_corpus


def run(seed=DEFAULT_SEED, output=None):
    try:
        from fastapi.testclient import TestClient
        from backend.agent_service.app import create_agent_app
    except ImportError as error:
        raise SystemExit("缺少 FastAPI TestClient 运行环境：{}".format(error))
    corpus = generate_corpus(seed)
    results = []
    with TestClient(create_agent_app()) as client:
        for sample in corpus["samples"]:
            response = client.post("/surface/import", json=sample["surface"])
            returned = response.json().get("surface") if response.status_code == 200 else None
            results.append({
                "id": sample["id"],
                "status": response.status_code,
                "completeMatch": returned == sample["surface"],
            })
    report = {
        "corpusVersion": corpus["corpusVersion"],
        "seed": seed,
        "coverage": corpus["coverage"],
        "passed": all(item["status"] == 200 and item["completeMatch"] for item in results),
        "samples": results,
    }
    if output:
        Path(output).write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                                encoding="utf-8")
    else:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["passed"] else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    return run(args.seed, args.output)


if __name__ == "__main__":
    sys.exit(main())
