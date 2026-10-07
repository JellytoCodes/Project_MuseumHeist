"""Verify and save only derived navigation for the approved exhibition maps.

Run in a dedicated delegated Editor after apply.json is complete. Authored
actors are compared before rebuild, before save and after reload. Native patrol
graph/occupancy queries are not character movement, interaction or User PIE.
"""
import hashlib
import json
import math
import runpy
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/Automation/ApprovedExhibition20261007"
APPLY = OUT / "apply.json"
REPORT_PATH = OUT / "navigation_verify.json"
MAPS = {"M01": "M01_ClassicalPrototype", "M02": "M02_MoonlitPrototype",
        "M03": "M03_GlasshousePrototype"}
EDITOR = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
snapshot = runpy.run_path(str(ROOT / "ProjectResources/Scripts/Editor/apply_loot_exhibition_layout.py"))["snapshot"]
verify_guard_navigation = runpy.run_path(
    str(ROOT / "ProjectResources/Scripts/Editor/verify_museum_levels_v2.py"))["verify_guard_navigation"]
STATE = dict(index=0, phase="load", stamp=0, stable_since=None, callback=None,
             busy=False, finished=False, entries=[], before=None, row=None)
REPORT = dict(status="RUNNING", maps=[], error=None,
              scope="Approved saved exhibition actors; derived Recast rebuild, native patrol graph and saved guard capsule occupancy",
              authored_preservation="NOT_TESTED", natural_patrol="NOT_TESTED",
              capsule_path_sweeps="NOT_TESTED", interaction="NOT_TESTED",
              user_pie="NOT_TESTED", multiplayer="NOT_TESTED")


def xyz(value):
    return [float(getattr(value, key)) for key in "xyz"]


def sha(code):
    return hashlib.sha256((ROOT / "Content/Maps" / (MAPS[code] + ".umap")).read_bytes()).hexdigest()


def authored_snapshot():
    rows = {}
    for actor in ACTORS.get_all_level_actors():
        row = snapshot(actor)
        if isinstance(actor, unreal.HeistGuardWaypoint):
            row["patrol"] = {key: str(actor.get_editor_property(key)) for key in
                             ("patrol_route_id", "patrol_order", "wait_duration_override")}
        if isinstance(actor, unreal.HeistPaintingDisplayCaseActor):
            row["painting"] = dict(case=str(actor.get_display_case_id()),
                                   artifact=str(actor.get_editor_property("target_artifact_id")))
        if isinstance(actor, unreal.HeistLaserBarrierActor):
            target = actor.get_protected_painting_case()
            row["protected_case"] = target.get_name() if target else None
        if isinstance(actor, unreal.HeistSecurityHoldButtonActor):
            laser = actor.get_linked_laser_barrier()
            row["linked_laser"] = laser.get_name() if laser else None
        rows[actor.get_name()] = row
    return rows


def flush():
    OUT.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2,
                                     allow_nan=False) + "\n", encoding="utf-8")


def finish(error=None):
    if STATE["finished"]:
        return
    STATE["finished"] = True
    if error:
        REPORT["error"] = error
        if STATE["row"] is not None:
            STATE["row"]["status"] = "FAIL"
            STATE["row"]["error"] = error
    REPORT["status"] = ("PASS" if not error and len(REPORT["maps"]) == 3
                        and all(row.get("status") == "PASS" for row in REPORT["maps"]) else "FAIL")
    if REPORT["status"] == "PASS":
        REPORT["authored_preservation"] = "PASS"
    try:
        flush()
        unreal.log("APPROVED_EXHIBITION_NAV_VERIFY_" + REPORT["status"])
    finally:
        if STATE["callback"] is not None:
            unreal.unregister_slate_post_tick_callback(STATE["callback"])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


def change(phase):
    STATE.update(phase=phase, stamp=time.monotonic(), stable_since=None)


def navigation_ready(world, minimum_wait):
    if time.monotonic() - STATE["stamp"] < minimum_wait:
        return False
    system = unreal.NavigationSystemV1.get_navigation_system(world)
    meshes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)
    if not system or not meshes or unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):
        STATE["stable_since"] = None
        return False
    if STATE["stable_since"] is None:
        STATE["stable_since"] = time.monotonic()
    return time.monotonic() - STATE["stable_since"] >= 2


