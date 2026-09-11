"""Authored gallery partitions and mounted exhibits for the three release maps.

Invoked by build_museum_levels_v2.py before its normal save/orphan cleanup.
All positions are centimetres; mount yaw is the visible face normal, not BP yaw.
"""
import json
import math
from pathlib import Path

import unreal


MOUNTS = {
    "M01": [
        ("Target", 7200, 2200, 180), ("HighValue", 7200, -2400, 180),
        ("01", -7200, -2500, 0), ("02", -5300, -1000, 180),
        ("03", -7200, 2100, 0), ("04", -4600, 4050, 180),
        ("05", -3600, 5200, -90), ("06", -2200, 3400, 180),
        ("07", 200, 4050, 0), ("08", 2600, 3400, 0),
        ("09", 7200, 4100, 180), ("10", 7200, 400, 180),
        ("11", 6000, -5200, 90), ("12", 2600, -3400, 0),
        ("13", 200, -4050, 0), ("14", -2200, -3400, 180),
        ("15", -2400, 800, 0), ("16", -2400, -800, 0),
        ("17", 2400, 800, 180), ("18", 2400, -800, 180),
    ],
    "M02": [
        ("Target", 6400, 3000, 180), ("HighValue", -6400, 3600, 0),
        ("01", -6400, -4000, 0), ("02", -5000, -1800, -90),
        ("03", -6400, 400, 0), ("04", -5000, 3000, -90),
        ("05", -3000, 5600, -90), ("06", -1000, 5600, -90),
        ("07", 2200, 5600, -90), ("08", 6400, 1800, 180),
        ("09", 4000, -600, 180), ("10", 6400, -3000, 180),
        ("11", 3000, -5600, 90), ("12", 600, -5600, 90),
        ("13", -1800, -5600, 90), ("14", -2400, -1600, 180),
        ("15", -4000, 1400, 0), ("16", 800, 400, 180),
        ("17", -800, -3000, 0), ("18", 3000, 2000, -90),
    ],
    "M03": [
        ("Target", 8000, 2400, 180), ("HighValue", 8000, -1600, 180),
        ("01", -8000, -2400, 0), ("02", -8000, 600, 0),
        ("03", -8000, 2400, 0), ("04", -5600, -4400, 90),
        ("05", -3200, -4400, 90), ("06", -800, -4400, 90),
        ("07", 1600, -4400, 90), ("08", 4000, -4400, 90),
        ("09", 5600, -3400, 180), ("10", 5600, 4400, -90),
        ("11", 3200, 4400, -90), ("12", 800, 4400, -90),
        ("13", -1600, 4400, -90), ("14", -4000, 4400, -90),
        ("15", -5600, 0, 180), ("16", -3000, 0, 0),
        ("17", 2200, 0, 180), ("18", 4800, 0, 0),
    ],
}


def box(builder, suffix, center, size, material, yaw=0.0,
        folder="Architecture/GalleryPartitions", collision="BlockAll"):
    # Shape_Cube has an imported pivot: derive the transform from real bounds.
    mesh = unreal.load_asset("/Game/Assets/StarterContent/Shapes/Shape_Cube")
    bounds = mesh.get_bounding_box()
    lo, hi = bounds.min, bounds.max
    scale = (size[0] / (hi.x - lo.x), size[1] / (hi.y - lo.y), size[2] / (hi.z - lo.z))
    local_center = ((lo.x + hi.x) * scale[0] * 0.5,
                    (lo.y + hi.y) * scale[1] * 0.5,
                    (lo.z + hi.z) * scale[2] * 0.5)
    r = math.radians(yaw)
    location = (center[0] - math.cos(r) * local_center[0] + math.sin(r) * local_center[1],
                center[1] - math.sin(r) * local_center[0] - math.cos(r) * local_center[1],
                center[2] - local_center[2])
    actor = builder.static("LDV2_{}_Gallery_{}".format(builder.code, suffix), "cube", location,
                           yaw, scale, material, folder, collision)
    if folder == "Architecture/GalleryPartitions":
        builder.add_tags(actor, "MuseumOpaquePartition")
    return actor


