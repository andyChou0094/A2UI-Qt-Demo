import json
from pathlib import Path
import unittest

from backend.agent_service.layout_plan import CompositionError, SurfaceCompiler
from backend.agent_service.surface_validator import validate_surface_spec


ROOT = Path(__file__).resolve().parents[3]
PLANS = ROOT / "shared" / "fixtures" / "layout-plan" / "valid"
SURFACES = ROOT / "shared" / "fixtures" / "surface-spec" / "valid"


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


class SurfaceCompilerTest(unittest.TestCase):
    def test_every_fixture_compiles_deterministically_and_validates(self):
        compiler = SurfaceCompiler()
        plans = sorted(PLANS.glob("*.json"))
        self.assertEqual(len(plans), 7)
        for plan_path in plans:
            plan = load(plan_path)
            if plan_path.name == "duplicate-calculators.json":
                current = load(SURFACES / "empty-surface.json")
            else:
                current = load(SURFACES / plan_path.name)
            first = compiler.compile(plan, current)
            second = compiler.compile(plan, current)
            self.assertEqual(first, second, plan_path.name)
            self.assertEqual(validate_surface_spec(first), [], plan_path.name)

    def test_existing_ids_are_reused_and_new_ids_are_stable_and_distinct(self):
        compiler = SurfaceCompiler()
        current = load(SURFACES / "top-bottom.json")
        plan = {
            "version": "0.1",
            "root": {
                "kind": "layout", "type": "Row", "children": [
                    {"kind": "existing", "id": "calculator-main", "type": "Calculator"},
                    {"kind": "new", "type": "Calculator"},
                ],
            },
        }
        surface = compiler.compile(plan, current)
        leaves = [node for node in surface["nodes"] if node["type"] == "Calculator"]
        self.assertEqual(leaves[0]["id"], "calculator-main")
        self.assertEqual(leaves[1]["id"], "calculator-1")

    def test_new_nodes_cannot_supply_final_ids(self):
        plan = {
            "version": "0.1",
            "root": {"kind": "new", "type": "Calculator", "id": "model-chosen"},
        }
        with self.assertRaises(CompositionError) as caught:
            SurfaceCompiler().compile(plan)
        self.assertEqual(caught.exception.code, "invalid_layout_plan")

    def test_unsupported_layouts_return_explicit_diagnostics(self):
        unsupported = (
            {"kind": "layout", "type": "Grid", "children": []},
            {"kind": "layout", "type": "Row", "children": [], "columnSpan": 2},
            {"kind": "layout", "type": "Row", "children": [], "overlap": True},
            {"kind": "layout", "type": "Row", "children": [], "wrap": True},
            {"kind": "layout", "type": "Splitter", "children": []},
            {"kind": "layout", "type": "Dock", "children": []},
            {"kind": "layout", "type": "Row", "children": [], "breakpoints": []},
        )
        for root in unsupported:
            with self.subTest(root=root):
                with self.assertRaises(CompositionError) as caught:
                    SurfaceCompiler().compile({"version": "0.1", "root": root})
                self.assertEqual(caught.exception.code, "unsupported_layout")


if __name__ == "__main__":
    unittest.main()