def validate(world):
    actors = list(ACTORS.get_all_level_actors())
    paintings = [a for a in actors if isinstance(a, unreal.HeistPaintingDisplayCaseActor)]
    spawns = [a for a in actors if isinstance(a, unreal.HeistLootSpawnPoint)]
    lasers = [a for a in actors if isinstance(a, unreal.HeistLaserBarrierActor)]
    buttons = [a for a in actors if isinstance(a, unreal.HeistSecurityHoldButtonActor)]
    assert len(paintings) == 60 and len({str(a.get_display_case_id()) for a in paintings}) == 60, "Painting candidates must be 60 unique cases"
    assert len(spawns) == 12, "Loot spawn candidates must remain 12"
    assert len(lasers) == 2 and len(buttons) == 2, "Expected two existing Laser/Button pairs"
    links = []
    for laser in lasers:
        target = laser.get_protected_painting_case()
        linked = [button for button in buttons if button.get_linked_laser_barrier() == laser]
        assert target in paintings and len(linked) == 1, laser.get_name() + " invalid protected case/button link"
        links.append(dict(laser=laser.get_name(), painting=target.get_name(),
                          case=str(target.get_display_case_id()), button=linked[0].get_name()))
    assert all(button.get_linked_laser_barrier() in lasers for button in buttons), "Button links outside authored barriers"

    authored = [a for a in actors if a.get_actor_label().startswith("AE_")]
    assert authored, "No approved AE_ actors found"
    contact = []
    for actor in authored:
        assert all(math.isfinite(v) and abs(v - round(v)) <= .001 for v in xyz(actor.get_actor_location())), actor.get_name() + " has fractional location"
        scales = [xyz(actor.get_actor_scale3d())] + [xyz(c.get_relative_transform().scale3d)
                  for c in actor.get_components_by_class(unreal.SceneComponent)]
        assert all(math.isfinite(v) and abs(v) > .001 and abs(v * 10 - round(v * 10)) <= .001
                   for scale in scales for v in scale), actor.get_name() + " has non-0.1 scale"
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        if (actor.actor_has_tag("ExhibitionRole_sign")
                and actor.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION):
            continue  # Side-mounted guide panel; its GuideSupport is checked separately.
        components = snapshot(actor)["components"]
        assert components, actor.get_name() + " has no mesh"
        bottom = min(c["bounds"][0][2] for c in components)
        lowest = min(components, key=lambda c: c["bounds"][0][2])["bounds"]
        x, y = [(lowest[0][i] + lowest[1][i]) / 2 for i in (0, 1)]
        hit = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(x, y, bottom + 5), unreal.Vector(x, y, bottom - 20),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [actor], unreal.DrawDebugTrace.NONE, True)
        fields = hit.to_dict() if hit is not None else {}
        supported = bool(fields.get("blocking_hit"))
        gap = bottom - fields["impact_point"].z if supported else None
        support = fields.get("hit_actor")
        contact.append(dict(actor=actor.get_name(), label=actor.get_actor_label(),
                            bottom_z=bottom, support=support.get_name() if support else None,
                            gap_cm=gap, status="PASS" if supported and abs(gap) <= 2 else "FAIL"))
    assert contact and all(row["status"] == "PASS" for row in contact), "AE_ mesh support contact failed: " + json.dumps([r for r in contact if r["status"] != "PASS"])

    starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
    start_rows = []
    for actor in starts:
        projected = unreal.NavigationSystemV1.project_point_to_navigation(
            world, actor.get_actor_location(), None, None, unreal.Vector(100, 100, 200))
        start_rows.append(dict(actor=actor.get_name(), projected=xyz(projected) if projected is not None else None))
    assert len(starts) == 4 and all(row["projected"] is not None for row in start_rows), "Four PlayerStarts must project to actual navigation"

    guards = [a for a in actors if isinstance(a, unreal.HeistGuardCharacter)]
    routes = {}
    for actor in actors:
        if isinstance(actor, unreal.HeistGuardWaypoint):
            route = str(actor.get_editor_property("patrol_route_id"))
            routes.setdefault(route, []).append(actor)
    for route in routes.values():
        route.sort(key=lambda a: (int(a.get_editor_property("patrol_order")), a.get_name()))
    result = verify_guard_navigation(world, guards, routes, "strict")
    checks = dict(painting_candidates=len(paintings), loot_spawn_candidates=len(spawns),
                  security_links=links, authored_ae_actors=len(authored),
                  support_contact=contact, player_starts=start_rows,
                  strict_navigation=result)
    STATE["row"]["checks"] = checks
    flush()
    assert result["status"] == "PASS", "Strict saved guard navigation failed: " + result.get("reason", "Unknown")
    return checks


