"""Verify authored Loose Loot stations in transient SIE worlds, without saving.

Reports saved placement, all eligible row/support combinations, actual runtime
loot supply, capsule/sphere overlap and navigation as separate evidence scopes.
No E input, pickup RPC, natural traversal or multiplayer success is inferred.
Run only after apply_loot_exhibition_layout.py has finished saving all maps.
"""

import hashlib
import itertools
import json
import math
from pathlib import Path
import time
import traceback
import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/Automation/LooseLootExhibition20261003"
PLAN = ROOT / "ProjectResources/SourceArt/Gallery/LooseLootExhibitionLayout.json"
EDITOR = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
STATE = {"index": 0, "phase": "load", "phase_time": 0.0, "started": 0.0,
         "callback": None, "probes": [], "finished": False, "nav_ready_since": None}
REPORT = {"status": "NOT_TESTED", "scope": "SavedLooseLootStations_TransientSIEQueries",
          "user_pie": "NOT_TESTED", "natural_E_pickup": "NOT_TESTED",
          "multiplayer": "NOT_TESTED", "packages_saved": False, "maps": []}


def vec(value):
    return [float(getattr(value, axis)) for axis in "xyz"]


def name(actor):
    return actor.get_name() if actor else None


def mesh_path(path):
    return str(path).split("'")[-2] if "'" in str(path) else str(path)


def status(checks):
    return "PASS" if checks and all(row["pass"] for row in checks) else "FAIL"


def component_corners(component):
    bounds = component.static_mesh.get_bounding_box()
    return [unreal.MathLibrary.transform_location(component.get_world_transform(), unreal.Vector(*point))
            for point in itertools.product(*zip(vec(bounds.min), vec(bounds.max)))]


def aabb(points):
    values = [vec(point) for point in points]
    return [[min(point[axis] for point in values) for axis in range(3)],
            [max(point[axis] for point in values) for axis in range(3)]]


def trace_support(world, support, x, y, top, floor, ignored):
    raw = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, top + 40), unreal.Vector(x, y, floor + 2),
        unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignored, unreal.DrawDebugTrace.NONE, True)
    hit = raw.to_dict() if raw is not None else {}
    return {"hit_support": bool(hit.get("blocking_hit") and hit.get("hit_actor") == support),
            "hit_actor": name(hit.get("hit_actor")),
            "surface_z": float(hit["impact_point"].z) if hit.get("blocking_hit") else None}


def rows_from_table(table):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))


def resolve_balance(mode):
    override = mode.get_editor_property("game_balance_data_asset")
    return override if override else unreal.get_default_object(unreal.HeistGameBalanceDataAsset)


def eligible_rows(balance):
    table_value = balance.get_editor_property("loot_data_table")
    item_value = balance.get_editor_property("item_data_table")
    table = table_value if isinstance(table_value, unreal.DataTable) else unreal.load_asset(str(table_value))
    item_table = item_value if isinstance(item_value, unreal.DataTable) else unreal.load_asset(str(item_value))
    if table is None or item_table is None:
        raise RuntimeError("Missing actual balance LootDataTable or ItemDataTable")
    item_rows = {row["Name"]: row for row in rows_from_table(item_table)}
    rows = [row for row in rows_from_table(table)
            if item_rows.get(row["Name"], {}).get("bAvailableInV1")
            and row["SpawnWeight"] > 0 and row["ScoreValue"] > 0]
    return table, rows


