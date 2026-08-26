import json
import math

import unreal


EXPECTED = {
    "M01": {
        "path": "/Game/Maps/M01_ClassicalPrototype",
        "half_x": 7200.0,
        "half_y": 5200.0,
        "floor_count": 234,
        "vent_points": 3,
        "guards": 5,
        "waypoints": 31,
        "cameras": 6,
        "lasers": 2,
        "generated_lights": 10,
        "ceiling_panels": 58,
        "ceiling_prefixes": ("LDV2_M01_Ceiling_",),
        "laser_case_ids": ("Case_M01_Optional_HighValue", "Case_M01_Optional_11"),
        "signature_labels": ("LDV2_M01_HeroPlinth", "LDV2_M01_Portal_00", "LDV2_M01_WarmLight_00"),
        "nav_scale": (72.0, 52.0, 10.0),
    },
    "M02": {
        "path": "/Game/Maps/M02_MoonlitPrototype",
        "half_x": 6400.0,
        "half_y": 5600.0,
        "floor_count": 224,
        "vent_points": 2,
        "guards": 5,
        "waypoints": 30,
        "cameras": 4,
        "lasers": 2,
        "generated_lights": 13,
        "ceiling_panels": 60,
        "ceiling_prefixes": ("LDV2_M02_Ceiling_",),
        "laser_case_ids": ("Case_M02_Optional_HighValue", "Case_M02_Optional_07"),
        "signature_labels": ("LDV2_M02_MoonPool", "LDV2_M02_FoldingScreen_00A", "LDV2_M02_MoonCourtLight"),
        "nav_scale": (64.0, 56.0, 10.0),
    },
    "M03": {
        "path": "/Game/Maps/M03_GlasshousePrototype",
        "half_x": 8000.0,
        "half_y": 4400.0,
        "floor_count": 220,
        "vent_points": 3,
        "guards": 4,
        "waypoints": 29,
        "cameras": 8,
        "lasers": 3,
        "generated_lights": 13,
        "ceiling_panels": 10,
        "ceiling_prefixes": ("LDV2_M03_GlassRoof_",),
        "laser_case_ids": ("Case_M03_Optional_HighValue", "Case_M03_Optional_08", "Case_M03_Optional_10"),
        "signature_labels": ("LDV2_M03_SpinePortal_00", "LDV2_M03_RestorationTable_00", "LDV2_M03_EmergencyLight_00"),
        "nav_scale": (80.0, 44.0, 10.0),
    },
}

MIN_CASE_GUARD_CLEARANCE_CM = 450.0
MIN_GUARD_OBSTACLE_CLEARANCE_CM = 100.0


def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None


def close_float(left, right, tolerance=0.1):
    return abs(float(left) - float(right)) <= tolerance


def actor_label(value):
    return value.get_actor_label() if value else ""


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

for code, expected in EXPECTED.items():
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
    non_starter_meshes = []
    for actor in ldv2_static:
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = prop(component, "static_mesh")
            if not mesh or not mesh.get_path_name().startswith("/Game/Assets/StarterContent/"):
                non_starter_meshes.append(actor.get_actor_label())
    if non_starter_meshes:
        failures.append("non-StarterContent LDV2 meshes: {}".format(len(set(non_starter_meshes))))

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
    for required_label in expected["signature_labels"]:
        if required_label not in labels:
            failures.append("missing map signature actor {}".format(required_label))

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
        "legacy_overlay_static_mesh_actors": len(legacy_static),
        "generated_lights": len(generated_lights),
        "ceiling_panels": len(ceiling_panels),
        "floor_bounds_cm": list(bounds),
        "painting_cases": len(cases),
        "unique_case_ids": len(set(case_ids)),
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
    }
    unreal.log_warning("MH_LEVEL_VERIFY=" + json.dumps(payload, ensure_ascii=True, sort_keys=True))
    all_failures.extend("{}: {}".format(code, failure) for failure in failures)

if all_failures:
    for failure in all_failures:
        unreal.log_error("MH_LEVEL_VERIFY_FAILED=" + failure)
    raise RuntimeError("Museum level verification failed: {} issue(s)".format(len(all_failures)))

unreal.log_warning("MH_LEVEL_VERIFY_ALL_PASS=3")
