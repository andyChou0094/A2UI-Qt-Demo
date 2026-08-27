import unittest

try:
    from fastapi.testclient import TestClient
    from backend.agent_service.app import create_agent_app
except ImportError:
    TestClient = None
    create_agent_app = None

from scripts.generate_legal_surfaces import generate_corpus


@unittest.skipUnless(TestClient is not None, "FastAPI test runtime is not installed")
class SurfaceImportBatchTest(unittest.TestCase):
    def test_all_fixed_seed_legal_samples_round_trip_through_import(self):
        corpus = generate_corpus()
        with TestClient(create_agent_app()) as client:
            for sample in corpus["samples"]:
                with self.subTest(sample=sample["id"]):
                    response = client.post("/surface/import", json=sample["surface"])
                    self.assertEqual(response.status_code, 200)
                    self.assertEqual(response.json(), {"surface": sample["surface"]})


if __name__ == "__main__":
    unittest.main()
