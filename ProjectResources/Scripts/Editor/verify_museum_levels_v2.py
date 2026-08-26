import json

import unreal


EXPECTED = {
    "M01": {
        "path": "/Game/Maps/M01_ClassicalPrototype",
        "half_x": 7200.0,
        "half_y": 5200.0,
        "floor_count": 234,
        "vent_points": 3,
        "guards": 5,
        "waypoints": 22,
        "cameras": 6,
        "lasers": 2,
        "nav_scale": (72.0, 52.0, 10.0),
    },
    "M02": {
        "path": "/Game/Maps/M02_MoonlitPrototype",
        "half_x": 6400.0,
        "half_y": 5600.0,
        "floor_count": 224,
        "vent_points": 2,
        "guards": 5,
        "waypoints": 21,
        "cameras": 4,
        "lasers": 2,
        "nav_scale": (64.0, 56.0, 10.0),
    },
    "M03": {
        "path": "/Game/Maps/M03_GlasshousePrototype",
        "half_x": 8000.0,
        "half_y": 4400.0,
        "floor_count": 220,
        "vent_points": 3,
        "guards": 4,
        "waypoints": 17,
        "cameras": 8,
        "lasers": 3,
        "nav_scale": (80.0, 44.0, 10.0),
    },
}


def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None


def close_float(left, right, tolerance=0.1):
    return abs(float(left) - float(right)) <= tolerance


def actor_label(value):
    return value.get_actor_label() if value else ""


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
    for barrier in by_class.get("BP_LaserBarrier_C", []):
        protected_case = prop(barrier, "protected_painting_case")
        protected_id = str(prop(protected_case, "display_case_id")) if protected_case else ""
        if not protected_case:
            failures.append("laser without protected case")
        elif protected_id.endswith("_Target"):
            failures.append("required target protected by mandatory laser")
        linked_case_labels.append(actor_label(protected_case))
    for button in by_class.get("BP_SecurityHoldButton_C", []):
        if not prop(button, "linked_laser_barrier"):
            failures.append("laser button without linked barrier")

    payload = {
        "map": expected["path"],
        "pass": not failures,
        "failures": failures,
        "actor_count": len(actors),
        "ldv2_starter_static_mesh_actors": len(ldv2_static),
        "floor_bounds_cm": list(bounds),
        "painting_cases": len(cases),
        "unique_case_ids": len(set(case_ids)),
        "vent_visual_points": len(vent_panels),
        "gameplay_exit_vents": len(by_class.get("BP_Vent_C", [])),
        "guards": len(by_class.get("BP_Guard_C", [])),
        "guard_waypoints": len(by_class.get("HeistGuardWaypoint", [])),
        "cameras": len(by_class.get("BP_SecurityCamera_C", [])),
        "lasers": len(by_class.get("BP_LaserBarrier_C", [])),
        "linked_laser_cases": linked_case_labels,
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
