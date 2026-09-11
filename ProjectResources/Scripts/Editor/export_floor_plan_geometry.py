"""Export or verify authored floor-plan geometry without saving Unreal packages.

-MuseumFloorPlanMode=verify compares current maps with the existing source JSON.
-MuseumFloorPlanReport=<path> writes the verification result for the PNG generator.
-MuseumFloorPlanQuit closes a dedicated verification editor when finished.
"""

import hashlib
import json
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_ROWS = PROJECT / "ProjectResources/DataTableImports/DT_MapPresentation.json"
OUTPUT = PROJECT / "ProjectResources/SourceArt/W7/FloorPlanGeometry.json"
DATA_TABLE_PATH = "/Game/Data/DataTable/DT_MapPresentation"
MAPS = {
    "M01": "M01_ClassicalPrototype",
    "M02": "M02_MoonlitPrototype",
    "M03": "M03_GlasshousePrototype",
}
EPSILON = 0.1
SLICE_WORLD_Z = 100.0
DOOR_MESH_SECTIONS = {}


def xy(value):
    return [round(float(value.x), 3), round(float(value.y), 3)]


def row_xy(value):
    return [float(value["X"]), float(value["Y"])]


def require_close(actual, expected, description):
    if len(actual) != len(expected) or any(abs(a - b) > EPSILON for a, b in zip(actual, expected)):
        raise RuntimeError("{}: {} != {}".format(description, actual, expected))


def mesh_component(actor):
    components = [component for component in actor.get_components_by_class(unreal.StaticMeshComponent)
                  if component.get_editor_property("static_mesh") is not None]
    if len(components) != 1:
        raise RuntimeError("Expected one mesh component: " + actor.get_actor_label())
    return components[0]


def local_rectangle(component, x_min, x_max, y_min, y_max, z):
    transform = component.get_world_transform()
    return [xy(transform.transform_location(unreal.Vector(x, y, z)))
            for x, y in ((x_min, y_min), (x_max, y_min), (x_max, y_max), (x_min, y_max))]


def footprint(actor):
    component = mesh_component(actor)
    bounds = component.get_editor_property("static_mesh").get_bounding_box()
    rotation = component.get_world_transform().rotation.rotator()
    if abs(rotation.pitch) > EPSILON or abs(rotation.roll) > EPSILON:
        raise RuntimeError("Floor plan requires an upright component: " + actor.get_actor_label())
    return {"label": actor.get_actor_label(), "polygon": local_rectangle(
        component, bounds.min.x, bounds.max.x, bounds.min.y, bounds.max.y, bounds.min.z)}


def merge_intervals(intervals):
    merged = []
    for start, end in sorted(intervals):
        if end - start <= EPSILON:
            continue
        if merged and start <= merged[-1][1] + EPSILON:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([start, end])
    return merged


def door_wall_intervals(mesh, z):
    # The StarterContent doorway uses complex collision; its aggregate simple
    # collision is empty. Read LOD0 triangles instead of guessing a doorway ratio.
    mesh_path = mesh.get_path_name()
    if mesh_path not in DOOR_MESH_SECTIONS:
        DOOR_MESH_SECTIONS[mesh_path] = [unreal.ProceduralMeshLibrary.get_section_from_static_mesh(
            mesh, 0, section_index)[:2] for section_index in range(mesh.get_num_sections(0))]
    intervals = []
    for vertices, triangles in DOOR_MESH_SECTIONS[mesh_path]:
        for triangle_index in range(0, len(triangles), 3):
            triangle = [vertices[triangles[triangle_index + offset]] for offset in range(3)]
            intersections = []
            for start, end in zip(triangle, triangle[1:] + triangle[:1]):
                if abs(start.z - z) <= EPSILON:
                    intersections.append(start.x)
                if (start.z < z < end.z) or (end.z < z < start.z):
                    alpha = (z - start.z) / (end.z - start.z)
                    intersections.append(start.x + alpha * (end.x - start.x))
            if len(intersections) >= 2:
                intervals.append([min(intersections), max(intersections)])
    merged = merge_intervals(intervals)
    bounds = mesh.get_bounding_box()
    if len(merged) != 2:
        raise RuntimeError("Expected two solid sides at doorway slice: {} {}".format(mesh.get_name(), merged))
    require_close([merged[0][0], merged[-1][1]], [bounds.min.x, bounds.max.x], "Door wall outer limits")
    return merged


