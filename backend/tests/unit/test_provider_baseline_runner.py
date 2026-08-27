from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from backend.agent_service.llm_adapter import LlmConfig
from scripts.run_provider_baseline import (
    atomic_write,
    build_report,
    classify_failure,
    load_corpus,
    load_regression_corpus,
    metric,
)


class ProviderBaselineRunnerTest(unittest.TestCase):
    def test_corpus_has_exactly_fifty_stable_unique_ids(self):
        corpus = load_corpus()
        self.assertEqual(len(corpus["samples"]), 50)
        self.assertEqual(corpus["samples"][0]["id"], "PB-001")
        self.assertEqual(corpus["samples"][-1]["id"], "PB-050")
        self.assertIn("unsupported", {item["category"] for item in corpus["samples"]})

    def test_confirmed_semantic_noop_has_a_fixed_regression_prompt(self):
        corpus = load_regression_corpus()
        self.assertEqual(corpus["corpusVersion"], "provider-regressions-v1")
        case = corpus["cases"][0]
        self.assertEqual(case["sourceSampleId"], "PB-048")
        self.assertEqual(case["expectedErrorCode"], "unsupported_layout")
        self.assertIn("自定义字体", case["prompt"])
        self.assertIn("不得", case["guardrail"])

    def test_average_and_p50_are_reported_without_tail_percentiles(self):
        self.assertEqual(metric([4.0, 1.0, 2.0, 3.0]), {
            "averageSeconds": 2.5,
            "p50Seconds": 2.5,
        })
        self.assertEqual(metric([]), {"averageSeconds": None, "p50Seconds": None})

    def test_failure_stage_classification_uses_first_known_boundary(self):
        self.assertEqual(classify_failure("llm_provider_error", ["timed out"]), "timeout")
        self.assertEqual(classify_failure("llm_provider_error", ["HTTP 429"]), "transport")
        self.assertEqual(classify_failure("invalid_layout_plan", ["invalid JSON"]), "parse")
        self.assertEqual(classify_failure("invalid_layout_plan", ["bad field"]), "semantic")
        self.assertEqual(classify_failure("invalid_compiled_surface", []), "compilation")

    def test_fixed_denominator_and_success_subset_metrics(self):
        corpus = load_corpus()
        config = LlmConfig("https://example.test", "model", "rotated-key", 90)
        results = [
            {
                "id": "PB-001",
                "providerSeconds": 1.0,
                "composeSeconds": 1.2,
                "validSurface": True,
                "failureStage": None,
            },
            {
                "id": "PB-002",
                "providerSeconds": 2.0,
                "composeSeconds": 2.2,
                "validSurface": False,
                "failureStage": "semantic",
            },
        ]
        report = build_report(corpus, config, results, "operator_interrupt")
        self.assertEqual(report["status"], "incomplete")
        self.assertEqual(report["completedSamples"], 2)
        self.assertEqual(report["validSurfaceRate"], 1 / 50.0)
        self.assertEqual(report["successfulSamplesProvider"]["averageSeconds"], 1.0)
        self.assertNotIn("p95Seconds", str(report))

    def test_incomplete_or_sensitive_report_never_replaces_formal_baseline(self):
        with TemporaryDirectory() as directory:
            target = Path(directory) / "formal.json"
            target.write_text("old\n", encoding="utf-8")
            incomplete = {"status": "incomplete", "completedSamples": 4, "samples": []}
            with self.assertRaises(ValueError):
                atomic_write(incomplete, target, "rotated-key")
            self.assertEqual(target.read_text(encoding="utf-8"), "old\n")
            unsafe = {"status": "complete", "completedSamples": 50,
                      "samples": [{"diagnostic": "Bearer token-value-123"}]}
            with self.assertRaises(ValueError):
                atomic_write(unsafe, target, "rotated-key")
            self.assertEqual(target.read_text(encoding="utf-8"), "old\n")


if __name__ == "__main__":
    unittest.main()
