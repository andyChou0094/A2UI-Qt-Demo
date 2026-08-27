import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[3]


class SharedContractsTest(unittest.TestCase):
    def test_catalog_covers_exactly_the_five_opaque_business_leaves(self):
        catalog = json.loads(
            (ROOT / "shared" / "catalog" / "component-catalog.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(set(catalog), {"catalogVersion", "components"})
        self.assertEqual(catalog["catalogVersion"], "0.1")
        self.assertEqual(
            {component["type"] for component in catalog["components"]},
            {"Calculator", "CalculationHistory", "CalculationStats", "Clock", "NotePad"},
        )
        for component in catalog["components"]:
            self.assertEqual(
                set(component),
                {
                    "type",
                    "version",
                    "description",
                    "multiple",
                    "allowedDisplayFields",
                    "layoutHints",
                },
            )
            self.assertIs(component["multiple"], True)
            self.assertEqual(component["allowedDisplayFields"], [])

    def test_surface_schema_is_closed_and_pins_protocol_layout_tokens(self):
        schema = json.loads(
            (ROOT / "shared" / "schema" / "surface-spec-v0.schema.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertIs(schema["additionalProperties"], False)
        self.assertEqual(schema["properties"]["version"]["const"], "0.1")
        self.assertEqual(schema["properties"]["surfaceId"]["const"], "main")
        self.assertEqual(schema["properties"]["nodes"]["maxItems"], 32)
        layout = schema["$defs"]["layoutNode"]
        business = schema["$defs"]["businessNode"]
        self.assertIs(layout["additionalProperties"], False)
        self.assertIs(business["additionalProperties"], False)
        self.assertEqual(layout["properties"]["type"]["enum"], ["Row", "Column"])
        self.assertEqual(
            layout["properties"]["gap"]["enum"],
            ["none", "small", "medium", "large"],
        )
        self.assertEqual(schema["$defs"]["weight"]["minimum"], 0)
        self.assertEqual(schema["$defs"]["weight"]["maximum"], 10)
        self.assertEqual(
            schema["x-rendererSemantics"]["gapLogicalPixels"],
            {"none": 0, "small": 4, "medium": 8, "large": 16},
        )
        self.assertEqual(schema["x-rendererSemantics"]["containerMargin"], 0)
        self.assertEqual(
            schema["x-semanticRules"]["positiveChildWeight"]["requiredParentJustify"],
            "start",
        )
        self.assertIs(
            schema["x-semanticRules"]["positiveChildWeight"]["allowsMainAxisSpacer"],
            False,
        )

    def test_layout_plan_schema_is_closed_and_new_instances_have_no_final_id(self):
        schema = json.loads(
            (ROOT / "shared" / "schema" / "layout-plan-v0.schema.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertIs(schema["additionalProperties"], False)
        self.assertEqual(schema["properties"]["version"]["const"], "0.1")
        self.assertIs(schema["$defs"]["layout"]["additionalProperties"], False)
        self.assertIs(schema["$defs"]["existing"]["additionalProperties"], False)
        self.assertIs(schema["$defs"]["new"]["additionalProperties"], False)
        self.assertNotIn("id", schema["$defs"]["new"]["properties"])

    def test_contracts_do_not_grant_business_request_or_arbitrary_style_fields(self):
        catalog_text = (ROOT / "shared" / "catalog" / "component-catalog.json").read_text(
            encoding="utf-8"
        ).lower()
        schema_text = (ROOT / "shared" / "schema" / "surface-spec-v0.schema.json").read_text(
            encoding="utf-8"
        ).lower()
        for forbidden in (
            '"url"',
            '"method"',
            '"body"',
            '"action"',
            '"script"',
            '"qss"',
            '"color"',
            '"font"',
            '"margin"',
            '"padding"',
            '"width"',
            '"height"',
        ):
            self.assertNotIn(forbidden, catalog_text)
            self.assertNotIn(forbidden, schema_text)


if __name__ == "__main__":
    unittest.main()
