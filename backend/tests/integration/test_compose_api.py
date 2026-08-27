import json
from pathlib import Path
import unittest

try:
    from fastapi.testclient import TestClient
    from backend.agent_service.app import create_agent_app
except ImportError:
    TestClient = None
    create_agent_app = None


ROOT = Path(__file__).resolve().parents[3]


def load_surface(name):
    return json.loads(
        (ROOT / "shared" / "fixtures" / "surface-spec" / "valid" / name).read_text(
            encoding="utf-8"
        )
    )


class CapturingAdapter(object):
    def __init__(self, plan):
        self.plan = plan
        self.calls = []

    def generate_layout_plan(self, instruction, current_surface, effective_catalog):
        self.calls.append((instruction, current_surface, effective_catalog))
        return self.plan


@unittest.skipUnless(TestClient is not None, "FastAPI test runtime is not installed")
class ComposeApiTest(unittest.TestCase):
    def test_success_returns_one_complete_valid_target_surface(self):
        current = load_surface("top-bottom.json")
        plan = json.loads(
            (ROOT / "shared" / "fixtures" / "layout-plan" / "valid" / "top-bottom.json").read_text(
                encoding="utf-8"
            )
        )
        adapter = CapturingAdapter(plan)
        with TestClient(create_agent_app(adapter)) as client:
            response = client.post(
                "/compose", json={"prompt": "上下排列", "currentSurface": current}
            )
        self.assertEqual(response.status_code, 200)
        self.assertEqual(set(response.json()), {"surface"})
        surface = response.json()["surface"]
        self.assertEqual(set(surface), {"version", "surfaceId", "root", "nodes"})
        self.assertEqual(surface["surfaceId"], "main")
        self.assertEqual(len(adapter.calls), 1)
        instruction, passed_surface, catalog = adapter.calls[0]
        self.assertEqual(instruction, "上下排列")
        self.assertEqual(passed_surface, current)
        self.assertEqual(catalog["catalogVersion"], "0.1")

    def test_unsupported_layout_and_invalid_plan_are_structured_errors(self):
        current = load_surface("empty-surface.json")
        cases = (
            (
                {"version": "0.1", "root": {"kind": "layout", "type": "Grid", "children": []}},
                "unsupported_layout",
            ),
            ({"version": "0.1", "root": {"kind": "new", "type": "Calculator", "id": "bad"}},
             "invalid_layout_plan"),
        )
        for plan, expected_code in cases:
            with self.subTest(expected_code=expected_code):
                with TestClient(create_agent_app(CapturingAdapter(plan))) as client:
                    response = client.post(
                        "/compose", json={"prompt": "compose", "currentSurface": current}
                    )
                self.assertEqual(response.status_code, 422)
                self.assertEqual(response.json()["error"]["code"], expected_code)
                self.assertTrue(response.json()["error"]["diagnostics"])

    def test_request_contract_is_closed(self):
        current = load_surface("empty-surface.json")
        adapter = CapturingAdapter({
            "version": "0.1",
            "root": {"kind": "layout", "type": "Column", "children": []},
        })
        with TestClient(create_agent_app(adapter)) as client:
            response = client.post(
                "/compose",
                json={"prompt": "empty", "currentSurface": current, "method": "DELETE"},
            )
        self.assertEqual(response.status_code, 422)
        self.assertEqual(adapter.calls, [])


if __name__ == "__main__":
    unittest.main()
