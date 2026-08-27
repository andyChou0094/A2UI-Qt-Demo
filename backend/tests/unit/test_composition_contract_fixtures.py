import copy
import json
from pathlib import Path
import unittest

from backend.agent_service.surface_validator import load_and_validate_surface_spec


ROOT = Path(__file__).resolve().parents[3]
VALID_SURFACES = ROOT / "shared" / "fixtures" / "surface-spec" / "valid"
INVALID_SURFACES = ROOT / "shared" / "fixtures" / "surface-spec" / "invalid"
VALID_PLANS = ROOT / "shared" / "fixtures" / "layout-plan" / "valid"

FORBIDDEN_FIELDS = (
    "url", "method", "request", "requestBody", "body", "action",
    "binding", "dataBinding", "signal", "slot", "script", "qss",
    "style", "color", "font", "margin", "padding", "x", "y",
    "width", "height", "minWidth", "maxWidth", "pixels",
)
PLAN_ROOT_KEYS = {"version", "root"}
PLAN_LAYOUT_KEYS = {"kind", "type", "children", "gap", "align", "justify", "weight"}
PLAN_EXISTING_KEYS = {"kind", "id", "type", "weight"}
PLAN_NEW_KEYS = {"kind", "type", "weight"}
BUSINESS_TYPES = {"Calculator", "CalculationHistory", "CalculationStats", "Clock", "NotePad"}


def plan_is_closed(plan):
    if not isinstance(plan, dict) or set(plan) != PLAN_ROOT_KEYS or plan.get("version") != "0.1":
        return False

    def validate_node(node, is_root=False, depth=1):
        if not isinstance(node, dict) or depth > 8:
            return False
        kind = node.get("kind")
        allowed = {
            "layout": PLAN_LAYOUT_KEYS,
            "existing": PLAN_EXISTING_KEYS,
            "new": PLAN_NEW_KEYS,
        }.get(kind)
        if allowed is None or not set(node).issubset(allowed):
            return False
        if is_root and "weight" in node:
            return False
        if kind == "layout":
            children = node.get("children")
            if node.get("type") not in {"Row", "Column"} or not isinstance(children, list):
                return False
            if any(not validate_node(child, depth=depth + 1) for child in children):
                return False
            if any(child.get("weight", 0) > 0 for child in children):
                if node.get("justify", "start") != "start":
                    return False
        elif node.get("type") not in BUSINESS_TYPES:
            return False
        elif kind == "existing" and not isinstance(node.get("id"), str):
            return False
        return True

    return validate_node(plan.get("root"), is_root=True)


class CompositionContractFixtureTest(unittest.TestCase):
    def test_all_valid_surface_fixtures_are_accepted(self):
        fixtures = sorted(VALID_SURFACES.glob("*.json"))
        self.assertEqual(len(fixtures), 7)
        for fixture in fixtures:
            self.assertEqual(load_and_validate_surface_spec(fixture), [], fixture.name)

    def test_all_invalid_surface_fixtures_are_rejected(self):
        fixtures = sorted(INVALID_SURFACES.glob("*.json"))
        self.assertGreaterEqual(len(fixtures), 11)
        for fixture in fixtures:
            self.assertTrue(load_and_validate_surface_spec(fixture), fixture.name)

    def test_all_valid_layout_plan_fixtures_use_the_closed_contract(self):
        fixtures = sorted(VALID_PLANS.glob("*.json"))
        self.assertEqual(len(fixtures), 7)
        for fixture in fixtures:
            plan = json.loads(fixture.read_text(encoding="utf-8"))
            self.assertTrue(plan_is_closed(plan), fixture.name)

    def test_forbidden_permissions_cannot_enter_any_agent_controlled_contract(self):
        catalog = json.loads((ROOT / "shared" / "catalog" / "component-catalog.json").read_text(encoding="utf-8"))
        surface_path = VALID_SURFACES / "top-bottom.json"
        surface = json.loads(surface_path.read_text(encoding="utf-8"))
        plan = json.loads((VALID_PLANS / "top-bottom.json").read_text(encoding="utf-8"))
        catalog_component_keys = {"type", "version", "description", "multiple", "allowedDisplayFields", "layoutHints"}
        for field in FORBIDDEN_FIELDS:
            poisoned_surface = copy.deepcopy(surface)
            poisoned_surface["nodes"][1][field] = "forbidden"
            self.assertTrue(load_and_validate_surface_spec(surface_path) == [])
            from backend.agent_service.surface_validator import validate_surface_spec
            self.assertTrue(validate_surface_spec(poisoned_surface), field)

            poisoned_plan = copy.deepcopy(plan)
            poisoned_plan["root"]["children"][0][field] = "forbidden"
            self.assertFalse(plan_is_closed(poisoned_plan), field)

            poisoned_component = copy.deepcopy(catalog["components"][0])
            poisoned_component[field] = "forbidden"
            self.assertFalse(set(poisoned_component).issubset(catalog_component_keys), field)


if __name__ == "__main__":
    unittest.main()
