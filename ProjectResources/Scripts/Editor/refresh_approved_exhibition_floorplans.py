"""Update only existing player floor-plan textures from saved architectural meshes.

Dedicated unattended Editor: MH_APPROVED_FLOORPLAN_STAGE=collect or import.
Bundled Python/Pillow: this_file.py --stage render.
Editor also accepts -MuseumFloorPlanStage=collect|import.
MH_APPROVED_FLOORPLAN_QUIT=1 (default) closes a dedicated Editor afterwards.

Collect does not save packages or generate PCG. Render preserves live DT bounds,
north and current texture resolution. Import overwrites only T_FloorPlan_M01~03;
no DT assignment, map save, asset creation or retired generator is used.
PNG contains architectural floor/wall/column/doorway geometry without labels,
characters, security coverage, artworks, loot or actor selection information.
"""
import argparse
import hashlib
import io
import itertools
import json
import math
import os
import re
import struct
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "Saved/Automation/ApprovedExhibition20261007/FloorPlans"
GEOMETRY = OUT / "geometry.json"
RENDER = OUT / "render.json"
DATA_TABLE = "/Game/Data/DataTable/DT_MapPresentation"
MAPS = {"M01": "M01_ClassicalPrototype", "M02": "M02_MoonlitPrototype",
        "M03": "M03_GlasshousePrototype"}
TAG = "ApprovedExhibition20261007"
SLICE_HEIGHT_CM = 160.
EPS = .00001
CANONICAL_SOURCES = {
    "M01": "ProjectResources/SourceArt/W7/Generated/T_FloorPlan_M01.png",
    "M02": "ProjectResources/SourceArt/W7/Generated/T_FloorPlan_M02.png",
    "M03": "ProjectResources/SourceArt/Gallery/M03/T_FloorPlan_M03.png"}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def fingerprint(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False,
                                    separators=(",", ":"), allow_nan=False).encode("utf-8")).hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")


def load_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def png_size(path):
    with Path(path).open("rb") as stream:
        header = stream.read(24)
    require(len(header) == 24 and header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR",
            "Expected PNG source: " + str(path))
    return list(struct.unpack(">II", header[16:24]))


def package_path(asset):
    package = asset.split(".")[0]
    require(package.startswith("/Game/"), "Only existing project assets permitted: " + asset)
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def asset_reference(value):
    match = re.search(r"(/Game/[^'\"]+)", str(value))
    require(match is not None, "Invalid project texture reference: " + str(value))
    return match.group(1).split(".")[0]


def source_path(relative):
    path = (ROOT / relative).resolve()
    require(path.is_relative_to((ROOT / "ProjectResources/SourceArt").resolve()) and path.is_file(),
            "Missing existing source or outside SourceArt: " + str(path))
    return path


def row_schema(rows):
    schema = load_json(ROOT / "ProjectResources/DataTableImports/DT_MapPresentation.json")
    expected = {r["Name"]: set(r) for r in schema if r.get("Name") in MAPS}
    require(isinstance(rows, list) and len({r.get("Name") for r in rows}) == len(rows), "Invalid DT row list")
    selected = {r["Name"]: r for r in rows if r.get("Name") in MAPS}
    require(set(selected) == set(MAPS) and set(expected) == set(MAPS), "Expected three current DT rows")
    for code, row in selected.items():
        require(set(row) == expected[code], code + ": current DT/source schema differs")
        require(row["MapId"] == code, code + ": MapId mismatch")
        require(asset_reference(row["FloorPlanTexture"]) == "/Game/Assets/UI/Map/T_FloorPlan_" + code,
                code + ": refusing different texture target")
        bounds = [float(row[k][a]) for k in ("WorldMin", "WorldMax") for a in ("X", "Y")]
        require(all(math.isfinite(v) for v in bounds) and bounds[2] > bounds[0] and bounds[3] > bounds[1],
                code + ": invalid current world bounds")
        require(str(row["MapNorthAxis"]).split("::")[-1] in ("PositiveX", "PositiveY", "NegativeX", "NegativeY"),
                code + ": unknown north axis")
        require(row["ZoneAnchors"] and row["DefaultExitAnchors"], code + ": missing existing markers")
    return selected


