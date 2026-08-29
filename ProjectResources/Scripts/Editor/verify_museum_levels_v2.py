import json
import math

import unreal


OBJECT_DISPLAY_CASE_ACTOR_TYPE = getattr(unreal, "HeistObjectDisplayCaseActor", None)


def is_deferred_object_case(actor):
    if OBJECT_DISPLAY_CASE_ACTOR_TYPE is not None and isinstance(actor, OBJECT_DISPLAY_CASE_ACTOR_TYPE):
        return True
    return actor.get_class().get_name() == "BP_ObjectDisplayCase_C"


EXPECTED = {
    "M01": {
        "path": "/Game/Maps/M01_ClassicalPrototype",
        "half_x": 7200.0,
        "half_y": 5200.0,
        "floor_count": 234,
        "vent_points": 3,
        "guards": 5,
        "waypoints": 39,
        "cameras": 6,
        "lasers": 2,
        "generated_lights": 12,
        "ceiling_panels": 68,
        "ceiling_prefixes": ("LDV2_M01_Ceiling_",),
        "laser_case_ids": ("Case_M01_Optional_HighValue", "Case_M01_Optional_09"),
        "signature_labels": ("LDV2_M01_HeroPlinth", "LDV2_M01_Topology_Figure8_North", "LDV2_M01_Topology_Figure8_South"),
        "signature_locations": {
            "LDV2_M01_Topology_Figure8_North": (0.0, 1520.0),
            "LDV2_M01_Topology_Figure8_South": (0.0, -1520.0),
        },
        "topology_door_labels": (),
        "exit_visual_label": "LDV2_M01_VentEntry_A_Panel",
        "minimum_target_exit_distance": 12000.0,
        "nav_scale": (72.0, 52.0, 10.0),
    },
    "M02": {
        "path": "/Game/Maps/M02_MoonlitPrototype",
        "half_x": 6400.0,
        "half_y": 5600.0,
        "floor_count": 224,
        "vent_points": 3,
        "guards": 5,
        "waypoints": 40,
        "cameras": 4,
        "lasers": 2,
        "generated_lights": 13,
        "ceiling_panels": 49,
        "ceiling_prefixes": ("LDV2_M02_Ceiling_",),
        "laser_case_ids": ("Case_M02_Optional_HighValue", "Case_M02_Optional_07"),
        "signature_labels": ("LDV2_M02_MoonPool", "LDV2_M02_Topology_Serpentine_Gate_A", "LDV2_M02_Topology_Serpentine_Gate_F"),
        "signature_locations": {
            "LDV2_M02_Topology_Serpentine_Gate_A": (-4000.0, -800.0),
            "LDV2_M02_Topology_Serpentine_Gate_F": (4000.0, 800.0),
        },
        "topology_door_labels": (
            "LDV2_M02_SerpentineGateA_05",
            "LDV2_M02_SerpentineGateB_04",
            "LDV2_M02_SerpentineGateC_03",
            "LDV2_M02_SerpentineGateD_01",
            "LDV2_M02_SerpentineGateE_05",
            "LDV2_M02_SerpentineGateF_04",
        ),
        "exit_visual_label": "LDV2_M02_VentEntry_B_Panel",
        "minimum_target_exit_distance": 3000.0,
        "nav_scale": (64.0, 56.0, 10.0),
    },
    "M03": {
        "path": "/Game/Maps/M03_GlasshousePrototype",
        "half_x": 8000.0,
        "half_y": 4400.0,
        "floor_count": 220,
        "vent_points": 3,
        "guards": 4,
        "waypoints": 28,
        "cameras": 8,
        "lasers": 3,
        "generated_lights": 14,
        "ceiling_panels": 11,
        "ceiling_prefixes": ("LDV2_M03_GlassRoof_",),
        "laser_case_ids": ("Case_M03_Optional_HighValue", "Case_M03_Optional_08", "Case_M03_Optional_10"),
        "signature_labels": ("LDV2_M03_Topology_BraidedCrossing", "LDV2_M03_CrossingSuspendedFrame", "LDV2_M03_SpineGlassBaffle_00"),
        "signature_locations": {
            "LDV2_M03_Topology_BraidedCrossing": (400.0, 0.0),
        },
        "topology_door_labels": (
            "LDV2_M03_SpineNorth_01",
            "LDV2_M03_SpineNorth_06",
            "LDV2_M03_SpineNorth_10",
            "LDV2_M03_SpineNorth_15",
            "LDV2_M03_SpineNorth_17",
            "LDV2_M03_SpineSouth_03",
            "LDV2_M03_SpineSouth_08",
            "LDV2_M03_SpineSouth_13",
            "LDV2_M03_SpineSouth_17",
        ),
        "exit_visual_label": "LDV2_M03_VentEntry_A_Panel",
        "minimum_target_exit_distance": 14000.0,
        "nav_scale": (80.0, 44.0, 10.0),
    },
}

LOWER_WALL_Z = -12.0
UPPER_WALL_Z = 388.0
CEILING_Z = 812.0

