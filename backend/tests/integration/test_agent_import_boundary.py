import ast
from pathlib import Path
import unittest


class AgentImportBoundaryTest(unittest.TestCase):
    def test_agent_service_does_not_import_business_persistence(self):
        agent_root = Path(__file__).resolve().parents[2] / "agent_service"
        forbidden = "backend.mock_business_api"
        for source_path in agent_root.rglob("*.py"):
            tree = ast.parse(source_path.read_text(encoding="utf-8"))
            imported = []
            for node in ast.walk(tree):
                if isinstance(node, ast.Import):
                    imported.extend(alias.name for alias in node.names)
                elif isinstance(node, ast.ImportFrom):
                    imported.append(node.module or "")
            self.assertFalse(
                any(name == forbidden or name.startswith(forbidden + ".") for name in imported),
                "{} imports the business API layer".format(source_path),
            )


if __name__ == "__main__":
    unittest.main()
