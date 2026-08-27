from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

try:
    from fastapi.testclient import TestClient
    from backend.mock_business_api.app import create_business_app
except ImportError:
    TestClient = None
    create_business_app = None


@unittest.skipUnless(TestClient is not None, "FastAPI test runtime is not installed")
class CalculationApiTest(unittest.TestCase):
    def test_create_list_patch_delete_and_summary(self):
        with TemporaryDirectory() as directory:
            application = create_business_app(Path(directory) / "calculations.sqlite3")
            with TestClient(application) as client:
                older_response = client.post(
                    "/api/calculations",
                    json={"expression": "1+2", "result": 3},
                )
                newer_response = client.post(
                    "/api/calculations",
                    json={"expression": "6*7", "result": 42},
                )
                self.assertEqual(older_response.status_code, 201)
                self.assertEqual(newer_response.status_code, 201)
                older = older_response.json()
                newer = newer_response.json()
                self.assertEqual(
                    set(older),
                    {"id", "expression", "result", "note", "createdAt", "updatedAt"},
                )

                recent = client.get("/api/calculations", params={"limit": 2})
                self.assertEqual(recent.status_code, 200)
                self.assertEqual(
                    [record["id"] for record in recent.json()],
                    [newer["id"], older["id"]],
                )
                self.assertEqual(
                    client.get("/api/calculations", params={"limit": 51}).status_code,
                    422,
                )

                updated = client.patch(
                    "/api/calculations/{}".format(older["id"]),
                    json={"note": "verified"},
                )
                self.assertEqual(updated.status_code, 200)
                self.assertEqual(updated.json()["note"], "verified")
                self.assertEqual(updated.json()["expression"], "1+2")
                self.assertEqual(
                    client.patch(
                        "/api/calculations/{}".format(older["id"]),
                        json={"note": "x", "result": 99},
                    ).status_code,
                    422,
                )

                summary = client.get("/api/calculations/summary")
                self.assertEqual(summary.status_code, 200)
                self.assertEqual(summary.json()["count"], 2)
                self.assertEqual(summary.json()["latest"]["id"], newer["id"])

                deleted = client.delete(
                    "/api/calculations/{}".format(newer["id"])
                )
                self.assertEqual(deleted.status_code, 204)
                self.assertEqual(client.get("/api/calculations/summary").json()["count"], 1)
                self.assertEqual(
                    client.delete("/api/calculations/missing").status_code,
                    404,
                )

    def test_file_database_persists_across_app_restart(self):
        with TemporaryDirectory() as directory:
            database_path = Path(directory) / "calculations.sqlite3"
            first_application = create_business_app(database_path)
            with TestClient(first_application) as client:
                created = client.post(
                    "/api/calculations",
                    json={"expression": "8/2", "result": 4},
                ).json()

            second_application = create_business_app(database_path)
            with TestClient(second_application) as client:
                records = client.get("/api/calculations").json()
                self.assertEqual(len(records), 1)
                self.assertEqual(records[0]["id"], created["id"])


if __name__ == "__main__":
    unittest.main()
