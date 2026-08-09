import unreal


MAP_PATH = "/Game/Maps/M01_ClassicalPrototype"


def safe_property(actor, name):
    try:
        return actor.get_editor_property(name)
    except Exception:
        return None


if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
    raise RuntimeError(f"Failed to load {MAP_PATH}")

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sorted(actor_subsystem.get_all_level_actors(), key=lambda actor: actor.get_name())

unreal.log(f"TASK-W6-CONTENT-AUDIT Map={MAP_PATH} Actors={len(actors)}")
for actor in actors:
    class_name = actor.get_class().get_name()
    if not any(token in class_name for token in ("DisplayCase", "Loot", "Guard", "Vent", "PlayerStart", "NavMesh", "Waypoint")):
        continue

    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    fields = []
    for property_name in (
        "display_case_id",
        "target_artifact_id",
        "object_case_id",
        "target_object_artifact_id",
        "object_family_id",
        "loot_data_row",
        "guard_profile_id",
        "route_id",
        "b_requires_escape_phase",
        "b_vent_manually_enabled",
    ):
        value = safe_property(actor, property_name)
        if value is not None:
            fields.append(f"{property_name}={value}")

    unreal.log(
        "TASK-W6-CONTENT-ACTOR "
        f"Name={actor.get_name()} Label={actor.get_actor_label()} Class={class_name} "
        f"Location=({location.x:.1f},{location.y:.1f},{location.z:.1f}) "
        f"Rotation=({rotation.pitch:.1f},{rotation.yaw:.1f},{rotation.roll:.1f}) "
        + " ".join(fields)
    )