def project_xy(point, row):
    """Match existing C++ UV orientation. Clip canvas, not each geometry vertex."""
    lo, hi = row["WorldMin"], row["WorldMax"]
    x, y = (point[0] - lo["X"]) / (hi["X"] - lo["X"]), (point[1] - lo["Y"]) / (hi["Y"] - lo["Y"])
    return {"PositiveX": (1-y, 1-x), "PositiveY": (x, 1-y),
            "NegativeX": (y, x), "NegativeY": (1-x, y)}[str(row["MapNorthAxis"]).split("::")[-1]]


def architecture_kind(mesh_path):
    """Strict original architectural families, independently of old actor labels."""
    package = mesh_path.split(".")[0]
    name = package.rsplit("/", 1)[-1]
    if package.startswith("/Game/Assets/MapAssets/Showcase/Meshes/"):
        if re.fullmatch(r"SM_Display_Wall_(?:(?:100|200|300|400)|Bend_400|Corner)_01[ab]", name) or name == "SM_Wall_1500_01a":
            return "wall"
        if re.fullmatch(r"SM_Structural_Beam_01[a-e]", name):
            return "column"
        if re.fullmatch(r"SM_Floor_(?:Panel_01[a-g]|1500_01a)", name):
            return "floor"
        if re.fullmatch(r"SM_Door_01[ab]", name):
            return "doorframe"
    if package.startswith("/Game/Assets/MapAssets/PCMHall/Meshes/"):
        if re.fullmatch(r"SM_(?:DISPLAY_)?WALL_[AB]_(?:\d+|CORNER_\d+|WINDOW_[AB]_\d+)", name):
            return "wall"
        if re.fullmatch(r"SM_(?:WALL_[AB]_DOOR_[AB](?:_\d+)?|DISPLAY_DOOR_\d+)", name):
            return "doorframe"
        if re.fullmatch(r"SM_(?:COLUMN|PILLAR)(?:_[A-Z0-9]+)+", name):
            return "column"
        if re.fullmatch(r"SM_DISPLAY_FLOOR_\d+", name):
            return "floor"
    if package.startswith("/Game/Assets/MapAssets/AIUE5_vol10_01/Mesh/"):
        if re.fullmatch(r"SM_AI_vol10_01_walls_[1-7]_1", name) or name == "SM_AI_vol10_01_wall_damage_1_1":
            return "wall"
        if re.fullmatch(r"SM_AI_vol10_01_(?:column|pillar)_\d+_1", name):
            return "column"
        if re.fullmatch(r"SM_AI_vol10_01_door(?:_glass)?_\d+_1", name):
            return "doorframe"
        if re.fullmatch(r"SM_AI_vol10_01_stone_tile_[12]_1", name):
            return "floor"
    if package == "/Game/Assets/MapAssets/ConferenceRoom/Meshes/Floor/SM_Floor_01":
        return "floor"
    return None


def xyz(vector):
    return [float(getattr(vector, k)) for k in "xyz"]