def partition(builder, name, start, end, openings=(), height=400.0, width=24.0):
    horizontal = start[1] == end[1]
    axis = 0 if horizontal else 1
    low, high = sorted((start[axis], end[axis]))
    cursor = low
    segments = []
    for center, gap in sorted(openings):
        if center - gap * 0.5 < cursor or center + gap * 0.5 > high:
            raise RuntimeError("Opening outside partition: " + name)
        segments.append((cursor, center - gap * 0.5))
        cursor = center + gap * 0.5
        lintel_xy = (center, start[1]) if horizontal else (start[0], center)
        box(builder, name + "_Lintel_" + str(center), (*lintel_xy, (260 + height) * 0.5),
            (gap, width, height - 260) if horizontal else (width, gap, height - 260),
            "m01_wall", folder="Architecture/GalleryTrim")
    segments.append((cursor, high))
    for index, (a, b) in enumerate(segments):
        if b - a < 1.0:
            continue
        xy = ((a + b) * 0.5, start[1]) if horizontal else (start[0], (a + b) * 0.5)
        size = (b - a, width, height) if horizontal else (width, b - a, height)
        box(builder, name + "_" + str(index), (*xy, height * 0.5), size, "m01_wall")
        box(builder, name + "_Skirt_" + str(index), (*xy, 9), (size[0], size[1] + 4, 18)
            if horizontal else (size[0] + 4, size[1], 18),
            "oak" if builder.code == "M02" else "burnished", folder="Architecture/GalleryTrim")


def add_partitions(builder):
    code = builder.code
    if code == "M01":
        for index, x in enumerate((-4600, -2200, 200, 2600, 5000)):
            partition(builder, "NorthRoom_" + str(index), (x, 1600), (x, 5200),
                      ((2300 if index % 2 == 0 else 3000, 400), (4500, 400)))
        partition(builder, "NorthRoomsRear", (-7200, 3800), (7200, 3800),
                  tuple((x, 400) for x in (-6100, -3400, -1000, 1400, 3800, 6100)))
        for index, x in enumerate((-2200, 200, 2600, 5000)):
            partition(builder, "SouthRoom_" + str(index), (x, -5200), (x, -1600),
                      ((-4500, 400), (-2300 if index % 2 == 0 else -3000, 400)))
        partition(builder, "SouthRoomsRear", (-4200, -3800), (7200, -3800),
                  tuple((x, 400) for x in (-3300, -1000, 1400, 3800, 6200)))
        partition(builder, "EntryTurnA", (-5300, -1600), (-5300, -400))
        partition(builder, "EntryTurnB", (-4400, -200), (-4400, 1200))
        ceilings = [((0, 3400), (14400, 3600)), ((1500, -3400), (11400, 3600)),
                    ((-5000, 0), (4400, 3200)), ((5000, 0), (4400, 3200))]
    elif code == "M02":
        for index, (y, door) in enumerate(((-1800, -5700), (800, -4600), (3000, -5600))):
            partition(builder, "WestRoom_" + str(index), (-6400, y), (-4000, y), ((door, 350),))
        for index, (y, door) in enumerate(((-1400, -3600), (2000, -2850))):
            partition(builder, "InnerWest_" + str(index), (-4000, y), (-2400, y), ((door, 350),))
        for index, (y, door) in enumerate(((-3000, 2800), (-100, 3550), (2000, 3550))):
            partition(builder, "InnerEast_" + str(index), (2400, y), (4000, y), ((door, 350),))
        for index, (y, door) in enumerate(((-1800, 5900), (1200, 4500))):
            partition(builder, "EastRoom_" + str(index), (4000, y), (6400, y), ((door, 350),))
        for index, x in enumerate((-2200, 800, 3400)):
            partition(builder, "NorthRoom_" + str(index), (x, 4000), (x, 5600),
                      ((5200 if x == 800 else 5000, 350),))
        partition(builder, "NorthSecureGallery", (800, 4000), (3400, 4000), ((2200, 350),))
        for index, x in enumerate((-3600, -800, 2000)):
            partition(builder, "SouthRoom_" + str(index), (x, -5600), (x, -4000), ((-5000, 350),))
        partition(builder, "GardenEast", (800, -800), (800, 2000), ((1400, 400),), height=320)
        partition(builder, "GardenSouth", (-1800, -800), (800, -800), ((-600, 350),), height=320)
        ceilings = [((-4300, 0), (4200, 11200)), ((4100, 0), (4600, 11200)),
                    ((-200, 4000), (4000, 3200)), ((-200, -3200), (4000, 4800))]
    else:
        for index, x in enumerate((-5600, -3000, -400, 2200, 4800)):
            a, b = (-1200, 800) if index % 2 == 0 else (-800, 1200)
            partition(builder, "SpineTurn_" + str(index), (x, a), (x, b))
            y = b if index % 2 == 0 else a
            partition(builder, "SpineReturn_" + str(index), (x, y), (x + 400, y))
        ceilings = [((0, 2800), (16000, 3200)), ((0, -2800), (16000, 3200))]
    for index, (xy, size) in enumerate(ceilings):
        box(builder, "Ceiling_" + str(index), (*xy, 420 if code != "M03" else 460),
            (*size, 16), "m01_wall", folder="Architecture/GalleryCeiling", collision="NoCollision")


