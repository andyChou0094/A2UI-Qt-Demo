import unittest

from backend.agent_service.surface_validator import validate_surface_spec
from scripts.generate_legal_surfaces import DEFAULT_SEED, generate_corpus


class LegalSurfaceGeneratorTest(unittest.TestCase):
    def test_fixed_seed_is_stable_and_every_sample_self_validates(self):
        first = generate_corpus(DEFAULT_SEED)
        second = generate_corpus(DEFAULT_SEED)
        self.assertEqual(first, second)
        self.assertGreaterEqual(len(first["samples"]), 20)
        for sample in first["samples"]:
            self.assertEqual(validate_surface_spec(sample["surface"]), [], sample["id"])

    def test_coverage_includes_contract_tokens_and_boundaries(self):
        coverage = generate_corpus()["coverage"]
        self.assertEqual(coverage["rootKinds"], ["layout", "leaf"])
        self.assertEqual(coverage["layoutTypes"], ["Column", "Row"])
        self.assertEqual(set(coverage["componentTypes"]), {
            "Calculator", "CalculationHistory", "CalculationStats", "Clock", "NotePad"
        })
        self.assertEqual(set(coverage["gaps"]), {"none", "small", "medium", "large"})
        self.assertEqual(set(coverage["aligns"]), {"start", "center", "end", "stretch"})
        self.assertEqual(set(coverage["justifies"]), {
            "start", "center", "end", "spaceBetween", "spaceAround", "spaceEvenly"
        })
        self.assertEqual(coverage["maxNodeCount"], 32)
        self.assertTrue(coverage["maxDepthBoundaryCovered"])
        self.assertTrue(coverage["emptyContainerCovered"])
        self.assertTrue(coverage["duplicateComponentsCovered"])


if __name__ == "__main__":
    unittest.main()