def matrix(transform):
    q, s = transform.rotation, xyz(transform.scale3d)
    x, y, z, w = q.x, q.y, q.z, q.w
    r = [[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
         [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
         [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]]
    return [[r[i][j] * s[j] for j in range(3)] for i in range(3)], xyz(transform.translation)


def transform_point(point, transform):
    r, t = transform
    return [t[i] + sum(r[i][j] * point[j] for j in range(3)) for i in range(3)]


def hull_xy(points):
    points = sorted(set((round(p[0], 5), round(p[1], 5)) for p in points))
    def cross(a, b, c):
        return (b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0])
    def half(values):
        result = []
        for p in values:
            while len(result) >= 2 and cross(result[-2], result[-1], p) <= EPS:
                result.pop()
            result.append(p)
        return result
    return half(points)[:-1] + half(list(reversed(points)))[:-1]


def slice_segments(vertices, triangles, z):
    """Intersect actual LOD0 triangles. Do not bridge separate doorway jambs."""
    regular, coplanar = {}, {}
    def key(p):
        return tuple(round(float(v), 4) for v in p[:2])
    def add(a, b, destination):
        a, b = key(a), key(b)
        if math.dist(a, b) > EPS:
            edge = tuple(sorted((a, b)))
            destination[edge] = destination.get(edge, 0) + 1
    for index in range(0, len(triangles), 3):
        tri = [vertices[triangles[index + k]] for k in range(3)]
        dz = [p[2] - z for p in tri]
        if all(abs(v) <= EPS for v in dz):
            for a, b in zip(tri, tri[1:] + tri[:1]):
                add(a, b, coplanar)
            continue
        if min(dz) > EPS or max(dz) < -EPS:
            continue
        cuts = []
        for a, b in zip(tri, tri[1:] + tri[:1]):
            if abs(a[2] - z) <= EPS:
                cuts.append(key(a))
            if (a[2] < z < b[2]) or (b[2] < z < a[2]):
                alpha = (z - a[2]) / (b[2] - a[2])
                cuts.append(key([a[k] + alpha*(b[k]-a[k]) for k in (0, 1)]))
        cuts = list(set(cuts))
        if len(cuts) >= 2:
            a, b = max(itertools.combinations(cuts, 2), key=lambda pair: math.dist(*pair))
            add(a, b, regular)
    return [[list(a), list(b)] for a, b in sorted(set(regular) | {edge for edge, n in coplanar.items() if n % 2})]


def export_rows(unreal):
    table = unreal.load_asset(DATA_TABLE)
    require(table is not None, "Missing current DT_MapPresentation")
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    row_schema(rows)
    return rows


def select_source(texture, code):
    filenames, errors = [], []
    try:
        data = texture.get_editor_property("asset_import_data")
        filenames = [str(v) for v in data.extract_filenames()] if data else []
    except Exception as error:
        errors.append(str(error))
    canonical = source_path(CANONICAL_SOURCES[code])
    alternatives = {canonical}
    if code == "M03":
        alternatives.add(source_path("ProjectResources/SourceArt/W7/Generated/T_FloorPlan_M03.png"))
    imported = next((Path(n).resolve() for n in filenames
                     if Path(n).is_absolute() and Path(n).resolve() in alternatives), None)
    selected = imported or canonical
    return dict(relative=str(selected.relative_to(ROOT)).replace("\\", "/"),
                selected_from="current_asset_import_data" if imported else "existing_canonical_source",
                import_filenames=filenames, import_data_errors=errors, sha256_before=sha(selected))


def collect(unreal):
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(editor.get_game_world() is None, "Collect must run outside PIE")
    rows = export_rows(unreal)
    selected = row_schema(rows)
    before = {code: sha(ROOT / "Content/Maps" / (name + ".umap")) for code, name in MAPS.items()}
    result = dict(status="RUNNING", stage="collect", schema_version=1, script_sha256=sha(__file__),
                  map_rows=rows, dt_rows_sha256=fingerprint(rows), dt_package_sha256=sha(package_path(DATA_TABLE)),
                  source_meshes={}, maps={}, units="cm", saves=0,
                  information_policy="Architecture only; no labels or gameplay data in PNG",
                  geometry_method="Actual LOD0 triangles at floor+160cm; saved PCG floor instance footprints")
    sections = {}
    try:
        for code, name in MAPS.items():
            require(unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/" + name), "Cannot load " + code)
            floor = 10. if code == "M02" else 0.
            texture_path = asset_reference(selected[code]["FloorPlanTexture"])
            texture = unreal.load_asset(texture_path)
            require(isinstance(texture, unreal.Texture2D), code + ": missing existing Texture2D")
            source = select_source(texture, code)
            size = [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]
            require(all(32 <= v <= 8192 for v in size), code + ": invalid current texture resolution")
            source.update(original_png_size=png_size(source_path(source["relative"])), texture=texture_path,
                          texture_size=size, texture_package_sha256=sha(package_path(texture_path)))
            entry = dict(code=code, map="/Game/Maps/" + name, map_sha256=before[code],
                         floor_z=floor, slice_z=floor+SLICE_HEIGHT_CM, row=selected[code], source=source,
                         floors=[], sections=[], excluded_by_height=0,
                         exact_approved_wall_components=0, exact_approved_wall_sections=0)
            for actor in actors.get_all_level_actors():
                # Architectural-looking meshes inside gameplay BPs are excluded.
                if not (isinstance(actor, unreal.StaticMeshActor) or actor.get_class().get_name() == "BP_FloorTilePCG_C"):
                    continue
                tags = [str(t) for t in actor.tags]
                exact_wall = TAG in tags and any(re.fullmatch(r"ExhibitionRole_Wall\d+", t) for t in tags)
                for component in actor.get_components_by_class(unreal.StaticMeshComponent):
                    mesh = component.get_editor_property("static_mesh")
                    if mesh is None:
                        continue
                    mesh_path = mesh.get_path_name()
                    kind = architecture_kind(mesh_path)
                    require(not exact_wall or kind == "wall", "Unrecognized original approved wall: " + mesh_path)
                    if kind is None:
                        continue
                    if exact_wall:
                        entry["exact_approved_wall_components"] += 1
                    if not component.get_editor_property("visible") or actor.get_editor_property("hidden"):
                        require(not exact_wall, "Approved wall is hidden: " + actor.get_name())
                        continue
                    box = mesh.get_bounding_box()
                    local_corners = list(itertools.product(*zip(xyz(box.min), xyz(box.max))))
                    transforms = [(i, component.get_instance_transform(i, True))
                                  for i in range(component.get_instance_count())] if isinstance(component, unreal.InstancedStaticMeshComponent) else [(None, component.get_world_transform())]
                    for instance, transform in transforms:
                        mat = matrix(transform)
                        corners = [transform_point(p, mat) for p in local_corners]
                        lo, hi = min(p[2] for p in corners), max(p[2] for p in corners)
                        metadata = dict(actor=actor.get_name(), component=component.get_name(), instance=instance,
                                        mesh=mesh_path, kind=kind, approved_wall=exact_wall, world_z_bounds=[lo, hi])
                        if kind == "floor":
                            if not floor-20 <= hi <= floor+30 or hi-lo > 100:
                                entry["excluded_by_height"] += 1
                                continue
                            entry["floors"].append(dict(metadata, polygon=hull_xy(corners)))
                        else:
                            if not lo-EPS <= entry["slice_z"] <= hi+EPS:
                                require(not exact_wall, "Approved wall misses player-height slice: " + actor.get_name())
                                entry["excluded_by_height"] += 1
                                continue
                            if mesh_path not in sections:
                                parts = []
                                for i in range(mesh.get_num_sections(0)):
                                    vertices, triangles = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, i)[:2]
                                    require(vertices and triangles and len(triangles) % 3 == 0,
                                            "Cannot read original LOD0 without source modification: " + mesh_path)
                                    parts.append(([xyz(v) for v in vertices], [int(v) for v in triangles]))
                                require(parts, "Original mesh has no LOD0 sections: " + mesh_path)
                                sections[mesh_path] = parts
                            expected = xyz(transform.transform_location(unreal.Vector(*local_corners[-1])))
                            require(math.dist(expected, corners[-1]) < .01, "Native transform convention mismatch")
                            segments = []
                            for vertices, triangles in sections[mesh_path]:
                                segments.extend(slice_segments([transform_point(v, mat) for v in vertices], triangles, entry["slice_z"]))
                            segments = [[list(a), list(b)] for a, b in sorted(set(tuple(sorted((tuple(a), tuple(b)))) for a, b in segments))]
                            require(segments, "No real slice geometry; refusing AABB substitute: " + mesh_path)
                            entry["sections"].append(dict(metadata, segments=segments))
                            if exact_wall:
                                entry["exact_approved_wall_sections"] += 1
                        p = package_path(mesh_path)
                        relative = str(p.relative_to(ROOT)).replace("\\", "/")
                        if relative not in result["source_meshes"]:
                            result["source_meshes"][relative] = sha(p)
            require(entry["floors"] and entry["sections"], code + ": missing measured architectural layer")
            require(entry["exact_approved_wall_components"] > 0 and
                    entry["exact_approved_wall_components"] == entry["exact_approved_wall_sections"],
                    code + ": new approved wall coverage incomplete")
            result["maps"][code] = entry
            unreal.log("APPROVED_FLOORPLAN_COLLECT " + code + " floors=" + str(len(entry["floors"])) +
                       " sections=" + str(len(entry["sections"])))
        require(rows == export_rows(unreal), "DT rows changed during collection")
        require(before == {c: sha(ROOT / "Content/Maps" / (n + ".umap")) for c, n in MAPS.items()}, "Collect changed map bytes")
        require(result["dt_package_sha256"] == sha(package_path(DATA_TABLE)), "Collect changed DT package")
        result.update(status="PASS", geometry_sha256=fingerprint(result["maps"]))
    except Exception:
        result.update(status="ERROR", error=traceback.format_exc())
        raise
    finally:
        write_json(GEOMETRY, result)
    return result


def verify_collected(data):
    require(data["status"] == "PASS" and data["schema_version"] == 1 and set(data["maps"]) == set(MAPS), "Incomplete collect")
    require(data["script_sha256"] == sha(__file__), "Script changed since collect")
    require(data["geometry_sha256"] == fingerprint(data["maps"]), "Collected geometry changed")
    row_schema(data["map_rows"])
    require(data["dt_rows_sha256"] == fingerprint(data["map_rows"]), "Collected DT rows changed")
    require(data["dt_package_sha256"] == sha(package_path(DATA_TABLE)), "DT package changed since collect")
    for code, name in MAPS.items():
        require(data["maps"][code]["map_sha256"] == sha(ROOT / "Content/Maps" / (name + ".umap")), code + ": map changed; collect again")
        source = data["maps"][code]["source"]
        require(source["texture_package_sha256"] == sha(package_path(source["texture"])), code + ": texture changed since collect")
    for relative, expected in data["source_meshes"].items():
        path = (ROOT / relative).resolve()
        require(path.is_relative_to((ROOT / "Content").resolve()) and expected == sha(path), "Source mesh changed: " + relative)


def draw_section(draw, segments, row, width, height, color):
    """Even-odd fill leaves true doorway gaps; open surfaces retain outlines."""
    lines = [[tuple(v*s for v, s in zip(project_xy(p, row), (width, height))) for p in segment] for segment in segments]
    lo = max(0, math.floor(min(p[1] for line in lines for p in line)))
    hi = min(height-1, math.ceil(max(p[1] for line in lines for p in line)))
    odd_rows = 0
    for y in range(lo, hi+1):
        scan, cuts = y+.5, []
        for a, b in lines:
            if min(a[1], b[1]) <= scan < max(a[1], b[1]):
                cuts.append(a[0] + (scan-a[1])*(b[0]-a[0])/(b[1]-a[1]))
        cuts.sort()
        unique = []
        for v in cuts:
            if not unique or abs(v-unique[-1]) > .0001:
                unique.append(v)
        if len(unique) % 2:
            odd_rows += 1
            continue
        for start, end in zip(unique[::2], unique[1::2]):
            if end > 0 and start < width:
                draw.line((max(0, start), y, min(width-1, end), y), fill=color, width=1)
    for line in lines:
        draw.line(line, fill=color, width=1)
    return odd_rows


def render():
    from PIL import Image, ImageDraw
    data = load_json(GEOMETRY)
    verify_collected(data)
    report = dict(status="RUNNING", stage="render", geometry_file_sha256=sha(GEOMETRY),
                  maps={}, no_gameplay_markers=True, dt_rows_changed=False, map_packages_changed=False)
    images = {}
    try:
        for code in MAPS:
            entry = data["maps"][code]
            source, row = entry["source"], entry["row"]
            path = source_path(source["relative"])
            require(source["sha256_before"] == sha(path), code + ": source PNG changed since collect")
            w, h = source["texture_size"]
            image = Image.new("RGB", (w*2, h*2), "#101720")
            draw = ImageDraw.Draw(image)
            for floor in entry["floors"]:
                require(floor["kind"] == "floor" and architecture_kind(floor["mesh"]) == "floor", "Nonfloor geometry")
                polygon = [tuple(v*s for v, s in zip(project_xy(p, row), image.size)) for p in floor["polygon"]]
                draw.polygon(polygon, fill="#42505a")
            odd_rows = 0
            for section in entry["sections"]:
                require(section["kind"] in ("wall", "column", "doorframe") and
                        architecture_kind(section["mesh"]) == section["kind"], "Nonarchitectural wall geometry")
                odd_rows += draw_section(draw, section["segments"], row, *image.size, "#dacbb3")
            image = image.resize((w, h), Image.Resampling.LANCZOS)
            buffer = io.BytesIO()
            image.save(buffer, format="PNG")
            images[code] = (path, buffer.getvalue())
            report["maps"][code] = dict(source=source["relative"], texture=source["texture"], size=[w, h],
                before_sha256=source["sha256_before"], source_selection=source["selected_from"],
                floor_instances=len(entry["floors"]), architecture_sections=len(entry["sections"]),
                approved_wall_sections=entry["exact_approved_wall_sections"],
                open_surface_scanlines_outlined_only=odd_rows,
                projection_bounds=[row["WorldMin"], row["WorldMax"]], north=row["MapNorthAxis"])
        # All three sources are validated before any PNG is replaced.
        verify_collected(data)
        for code, (path, content) in images.items():
            temp = path.with_name(path.name + ".approved.tmp")
            try:
                temp.write_bytes(content)
                os.replace(temp, path)
            finally:
                if temp.is_file():
                    temp.unlink()
            report["maps"][code]["sha256"] = sha(path)
            require(png_size(path) == report["maps"][code]["size"], "Rendered PNG resolution changed")
        report["status"] = "PASS"
    except Exception:
        report.update(status="ERROR", error=traceback.format_exc())
        raise
    finally:
        write_json(RENDER, report)
    print("APPROVED_FLOORPLAN_RENDER_PASS maps=3 geometry_only=1")
    return report


def import_textures(unreal):
    data, rendered = load_json(GEOMETRY), load_json(RENDER)
    verify_collected(data)
    require(rendered["status"] == "PASS" and rendered["geometry_file_sha256"] == sha(GEOMETRY), "Stale/incomplete render")
    before = export_rows(unreal)
    require(before == data["map_rows"], "Live DT rows changed since collect")
    require(set(rendered["maps"]) == set(MAPS), "Missing rendered PNG")
    report = dict(status="RUNNING", stage="import", geometry_file_sha256=sha(GEOMETRY), render_file_sha256=sha(RENDER),
                  maps=[], dt_rows_sha256_before=fingerprint(before), saved_assets=[], map_packages_changed=False)
    try:
        tasks = []
        for code in MAPS:
            source, item = data["maps"][code]["source"], rendered["maps"][code]
            path = source_path(source["relative"])
            require(item["source"] == source["relative"] and item["texture"] == source["texture"], "Render target mismatch")
            require(item["sha256"] == sha(path) and png_size(path) == source["texture_size"], "Rendered PNG changed")
            require(isinstance(unreal.load_asset(source["texture"]), unreal.Texture2D), "Refusing texture creation")
            task = unreal.AssetImportTask()
            for key, value in dict(filename=str(path), destination_path="/Game/Assets/UI/Map",
                                   destination_name="T_FloorPlan_"+code, replace_existing=True,
                                   replace_existing_settings=True, automated=True, save=False).items():
                task.set_editor_property(key, value)
            tasks.append((code, task))
        # Revalidate map/source fingerprints immediately before the first mutation.
        verify_collected(data)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task for _, task in tasks])
        for code, task in tasks:
            imported = list(task.get_editor_property("imported_object_paths"))
            expected = data["maps"][code]["source"]["texture"]
            require(len(imported) == 1 and imported[0].split(".")[0] == expected, "Unexpected imported asset")
            texture = unreal.load_asset(imported[0])
            require(isinstance(texture, unreal.Texture2D), "Not Texture2D")
            require([texture.blueprint_get_size_x(), texture.blueprint_get_size_y()] ==
                    data["maps"][code]["source"]["texture_size"], "Import changed current resolution")
            texture.modify()
            settings = dict(compression_settings=unreal.TextureCompressionSettings.TC_DEFAULT,
                            mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                            lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,
                            address_x=unreal.TextureAddress.TA_CLAMP, address_y=unreal.TextureAddress.TA_CLAMP,
                            never_stream=True, srgb=True)
            for key, value in settings.items():
                texture.set_editor_property(key, value)
            require(unreal.EditorAssetLibrary.save_loaded_asset(texture, False), "Could not save existing texture")
            require(all(texture.get_editor_property(k) == v for k, v in settings.items()), "Texture settings mismatch")
            report["saved_assets"].append(expected)
            report["maps"].append(dict(code=code, source=data["maps"][code]["source"]["relative"], texture=expected,
                png_sha256=rendered["maps"][code]["sha256"], texture_sha256=sha(package_path(expected)),
                settings={k: str(v) for k, v in settings.items()}))
        after = export_rows(unreal)
        require(before == after and data["dt_package_sha256"] == sha(package_path(DATA_TABLE)), "Import changed DT")
        for code, name in MAPS.items():
            require(data["maps"][code]["map_sha256"] == sha(ROOT / "Content/Maps" / (name+".umap")), "Import changed map")
        for relative, expected in data["source_meshes"].items():
            require(expected == sha(ROOT / relative), "Import changed source mesh")
        report.update(status="PASS", dt_rows_sha256_after=fingerprint(after), dt_fields_changed=False)
        unreal.log("APPROVED_FLOORPLAN_IMPORT_PASS textures=3 dt_changed=0 map_changed=0")
    except Exception:
        report.update(status="ERROR", error=traceback.format_exc())
        raise
    finally:
        write_json(OUT / "import.json", report)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", choices=("collect", "render", "import"))
    args, _ = parser.parse_known_args()
    stage = args.stage or os.environ.get("MH_APPROVED_FLOORPLAN_STAGE")
    unreal = None
    if stage != "render":
        import unreal
        command = unreal.SystemLibrary.get_command_line()
        match = re.search(r"-MuseumFloorPlanStage=(collect|import)(?:\s|$)", command, re.IGNORECASE)
        stage = stage or (match.group(1).lower() if match else None)
    require(stage in ("collect", "render", "import"), "An explicit stage is required")
    OUT.mkdir(parents=True, exist_ok=True)
    if stage == "render":
        render()
        return
    command = unreal.SystemLibrary.get_command_line().lower()
    require("-unattended" in command, "Use a dedicated unattended Editor")
    try:
        (collect if stage == "collect" else import_textures)(unreal)
    except Exception:
        write_json(OUT / (stage+"_error.json"), dict(stage=stage, status="ERROR", error=traceback.format_exc()))
        raise
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        if os.environ.get("MH_APPROVED_FLOORPLAN_QUIT", "1") == "1" and "-run=pythonscript" not in command:
            # Discard load-time construction dirtiness; never save loaded maps.
            unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
            unreal.SystemLibrary.quit_editor()


if __name__ == "__main__":
    main()
