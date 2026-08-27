"""Closed structural and semantic validation for SurfaceSpec v0."""

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
NODE_ID = re.compile(r"^[A-Za-z][A-Za-z0-9_-]{0,63}$")
LAYOUT_TYPES = {"Row", "Column"}
LAYOUT_KEYS = {"id", "type", "children", "gap", "align", "justify", "weight"}
BUSINESS_KEYS = {"id", "type", "weight"}
ROOT_KEYS = {"version", "surfaceId", "root", "nodes"}
GAPS = {"none", "small", "medium", "large"}
ALIGNS = {"start", "center", "end", "stretch"}
JUSTIFIES = {"start", "center", "end", "spaceBetween", "spaceAround", "spaceEvenly"}


def _catalog_types(catalog_path=None):
    path = catalog_path or ROOT / "shared" / "catalog" / "component-catalog.json"
    catalog = json.loads(Path(path).read_text(encoding="utf-8"))
    return {
        item["type"]: bool(item["multiple"])
        for item in catalog["components"]
    }


def validate_surface_spec(document, max_nodes=32, max_depth=8, catalog_path=None):
    """Return a stable list of diagnostics; an empty list means valid."""
    errors = []
    if not isinstance(document, dict):
        return ["document must be an object"]
    if set(document) != ROOT_KEYS:
        errors.append("document fields must be exactly version, surfaceId, root, nodes")
    if document.get("version") != "0.1":
        errors.append("version must be 0.1")
    if document.get("surfaceId") != "main":
        errors.append("surfaceId must be main")

    root_id = document.get("root")
    if not isinstance(root_id, str) or not NODE_ID.match(root_id):
        errors.append("root must be a valid node ID")
    nodes = document.get("nodes")
    if not isinstance(nodes, list):
        return errors + ["nodes must be an array"]
    if len(nodes) > max_nodes:
        errors.append("node limit exceeded")

    catalog = _catalog_types(catalog_path)
    by_id = {}
    type_counts = {}
    children_by_id = {}
    for index, node in enumerate(nodes):
        label = "nodes[{}]".format(index)
        if not isinstance(node, dict):
            errors.append(label + " must be an object")
            continue
        node_id = node.get("id")
        node_type = node.get("type")
        if not isinstance(node_id, str) or not NODE_ID.match(node_id):
            errors.append(label + " has an invalid id")
            continue
        if node_id in by_id:
            errors.append("duplicate node id: " + node_id)
        by_id[node_id] = node
        if node_type in LAYOUT_TYPES:
            if not {"id", "type", "children"}.issubset(node):
                errors.append(label + " is missing layout fields")
            if not set(node).issubset(LAYOUT_KEYS):
                errors.append(label + " contains forbidden layout fields")
            children = node.get("children")
            if not isinstance(children, list) or any(not isinstance(item, str) for item in children):
                errors.append(label + ".children must be an array of IDs")
                children = []
            if len(children) > max_nodes:
                errors.append(label + ".children exceeds the node limit")
            children_by_id[node_id] = children
            if "gap" in node and node["gap"] not in GAPS:
                errors.append(label + " has an invalid gap")
            if "align" in node and node["align"] not in ALIGNS:
                errors.append(label + " has an invalid align")
            if "justify" in node and node["justify"] not in JUSTIFIES:
                errors.append(label + " has an invalid justify")
        elif node_type in catalog:
            if not {"id", "type"}.issubset(node):
                errors.append(label + " is missing business fields")
            if not set(node).issubset(BUSINESS_KEYS):
                errors.append(label + " contains forbidden business fields")
            type_counts[node_type] = type_counts.get(node_type, 0) + 1
            children_by_id[node_id] = []
        else:
            errors.append(label + " has an unknown or forbidden type")
            children_by_id[node_id] = []
        if "weight" in node:
            weight = node["weight"]
            if isinstance(weight, bool) or not isinstance(weight, int) or weight < 0 or weight > 10:
                errors.append(label + " has an invalid weight")

    if isinstance(root_id, str) and root_id not in by_id:
        errors.append("root does not reference a declared node")
    elif root_id in by_id and "weight" in by_id[root_id]:
        errors.append("root must not declare weight")

    parent_count = {node_id: 0 for node_id in by_id}
    for parent_id, children in children_by_id.items():
        parent = by_id.get(parent_id, {})
        has_positive_weight = False
        for child_id in children:
            if child_id not in by_id:
                errors.append(parent_id + " references unknown child " + child_id)
                continue
            parent_count[child_id] += 1
            if by_id[child_id].get("weight", 0) > 0:
                has_positive_weight = True
        if has_positive_weight and parent.get("justify", "start") != "start":
            errors.append(parent_id + " combines positive child weight with non-start justify")

    for node_id, count in parent_count.items():
        expected = 0 if node_id == root_id else 1
        if count != expected:
            errors.append(node_id + " has {} parents; expected {}".format(count, expected))

    visited = set()
    active = set()

    def visit(node_id, depth):
        if node_id in active:
            errors.append("cycle detected at " + node_id)
            return
        if node_id in visited or node_id not in by_id:
            return
        if depth > max_depth:
            errors.append("depth limit exceeded at " + node_id)
        active.add(node_id)
        for child_id in children_by_id.get(node_id, []):
            visit(child_id, depth + 1)
        active.remove(node_id)
        visited.add(node_id)

    if root_id in by_id:
        visit(root_id, 1)
    unreachable = set(by_id) - visited
    if unreachable:
        errors.append("unreachable nodes: " + ", ".join(sorted(unreachable)))

    for component_type, multiple in catalog.items():
        if not multiple and type_counts.get(component_type, 0) > 1:
            errors.append(component_type + " does not allow multiple instances")
    return errors


def load_and_validate_surface_spec(path, max_nodes=32, max_depth=8):
    try:
        document = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        return ["invalid JSON: " + str(error)]
    return validate_surface_spec(document, max_nodes=max_nodes, max_depth=max_depth)