def spawn_deferred(world, actor_class, transform, table=None, row_id=None):
    gs = unreal.get_default_object(unreal.GameplayStatics)
    actor = gs.call_method("BeginDeferredActorSpawnFromClass", (
        world, actor_class, transform, unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,
        None, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    if actor is None:
        raise RuntimeError("Transient probe deferred spawn failed")
    STATE["probes"].append(actor)
    if table is not None:
        handle = unreal.DataTableRowHandle()
        handle.set_editor_property("data_table", table)
        handle.set_editor_property("row_name", unreal.Name(row_id))
        actor.set_editor_property("loot_data_row", handle)
    return gs.call_method("FinishSpawningActor", (
        actor, transform, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))


def destroy_probe(actor):
    if actor:
        actor.destroy_actor()
    if actor in STATE["probes"]:
        STATE["probes"].remove(actor)


def capsule_probe(world, template, runtime_capsule):
    actor = spawn_deferred(world, unreal.Actor.static_class(), unreal.Transform())
    component = actor.call_method("AddComponentByClass", (
        unreal.CapsuleComponent.static_class(), False, unreal.Transform(), False))
    component.set_capsule_size(template.get_scaled_capsule_radius(), template.get_scaled_capsule_half_height(), True)
    component.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
    # HeistPlayerCharacter::BeginPlay changes these two channels on the live pawn.
    component.set_collision_object_type(template.get_collision_object_type() if runtime_capsule else unreal.CollisionChannel.ECC_HEIST_PLAYER)
    component.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    channel = unreal.CollisionChannel.ECC_HEIST_INTERACTABLE
    component.set_collision_response_to_channel(channel, template.get_collision_response_to_channel(channel) if runtime_capsule else unreal.CollisionResponseType.ECR_OVERLAP)
    component.set_editor_property("generate_overlap_events", True)
    return actor, component


def capsule_clear(world, position, capsule, ignored):
    raw = unreal.SystemLibrary.capsule_trace_single_by_profile(
        world, position, position, capsule.get_scaled_capsule_radius() - 1,
        capsule.get_scaled_capsule_half_height() - 1, capsule.get_collision_profile_name(),
        False, ignored, unreal.DrawDebugTrace.NONE, True)
    hit = raw.to_dict() if raw is not None else {}
    return {"pass": not bool(hit.get("blocking_hit")), "blocking_actor": name(hit.get("hit_actor")),
            "query_inset_cm": 1, "initial_overlap": bool(hit.get("initial_overlap"))}


def station_category(anchor):
    value = str(anchor.get_editor_property("spawn_category"))
    return "VaultFixed" if "VAULT" in value.upper() else "ExhibitionRoom"


def saved_placement(entry, actors):
    checks = []
    for station in entry["stations"]:
        anchor, support = actors[station["spawn_actor"]], actors[station["support_actor"]]
        expected_yaw = station.get("anchor_yaw", station["yaw"])
        yaw_error = abs((anchor.get_actor_rotation().yaw - expected_yaw + 180) % 360 - 180)
        support_yaw_error = abs((support.get_actor_rotation().yaw - station["yaw"] + 180) % 360 - 180)
        expected_support = unreal.Vector(station["location"][0], station["location"][1], station["floor_z"])
        valid = (isinstance(anchor, unreal.HeistLootSpawnPoint)
                 and isinstance(support, unreal.StaticMeshActor)
                 and (anchor.get_actor_location() - unreal.Vector(*station["location"])).length() < 0.1
                 and (support.get_actor_location() - expected_support).length() < 0.1
                 and yaw_error < 0.01 and support_yaw_error < 0.01
                 and (support.get_actor_scale3d() - unreal.Vector(*station["scale"])).length() < 0.0001
                 and support.static_mesh_component.static_mesh.get_path_name().split(".")[0] == mesh_path(station["mesh"]).split(".")[0]
                 and bool(anchor.get_editor_property("spawn_enabled")))
        checks.append({"spawn": anchor.get_name(), "support": support.get_name(),
                       "category": station_category(anchor), "anchor": vec(anchor.get_actor_location()),
                       "anchor_yaw": float(anchor.get_actor_rotation().yaw), "pass": valid})
    spawn_points = [actor for actor in actors.values() if isinstance(actor, unreal.HeistLootSpawnPoint)]
    occupied_pair = []
    for left, right in itertools.combinations(spawn_points, 2):
        distance = (left.get_actor_location() - right.get_actor_location()).length()
        required = max(float(left.get_editor_property("occupancy_radius")), float(right.get_editor_property("occupancy_radius")))
        if distance <= required:
            occupied_pair.append({"left": name(left), "right": name(right), "distance_cm": distance, "required_cm": required})
    authored_pickups = [name(actor) for actor in actors.values() if isinstance(actor, unreal.HeistLootActor)]
    return {"status": "PASS" if status(checks) == "PASS" and not occupied_pair and not authored_pickups else "FAIL",
            "stations": checks, "candidate_count": len(spawn_points), "occupied_anchor_pairs": occupied_pair,
            "authored_pickups": authored_pickups}


def runtime_supply(world, entry, actors, balance, row_definitions):
    vault = int(balance.get_editor_property("match_start_vault_loot_count"))
    exhibition = int(balance.get_editor_property("match_start_exhibition_loot_count"))
    loot = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HeistLootActor)
            if actor.actor_has_tag("HeistMatchSpawnedLooseLoot")]
    checks, category_counts = [], {"VaultFixed": 0, "ExhibitionRoom": 0}
    row_lookup = {row["Name"]: row for row in row_definitions}
    anchors = [actors[station["spawn_actor"]] for station in entry["stations"]]
    for actor in loot:
        row_id = str(actor.get_editor_property("loot_row_id"))
        row = row_lookup.get(row_id)
        matches = [anchor for anchor in anchors if (actor.get_actor_location() - anchor.get_actor_location()).length() <= 1]
        resolved_category = row["SpawnCategory"] if row else "Unknown"
        category_counts[resolved_category] = category_counts.get(resolved_category, 0) + 1
        mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
        visible_mesh = bool(row and mesh and mesh.static_mesh
                            and mesh.static_mesh.get_path_name() == mesh_path(row["WorldMesh"]) and mesh.is_visible())
        valid = bool(row and len(matches) == 1 and station_category(matches[0]) == resolved_category
                     and actor.get_editor_property("is_available") and int(actor.get_editor_property("score_value")) == row["ScoreValue"]
                     and visible_mesh)
        checks.append({"actor": name(actor), "row": row_id, "category": resolved_category,
                       "matching_anchor": name(matches[0]) if len(matches) == 1 else None,
                       "mesh_resolved_visible": visible_mesh, "pass": valid})
    valid = (vault == 1 and exhibition == 4 and len(loot) == vault + exhibition
             and category_counts.get("VaultFixed") == vault and category_counts.get("ExhibitionRoom") == exhibition
             and status(checks) == "PASS")
    return {"status": "PASS" if valid else "FAIL", "balance_source": balance.get_path_name(),
            "counts": {"vault": vault, "exhibition": exhibition}, "actual_loot_count": len(loot),
            "actual_categories": category_counts, "checks": checks}