def split_door_wall(actor):
    component = mesh_component(actor)
    mesh = component.get_editor_property("static_mesh")
    bounds = mesh.get_bounding_box()
    transform = component.get_world_transform()
    world_origin = transform.transform_location(unreal.Vector(0, 0, 0))
    z = transform.inverse_transform_location(unreal.Vector(world_origin.x, world_origin.y, SLICE_WORLD_Z)).z
    intervals = door_wall_intervals(mesh, z)
    # A second height rejects a sloped/arched opening that a rectangle would hide.
    upper_z = transform.inverse_transform_location(unreal.Vector(world_origin.x, world_origin.y, 180.0)).z
    upper_intervals = door_wall_intervals(mesh, upper_z)
    require_close(sum(intervals, []), sum(upper_intervals, []), "Door opening at 100/180cm")
    walls = [{"label": actor.get_actor_label() + ":Side" + str(index + 1),
              "polygon": local_rectangle(component, start, end, bounds.min.y, bounds.max.y, z)}
             for index, (start, end) in enumerate(intervals)]
    polygon = local_rectangle(component, intervals[0][1], intervals[1][0], bounds.min.y, bounds.max.y, z)
    width = ((polygon[1][0] - polygon[0][0]) ** 2 + (polygon[1][1] - polygon[0][1]) ** 2) ** 0.5
    return walls, {"label": actor.get_actor_label(), "polygon": polygon}, round(width, 3)


def m01_opening(frame, actors_by_label):
    # M01's decorative frame has NoCollision. The two authored solid wall
    # modules define the traversable gap, independently of the frame silhouette.
    base = frame.get_actor_label().removesuffix("_DoorFrame")
    left = mesh_component(actors_by_label[base + "_A"])
    right = mesh_component(actors_by_label[base + "_B"])
    left_bounds = left.get_editor_property("static_mesh").get_bounding_box()
    right_bounds = right.get_editor_property("static_mesh").get_bounding_box()
    left_transform = left.get_world_transform()
    right_transform = right.get_world_transform()
    right_points = [left_transform.inverse_transform_location(right_transform.transform_location(
        unreal.Vector(x, y, right_bounds.min.z)))
        for x, y in ((right_bounds.min.x, right_bounds.min.y), (right_bounds.max.x, right_bounds.min.y),
                     (right_bounds.max.x, right_bounds.max.y), (right_bounds.min.x, right_bounds.max.y))]
    start, end = left_bounds.max.x, min(point.x for point in right_points)
    polygon = local_rectangle(left, start, end, left_bounds.min.y, left_bounds.max.y, left_bounds.min.z)
    width = ((polygon[1][0] - polygon[0][0]) ** 2 + (polygon[1][1] - polygon[0][1]) ** 2) ** 0.5
    require_close([width], [400.0], "M01 solid-wall opening " + base)
    frame_polygon = footprint(frame)["polygon"]
    require_close([sum(point[axis] for point in polygon) / 4 for axis in (0, 1)],
                  [sum(point[axis] for point in frame_polygon) / 4 for axis in (0, 1)], "M01 frame center " + base)
    return {"label": frame.get_actor_label(), "polygon": polygon}, round(width, 3)


def map_hashes():
    return {code: hashlib.sha256((PROJECT / "Content/Maps" / (name + ".umap")).read_bytes()).hexdigest()
            for code, name in MAPS.items()}


