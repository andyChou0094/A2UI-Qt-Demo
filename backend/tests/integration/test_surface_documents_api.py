import json
from pathlib import Path
import unittest

try:
    from fastapi.testclient import TestClient
    from backend.agent_service.app import create_agent_app
    from backend.agent_service.surface_documents import MAX_SURFACE_DOCUMENT_BYTES
except ImportError:
    TestClient = None
    create_agent_app = None
    MAX_SURFACE_DOCUMENT_BYTES = 64 * 1024


ROOT = Path(__file__).resolve().parents[3]
VALID = ROOT / "shared" / "fixtures" / "surface-spec" / "valid"
INVALID = ROOT / "shared" / "fixtures" / "surface-spec" / "invalid"
DEFAULT = ROOT / "shared" / "default-surface.json"


@unittest.skipUnless(TestClient is not None, "FastAPI test runtime is not installed")
class SurfaceDocumentsApiTest(unittest.TestCase):
    def setUp(self):
        self.client_context = TestClient(create_agent_app())
        self.client = self.client_context.__enter__()

    def tearDown(self):
        self.client_context.__exit__(None, None, None)

    def test_import_accepts_complete_valid_document(self):
        document = json.loads((VALID / "nested-2x2.json").read_text(encoding="utf-8"))
        response = self.client.post("/surface/import", json=document)
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json(), {"surface": document})

    def test_import_rejects_invalid_json_utf8_and_oversized_documents(self):
        cases = (
            b'{"version":',
            b"\xff\xfe",
            b" " * (MAX_SURFACE_DOCUMENT_BYTES + 1),
        )
        for payload in cases:
            with self.subTest(size=len(payload)):
                response = self.client.post(
                    "/surface/import",
                    content=payload,
                    headers={"Content-Type": "application/json"},
                )
                self.assertEqual(response.status_code, 422)
                self.assertEqual(
                    response.json()["error"]["code"], "invalid_surface_document"
                )

    def test_import_rejects_closed_schema_boundaries_and_graph_errors(self):
        fixture_names = (
            "unknown-field.json",
            "unknown-business-type.json",
            "node-limit-exceeded.json",
            "depth-limit-exceeded.json",
            "cycle.json",
            "multiple-parents.json",
            "unreachable-node.json",
        )
        for name in fixture_names:
            with self.subTest(name=name):
                response = self.client.post(
                    "/surface/import",
                    content=(INVALID / name).read_bytes(),
                    headers={"Content-Type": "application/json"},
                )
                self.assertEqual(response.status_code, 422)
                error = response.json()["error"]
                self.assertEqual(error["code"], "invalid_surface_document")
                self.assertTrue(error["diagnostics"])

    def test_export_is_canonical_and_round_trips_through_import(self):
        document = json.loads((VALID / "sidebar.json").read_text(encoding="utf-8"))
        exported = self.client.post("/surface/export", json=document)
        self.assertEqual(exported.status_code, 200)
        self.assertEqual(exported.headers["content-type"], "application/json")
        self.assertIn("surface-main.json", exported.headers["content-disposition"])
        self.assertTrue(exported.content.endswith(b"\n"))
        self.assertEqual(json.loads(exported.content.decode("utf-8")), document)
        imported = self.client.post(
            "/surface/import",
            content=exported.content,
            headers={"Content-Type": "application/json"},
        )
        self.assertEqual(imported.json(), {"surface": document})

    def test_export_revalidates_current_surface(self):
        response = self.client.post(
            "/surface/export",
            json={"version": "0.1", "surfaceId": "main", "root": "missing", "nodes": []},
        )
        self.assertEqual(response.status_code, 422)
        self.assertEqual(response.json()["error"]["code"], "invalid_surface_document")

    def test_default_endpoint_matches_versioned_source(self):
        expected = json.loads(DEFAULT.read_text(encoding="utf-8"))
        response = self.client.get("/surface/default")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json(), {"surface": expected})


if __name__ == "__main__":
    unittest.main()