def row_geometry(world, station, support, visual, ignored):
    corners = component_corners(visual)
    visual_bounds = aabb(corners)
    support_bounds = aabb(component_corners(support.static_mesh_component))
    local_bounds = support.static_mesh_component.static_mesh.get_bounding_box()
    local_corners = [unreal.MathLibrary.inverse_transform_location(support.static_mesh_component.get_world_transform(), point) for point in corners]
    inside = all(local_bounds.min.x - 0.5 <= point.x <= local_bounds.max.x + 0.5
                 and local_bounds.min.y - 0.5 <= point.y <= local_bounds.max.y + 0.5 for point in local_corners)
    center_x = (visual_bounds[0][0] + visual_bounds[1][0]) / 2
    center_y = (visual_bounds[0][1] + visual_bounds[1][1]) / 2
    contact = trace_support(world, support, center_x, center_y, support_bounds[1][2], station["floor_z"], ignored)
    gap = visual_bounds[0][2] - contact["surface_z"] if contact["surface_z"] is not None else None
    valid = inside and contact["hit_support"] and gap is not None and -0.25 <= gap <= 2
    return {"pass": bool(valid), "visual_world_bounds": visual_bounds, "support_world_bounds": support_bounds,
            "footprint_within_support_local_bounds": inside, "support_contact": contact,
            "visual_bottom_surface_gap_cm": gap, "gap_tolerance_cm": [-0.25, 2],
            "scope": "Actual row mesh bounds plus complex support-surface trace; no mesh-to-mesh penetration certification"}


