"""Create a developer placement review from measured map snapshots and the approved plan.

This script reads JSON/screenshots and writes a standalone developer HTML review.
It does not import Unreal, change game assets, or generate the player floor plan.
"""

import argparse
import base64
import hashlib
import io
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
DEFAULT_SNAPSHOTS = ROOT / "Saved/Automation/LooseLootExhibition20261003"
DEFAULT_PLAN = ROOT / "ProjectResources/SourceArt/Gallery/LooseLootExhibitionLayout.json"
DEFAULT_OUTPUT = Path("C:/Users/User/.codex/visualizations/2026/09/10/01a08b8f-da17-7b93-94c0-0baa20f17791/loot-exhibition-layout.html")
TEMPLATE = Path(__file__).with_name("loot_exhibition_layout.template.html")


def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def point(values):
    return [round(float(value), 2) for value in values[:3]]


def actor_index(snapshot):
    return {actor["name"]: actor for actor in snapshot["actors"]}


def actor_class(actor):
    return actor.get("klass", actor.get("class", ""))


def footprint(component):
    """The audited component World AABB projected onto XY, in centimeters."""
    lo, hi = component["bounds"]
    return [round(float(lo[0]), 1), round(float(lo[1]), 1),
            round(float(hi[0]), 1), round(float(hi[1]), 1)]


def geometry(snapshot, floor_z):
    walls, paintings, seen = [], [], set()
    for actor in snapshot["actors"]:
        for component in actor.get("components", []):
            mesh = component.get("mesh") or ""
            if not component.get("bounds"):
                continue
            lower = mesh.lower()
            rect = footprint(component)
            bottom, top = component["bounds"][0][2], component["bounds"][1][2]
            body_wall = ("wall" in lower and not any(token in lower for token in
                         ("trim", "skirt", "ceiling", "roof", "floor", "glass", "door")))
            if body_wall and bottom <= floor_z + 180 and top > floor_z + 80:
                key = tuple(round(value) for value in rect)
                if key not in seen:
                    seen.add(key)
                    walls.append(rect)
            if "sm_canvas_painting_" in lower:
                paintings.append({"name": actor["name"], "label": actor.get("label", actor["name"]),
                                  "rect": rect,
                                  "active": actor_class(actor) == "BP_PaintingDisplayCase_C"})
    return {"walls": walls, "paintings": paintings}


def support(actor):
    if actor is None:
        return None
    return {"name": actor["name"], "label": actor.get("label", actor["name"]),
            "location": point(actor["transform"]["location"]),
            "scale": point(actor["transform"]["scale"]),
            "components": [{"mesh": component.get("mesh"), "rect": footprint(component)}
                           for component in actor.get("components", []) if component.get("bounds")]}


def category(actor):
    text = str(actor.get("spawn", {}).get("spawn_category", ""))
    return "VaultFixed" if "VAULT" in text.upper() or any("VaultFixed" in tag for tag in actor.get("tags", [])) else "ExhibitionRoom"


def capture_metadata(directory):
    records = []
    for path in sorted(directory.glob("*.json")):
        try:
            data = load(path)
        except (ValueError, OSError):
            continue
        if isinstance(data, dict):
            records.extend(data.get("captures", []))
        elif isinstance(data, list):
            records.extend(record for record in data if isinstance(record, dict))
    return records


def image_data(path):
    """Bound screenshot payload size without changing the source capture."""
    try:
        from PIL import Image
    except ImportError:
        if path.stat().st_size > 90000:
            raise RuntimeError("Pillow is needed for capture thumbnails; run with the bundled workspace Python")
        mime = "image/png" if path.suffix.lower() == ".png" else "image/jpeg"
        return "data:" + mime + ";base64," + base64.b64encode(path.read_bytes()).decode("ascii")
    with Image.open(path) as source:
        image = source.convert("RGB")
        image.thumbnail((640, 400))
        buffer = io.BytesIO()
        image.save(buffer, "JPEG", quality=72, optimize=True)
    return "data:image/jpeg;base64," + base64.b64encode(buffer.getvalue()).decode("ascii")


def captures(directory, code):
    if not directory.exists():
        return []
    records = capture_metadata(directory)
    result = []
    for path in sorted(directory.iterdir()):
        if path.suffix.lower() not in (".png", ".jpg", ".jpeg") or code.lower() not in path.stem.lower():
            continue
        record = next((row for row in records if Path(str(row.get("path", row.get("file", "")))).name == path.name), {})
        data = image_data(path)
        if data:
            context = record.get("context", record.get("environment", record.get("world", "캡처 환경 메타데이터 미확인")))
            if str(context).startswith("Editor Lit / Lumen SceneCapture2D"):
                context = "Editor Lit · Lumen SceneCapture2D · 임시 Row 미리보기"
            base_color = "basecolor" in path.stem.lower() or "base color" in str(context).lower()
            if base_color:
                context = "배치 확인용 Base Color · 실제 야간 밝기 아님"
            flashlight_cd = None if base_color else record.get("flashlight_cd", record.get("flashlight_intensity_cd"))
            result.append({"name": path.name, "data": data, "context": str(context),
                           "flashlight_cd": flashlight_cd,
                           "station": record.get("spawn_actor", record.get("station"))})
        if len(result) == 4:
            break
    return result