NIGHT_EXPECTED = {
    "M01": {
        "moon_rotation": (-20.0, -35.0, 0.0),
        "moon_color": (150, 180, 255),
        "moon_intensity": 0.50,
        "sky_color": (125, 150, 210),
        "sky_intensity": 0.26,
        "fog_density": 0.008,
        "fog_start_distance": 450.0,
        "fog_max_opacity": 0.35,
        "exposure_ev100": 1.50,
        "bloom_intensity": 0.25,
    },
    "M02": {
        "moon_rotation": (-15.0, 20.0, 0.0),
        "moon_color": (125, 155, 235),
        "moon_intensity": 0.40,
        "sky_color": (95, 120, 185),
        "sky_intensity": 0.20,
        "fog_density": 0.014,
        "fog_start_distance": 350.0,
        "fog_max_opacity": 0.45,
        "exposure_ev100": 1.00,
        "bloom_intensity": 0.30,
    },
    "M03": {
        "moon_rotation": (-24.0, -70.0, 0.0),
        "moon_color": (165, 200, 255),
        "moon_intensity": 0.55,
        "sky_color": (125, 155, 215),
        "sky_intensity": 0.24,
        "fog_density": 0.006,
        "fog_start_distance": 500.0,
        "fog_max_opacity": 0.30,
        "exposure_ev100": 1.75,
        "bloom_intensity": 0.22,
    },
}


def resolve_level_codes(available_codes):
    _, command_line_switches, command_line_parameters = unreal.SystemLibrary.parse_command_line(
        unreal.SystemLibrary.get_command_line()
    )
    parameter_name = "MuseumLevelCodes"
    normalized_parameter_name = parameter_name.casefold()
    bare_switches = [
        switch
        for switch in command_line_switches
        if str(switch).casefold() == normalized_parameter_name
    ]
    matching_values = [
        value
        for key, value in command_line_parameters.items()
        if str(key).casefold() == normalized_parameter_name
    ]

    if bare_switches:
        raise RuntimeError("-{} requires a non-empty comma-separated value".format(parameter_name))
    if not matching_values:
        return list(available_codes)
    if len(matching_values) != 1:
        raise RuntimeError("-{} was specified more than once".format(parameter_name))

    requested_values = str(matching_values[0]).split(",")
    if any(not value.strip() for value in requested_values):
        raise RuntimeError("-{} contains an empty level code".format(parameter_name))

    selected_codes = []
    selected_set = set()
    for value in requested_values:
        code = value.strip().upper()
        if code not in available_codes:
            raise RuntimeError(
                "Unknown level code '{}' in -{}. Expected one of: {}".format(
                    code,
                    parameter_name,
                    ",".join(available_codes),
                )
            )
        if code not in selected_set:
            selected_codes.append(code)
            selected_set.add(code)
    return selected_codes


selected_level_codes = resolve_level_codes(EXPECTED)

MIN_CASE_GUARD_CLEARANCE_CM = 450.0
MIN_GUARD_OBSTACLE_CLEARANCE_CM = 100.0


def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None


def close_float(left, right, tolerance=0.1):
    return abs(float(left) - float(right)) <= tolerance


def color_tuple(value):
    return (int(value.r), int(value.g), int(value.b)) if value is not None else ()


def actor_label(value):
    return value.get_actor_label() if value else ""


def actor_tags(value):
    return {str(tag) for tag in (prop(value, "tags") or [])}


def sampled_segment_aabb_clearance(start, end, origin, extent, sample_spacing=50.0):
    length = math.hypot(end.x - start.x, end.y - start.y)
    sample_count = max(1, int(math.ceil(length / sample_spacing)))
    minimum = float("inf")
    for sample_index in range(sample_count + 1):
        alpha = float(sample_index) / sample_count
        x = start.x + (end.x - start.x) * alpha
        y = start.y + (end.y - start.y) * alpha
        delta_x = max(abs(x - origin.x) - extent.x, 0.0)
        delta_y = max(abs(y - origin.y) - extent.y, 0.0)
        minimum = min(minimum, math.hypot(delta_x, delta_y))
    return minimum


actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_failures = []