def test_rows_and_approaches(world, entry, actors, balance, mode):
    table, definitions = eligible_rows(balance)
    loot_class_value = balance.get_editor_property("world_loot_actor_class")
    loot_class = loot_class_value if isinstance(loot_class_value, unreal.Class) else unreal.load_class(None, str(loot_class_value))
    if loot_class is None:
        raise RuntimeError("Canonical BP_Loot class could not load")
    default_pawn = unreal.get_default_object(mode.get_editor_property("default_pawn_class"))
    live_pawns = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HeistPlayerCharacter)
    template = live_pawns[0].get_component_by_class(unreal.CapsuleComponent) if live_pawns else default_pawn.get_component_by_class(unreal.CapsuleComponent)
    if template is None:
        raise RuntimeError("Actual DefaultPawnClass has no capsule")
    radius, half = template.get_scaled_capsule_radius(), template.get_scaled_capsule_half_height()
    probe, capsule = capsule_probe(world, template, bool(live_pawns))
    ignored = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)) + [probe]
    row_checks, overlap_checks, approach_checks = [], [], []
    try:
        for station in entry["stations"]:
            anchor, support = actors[station["spawn_actor"]], actors[station["support_actor"]]
            position = unreal.Vector(station["approach"][0], station["approach"][1], station["floor_z"] + half + 1)
            probe.set_actor_location(position, False, True)
            clear = capsule_clear(world, position, template, ignored)
            clear.update({"spawn": name(anchor), "approach": vec(position)})
            approach_checks.append(clear)
            for row in definitions:
                if row["SpawnCategory"] != station_category(anchor):
                    continue
                loot = spawn_deferred(world, loot_class, anchor.get_actor_transform(), table, row["Name"])
                try:
                    visual = loot.get_component_by_class(unreal.StaticMeshComponent)
                    sphere = loot.get_component_by_class(unreal.SphereComponent)
                    if visual is None or visual.static_mesh is None or sphere is None:
                        raise RuntimeError("BP_Loot row visual or actual interaction sphere missing: " + row["Name"])
                    geometry = row_geometry(world, station, support, visual, ignored + [loot])
                    geometry.update(spawn=name(anchor), row=row["Name"], mesh=visual.static_mesh.get_path_name())
                    row_checks.append(geometry)
                    # Force movement after this row's sphere has registered its overlap component.
                    probe.set_actor_location(position + unreal.Vector(0, 0, 0.01), False, True)
                    probe.set_actor_location(position, False, True)
                    center = sphere.get_world_location()
                    sphere_radius = sphere.get_scaled_sphere_radius()
                    vertical = max(0, abs(center.z - position.z) - (half - radius))
                    separation = math.sqrt((center.x-position.x)**2 + (center.y-position.y)**2 + vertical**2)
                    expected_overlap = separation <= sphere_radius + radius
                    overlap = bool(sphere.is_overlapping_component(capsule))
                    overlap_checks.append({"spawn": name(anchor), "row": row["Name"],
                                           "sphere_radius_cm": sphere_radius, "approach": vec(position),
                                           "capsule_sphere_separation_cm": separation, "shape_overlap_expected": expected_overlap,
                                           "actual_component_overlap": overlap, "pass": bool(expected_overlap and overlap and clear["pass"])})
                finally:
                    destroy_probe(loot)
    finally:
        destroy_probe(probe)
    expected = sum(sum(row["SpawnCategory"] == station_category(actors[station["spawn_actor"]]) for row in definitions)
                   for station in entry["stations"])
    return ({"status": "PASS" if status(row_checks) == "PASS" and len(row_checks) == 42 else "FAIL",
             "expected_combinations": expected, "actual_combinations": len(row_checks), "checks": row_checks},
            {"status": status(overlap_checks), "capsule_source": template.get_path_name(),
             "capsule_source_kind": "ActualRuntimePawn" if live_pawns else "DefaultPawnClassCDO",
             "probe_kind": "Transient CapsuleComponent; copied live capsule or CDO dimensions with HeistPlayerCharacter BeginPlay channel contract",
             "capsule_radius_cm": radius, "capsule_half_height_cm": half,
             "physics_approach": {"status": status(approach_checks), "checks": approach_checks},
             "row_overlap_checks": overlap_checks})


