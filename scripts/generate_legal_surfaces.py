"""Generate deterministic, validator-proven legal SurfaceSpec documents."""

import argparse
import json
import random
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from backend.agent_service.surface_validator import validate_surface_spec


DEFAULT_SEED = 20260821
BUSINESS_TYPES = (
    "Calculator", "CalculationHistory", "CalculationStats", "Clock", "NotePad"
)
GAPS = ("none", "small", "medium", "large")
ALIGNS = ("start", "center", "end", "stretch")
JUSTIFIES = ("start", "center", "end", "spaceBetween", "spaceAround", "spaceEvenly")


def document(root, nodes):
    return {"version": "0.1", "surfaceId": "main", "root": root, "nodes": nodes}


def edge_samples():
    samples = [
        ("root-leaf", document("clock-root", [{"id": "clock-root", "type": "Clock"}])),
        ("empty-row", document("root", [{"id": "root", "type": "Row", "children": []}])),
        ("empty-column", document("root", [{"id": "root", "type": "Column", "children": []}])),
    ]
    depth_nodes = []
    for level in range(1, 8):
        node_id = "level{}".format(level)
        child = "level{}".format(level + 1) if level < 7 else "calculator-main"
        depth_nodes.append({
            "id": node_id,
            "type": "Column" if level % 2 else "Row",
            "children": [child],
        })
    depth_nodes.append({"id": "calculator-main", "type": "Calculator"})
    samples.append(("depth-boundary", document("level1", depth_nodes)))
    boundary_nodes = [{"id": "root", "type": "Column",
                       "children": ["calculator-{}".format(index) for index in range(1, 32)]}]
    boundary_nodes.extend(
        {"id": "calculator-{}".format(index), "type": "Calculator"}
        for index in range(1, 32)
    )
    samples.append(("node-boundary", document("root", boundary_nodes)))
    return samples


def random_surface(rng, index):
    nodes = []
    counter = [0]

    def next_id(prefix):
        counter[0] += 1
        return "{}-{}-{}".format(prefix, index, counter[0])

    def leaf(component_type=None, weight=None):
        component_type = component_type or rng.choice(BUSINESS_TYPES)
        node = {"id": next_id(component_type.lower()), "type": component_type}
        if weight is not None:
            node["weight"] = weight
        nodes.append(node)
        return node["id"]

    def layout(depth, force_all_types=False):
        node_id = next_id("layout")
        child_ids = []
        node = {
            "id": node_id,
            "type": rng.choice(("Row", "Column")),
            "children": child_ids,
            "gap": GAPS[(index + depth) % len(GAPS)],
            "align": ALIGNS[(index * 2 + depth) % len(ALIGNS)],
            "justify": JUSTIFIES[(index + depth) % len(JUSTIFIES)],
        }
        nodes.append(node)
        types = BUSINESS_TYPES if force_all_types else tuple(
            rng.choice(BUSINESS_TYPES) for _ in range(rng.randint(2, 4))
        )
        for position, component_type in enumerate(types):
            if depth < 3 and position == 0 and index % 3 == 0:
                child_ids.append(layout(depth + 1))
                continue
            weight = (position % 3) + 1 if index % 4 == 0 else None
            child_ids.append(leaf(component_type, weight))
        if any(next(item for item in nodes if item["id"] == child).get("weight", 0) > 0
               for child in child_ids):
            node["justify"] = "start"
        return node_id

    root = layout(1, force_all_types=index == 1)
    return document(root, nodes)


def coverage_matrix(samples):
    matrix = {
        "rootKinds": set(), "layoutTypes": set(), "componentTypes": set(),
        "gaps": set(), "aligns": set(), "justifies": set(),
        "weights": set(), "maxNodeCount": 0, "maxDepthBoundaryCovered": False,
        "emptyContainerCovered": False, "duplicateComponentsCovered": False,
    }
    for sample in samples:
        surface = sample["surface"]
        by_id = {node["id"]: node for node in surface["nodes"]}
        root = by_id[surface["root"]]
        matrix["rootKinds"].add("layout" if root["type"] in {"Row", "Column"} else "leaf")
        matrix["maxNodeCount"] = max(matrix["maxNodeCount"], len(surface["nodes"]))
        counts = {}
        for node in surface["nodes"]:
            if node["type"] in {"Row", "Column"}:
                matrix["layoutTypes"].add(node["type"])
                if not node["children"]:
                    matrix["emptyContainerCovered"] = True
                for field, target in (("gap", "gaps"), ("align", "aligns"),
                                      ("justify", "justifies")):
                    if field in node:
                        matrix[target].add(node[field])
            else:
                matrix["componentTypes"].add(node["type"])
                counts[node["type"]] = counts.get(node["type"], 0) + 1
            if "weight" in node:
                matrix["weights"].add(node["weight"])
        matrix["duplicateComponentsCovered"] |= any(value > 1 for value in counts.values())
        matrix["maxDepthBoundaryCovered"] |= sample["category"] == "depth-boundary"
    return {key: sorted(value) if isinstance(value, set) else value
            for key, value in matrix.items()}


def generate_corpus(seed=DEFAULT_SEED):
    rng = random.Random(seed)
    pairs = edge_samples()
    pairs.extend(("generated", random_surface(rng, index)) for index in range(1, 21))
    samples = []
    for index, (category, surface) in enumerate(pairs, 1):
        diagnostics = validate_surface_spec(surface)
        if diagnostics:
            raise ValueError("生成样本 DSL-{0:03d} 非法：{1}".format(
                index, "; ".join(diagnostics)
            ))
        samples.append({
            "id": "DSL-{:03d}".format(index),
            "category": category,
            "surface": surface,
        })
    return {
        "corpusVersion": "legal-surface-corpus-v1",
        "seed": seed,
        "coverage": coverage_matrix(samples),
        "samples": samples,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    corpus = generate_corpus(args.seed)
    payload = json.dumps(corpus, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8")
    else:
        sys.stdout.write(payload)
    return 0


if __name__ == "__main__":
    sys.exit(main())