def build_map(plan, directory):
    code = plan["code"]
    before_path = directory / (code + "_before.json")
    after_path = directory / (code + "_after.json")
    before = load(before_path)
    after = load(after_path) if after_path.exists() else None
    if plan.get("expected_before_sha256") and before["sha256"] != plan["expected_before_sha256"]:
        raise ValueError(code + ": before snapshot does not match approved plan hash")
    old_actors, new_actors = actor_index(before), actor_index(after) if after else {}
    stations = []
    for index, station in enumerate(plan["stations"], 1):
        old = old_actors[station["spawn_actor"]]
        new = new_actors.get(station["spawn_actor"])
        focus = new_actors.get(station["focus_painting"], old_actors.get(station["focus_painting"]))
        stations.append({"number": index, "name": station["spawn_actor"], "label": old.get("label", old["name"]),
                         "category": category(old), "before": point(old["transform"]["location"]),
                         "after": point(new["transform"]["location"]) if new else point(station["location"]),
                         "support_before": support(old_actors.get(station["support_actor"])),
                         "support_after": support(new_actors.get(station["support_actor"])),
                         "support_name": station["support_actor"], "mesh": station["mesh"],
                         "scale": point(station["scale"]), "yaw": station["yaw"],
                         "floor_z": station["floor_z"], "approach": point(station["approach"]),
                         "focus_name": station["focus_painting"], "focus_label": focus.get("label", focus["name"]) if focus else station["focus_painting"] or "연계 작품 없음"})
    if len(stations) != 12:
        raise ValueError(code + ": expected 12 candidate stations")
    floor_z = min(float(station["floor_z"]) for station in plan["stations"])
    before_geometry = geometry(before, floor_z)
    after_geometry = geometry(after, floor_z) if after else before_geometry
    rects = before_geometry["walls"] + after_geometry["walls"]
    points = [(rect[0], rect[1]) for rect in rects] + [(rect[2], rect[3]) for rect in rects]
    points.extend(station[state][:2] for station in stations for state in ("before", "after"))
    bounds = [min(p[0] for p in points), min(p[1] for p in points), max(p[0] for p in points), max(p[1] for p in points)]
    map_file = ROOT / "Content/Maps" / (plan["map"] + ".umap")
    after_hash = (after.get("sha256") or hashlib.sha256(map_file.read_bytes()).hexdigest()) if after else None
    return {"code": code, "map": plan["map"], "before_hash": before["sha256"],
            "after_hash": after_hash,
            "after_measured": after is not None, "bounds": bounds,
            "before_geometry": before_geometry, "after_geometry": after_geometry,
            "stations": stations, "captures": captures(directory / "Captures", code)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--snapshots", type=Path, default=DEFAULT_SNAPSHOTS)
    parser.add_argument("--plan", type=Path, default=DEFAULT_PLAN)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    plan = load(args.plan)
    payload = {"maps": [build_map(row, args.snapshots) for row in plan["maps"]],
               "plan_hash": hashlib.sha256(args.plan.read_bytes()).hexdigest(),
               "runtime_counts": plan.get("runtime_counts", {"vault": 1, "exhibition": 4, "verified": False})}
    qa_file = args.snapshots / "qa.json"
    if qa_file.exists():
        qa = load(qa_file)
        runtime = [row["runtime"] for row in qa.get("maps", [])]
        if len(runtime) == 3 and all(row["status"] == "PASS" for row in runtime) and qa.get("plan_sha256") == payload["plan_hash"]:
            payload["runtime_counts"] = {**runtime[0]["counts"], "verified": True}
    encoded = json.dumps(payload, ensure_ascii=False, separators=(",", ":"), allow_nan=False).replace("<", "\\u003c")
    template = TEMPLATE.read_text(encoding="utf-8")
    result = template.replace("__LOOT_LAYOUT_DATA__", encoded)
    if len(result.encode("utf-8")) >= 1_000_000:
        for row in payload["maps"]:
            row["captures"] = row["captures"][:1]
        encoded = json.dumps(payload, ensure_ascii=False, separators=(",", ":"), allow_nan=False).replace("<", "\\u003c")
        result = template.replace("__LOOT_LAYOUT_DATA__", encoded)
    if len(result.encode("utf-8")) >= 1_000_000:
        raise ValueError("Visualization payload exceeds 1 MB")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(result, encoding="utf-8")
    print(json.dumps({"output": str(args.output), "bytes": len(result.encode("utf-8")),
                      "maps": [{"code": row["code"], "stations": len(row["stations"]),
                                "walls": len(row["after_geometry"]["walls"]),
                                "paintings": len(row["after_geometry"]["paintings"]),
                                "after_measured": row["after_measured"], "captures": len(row["captures"])} for row in payload["maps"]]}))


if __name__ == "__main__":
    main()