def torch_properties(world, mode):
    def inspect(actor):
        result = []
        for component in actor.get_components_by_class(unreal.SpotLightComponent):
            if "flashlight" not in component.get_name().lower():
                continue
            result.append({"component": component.get_name(),
                           "intensity": float(component.get_editor_property("intensity")),
                           "units": str(component.get_editor_property("intensity_units")),
                           "visible": bool(component.is_visible()),
                           "attenuation_radius_cm": float(component.get_editor_property("attenuation_radius"))})
        return result
    defaults = inspect(unreal.get_default_object(mode.get_editor_property("default_pawn_class")))
    pawns = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HeistPlayerCharacter)
    live = [{"pawn": name(pawn), "components": inspect(pawn)} for pawn in pawns]
    return {"source": "Actual pawn CDO and any existing SIE pawn components",
            "defaults": defaults, "runtime_instances": live,
            "runtime_instance_status": "INSPECTED" if live else "NOT_TESTED",
            "player_toggle_input": "NOT_TESTED"}


def navigation(world, entry, actors, mode):
    result = {"status": "NOT_TESTED", "checks": [], "projected_targets": [],
              "scope": "Complete Nav paths to projected targets plus actual capsule-clear and Loot-sphere overlap there; no traversal sweep or player input"}
    probe = None
    if unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):
        result["reason"] = "NavigationBuildingOrLocked"
        return result
    system = unreal.NavigationSystemV1.get_navigation_system(world)
    if system is None:
        result["reason"] = "MissingNavigationSystem"
        return result
    try:
        agents = system.get_editor_property("supported_agents")
        if len(agents) > 1:
            result["reason"] = "MultipleAgentsNeedExplicitNavData"
            return result
        starts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerStart)
        if not starts:
            result["reason"] = "MissingPlayerStart"
            return result
        pawns = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HeistPlayerCharacter)
        pawn_source = pawns[0] if pawns else unreal.get_default_object(mode.get_editor_property("default_pawn_class"))
        template = pawn_source.get_component_by_class(unreal.CapsuleComponent)
        radius, half = template.get_scaled_capsule_radius(), template.get_scaled_capsule_half_height()
        balance = resolve_balance(mode)
        table, definitions = eligible_rows(balance)
        loot_class_value = balance.get_editor_property("world_loot_actor_class")
        loot_class = loot_class_value if isinstance(loot_class_value, unreal.Class) else unreal.load_class(None, str(loot_class_value))
        probe, capsule = capsule_probe(world, template, bool(pawns))
        ignored = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)) + [probe]
        result.update(capsule_radius_cm=radius, capsule_half_height_cm=half,
                      capsule_source_kind="ActualRuntimePawn" if pawns else "DefaultPawnClassCDO")
        projected_starts = [(start, unreal.NavigationSystemV1.project_point_to_navigation(world, start.get_actor_location(), None, None, unreal.Vector(50,50,200))) for start in starts]
        if all(point is None for _, point in projected_starts):
            result["reason"] = "NoQueryableNavigationAtPlayerStarts"
            return result
        for station in entry["stations"]:
            position = unreal.Vector(station["approach"][0], station["approach"][1], station["floor_z"] + half + 1)
            target = unreal.NavigationSystemV1.project_point_to_navigation(world, position, None, None, unreal.Vector(40,40,200))
            goal_clear, goal_overlap = False, False
            detail = {"spawn": station["spawn_actor"], "target_projected": vec(target) if target else None,
                      "status": "FAIL", "row_scope": "One representative row for this category; all row-support cases checked separately"}
            if target:
                goal = target + unreal.Vector(0, 0, half + 1)
                anchor = actors[station["spawn_actor"]]
                representative = next(row for row in definitions if row["SpawnCategory"] == station_category(anchor))
                loot = spawn_deferred(world, loot_class, anchor.get_actor_transform(), table, representative["Name"])
                try:
                    clear = capsule_clear(world, goal, template, ignored+[loot])
                    sphere = loot.get_component_by_class(unreal.SphereComponent)
                    probe.set_actor_location(goal + unreal.Vector(0, 0, 0.01), False, True)
                    probe.set_actor_location(goal, False, True)
                    center, sphere_radius = sphere.get_world_location(), sphere.get_scaled_sphere_radius()
                    vertical = max(0, abs(center.z-goal.z) - (half-radius))
                    separation = math.sqrt((center.x-goal.x)**2+(center.y-goal.y)**2+vertical**2)
                    expected_overlap = separation <= sphere_radius+radius
                    actual_overlap = bool(sphere.is_overlapping_component(capsule))
                    goal_clear = clear["pass"]
                    goal_overlap = bool(expected_overlap and actual_overlap)
                    detail.update(status="PASS" if goal_clear and goal_overlap else "FAIL",
                                  representative_row=representative["Name"], capsule_center=vec(goal),
                                  capsule_clear=clear, sphere_radius_cm=sphere_radius,
                                  capsule_sphere_separation_cm=separation, shape_overlap_expected=expected_overlap,
                                  actual_component_overlap=actual_overlap,
                                  capsule_z_scope="Projected Nav floor Z + actual capsule half-height + 1cm")
                finally:
                    destroy_probe(loot)
            result["projected_targets"].append(detail)
            for start, origin in projected_starts:
                path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, origin, target) if origin and target else None
                at_goal = bool(origin and target and (origin-target).length() < 1)
                drift = math.hypot(target.x-position.x,target.y-position.y) if target else None
                complete = bool(target and drift <= 40 and (at_goal or path and path.is_valid() and not path.is_partial()))
                result["checks"].append({"start": name(start), "spawn": station["spawn_actor"],
                                         "target_projected": vec(target) if target else None,
                                         "projection_xy_drift_cm": drift, "complete_path": complete,
                                         "projected_goal_capsule_clear": goal_clear,
                                         "projected_goal_sphere_overlap": goal_overlap,
                                         "pass": bool(complete and goal_clear and goal_overlap)})
        if unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):
            result["reason"] = "NavigationChangedDuringQueries"
            return result
        result["status"] = status(result["checks"])
    except Exception as error:
        result.update(reason="NavigationQueryUnavailable", error=str(error))
    finally:
        destroy_probe(probe)
    return result


