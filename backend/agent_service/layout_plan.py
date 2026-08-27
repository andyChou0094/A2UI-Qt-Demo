"""Closed LayoutPlan parsing and deterministic SurfaceSpec compilation."""

import json
import re
from pathlib import Path

from backend.agent_service.surface_validator import ROOT, validate_surface_spec


NODE_ID = re.compile(r"^[A-Za-z][A-Za-z0-9_-]{0,63}$")
LAYOUT_KEYS = {"kind", "type", "children", "gap", "align", "justify", "weight"}
EXISTING_KEYS = {"kind", "id", "type", "weight"}
NEW_KEYS = {"kind", "type", "weight"}
GAPS = {"none", "small", "medium", "large"}
ALIGNS = {"start", "center", "end", "stretch"}
JUSTIFIES = {"start", "center", "end", "spaceBetween", "spaceAround", "spaceEvenly"}
UNSUPPORTED_TYPES = {"Grid", "Splitter", "Dock", "Wrap", "Overlay", "Responsive"}
UNSUPPORTED_FIELDS = {
    "grid", "row", "column", "rowSpan", "columnSpan", "overlap", "wrap",
    "splitter", "dock", "breakpoint", "breakpoints", "responsive",
}


class CompositionError(Exception):
    def __init__(self, code, diagnostics):
        Exception.__init__(self, "; ".join(diagnostics))
        self.code = code
        self.diagnostics = diagnostics


def parse_layout_plan(payload):
    try:
        if isinstance(payload, bytes):
            payload = payload.decode("utf-8")
        document = json.loads(payload) if isinstance(payload, str) else payload
    except (UnicodeError, ValueError) as error:
        raise CompositionError("invalid_layout_plan", ["invalid JSON: " + str(error)])
    return document


def _load_catalog(catalog_path=None):
    path = Path(catalog_path) if catalog_path else ROOT / "shared" / "catalog" / "component-catalog.json"
    catalog = json.loads(path.read_text(encoding="utf-8"))
    return {item["type"]: bool(item["multiple"]) for item in catalog["components"]}


def _current_business_nodes(current_surface):
    if current_surface is None:
        return {}
    errors = validate_surface_spec(current_surface)
    if errors:
        raise CompositionError("invalid_current_surface", errors)
    return {
        node["id"]: node["type"]
        for node in current_surface["nodes"]
        if node["type"] not in {"Row", "Column"}
    }