def tick(_):
    if STATE["busy"] or STATE["finished"]:
        return
    STATE["busy"] = True
    try:
        assert EDITOR.get_game_world() is None, "Do not run during PIE/SIE"
        if STATE["index"] == len(STATE["entries"]):
            finish()
            return
        entry = STATE["entries"][STATE["index"]]
        code, path = entry["code"], "/Game/Maps/" + entry["map"]
        if STATE["phase"] == "load":
            assert sha(code) == entry["after_sha256"], code + " changed since approved apply"
            assert unreal.EditorLoadingAndSavingUtils.load_map(path), "Map load failed: " + code
            row = dict(code=code, map=entry["map"], status="RUNNING", packages_saved=False,
                       before_sha256=sha(code), authored_actors_preserved=False,
                       natural_patrol="NOT_TESTED", interaction="NOT_TESTED", user_pie="NOT_TESTED")
            STATE["row"] = row
            REPORT["maps"].append(row)
            change("prepare")
            flush()
            return
        world = EDITOR.get_editor_world()
        assert world, "Missing Editor world"
        elapsed = time.monotonic() - STATE["stamp"]
        assert elapsed <= 120, code + " navigation readiness timeout in " + STATE["phase"]
        if STATE["phase"] == "prepare":
            if not navigation_ready(world, 6):
                return
            STATE["before"] = authored_snapshot()
            system = unreal.NavigationSystemV1.get_navigation_system(world)
            volumes = [a for a in ACTORS.get_all_level_actors() if isinstance(a, unreal.NavMeshBoundsVolume)]
            meshes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)
            assert system and volumes and len(meshes) == 1, "Expected loaded NavBounds and exactly one RecastNavMesh"
            assert abs(float(meshes[0].get_editor_property("max_simplification_error")) - .1) <= .00001, "Keep authored max_simplification_error=0.1"
            for volume in volumes:
                system.on_navigation_bounds_updated(volume)
            unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
            change("rebuilt")
            return
        if STATE["phase"] == "rebuilt":
            if not navigation_ready(world, 8):
                return
            validate(world)
            assert STATE["before"] == authored_snapshot(), "Authored actors changed during navigation rebuild"
            assert sha(code) == entry["after_sha256"], "Unexpected package write before navigation save"
            STATE["row"]["authored_actors_preserved"] = True
            assert unreal.EditorLoadingAndSavingUtils.save_map(world, path), "Navigation map save failed"
            STATE["row"].update(packages_saved=True, after_sha256=sha(code))
            flush()
            assert unreal.EditorLoadingAndSavingUtils.load_map(path), "Saved map reload failed"
            change("reloaded")
            return
        if STATE["phase"] == "reloaded":
            if not navigation_ready(world, 6):
                return
            assert STATE["before"] == authored_snapshot(), "Reload changed authored actors"
            STATE["row"]["reload_checks"] = validate(world)
            assert STATE["before"] == authored_snapshot(), "Reload verification changed authored actors"
            STATE["row"].update(status="PASS", reloaded_actors_preserved=True,
                                 authored_actor_count=len(STATE["before"]),
                                 after_sha256=sha(code))
            flush()
            STATE.update(index=STATE["index"] + 1, row=None, before=None)
            change("load")
    except Exception:
        finish(traceback.format_exc())
    finally:
        STATE["busy"] = False


def main():
    try:
        assert EDITOR.get_game_world() is None, "End existing PIE/SIE first"
        assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), "Save delegated map edits before verification"
        apply = json.loads(APPLY.read_text(encoding="utf-8-sig"))
        assert apply.get("status") == "PASS", "Approved exhibition apply must finish with PASS"
        entries = apply["maps"]
        assert len(entries) == 3 and {e["code"] for e in entries} == set(MAPS), "Apply must include all three canonical maps"
        for entry in entries:
            assert entry["map"] == MAPS[entry["code"]], "Unexpected map path"
            assert sha(entry["code"]) == entry["after_sha256"], entry["code"] + " no longer matches apply"
        STATE["entries"] = entries
        REPORT["apply_sha256"] = hashlib.sha256(APPLY.read_bytes()).hexdigest()
        flush()
        unreal.EditorPythonScripting.set_keep_python_script_alive(True)
        STATE["callback"] = unreal.register_slate_post_tick_callback(tick)
    except Exception:
        finish(traceback.format_exc())


if __name__ == "__main__":
    main()