def flush():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "qa.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def finish(reason):
    if STATE["finished"]:
        return
    STATE["finished"] = True
    for actor in list(STATE["probes"]):
        destroy_probe(actor)
    if STATE["callback"] is not None:
        unreal.unregister_slate_post_tick_callback(STATE["callback"])
    if EDITOR.get_game_world():
        LEVELS.editor_request_end_play()
    REPORT["reason"] = reason
    REPORT["elapsed_seconds"] = time.monotonic() - STATE["started"]
    REPORT["map_sha256_after"] = {row["code"]: hashlib.sha256((ROOT/"Content/Maps"/(row["map"]+".umap")).read_bytes()).hexdigest() for row in STATE["plan"]["maps"]}
    REPORT["map_files_unchanged"] = REPORT["map_sha256_before"] == REPORT["map_sha256_after"]
    REPORT["probe_actors_remaining"] = len(STATE["probes"])
    scopes = [row[scope]["status"] for row in REPORT["maps"] for scope in ("geometry", "runtime", "interaction", "navigation")]
    if reason != "Complete" or not REPORT["map_files_unchanged"] or any(value == "FAIL" for value in scopes):
        REPORT["status"] = "FAIL"
    elif len(REPORT["maps"]) == 3 and all(value == "PASS" for value in scopes):
        REPORT["status"] = "PASS"
    else:
        REPORT["status"] = "NOT_TESTED"
    REPORT["row_support_combinations"] = sum(row["geometry"].get("rows", {}).get("actual_combinations", 0) for row in REPORT["maps"])
    flush()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.log("LOOT_EXHIBITION_QA_DONE status=" + REPORT["status"] + " rows=" + str(REPORT["row_support_combinations"]) + " user_pie=NOT_TESTED")


