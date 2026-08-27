import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from backend.agent_service.llm_adapter import LlmConfig

from scripts.run_llm_acceptance import (
    REQUIRED_ENDPOINT,
    REQUIRED_MODEL,
    SCENARIOS,
    evaluate,
    load_surface,
    report_is_complete,
    sensitive_report_reason,
    structure,
    validate_acceptance_config,
    write_report_safely,
)


class LlmAcceptanceRunnerTest(unittest.TestCase):
    def test_missing_or_non_fixed_config_fails_before_provider_call(self):
        with self.assertRaises(SystemExit):
            validate_acceptance_config(None)
        with self.assertRaises(SystemExit):
            validate_acceptance_config(LlmConfig(
                "https://example.test", REQUIRED_MODEL, "rotated", 30
            ))

    def test_sensitive_report_scanner_rejects_key_authorization_and_fields(self):
        self.assertIsNotNone(sensitive_report_reason({"raw": "rotated-key"}, "rotated-key"))
        self.assertIsNotNone(sensitive_report_reason({"raw": "Authorization: hidden"}, "key"))
        self.assertIsNotNone(sensitive_report_reason({"raw": "Bearer token-value-123"}, "key"))
        self.assertIsNotNone(sensitive_report_reason({"api_key": "redacted"}, "key"))
        self.assertIsNone(sensitive_report_reason({"apiKeyRecorded": False}, "key"))

    def test_report_is_complete_only_for_seven_passed_scenarios(self):
        report = {"passed": True, "scenarios": [{"passed": True}] * 7}
        self.assertTrue(report_is_complete(report))
        report["scenarios"][3] = {"passed": False}
        self.assertFalse(report_is_complete(report))

    def test_atomic_writer_keeps_target_when_gate_or_scan_fails(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "docs" / "result.json"
            target.parent.mkdir()
            target.write_text("old\n", encoding="utf-8")
            report = {"passed": True, "scenarios": [{"passed": True}] * 7}
            write_report_safely(report, target, "rotated-key", root / "state")
            self.assertIn('"passed": true', target.read_text(encoding="utf-8"))
            unsafe = dict(report, raw="Bearer token-value-123")
            with self.assertRaises(ValueError):
                write_report_safely(unsafe, target, "rotated-key", root / "state")
            self.assertNotIn("Bearer", target.read_text(encoding="utf-8"))
            with self.assertRaises(ValueError):
                write_report_safely(
                    {"passed": False, "scenarios": [{"passed": True}] * 6},
                    target, "rotated-key", root / "state"
                )

    def test_reference_fixtures_match_structural_expectations(self):
        for scenario in SCENARIOS[:4]:
            with self.subTest(scenario=scenario["name"]):
                surface = load_surface(scenario["fixture"])
                if scenario["name"] not in {"左右", "2x2"}:
                    self.assertEqual(structure(surface), scenario["expected"])

    def test_provider_failure_is_recorded_as_failed_instead_of_crashing(self):
        scenario = SCENARIOS[0]
        current = load_surface(scenario["fixture"])
        checks, before, after, new_ids = evaluate(
            scenario, current, None, "llm_provider_error"
        )
        self.assertEqual(checks, [False])
        self.assertTrue(before)
        self.assertEqual(after, {})
        self.assertEqual(new_ids, {})

    def test_unsupported_layout_preserves_current_business_ids(self):
        scenario = SCENARIOS[-1]
        current = load_surface(scenario["fixture"])
        checks, before, after, new_ids = evaluate(
            scenario, current, None, "unsupported_layout"
        )
        self.assertEqual(checks, [True])
        self.assertEqual(after, before)
        self.assertEqual(new_ids, {})


if __name__ == "__main__":
    unittest.main()