def export_geometry():
    source_rows = {row["Name"]: row for row in json.loads(SOURCE_ROWS.read_text(encoding="utf-8-sig"))}
    data_table = unreal.load_asset(DATA_TABLE_PATH)
    if data_table is None:
        raise RuntimeError("Missing " + DATA_TABLE_PATH)
    live_rows = {row["Name"]: row for row in json.loads(
        unreal.DataTableFunctionLibrary.export_data_table_to_json_string(data_table))}
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    maps = []
    for code, name in MAPS.items():
        row, source = live_rows[code], source_rows[code]
        for key in ("WorldMin", "WorldMax"):
            require_close(row_xy(row[key]), row_xy(source[key]), code + " live/source " + key)
        if row["MapNorthAxis"] != source["MapNorthAxis"] or row["MapNorthAxis"] != "PositiveY":
            raise RuntimeError(code + " requires matching PositiveY map projection")
        if not unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/" + name):
            raise RuntimeError("Map load failed: " + code)
        actors = sorted(actor_subsystem.get_all_level_actors(), key=lambda actor: actor.get_actor_label())
        by_label = {actor.get_actor_label(): actor for actor in actors}
        prefix = "LDV2_" + code + "_"
        floors, walls, doors, door_widths, fixed_partitions = [], [], [], [], []
        for actor in actors:
            label = actor.get_actor_label()
            if not label.startswith(prefix) or actor.get_class().get_name() != "StaticMeshActor":
                continue
            folder = str(actor.get_folder_path())
            if label.startswith(prefix + "Floor_") and folder == "LDV2/" + code + "/Architecture/Floor":
                floors.append(footprint(actor))
            elif folder == "LDV2/" + code + "/Architecture/Walls" and not label.endswith("_Upper"):
                mesh = mesh_component(actor).get_editor_property("static_mesh")
                if mesh.get_name() == "Wall_Door_400x400":
                    sides, door, width = split_door_wall(actor)
                    walls.extend(sides)
                    doors.append(door)
                    door_widths.append(width)
                else:
                    walls.append(footprint(actor))
            elif code == "M01" and folder == "LDV2/M01/Architecture/DoorFrames" and label.endswith("_DoorFrame"):
                door, width = m01_opening(actor, by_label)
                doors.append(door)
                door_widths.append(width)
            elif code == "M03" and label.startswith(prefix + "SpineGlassBaffle_") and folder == "LDV2/M03/Theme/PublicSpine":
                walls.append(footprint(actor))
                fixed_partitions.append(label)
            elif folder == "LDV2/" + code + "/Architecture/GalleryPartitions":
                walls.append(footprint(actor))
                fixed_partitions.append(label)
        if not floors or not walls or not doors:
            raise RuntimeError(code + " empty geometry category")
        floor_points = [point for floor in floors for point in floor["polygon"]]
        actual_min = [min(point[axis] for point in floor_points) for axis in (0, 1)]
        actual_max = [max(point[axis] for point in floor_points) for axis in (0, 1)]
        require_close(actual_min, row_xy(row["WorldMin"]), code + " actual/live floor minimum")
        require_close(actual_max, row_xy(row["WorldMax"]), code + " actual/live floor maximum")
        actual_floor_bounds_match = all(abs(actual - configured) <= EPSILON
                                       for actual, configured in zip(actual_min + actual_max,
                                           row_xy(row["WorldMin"]) + row_xy(row["WorldMax"])))
        vents = [actor for actor in actors if actor.get_class().get_name() == "BP_Vent_C"]
        if len(vents) != 1 or len(row["DefaultExitAnchors"]) != 1 or len(source["DefaultExitAnchors"]) != 1:
            raise RuntimeError(code + " expected exactly one actual/configured exit")
        exit_xy = xy(vents[0].get_actor_location())
        require_close(exit_xy, row_xy(row["DefaultExitAnchors"][0]["WorldLocation"]), code + " actual/live exit")
        require_close(exit_xy, row_xy(source["DefaultExitAnchors"][0]["WorldLocation"]), code + " actual/source exit")
        maps.append({"mapId": code, "worldMin": row_xy(row["WorldMin"]), "worldMax": row_xy(row["WorldMax"]),
                     "floors": floors, "walls": walls, "doors": doors})
        unreal.log_warning("MH_FLOOR_PLAN_GEOMETRY=" + json.dumps({
            "map": code, "floors": len(floors), "walls": len(walls), "doors": len(doors),
            "door_widths_cm": sorted(set(door_widths)), "fixed_partitions": fixed_partitions,
            "worldMin": row_xy(row["WorldMin"]), "worldMax": row_xy(row["WorldMax"]), "exit": exit_xy,
            "actual_floor_min": actual_min, "actual_floor_max": actual_max,
            "live_source_bounds_match": True, "actual_floor_bounds_match": actual_floor_bounds_match,
            "actual_exit_match": True}, sort_keys=True))
    return {"maps": maps}


def geometry_fingerprint(geometry):
    # The geometry, not package bytes, determines staleness. Rebuilding only
    # navigation changes .umap hashes without changing this fingerprint.
    return hashlib.sha256(json.dumps(
        geometry, sort_keys=True, separators=(",", ":"), allow_nan=False,
    ).encode("utf-8")).hexdigest()


def run():
    _, switches, parameters = unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())
    parameters = {str(key).casefold(): str(value) for key, value in parameters.items()}
    mode = parameters.get("museumfloorplanmode", "export").casefold()
    report_path = parameters.get("museumfloorplanreport")
    quit_when_done = "museumfloorplanquit" in {str(value).casefold() for value in switches}
    report = {"status": "FAIL", "mode": mode, "source": str(OUTPUT), "packages_saved": False}
    try:
        if mode not in ("export", "verify"):
            raise RuntimeError("MuseumFloorPlanMode must be export or verify")
        before = map_hashes()
        try:
            geometry = export_geometry()
        finally:
            if before != map_hashes():
                raise RuntimeError("Map files changed during read-only geometry inspection")
        report.update(maps=len(geometry["maps"]), map_hashes_unchanged=True,
                      current_geometry_sha256=geometry_fingerprint(geometry))
        if mode == "verify":
            saved_bytes = OUTPUT.read_bytes()
            saved = json.loads(saved_bytes.decode("utf-8-sig"))
            report.update(source_file_sha256=hashlib.sha256(saved_bytes).hexdigest(),
                          saved_geometry_sha256=geometry_fingerprint(saved))
            if report["current_geometry_sha256"] != report["saved_geometry_sha256"]:
                raise RuntimeError("Current map geometry differs from FloorPlanGeometry.json; run geometry export first")
        else:
            OUTPUT.write_text(json.dumps(geometry, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        report["status"] = "PASS"
        unreal.log_warning("MH_FLOOR_PLAN_GEOMETRY_" + mode.upper() + "_COMPLETE=" + json.dumps(report, sort_keys=True))
    except Exception as error:
        report["error"] = str(error)
        unreal.log_error("MH_FLOOR_PLAN_GEOMETRY_FAILED=" + json.dumps(report, sort_keys=True))
        raise
    finally:
        try:
            if report_path:
                Path(report_path).write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        finally:
            if quit_when_done:
                unreal.SystemLibrary.quit_editor()


run()