def change_phase(phase):
    STATE.update(phase=phase, phase_time=time.monotonic())


def tick(_):
    now = time.monotonic()
    if now - STATE["started"] > 240 or now - STATE["phase_time"] > 75:
        finish("Timeout:" + STATE["phase"])
        return
    entries = STATE["plan"]["maps"]
    if STATE["phase"] == "load":
        if STATE["index"] == len(entries):
            finish("Complete")
            return
        if EDITOR.get_game_world():
            finish("UnexpectedExistingGameWorld")
            return
        entry = entries[STATE["index"]]
        if not unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/" + entry["map"]):
            finish("MapLoadFailed:" + entry["code"])
            return
        actors = {actor.get_name(): actor for actor in ACTORS.get_all_level_actors()}
        STATE["saved_check"] = saved_placement(entry, actors)
        LEVELS.editor_play_simulate()
        STATE["nav_ready_since"] = None
        change_phase("wait_runtime")
        return
    if STATE["phase"] == "wait_runtime":
        world = EDITOR.get_game_world()
        if world is None or unreal.GameplayStatics.get_game_mode(world) is None or unreal.GameplayStatics.get_time_seconds(world) < 2:
            return
        locked = unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world)
        if not locked and STATE["nav_ready_since"] is None:
            STATE["nav_ready_since"] = now
        if (STATE["nav_ready_since"] is None or now - STATE["nav_ready_since"] < 0.5) and now - STATE["phase_time"] < 15:
            return
        entry = entries[STATE["index"]]
        actors = {actor.get_name(): actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)}
        mode = unreal.GameplayStatics.get_game_mode(world)
        balance = resolve_balance(mode)
        _, definitions = eligible_rows(balance)
        runtime = runtime_supply(world, entry, actors, balance, definitions)
        geometry, interaction = test_rows_and_approaches(world, entry, actors, balance, mode)
        result = {"code": entry["code"], "world": world.get_path_name(),
                  "geometry": {"status": "PASS" if STATE["saved_check"]["status"] == geometry["status"] == "PASS" else "FAIL",
                               "saved_placement": STATE["saved_check"], "rows": geometry},
                  "runtime": runtime, "interaction": interaction, "navigation": navigation(world, entry, actors, mode),
                  "torch": torch_properties(world, mode)}
        REPORT["maps"].append(result)
        flush()
        unreal.log("LOOT_EXHIBITION_QA_MAP " + entry["code"] + " " + json.dumps({scope:result[scope]["status"] for scope in ("geometry","runtime","interaction","navigation")}))
        LEVELS.editor_request_end_play()
        change_phase("wait_end")
        return
    if STATE["phase"] == "wait_end" and EDITOR.get_game_world() is None:
        STATE["index"] += 1
        change_phase("load")


def safe_tick(delta):
    try:
        tick(delta)
    except Exception:
        REPORT["error"] = traceback.format_exc()
        unreal.log_error(REPORT["error"])
        finish("Exception:" + STATE["phase"])


def main():
    if EDITOR.get_game_world():
        raise RuntimeError("End the existing game world before running this dedicated SIE verifier")
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError("Save delegated map edits before running this verifier")
    STATE["plan"] = json.loads(PLAN.read_text(encoding="utf-8-sig"))
    REPORT["map_sha256_before"] = {row["code"]: hashlib.sha256((ROOT/"Content/Maps"/(row["map"]+".umap")).read_bytes()).hexdigest() for row in STATE["plan"]["maps"]}
    REPORT["plan_sha256"] = hashlib.sha256(PLAN.read_bytes()).hexdigest()
    STATE.update(started=time.monotonic(), phase_time=time.monotonic())
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    STATE["callback"] = unreal.register_slate_post_tick_callback(safe_tick)
    unreal.log("LOOT_EXHIBITION_QA_STARTED no_save=1 user_pie=NOT_TESTED")


if __name__ == "__main__":
    main()