def mounting_walls(builder):
    result = []
    for actor in builder.actors:
        if actor.get_class().get_name() != "StaticMeshActor":
            continue
        folder = str(actor.get_folder_path())
        if not (folder.endswith("/Architecture/Walls") or folder.endswith("/Architecture/GalleryPartitions")):
            continue
        origin, extent = actor.get_actor_bounds(False)
        if origin.z - extent.z <= 90 and origin.z + extent.z >= 235:
            result.append((actor, origin, extent))
    return result


def mount_surface(walls, x, y, facing, required=True):
    angle = math.radians(facing)
    n = (round(math.cos(angle), 6), round(math.sin(angle), 6))
    tangent = (-n[1], n[0])
    candidates = []
    for actor, o, e in walls:
        normal_extent = abs(n[0]) * e.x + abs(n[1]) * e.y
        tangent_extent = abs(tangent[0]) * e.x + abs(tangent[1]) * e.y
        delta = (x - o.x, y - o.y)
        if normal_extent > 100 or tangent_extent < 60:
            continue
        if abs(delta[0] * tangent[0] + delta[1] * tangent[1]) > tangent_extent + 0.1:
            continue
        depth = delta[0] * n[0] + delta[1] * n[1]
        if abs(depth) > 120:
            continue
        correction = normal_extent - depth + 5
        candidates.append((abs(correction), x + n[0] * correction, y + n[1] * correction, actor))
    if not candidates:
        if required:
            raise RuntimeError("No support wall at {} {} facing {}".format(x, y, facing))
        return None
    _, px, py, wall = min(candidates, key=lambda candidate: candidate[0])
    return px, py, n, tangent, wall


def print_materials(code):
    root = Path(unreal.Paths.project_dir()).resolve()
    rows = json.loads((root / "ProjectResources/DataTableImports/DT_ForgeryTemplateRow.json").read_text(encoding="utf-8-sig"))
    rows = [row for row in rows if row["SurfacePoolId"] == code][:8]
    if len(rows) != 8:
        raise RuntimeError("Expected eight existing references for gallery prints: " + code)
    parent = unreal.load_asset("/Game/Assets/Art/SurfaceForgery/Materials/M_HeistPaintingSurface")
    folder = "/Game/Assets/Art/SurfaceForgery/Materials/GalleryPrints"
    result = []
    for index, row in enumerate(rows):
        texture_path = row["ReferenceImage"]
        if "'" in texture_path:
            texture_path = texture_path.split("'")[1]
        texture = unreal.load_asset(texture_path)
        if texture is None:
            raise RuntimeError("Missing existing print reference: " + texture_path)
        name = "MI_GalleryPrint_{}_{:02d}".format(code, index + 1)
        material = unreal.load_asset(folder + "/" + name)
        if material is None:
            material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(material, parent)
        # UE 5.8's setter leaves its bool result false even after applying the
        # value. Verify the stored parameter instead of treating that as failure.
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(material, "PaintingTexture", texture)
        unreal.MaterialEditingLibrary.update_material_instance(material)
        if unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(material, "PaintingTexture") != texture:
            raise RuntimeError("Gallery print texture read-back failed: " + name)
        if not unreal.EditorAssetLibrary.save_loaded_asset(material):
            raise RuntimeError("Gallery print save failed: " + name)
        result.append(material)
    return result


def frame(builder, name, xy, facing, width, height, center_z, active):
    r = math.radians(facing)
    n, t = (math.cos(r), math.sin(r)), (-math.sin(r), math.cos(r))
    thickness = 8 if active else 3
    material = {"M01": "gold", "M02": "copper", "M03": "nickel"}[builder.code] if active else "oak"
    folder = "Theme/Exhibits/" + ("SecureFrames" if active else "DecorativeFrames")
    for suffix, along, z, length, tall in (
            ("Top", 0, center_z + height * 0.5, width + thickness, thickness),
            ("Bottom", 0, center_z - height * 0.5, width + thickness, thickness),
            ("Left", -width * 0.5, center_z, thickness, height),
            ("Right", width * 0.5, center_z, thickness, height)):
        box(builder, name + "_" + suffix,
            (xy[0] + n[0] * 6 + t[0] * along, xy[1] + n[1] * 6 + t[1] * along, z),
            (8 if active else 3, length, tall), material, facing, folder, "NoCollision")
    if active:
        box(builder, name + "_SecurityPanel", (xy[0] + n[0] * 8, xy[1] + n[1] * 8, 68),
            (8, 48, 20), "burnished", facing, folder, "NoCollision")
        # Two physical mounting clamps distinguish it even without colour/lighting.
        for sign in (-1, 1):
            box(builder, name + "_Clamp_" + str(sign),
                (xy[0] + n[0] * 10 + t[0] * 27 * sign, xy[1] + n[1] * 10 + t[1] * 27 * sign, 86),
                (12, 9, 18), material, facing, folder, "NoCollision")


