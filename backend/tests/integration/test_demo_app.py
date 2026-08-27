import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

try:
    from fastapi.testclient import TestClient
    from backend.demo_app import create_demo_app
except ImportError:
    TestClient = None
    create_demo_app = None


ROOT = Path(__file__).resolve().parents[3]


class CapturingAdapter(object):
    def generate_layout_plan(self, instruction, current_surface, effective_catalog):
        return {
            "version": "0.1",
            "root": {
                "kind": "layout",
                "type": "Row",
                "children": [
                    {
                        "kind": "existing",
                        "id": "calculator-main",
                        "type": "Calculator",
                    },
                    {
                        "kind": "existing",
                        "id": "history-main",
                        "type": "CalculationHistory",
                    },
                ],
            },
        }


@unittest.skipUnless(TestClient is not None, "FastAPI test runtime is not installed")
class DemoAppTest(unittest.TestCase):
    def test_one_process_serves_health_business_and_composition_routes(self):
        current = json.loads(
            (ROOT / "shared" / "fixtures" / "surface-spec" / "valid" / "top-bottom.json").read_text(
                encoding="utf-8"
            )
        )
        with TemporaryDirectory() as directory:
            application = create_demo_app(
                Path(directory) / "calculations.sqlite3", CapturingAdapter()
            )
            with TestClient(application) as client:
                health = client.get("/health")
                created = client.post(
                    "/api/calculations",
                    json={"expression": "6*7", "result": 42},
                )
                composed = client.post(
                    "/compose",
                    json={"prompt": "左右排列", "currentSurface": current},
                )

        self.assertEqual(health.status_code, 200)
        self.assertEqual(health.json()["status"], "ok")
        self.assertEqual(created.status_code, 201)
        self.assertEqual(created.json()["result"], 42)
        self.assertEqual(composed.status_code, 200)
        self.assertEqual(composed.json()["surface"]["nodes"][0]["type"], "Row")


if __name__ == "__main__":
    unittest.main()