def validate_layout_plan(plan, current_surface=None, catalog_path=None,
                         max_nodes=32, max_depth=8):
    errors = []
    if not isinstance(plan, dict):
        return ["LayoutPlan must be an object"]
    if set(plan) != {"version", "root"}:
        errors.append("LayoutPlan fields must be exactly version and root")
    if plan.get("version") != "0.1":
        errors.append("LayoutPlan version must be 0.1")
    catalog = _load_catalog(catalog_path)
    current = _current_business_nodes(current_surface)
    node_count = [0]
    existing_ids = set()
    type_counts = {}

    def visit(node, is_root=False, depth=1):
        node_count[0] += 1
        if depth > max_depth:
            errors.append("LayoutPlan depth limit exceeded")
        if not isinstance(node, dict):
            errors.append("plan node must be an object")
            return
        if UNSUPPORTED_FIELDS.intersection(node):
            raise CompositionError(
                "unsupported_layout",
                ["LayoutPlan requests unsupported layout fields: "
                 + ", ".join(sorted(UNSUPPORTED_FIELDS.intersection(node)))],
            )
        kind = node.get("kind")
        node_type = node.get("type")
        if node_type in UNSUPPORTED_TYPES:
            raise CompositionError(
                "unsupported_layout",
                ["SurfaceSpec v0 cannot express layout type " + node_type],
            )
        allowed = {"layout": LAYOUT_KEYS, "existing": EXISTING_KEYS, "new": NEW_KEYS}.get(kind)
        if allowed is None:
            errors.append("plan node has an invalid kind")
            return
        required = {
            "layout": {"kind", "type", "children"},
            "existing": {"kind", "id", "type"},
            "new": {"kind", "type"},
        }[kind]
        if not required.issubset(node) or not set(node).issubset(allowed):
            errors.append(kind + " node has missing or forbidden fields")
        if is_root and "weight" in node:
            errors.append("LayoutPlan root must not declare weight")
        if "weight" in node:
            weight = node["weight"]
            if isinstance(weight, bool) or not isinstance(weight, int) or not 0 <= weight <= 10:
                errors.append("plan node has an invalid weight")

        if kind == "layout":
            if node_type not in {"Row", "Column"}:
                errors.append("layout node type must be Row or Column")
            children = node.get("children")
            if not isinstance(children, list):
                errors.append("layout children must be an array")
                return
            if len(children) > max_nodes:
                errors.append("layout child limit exceeded")
            if "gap" in node and node["gap"] not in GAPS:
                errors.append("layout node has an invalid gap")
            if "align" in node and node["align"] not in ALIGNS:
                errors.append("layout node has an invalid align")
            if "justify" in node and node["justify"] not in JUSTIFIES:
                errors.append("layout node has an invalid justify")
            if any(isinstance(child, dict) and child.get("weight", 0) > 0 for child in children):
                if node.get("justify", "start") != "start":
                    errors.append("positive child weight requires start justify")
            for child in children:
                visit(child, depth=depth + 1)
            return

        if node_type not in catalog:
            errors.append("unknown business component type: " + str(node_type))
            return
        type_counts[node_type] = type_counts.get(node_type, 0) + 1
        if kind == "existing":
            component_id = node.get("id")
            if not isinstance(component_id, str) or not NODE_ID.match(component_id):
                errors.append("existing node has an invalid id")
            elif component_id in existing_ids:
                errors.append("existing component is referenced more than once: " + component_id)
            else:
                existing_ids.add(component_id)
                if current.get(component_id) != node_type:
                    errors.append("existing component does not match current Surface: " + component_id)

    try:
        visit(plan.get("root"), is_root=True)
    except CompositionError:
        raise
    if node_count[0] > max_nodes:
        errors.append("LayoutPlan node limit exceeded")
    for component_type, count in type_counts.items():
        if not catalog[component_type] and count > 1:
            errors.append(component_type + " does not allow multiple instances")
    return errors


def _id_base(component_type):
    return re.sub(r"(?<!^)(?=[A-Z])", "-", component_type).lower()


class SurfaceCompiler(object):
    def __init__(self, catalog_path=None, max_nodes=32, max_depth=8):
        self.catalog_path = catalog_path
        self.max_nodes = max_nodes
        self.max_depth = max_depth

    def compile(self, plan, current_surface=None):
        plan = parse_layout_plan(plan)
        errors = validate_layout_plan(
            plan,
            current_surface=current_surface,
            catalog_path=self.catalog_path,
            max_nodes=self.max_nodes,
            max_depth=self.max_depth,
        )
        if errors:
            raise CompositionError("invalid_layout_plan", errors)

        current = _current_business_nodes(current_surface)
        used_ids = set(current)
        counters = {}
        nodes = []

        def allocate(prefix):
            counter = counters.get(prefix, 0)
            while True:
                counter += 1
                candidate = "{}-{}".format(prefix, counter)
                if candidate not in used_ids:
                    counters[prefix] = counter
                    used_ids.add(candidate)
                    return candidate

        def compile_node(plan_node, is_root=False):
            kind = plan_node["kind"]
            if kind == "layout":
                node_id = allocate("layout")
                output = {
                    "id": node_id,
                    "type": plan_node["type"],
                    "children": [],
                }
                for field in ("gap", "align", "justify", "weight"):
                    if field in plan_node and not (is_root and field == "weight"):
                        output[field] = plan_node[field]
                nodes.append(output)
                for child in plan_node["children"]:
                    output["children"].append(compile_node(child))
                return node_id
            component_type = plan_node["type"]
            node_id = plan_node["id"] if kind == "existing" else allocate(_id_base(component_type))
            output = {"id": node_id, "type": component_type}
            if "weight" in plan_node and not is_root:
                output["weight"] = plan_node["weight"]
            nodes.append(output)
            return node_id

        root_id = compile_node(plan["root"], is_root=True)
        surface = {"version": "0.1", "surfaceId": "main", "root": root_id, "nodes": nodes}
        surface_errors = validate_surface_spec(
            surface,
            max_nodes=self.max_nodes,
            max_depth=self.max_depth,
            catalog_path=self.catalog_path,
        )
        if surface_errors:
            raise CompositionError("invalid_compiled_surface", surface_errors)
        return surface