def refine_exhibits(builder, cases, walls):
    materials = print_materials(builder.code)
    plane_mesh = unreal.load_asset("/Engine/BasicShapes/Plane")
    report = []
    for index, (key, x, y, facing) in enumerate(MOUNTS[builder.code]):
        px, py, n, tangent, wall = mount_surface(walls, x, y, facing)
        actor = cases[key]
        actor.set_actor_location(unreal.Vector(px, py, 0), False, False)
        actor.set_actor_rotation(unreal.Rotator(yaw=facing + 180), False)
        components = {component.get_name(): component for component in actor.get_components_by_class(unreal.StaticMeshComponent)}
        backing = components["VisualMeshComponent"]
        backing.set_editor_property("relative_location", unreal.Vector(0, -70, 90))
        backing.set_editor_property("relative_rotation", unreal.Rotator(yaw=90))
        backing.set_editor_property("relative_scale3d", unreal.Vector(0.35, 0.35, 0.35))
        # Original and Replica share the exact visual plane. Runtime assignment
        # still replaces the Original texture using the existing server snapshot.
        for name in ("OriginalVisualComponent", "ReplicaVisualComponent"):
            component = components[name]
            component.set_editor_property("relative_location", unreal.Vector(200, 12, 200))
            component.set_editor_property("relative_rotation", unreal.Rotator(roll=90))
            component.set_editor_property("relative_scale3d", unreal.Vector(3.8, 3.8, 1))
        components["OriginalVisualComponent"].set_material(0, materials[index % len(materials)])
        builder.add_tags(actor, "MuseumMountedPainting")
        frame(builder, "Secure_" + key, (px, py), facing, 142, 142, 160, True)
        light = builder.point_light("LDV2_{}_ExhibitLight_{}".format(builder.code, key),
                                    (px + n[0] * 100, py + n[1] * 100, 280),
                                    (255, 221, 180), 350, 1800, "Lighting/Exhibits")
        light.get_component_by_class(unreal.PointLightComponent).set_editor_property("cast_shadows", False)
        decorative = []
        for sign in (-1, 1):
            dx, dy = x + tangent[0] * sign * 150, y + tangent[1] * sign * 150
            support = mount_surface(walls, dx, dy, facing, required=False)
            if support is None:
                continue
            qx, qy = support[:2]
            suffix = "Print_{}_{}".format(key, "A" if sign < 0 else "B")
            print_actor = box(builder, suffix, (qx + n[0] * 2, qy + n[1] * 2, 160),
                              (1, 76, 76), "m01_wall", facing, "Theme/Exhibits/Decorative", "NoCollision")
            component = print_actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_editor_property("static_mesh", plane_mesh)
            component.set_material(0, materials[(index + (2 if sign < 0 else 5)) % len(materials)])
            print_actor.set_actor_location(unreal.Vector(qx + n[0] * 2, qy + n[1] * 2, 160), False, False)
            print_actor.set_actor_rotation(unreal.Rotator(yaw=facing - 90, roll=90), False)
            print_actor.set_actor_scale3d(unreal.Vector(0.76, 0.76, 1))
            builder.add_tags(print_actor, "MuseumDecorativePainting")
            frame(builder, suffix, (qx, qy), facing, 78, 78, 160, False)
            decorative.append(print_actor.get_actor_label())
        report.append({"case": actor.get_actor_label(), "wall": wall.get_actor_label(),
                       "location": [px, py, 0], "facing": facing, "decorative": decorative})
    path = Path(unreal.Paths.project_saved_dir()) / "Automation/GalleryRefinement"
    path.mkdir(parents=True, exist_ok=True)
    (path / (builder.code + "_mounts.json")).write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log_warning("MH_GALLERY_REFINEMENT={} paintings={} decorative={}".format(
        builder.code, len(report), sum(len(row["decorative"]) for row in report)))


def refine_gallery(builder, cases):
    add_partitions(builder)
    refine_exhibits(builder, cases, mounting_walls(builder))
    # Low, broad fill keeps the new enclosed circulation legible without
    # changing the map's fixed exposure or the focused exhibit lighting.
    xs = {"M01": (-5000, 0, 5000), "M02": (-4500, 0, 4500), "M03": (-5500, 0, 5500)}[builder.code]
    ys = (-2600, 2600) if builder.code == "M03" else (-3000, 3000)
    intensity = {"M01": 12.0, "M02": 1.5, "M03": 2.5}[builder.code]
    for i, x in enumerate(xs):
        for j, y in enumerate(ys):
            light = builder.point_light("LDV2_{}_GalleryFill_{}_{}".format(builder.code, i, j),
                (x, y, 350), (210, 222, 255), intensity, 5000, "Lighting/GalleryFill")
            component = light.get_component_by_class(unreal.PointLightComponent)
            component.set_editor_property("use_inverse_squared_falloff", False)
            component.set_editor_property("light_falloff_exponent", 2.0)
            component.set_editor_property("cast_shadows", False)