for code in selected_level_codes:
    expected = EXPECTED[code]
    world = unreal.EditorLoadingAndSavingUtils.load_map(expected["path"])
    if not world:
        all_failures.append("{}: map load failed".format(code))
        continue

    actors = list(actor_subsystem.get_all_level_actors())
    by_class = {}
    for actor in actors:
        by_class.setdefault(actor.get_class().get_name(), []).append(actor)

    failures = []
    labels = [actor.get_actor_label() for actor in actors]
    by_label = {actor.get_actor_label(): actor for actor in actors}
    deferred_object_cases = [actor for actor in actors if is_deferred_object_case(actor)]
    if len(labels) != len(set(labels)):
        failures.append("duplicate actor labels")

    cases = by_class.get("BP_PaintingDisplayCase_C", [])
    case_ids = [str(prop(actor, "display_case_id")) for actor in cases]
    expected_case_ids = {
        "Case_{}_Target".format(code),
        "Case_{}_Optional_HighValue".format(code),
    }
    expected_case_ids.update("Case_{}_Optional_{:02d}".format(code, index) for index in range(1, 19))
    if len(cases) != 20:
        failures.append("painting case count {} != 20".format(len(cases)))
    if len(case_ids) != len(set(case_ids)):
        failures.append("duplicate display_case_id")
    if set(case_ids) != expected_case_ids:
        failures.append("display_case_id set mismatch")
    if sum(1 for case_id in case_ids if case_id.endswith("_Target")) != 1:
        failures.append("required target case count mismatch")
    if any(not str(prop(actor, "target_artifact_id")) for actor in cases):
        failures.append("empty target_artifact_id")

    guard_path_actors = by_class.get("HeistGuardWaypoint", []) + by_class.get("BP_Guard_C", [])
    case_guard_clearances = []
    for guard_path_actor in guard_path_actors:
        guard_location = guard_path_actor.get_actor_location()
        for case in cases:
            case_location = case.get_actor_location()
            distance = math.hypot(guard_location.x - case_location.x, guard_location.y - case_location.y)
            case_guard_clearances.append((distance, guard_path_actor.get_actor_label(), case.get_actor_label()))
    minimum_case_guard_clearance = min(case_guard_clearances, default=(float("inf"), "", ""))
    if minimum_case_guard_clearance[0] < MIN_CASE_GUARD_CLEARANCE_CM:
        failures.append(
            "guard path clearance {:.1f}cm below {:.1f}cm: {} vs {}".format(
                minimum_case_guard_clearance[0],
                MIN_CASE_GUARD_CLEARANCE_CM,
                minimum_case_guard_clearance[1],
                minimum_case_guard_clearance[2],
            )
        )

    class_expectations = {
        "PlayerStart": 4,
        "BP_Vent_C": 1,
        "BP_Guard_C": expected["guards"],
        "HeistGuardWaypoint": expected["waypoints"],
        "BP_SecurityCamera_C": expected["cameras"],
        "BP_LaserBarrier_C": expected["lasers"],
        "BP_SecurityHoldButton_C": expected["lasers"],
    }
    for class_name, count in class_expectations.items():
        actual = len(by_class.get(class_name, []))
        if actual != count:
            failures.append("{} count {} != {}".format(class_name, actual, count))
    if deferred_object_cases:
        failures.append("deferred object display case count {} != 0".format(len(deferred_object_cases)))

    retired_loot_prefixes = ("W6_Loot_", "W8_RELEASE_{}_Loot_".format(code))
    retired_loot = [
        actor for actor in by_class.get("BP_Loot_C", [])
        if actor.get_actor_label().startswith(retired_loot_prefixes)
    ]
    if retired_loot:
        failures.append("retired authored loose-loot actors remain: {}".format(len(retired_loot)))

    prefix = "LDV2_{}_".format(code)
    legacy_static = [
        actor for actor in by_class.get("StaticMeshActor", [])
        if actor.get_actor_label().startswith(code + "_") and str(actor.get_folder_path()).startswith("Architecture/")
    ]
    if legacy_static:
        failures.append("legacy overlay static mesh actors remain: {}".format(len(legacy_static)))
    ldv2_static = [
        actor for actor in by_class.get("StaticMeshActor", [])
        if actor.get_actor_label().startswith(prefix)
    ]
    static_transforms = {}
    for actor in by_class.get("StaticMeshActor", []):
        location = actor.get_actor_location()
        rotation = actor.get_actor_rotation()
        scale = actor.get_actor_scale3d()
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
        mesh = prop(components[0], "static_mesh") if components else None
        mesh_path = mesh.get_path_name() if mesh else ""
        key = (
            round(location.x, 1), round(location.y, 1), round(location.z, 1),
            round(rotation.pitch, 1), round(rotation.yaw, 1), round(rotation.roll, 1),
            round(scale.x, 3), round(scale.y, 3), round(scale.z, 3),
            mesh_path,
        )
        static_transforms.setdefault(key, []).append(actor.get_actor_label())
    duplicate_static_locations = [
        {
            "location": list(transform[0:3]),
            "rotation": list(transform[3:6]),
            "scale": list(transform[6:9]),
            "mesh": transform[9],
            "labels": sorted(group_labels),
        }
        for transform, group_labels in static_transforms.items()
        if len(group_labels) > 1
    ]
    if duplicate_static_locations:
        failures.append("duplicate StaticMeshActor locations: {}".format(len(duplicate_static_locations)))
    non_starter_meshes = []
    for actor in ldv2_static:
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = prop(component, "static_mesh")
            if not mesh or not mesh.get_path_name().startswith("/Game/Assets/StarterContent/"):
                non_starter_meshes.append(actor.get_actor_label())
    if non_starter_meshes:
        failures.append("non-StarterContent LDV2 meshes: {}".format(len(set(non_starter_meshes))))

    wall_folder = "LDV2/{}/Architecture/Walls".format(code)
    lower_walls = [
        actor
        for actor in ldv2_static
        if str(actor.get_folder_path()) == wall_folder
        and not actor.get_actor_label().endswith("_Upper")
    ]
    upper_walls = [
        actor
        for actor in ldv2_static
        if str(actor.get_folder_path()) == wall_folder
        and actor.get_actor_label().endswith("_Upper")
    ]
    if not lower_walls:
        failures.append("generated lower wall set is empty")
    if len(upper_walls) != len(lower_walls):
        failures.append(
            "upper wall count {} != lower wall count {}".format(len(upper_walls), len(lower_walls))
        )
    for lower in lower_walls:
        lower_label = lower.get_actor_label()
        lower_location = lower.get_actor_location()
        lower_scale = lower.get_actor_scale3d()
        if not close_float(lower_location.z, LOWER_WALL_Z) or not close_float(lower_scale.z, 1.0):
            failures.append("lower wall transform changed: " + lower_label)
        upper = by_label.get(lower_label + "_Upper")
        if upper is None:
            failures.append("missing upper wall: " + lower_label)
            continue
        upper_location = upper.get_actor_location()
        upper_scale = upper.get_actor_scale3d()
        if (
            not close_float(upper_location.x, lower_location.x)
            or not close_float(upper_location.y, lower_location.y)
            or not close_float(upper_location.z, UPPER_WALL_Z)
            or not close_float(upper_scale.x, lower_scale.x)
            or not close_float(upper_scale.y, lower_scale.y)
            or not close_float(upper_scale.z, 1.0)
        ):
            failures.append("upper wall transform mismatch: " + upper.get_actor_label())
        if "MuseumTallUpperWall" not in actor_tags(upper):
            failures.append("upper wall tag missing: " + upper.get_actor_label())
        lower_components = lower.get_components_by_class(unreal.StaticMeshComponent)
        upper_components = upper.get_components_by_class(unreal.StaticMeshComponent)
        lower_mesh = prop(lower_components[0], "static_mesh") if lower_components else None
        upper_mesh = prop(upper_components[0], "static_mesh") if upper_components else None
        lower_mesh_path = lower_mesh.get_path_name() if lower_mesh else ""
        upper_mesh_path = upper_mesh.get_path_name() if upper_mesh else ""
        expected_upper_mesh = "Wall_Window_400x400" if "Wall_Window_400x400" in lower_mesh_path else "Wall_400x400"
        if expected_upper_mesh not in upper_mesh_path:
            failures.append("upper wall mesh mismatch: " + upper.get_actor_label())

    glass_visibility_failures = []
    if code == "M03":
        glass_actors = [
            actor for actor in ldv2_static
            if "GlassRoof_" in actor.get_actor_label()
            or "SpineGlassBaffle_" in actor.get_actor_label()
            or "GlassLaneDisplay_" in actor.get_actor_label()
        ]
        for actor in glass_actors:
            for component in actor.get_components_by_class(unreal.StaticMeshComponent):
                try:
                    profile_name = str(component.get_collision_profile_name())
                    response = component.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY)
                    if profile_name != "InvisibleWall" or response != unreal.CollisionResponseType.ECR_IGNORE:
                        glass_visibility_failures.append(actor.get_actor_label())
                except Exception:
                    glass_visibility_failures.append(actor.get_actor_label() + ":query_failed")
        if glass_visibility_failures:
            failures.append("M03 glass blocks Visibility: {}".format(len(set(glass_visibility_failures))))

    floors = [actor for actor in ldv2_static if actor.get_actor_label().startswith(prefix + "Floor_")]
    if len(floors) != expected["floor_count"]:
        failures.append("floor tile count {} != {}".format(len(floors), expected["floor_count"]))
    if floors:
        centers_x = [actor.get_actor_location().x for actor in floors]
        centers_y = [actor.get_actor_location().y for actor in floors]
        bounds = (
            min(centers_x) - 400.0,
            max(centers_x) + 400.0,
            min(centers_y) - 400.0,
            max(centers_y) + 400.0,
        )
        expected_bounds = (-expected["half_x"], expected["half_x"], -expected["half_y"], expected["half_y"])
        if any(not close_float(actual, target) for actual, target in zip(bounds, expected_bounds)):
            failures.append("floor bounds {} != {}".format(bounds, expected_bounds))
    else:
        bounds = ()
    floor_xy = [(round(actor.get_actor_location().x, 1), round(actor.get_actor_location().y, 1)) for actor in floors]
    if len(floor_xy) != len(set(floor_xy)):
        failures.append("duplicate LDV2 floor XY locations")

    vent_panels = [actor for actor in ldv2_static if "VentEntry_" in actor.get_actor_label() and actor.get_actor_label().endswith("_Panel")]
    vent_slats = [actor for actor in ldv2_static if "VentEntry_" in actor.get_actor_label() and "_Slat_" in actor.get_actor_label()]
    if len(vent_panels) != expected["vent_points"]:
        failures.append("vent visual panel count mismatch")
    if len(vent_slats) != expected["vent_points"] * 4:
        failures.append("vent visual slat count mismatch")

    required_spatial_labels = [
        prefix + "SecurityDesk",
        prefix + "SecurityChair_00",
        prefix + "EvidenceRack_00",
        prefix + "DetentionBar_00",
    ]
    for required_label in required_spatial_labels:
        if required_label not in labels:
            failures.append("missing spatial marker {}".format(required_label))

    evidence_table = by_label.get(prefix + "SecurityDesk")
    if evidence_table:
        if evidence_table.get_class().get_name() != "StaticMeshActor":
            failures.append("SecurityDesk evidence table is not a StaticMeshActor")
        table_components = evidence_table.get_components_by_class(unreal.StaticMeshComponent)
        table_mesh = prop(table_components[0], "static_mesh") if table_components else None
        if not table_mesh or not table_mesh.get_path_name().endswith("/SM_TableRound.SM_TableRound"):
            failures.append("SecurityDesk evidence table mesh mismatch")
        if "HeistEvidenceTableVisual" not in actor_tags(evidence_table):
            failures.append("SecurityDesk missing HeistEvidenceTableVisual tag")

    detention_anchors = sorted(
        (
            actor for actor in by_class.get("TargetPoint", [])
            if actor.get_actor_label().startswith(prefix + "DetentionSpawn_")
        ),
        key=actor_label,
    )
    expected_detention_labels = {
        prefix + "DetentionSpawn_{:02d}".format(index)
        for index in range(1, 5)
    }
    if {actor.get_actor_label() for actor in detention_anchors} != expected_detention_labels:
        failures.append("detention anchor label set mismatch")
    if any("HeistDetentionSpawn" not in actor_tags(actor) for actor in detention_anchors):
        failures.append("detention anchor missing HeistDetentionSpawn tag")
    detention_locations = {
        tuple(round(value, 1) for value in (actor.get_actor_location().x, actor.get_actor_location().y, actor.get_actor_location().z))
        for actor in detention_anchors
    }
    if len(detention_locations) != 4:
        failures.append("detention anchor locations are not four unique points")

    evidence_anchors = sorted(
        (
            actor for actor in by_class.get("TargetPoint", [])
            if actor.get_actor_label().startswith(prefix + "EvidenceSlot_")
        ),
        key=actor_label,
    )
    expected_evidence_labels = {
        prefix + "EvidenceSlot_{:02d}".format(index)
        for index in range(1, 26)
    }
    if {actor.get_actor_label() for actor in evidence_anchors} != expected_evidence_labels:
        failures.append("evidence anchor label set mismatch")
    required_evidence_tags = {"HeistEvidenceTableAnchor", "HeistEvidenceSlot"}
    if any(not required_evidence_tags.issubset(actor_tags(actor)) for actor in evidence_anchors):
        failures.append("evidence anchor missing table/slot tag contract")
    evidence_locations = {
        tuple(round(value, 1) for value in (actor.get_actor_location().x, actor.get_actor_location().y, actor.get_actor_location().z))
        for actor in evidence_anchors
    }
    if len(evidence_locations) != 25:
        failures.append("evidence anchor locations are not 25 unique points")
    if evidence_table:
        table_location = evidence_table.get_actor_location()
        if any(
            math.hypot(
                actor.get_actor_location().x - table_location.x,
                actor.get_actor_location().y - table_location.y,
            ) > 200.0
            for actor in evidence_anchors
        ):
            failures.append("evidence anchor outside SecurityDesk table footprint")
    for required_label in expected["signature_labels"]:
        if required_label not in labels:
            failures.append("missing map signature actor {}".format(required_label))
    for signature_label, expected_xy in expected["signature_locations"].items():
        actor = by_label.get(signature_label)
        if actor is None:
            continue
        location = actor.get_actor_location()
        if not close_float(location.x, expected_xy[0]) or not close_float(location.y, expected_xy[1]):
            failures.append(
                "topology signature location mismatch {}: ({:.1f},{:.1f})".format(
                    signature_label, location.x, location.y
                )
            )

    for door_label in expected["topology_door_labels"]:
        actor = by_label.get(door_label)
        if actor is None:
            failures.append("missing topology door " + door_label)
            continue
        mesh_paths = [
            str(prop(component, "static_mesh").get_path_name())
            for component in actor.get_components_by_class(unreal.StaticMeshComponent)
            if prop(component, "static_mesh")
        ]
        if not any("Wall_Door_400x400" in mesh_path for mesh_path in mesh_paths):
            failures.append("topology opening is not a door mesh: " + door_label)

    exit_actors = by_class.get("BP_Vent_C", [])
    exit_visual = by_label.get(expected["exit_visual_label"])
    exit_visual_distance = None
    if len(exit_actors) == 1 and exit_visual is not None:
        exit_location = exit_actors[0].get_actor_location()
        visual_location = exit_visual.get_actor_location()
        exit_visual_distance = math.hypot(exit_location.x - visual_location.x, exit_location.y - visual_location.y)
        if exit_visual_distance > 100.0:
            failures.append("gameplay exit is {:.1f}cm from its vent visual".format(exit_visual_distance))
    elif exit_visual is None:
        failures.append("missing gameplay exit vent visual " + expected["exit_visual_label"])

    target_case = next((actor for actor in cases if str(prop(actor, "display_case_id")).endswith("_Target")), None)
    target_exit_distance = None
    if target_case is not None and len(exit_actors) == 1:
        target_location = target_case.get_actor_location()
        exit_location = exit_actors[0].get_actor_location()
        target_exit_distance = math.hypot(target_location.x - exit_location.x, target_location.y - exit_location.y)
        if target_exit_distance < expected["minimum_target_exit_distance"]:
            failures.append(
                "target-exit straight distance {:.1f}cm below {:.1f}cm".format(
                    target_exit_distance, expected["minimum_target_exit_distance"]
                )
            )

    generated_lights = [
        actor for actor in by_class.get("PointLight", [])
        if actor.get_actor_label().startswith(prefix)
    ]
    if len(generated_lights) != expected["generated_lights"]:
        failures.append("generated light count {} != {}".format(len(generated_lights), expected["generated_lights"]))

    ceiling_panels = [
        actor for actor in ldv2_static
        if any(actor.get_actor_label().startswith(prefix_value) for prefix_value in expected["ceiling_prefixes"])
    ]
    if len(ceiling_panels) != expected["ceiling_panels"]:
        failures.append("ceiling panel count {} != {}".format(len(ceiling_panels), expected["ceiling_panels"]))
    for ceiling_panel in ceiling_panels:
        target_z = 1000.0 if code == "M03" and "GlassRoof_Crossing" in ceiling_panel.get_actor_label() else CEILING_Z
        if not close_float(ceiling_panel.get_actor_location().z, target_z):
            failures.append(
                "ceiling height mismatch {}: {:.1f} != {:.1f}".format(
                    ceiling_panel.get_actor_label(), ceiling_panel.get_actor_location().z, target_z
                )
            )

    fixture_prefix = prefix + ("GalleryLamp_" if code == "M02" else "CeilingLamp_")
    fixtures = [actor for actor in ldv2_static if actor.get_actor_label().startswith(fixture_prefix)]
    for fixture in fixtures:
        components = fixture.get_components_by_class(unreal.StaticMeshComponent)
        material = components[0].get_material(0) if components else None
        material_path = material.get_path_name() if material else ""
        if "M_Lamp" not in material_path:
            failures.append("lamp default emissive material missing: " + fixture.get_actor_label())

    point_expectations = {}
    if code == "M01":
        for index in range(12):
            point_expectations[prefix + "WarmLight_{:02d}".format(index)] = {
                "z": 720.0,
                "color": (255, 205, 145),
                "intensity": 1600.0 if index == 0 else 1100.0,
                "radius": 1650.0 if index == 0 else 1500.0,
            }
        for fixture in fixtures:
            if not close_float(fixture.get_actor_location().z, 770.0):
                failures.append("M01 ceiling fixture height mismatch: " + fixture.get_actor_label())
        for actor in ldv2_static:
            if actor.get_actor_label().startswith(prefix + "Skylight") and not close_float(actor.get_actor_location().z, 790.0):
                failures.append("M01 skylight structure height mismatch: " + actor.get_actor_label())
        for actor in ldv2_static:
            if actor.get_actor_label().startswith(prefix + "RotundaPillar_") and not close_float(actor.get_actor_scale3d().z, 1.6):
                failures.append("M01 rotunda pillar height mismatch: " + actor.get_actor_label())
    elif code == "M02":
        for index in range(12):
            point_expectations[prefix + "WarmLight_{:02d}".format(index)] = {
                "color": (255, 176, 105),
                "intensity": 900.0 if index < 10 else 800.0,
                "radius": 1050.0 if index < 10 else 900.0,
            }
        point_expectations[prefix + "MoonCourtLight"] = {
            "z": 760.0,
            "color": (145, 185, 255),
            "intensity": 1500.0,
            "radius": 2400.0,
        }
    else:
        for index in range(9):
            point_expectations[prefix + "SpineLight_{:02d}".format(index)] = {
                "z": 715.0,
                "color": (172, 216, 255),
                "intensity": 1000.0,
                "radius": 1500.0,
            }
        for index in range(4):
            point_expectations[prefix + "EmergencyLight_{:02d}".format(index)] = {
                "z": 240.0,
                "color": (255, 72, 58),
                "intensity": 500.0,
                "radius": 850.0,
            }
        point_expectations[prefix + "CrossingLight"] = {
            "z": 920.0,
            "color": (150, 220, 255),
            "intensity": 1600.0,
            "radius": 2300.0,
        }
        for fixture in fixtures:
            if not close_float(fixture.get_actor_location().z, 760.0):
                failures.append("M03 ceiling fixture height mismatch: " + fixture.get_actor_label())
        for index in range(12):
            rib = by_label.get(prefix + "RoofRib_{:02d}".format(index))
            expected_rib_z = 988.0 if index in (5, 6, 7) else 800.0
            if rib is None or not close_float(rib.get_actor_location().z, expected_rib_z):
                failures.append("M03 roof rib height mismatch: {:02d}".format(index))
        for index in range(2):
            rail = by_label.get(prefix + "RoofRail_{:02d}".format(index))
            if rail is None or not close_float(rail.get_actor_location().z, 800.0):
                failures.append("M03 roof rail height mismatch: {:02d}".format(index))
        crossing = by_label.get(prefix + "Topology_BraidedCrossing")
        frame = by_label.get(prefix + "CrossingSuspendedFrame")
        if crossing is None or not close_float(crossing.get_actor_location().z, 905.0):
            failures.append("M03 braided crossing height mismatch")
        if frame is None or not close_float(frame.get_actor_location().z, 780.0):
            failures.append("M03 suspended frame height mismatch")

    for light_label, light_expected in point_expectations.items():
        light = by_label.get(light_label)
        component = light.get_component_by_class(unreal.PointLightComponent) if light else None
        if component is None:
            failures.append("missing generated point light: " + light_label)
            continue
        if "z" in light_expected and not close_float(light.get_actor_location().z, light_expected["z"]):
            failures.append("point light height mismatch: " + light_label)
        if color_tuple(prop(component, "light_color")) != light_expected["color"]:
            failures.append("point light color mismatch: " + light_label)
        if not close_float(prop(component, "intensity"), light_expected["intensity"]):
            failures.append("point light intensity mismatch: " + light_label)
        if not close_float(prop(component, "attenuation_radius"), light_expected["radius"]):
            failures.append("point light radius mismatch: " + light_label)

    night_expected = NIGHT_EXPECTED[code]
    directional_lights = by_class.get("DirectionalLight", [])
    sky_lights = by_class.get("SkyLight", [])
    fog_actors = by_class.get("ExponentialHeightFog", [])
    post_process_volumes = by_class.get("PostProcessVolume", [])
    night_snapshot = {}
    if len(directional_lights) != 1:
        failures.append("DirectionalLight count {} != 1".format(len(directional_lights)))
    else:
        directional = directional_lights[0]
        component = directional.get_component_by_class(unreal.DirectionalLightComponent)
        rotation = directional.get_actor_rotation()
        night_snapshot["directional_intensity"] = prop(component, "intensity")
        night_snapshot["directional_color"] = list(color_tuple(prop(component, "light_color")))
        if component is None:
            failures.append("DirectionalLightComponent missing")
        else:
            if not close_float(prop(component, "intensity"), night_expected["moon_intensity"], 0.01):
                failures.append("moon intensity mismatch")
            if color_tuple(prop(component, "light_color")) != night_expected["moon_color"]:
                failures.append("moon color mismatch")
        if any(
            not close_float(actual, target, 0.1)
            for actual, target in zip(
                (rotation.pitch, rotation.yaw, rotation.roll), night_expected["moon_rotation"]
            )
        ):
            failures.append("moon rotation mismatch")
    if len(sky_lights) != 1:
        failures.append("SkyLight count {} != 1".format(len(sky_lights)))
    else:
        component = sky_lights[0].get_component_by_class(unreal.SkyLightComponent)
        night_snapshot["sky_intensity"] = prop(component, "intensity")
        night_snapshot["sky_color"] = list(color_tuple(prop(component, "light_color")))
        if component is None:
            failures.append("SkyLightComponent missing")
        else:
            if not close_float(prop(component, "intensity"), night_expected["sky_intensity"], 0.01):
                failures.append("sky intensity mismatch")
            if color_tuple(prop(component, "light_color")) != night_expected["sky_color"]:
                failures.append("sky color mismatch")
            if bool(prop(component, "real_time_capture")):
                failures.append("SkyLight real-time capture must remain disabled")
    if len(fog_actors) != 1:
        failures.append("ExponentialHeightFog count {} != 1".format(len(fog_actors)))
    else:
        fog_actor = fog_actors[0]
        component = fog_actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
        night_snapshot["fog_density"] = prop(component, "fog_density")
        if component is None:
            failures.append("ExponentialHeightFogComponent missing")
        else:
            if not close_float(fog_actor.get_actor_location().z, 0.0):
                failures.append("fog height mismatch")
            if not close_float(prop(component, "fog_density"), night_expected["fog_density"], 0.0001):
                failures.append("fog density mismatch")
            if not close_float(prop(component, "start_distance"), night_expected["fog_start_distance"], 0.1):
                failures.append("fog start distance mismatch")
            if not close_float(prop(component, "fog_max_opacity"), night_expected["fog_max_opacity"], 0.01):
                failures.append("fog max opacity mismatch")
            if bool(prop(component, "enable_volumetric_fog")):
                failures.append("volumetric fog must remain disabled")
    if len(post_process_volumes) != 1:
        failures.append("PostProcessVolume count {} != 1".format(len(post_process_volumes)))
    else:
        post_process = post_process_volumes[0]
        settings = prop(post_process, "settings")
        exposure_min = prop(settings, "auto_exposure_min_brightness")
        exposure_max = prop(settings, "auto_exposure_max_brightness")
        night_snapshot["exposure_ev100"] = exposure_min
        night_snapshot["bloom_intensity"] = prop(settings, "bloom_intensity")
        if post_process.get_actor_label() != prefix + "NightPostProcess":
            failures.append("night PostProcessVolume label mismatch")
        if not bool(prop(post_process, "unbound")) or not close_float(prop(post_process, "blend_weight"), 1.0):
            failures.append("night PostProcessVolume is not fully unbound")
        if not bool(prop(settings, "override_auto_exposure_min_brightness")) or not bool(prop(settings, "override_auto_exposure_max_brightness")):
            failures.append("night exposure override is disabled")
        if not close_float(exposure_min, night_expected["exposure_ev100"], 0.01) or not close_float(exposure_max, night_expected["exposure_ev100"], 0.01):
            failures.append("night exposure EV100 mismatch")
        if not bool(prop(settings, "override_bloom_intensity")) or not close_float(prop(settings, "bloom_intensity"), night_expected["bloom_intensity"], 0.01):
            failures.append("night bloom mismatch")

    waypoint_routes = {}
    for waypoint in by_class.get("HeistGuardWaypoint", []):
        route_id = str(prop(waypoint, "patrol_route_id"))
        waypoint_routes.setdefault(route_id, []).append(waypoint)
    for route_id in waypoint_routes:
        waypoint_routes[route_id].sort(key=lambda actor: int(prop(actor, "patrol_order") or 0))
    guard_obstacles = [
        actor for actor in ldv2_static
        if "FoldingScreen_" in actor.get_actor_label()
        or "SpineTechPlinth_" in actor.get_actor_label()
        or "SpineGlassBaffle_" in actor.get_actor_label()
    ]
    obstacle_clearances = []
    for obstacle in guard_obstacles:
        origin, extent = obstacle.get_actor_bounds(False)
        for route_id, route_waypoints in waypoint_routes.items():
            for start, end in zip(route_waypoints, route_waypoints[1:]):
                distance = sampled_segment_aabb_clearance(start.get_actor_location(), end.get_actor_location(), origin, extent)
                obstacle_clearances.append((distance, route_id, obstacle.get_actor_label()))
    minimum_guard_obstacle_clearance = min(obstacle_clearances, default=(None, "", ""))
    if minimum_guard_obstacle_clearance[0] is not None and minimum_guard_obstacle_clearance[0] < MIN_GUARD_OBSTACLE_CLEARANCE_CM:
        failures.append(
            "guard obstacle clearance {:.1f}cm below {:.1f}cm: {} vs {}".format(
                minimum_guard_obstacle_clearance[0],
                MIN_GUARD_OBSTACLE_CLEARANCE_CM,
                minimum_guard_obstacle_clearance[1],
                minimum_guard_obstacle_clearance[2],
            )
        )

    navs = by_class.get("NavMeshBoundsVolume", [])
    if len(navs) != 1:
        failures.append("NavMeshBoundsVolume count {} != 1".format(len(navs)))
        nav_scale = ()
    else:
        scale = navs[0].get_actor_scale3d()
        nav_scale = (round(scale.x, 2), round(scale.y, 2), round(scale.z, 2))
        if any(not close_float(actual, target) for actual, target in zip(nav_scale, expected["nav_scale"])):
            failures.append("nav scale {} != {}".format(nav_scale, expected["nav_scale"]))

    linked_case_labels = []
    linked_case_ids = []
    for barrier in by_class.get("BP_LaserBarrier_C", []):
        protected_case = prop(barrier, "protected_painting_case")
        protected_id = str(prop(protected_case, "display_case_id")) if protected_case else ""
        if not protected_case:
            failures.append("laser without protected case")
        elif protected_id.endswith("_Target"):
            failures.append("required target protected by mandatory laser")
        linked_case_labels.append(actor_label(protected_case))
        linked_case_ids.append(protected_id)
    if set(linked_case_ids) != set(expected["laser_case_ids"]):
        failures.append("laser protected case set mismatch: {}".format(sorted(linked_case_ids)))
    if len(linked_case_ids) != len(set(linked_case_ids)):
        failures.append("duplicate laser protected case")
    for button in by_class.get("BP_SecurityHoldButton_C", []):
        linked_barrier = prop(button, "linked_laser_barrier")
        if not linked_barrier:
            failures.append("laser button without linked barrier")
            continue
        expected_barrier_label = button.get_actor_label().replace("LaserButton_", "Laser_")
        if actor_label(linked_barrier) != expected_barrier_label:
            failures.append("laser button pair mismatch: {} -> {}".format(button.get_actor_label(), actor_label(linked_barrier)))

    payload = {
        "map": expected["path"],
        "pass": not failures,
        "failures": failures,
        "actor_count": len(actors),
        "ldv2_starter_static_mesh_actors": len(ldv2_static),
        "duplicate_static_mesh_locations": duplicate_static_locations,
        "legacy_overlay_static_mesh_actors": len(legacy_static),
        "generated_lights": len(generated_lights),
        "ceiling_panels": len(ceiling_panels),
        "ceiling_height_cm": CEILING_Z,
        "lower_wall_panels": len(lower_walls),
        "upper_wall_panels": len(upper_walls),
        "upper_wall_base_z_cm": UPPER_WALL_Z,
        "night_environment": night_snapshot,
        "floor_bounds_cm": list(bounds),
        "painting_cases": len(cases),
        "unique_case_ids": len(set(case_ids)),
        "authored_loose_loot": len(by_class.get("BP_Loot_C", [])),
        "retired_authored_loot": len(retired_loot),
        "deferred_object_cases": len(deferred_object_cases),
        "vent_visual_points": len(vent_panels),
        "gameplay_exit_vents": len(by_class.get("BP_Vent_C", [])),
        "guards": len(by_class.get("BP_Guard_C", [])),
        "guard_waypoints": len(by_class.get("HeistGuardWaypoint", [])),
        "minimum_case_guard_clearance_cm": round(minimum_case_guard_clearance[0], 1),
        "minimum_guard_obstacle_clearance_cm": None if minimum_guard_obstacle_clearance[0] is None else round(minimum_guard_obstacle_clearance[0], 1),
        "cameras": len(by_class.get("BP_SecurityCamera_C", [])),
        "lasers": len(by_class.get("BP_LaserBarrier_C", [])),
        "linked_laser_cases": linked_case_labels,
        "linked_laser_case_ids": sorted(linked_case_ids),
        "nav_scale": list(nav_scale),
        "security_detention_spatial_shell": all(label in labels for label in required_spatial_labels),
        "detention_spawn_anchors": len(detention_anchors),
        "evidence_table_anchors": len(evidence_anchors),
        "exit_visual_distance_cm": None if exit_visual_distance is None else round(exit_visual_distance, 1),
        "target_exit_straight_distance_cm": None if target_exit_distance is None else round(target_exit_distance, 1),
        "topology_signature_count": sum(1 for label in expected["signature_labels"] if label in labels),
        "topology_door_count": sum(1 for label in expected["topology_door_labels"] if label in labels),
        "glass_visibility_failures": sorted(set(glass_visibility_failures)),
    }
    unreal.log_warning("MH_LEVEL_VERIFY=" + json.dumps(payload, ensure_ascii=True, sort_keys=True))
    all_failures.extend("{}: {}".format(code, failure) for failure in failures)

if all_failures:
    for failure in all_failures:
        unreal.log_error("MH_LEVEL_VERIFY_FAILED=" + failure)
    raise RuntimeError("Museum level verification failed: {} issue(s)".format(len(all_failures)))

unreal.log_warning("MH_LEVEL_VERIFY_ALL_PASS={}".format(len(selected_level_codes)))
