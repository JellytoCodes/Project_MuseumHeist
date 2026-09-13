"""Read-only strict verifier harness: authored starts/routes against stable SIE navigation.

No build, save, input, AI command, actor movement, or settings write is performed.
Editor guards supply authored starts/capsules; SIE world supplies initialized NavData.
The only native-call override adds PIE guard counterparts to the capsule ignore list.
"""

import runpy
import ast
import hashlib
import json
import math
from pathlib import Path
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / "ProjectResources/Scripts/Editor/verify_museum_levels_v2.py"
_, switches, parameters = unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())
parameters = {str(k).casefold(): str(v) for k,v in parameters.items()}
review = "museumreviewlayouts" in {str(v).casefold() for v in switches}
OUTPUT = Path(parameters.get("museumlayoutreport", str(ROOT / "Saved/Automation/MuseumLayout/verification.json")))
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
MAPS = (("M01", "M01_ClassicalPrototype"), ("M02", "M02_MoonlitPrototype"), ("M03", "M03_GlasshousePrototype"))
if review:
    MAPS = tuple((code, "Review/" + name + "_LayoutReview") for code,name in MAPS)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
state = {"phase": "load", "phase_start": time.monotonic(), "started": time.monotonic(),
         "map_index": 0, "callback": None, "finished": False, "samples": [], "sample_at": 0.0,
         "guards": [], "routes": {}, "runtime_guards": [], "stable_since": None}


def map_hashes():
    return {code: hashlib.sha256((ROOT / "Content/Maps" / (name + ".umap")).read_bytes()).hexdigest()
            for code, name in MAPS}


report = {"status": "NOT_TESTED", "scope": "SavedGuardStartsAndRoutes_StrictVerifier_StableRuntimeSIEQueries",
          "source": str(SOURCE), "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
          "map_sha256_before": map_hashes(), "maps": [], "packages_saved": False,
          "navigation_rebuild_requested": False, "user_pie": "NOT_TESTED",
          "override": "Append all PIE guard counterparts to native capsule ActorsToIgnore; every other argument unchanged"}


class SystemLibraryProxy:
    def __getattr__(self, name):
        return getattr(unreal.SystemLibrary, name)

    def capsule_trace_single_by_profile(self, world, start, end, radius, half_height, profile,
                                      trace_complex, ignored, debug_draw, ignore_self):
        return unreal.SystemLibrary.capsule_trace_single_by_profile(
            world, start, end, radius, half_height, profile, trace_complex,
            list(ignored) + state["runtime_guards"], debug_draw, ignore_self)


class UnrealProxy:
    SystemLibrary = SystemLibraryProxy()

    def __getattr__(self, name):
        return getattr(unreal, name)


namespace = {"math": math, "unreal": UnrealProxy()}
tree = ast.parse(SOURCE.read_text(encoding="utf-8-sig"))
function_names = {"prop", "actor_label", "verify_guard_navigation"}
functions = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in function_names]
assert {node.name for node in functions} == function_names
exec(compile(ast.Module(body=functions, type_ignores=[]), str(SOURCE), "exec"), namespace)
verify = namespace["verify_guard_navigation"]
gallery_verify = runpy.run_path(str(ROOT / "ProjectResources/Scripts/Editor/verify_gallery_refinement.py"))["verify_gallery"]


def change_phase(phase):
    state["phase"] = phase
    state["phase_start"] = time.monotonic()


def flush():
    OUTPUT.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def finish(reason):
    if state["finished"]:
        return
    state["finished"] = True
    report.update(reason=reason, elapsed_wall_seconds=round(time.monotonic() - state["started"], 3),
                  map_sha256_after=map_hashes(), source_sha256_after=hashlib.sha256(SOURCE.read_bytes()).hexdigest())
    report["map_files_unchanged"] = report["map_sha256_before"] == report["map_sha256_after"]
    report["source_unchanged"] = report["source_sha256"] == report["source_sha256_after"]
    report["dirty_map_packages"] = [package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    report["status"] = ("PASS" if reason == "Complete" and len(report["maps"]) == 3
                        and all(row.get("strict_runtime", {}).get("status") == "PASS" for row in report["maps"])
                        and all(row.get("gallery", {}).get("status") == "PASS" for row in report["maps"])
                        and all(row.get("layout", {}).get("status") == "PASS" for row in report["maps"])
                        and report["map_files_unchanged"] and report["source_unchanged"]
                        and not report["dirty_map_packages"] else "FAIL")
    try:
        flush()
        unreal.log_warning("MH_STRICT_STABLE_DONE=" + json.dumps({
            "status": report["status"], "reason": reason, "output": str(OUTPUT),
            "map_files_unchanged": report["map_files_unchanged"]}))
    finally:
        if state["callback"] is not None:
            unreal.unregister_slate_post_tick_callback(state["callback"])
        if editor.get_game_world():
            levels.editor_request_end_play()
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


def tick(delta_seconds):
    del delta_seconds
    try:
        now = time.monotonic()
        if now - state["started"] > 240 or now - state["phase_start"] > 70:
            finish("Timeout:" + state["phase"])
            return
        if state["phase"] == "load":
            if state["map_index"] == len(MAPS):
                finish("Complete")
                return
            if editor.get_game_world():
                finish("UnexpectedExistingGameWorld")
                return
            code, name = MAPS[state["map_index"]]
            change_phase("loading")
            if not levels.load_level("/Game/Maps/" + name):
                finish("MapLoadFailed:" + code)
                return
            state["authored_actors"] = list(actors.get_all_level_actors())
            state["guards"] = sorted([actor for actor in actors.get_all_level_actors()
                                      if isinstance(actor, unreal.HeistGuardCharacter)], key=lambda a: a.get_actor_label())
            state["routes"] = {}
            for actor in actors.get_all_level_actors():
                if isinstance(actor, unreal.HeistGuardWaypoint):
                    route_id = str(actor.get_editor_property("patrol_route_id"))
                    state["routes"].setdefault(route_id, []).append(actor)
            for route in state["routes"].values():
                route.sort(key=lambda actor: (int(actor.get_editor_property("patrol_order")), actor.get_name()))
            state["immediate_strict"] = verify(editor.get_editor_world(), state["guards"], state["routes"], "strict")
            state["samples"] = []
            state["sample_at"] = 0.0
            state["runtime_guards"] = []
            change_phase("wait_editor")
            return
        if state["phase"] == "wait_editor":
            elapsed = now - state["phase_start"]
            if elapsed >= state["sample_at"]:
                world = editor.get_editor_world()
                sample = {"elapsed_wall_seconds": round(elapsed, 3),
                          "building_or_locked": bool(unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world))}
                state["samples"].append(sample)
                state["sample_at"] = elapsed + 2.0
                unreal.log_warning("MH_STRICT_EDITOR_WAIT=" + json.dumps({"map": MAPS[state["map_index"]][0], **sample}))
            if elapsed < 10.0:
                return
            editor_result = verify(editor.get_editor_world(), state["guards"], state["routes"], "strict")
            report["maps"].append({
                "map": MAPS[state["map_index"]][0], "editor_nav_wait_samples": state["samples"],
                "strict_editor_immediately_after_load": state["immediate_strict"],
                "strict_editor_after_wait": editor_result,
                "authored_capsule_profiles": [str(guard.get_component_by_class(unreal.CapsuleComponent).get_collision_profile_name())
                                              for guard in state["guards"]],
            })
            flush()
            levels.editor_play_simulate()
            state["stable_since"] = None
            change_phase("wait_runtime")
            return
        if state["phase"] == "wait_runtime":
            world = editor.get_game_world()
            if world is None or unreal.GameplayStatics.is_game_paused(world) or unreal.GameplayStatics.get_time_seconds(world) < 2.0:
                return
            if unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):
                state["stable_since"] = None
                return
            if state["stable_since"] is None:
                state["stable_since"] = now
                return
            if now - state["stable_since"] < 0.5:
                return
            state["runtime_guards"] = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HeistGuardCharacter))
            result = verify(world, state["guards"], state["routes"], "strict")
            row = report["maps"][-1]
            row["layout"] = runpy.run_path(str(ROOT / "ProjectResources/Scripts/Editor/verify_approved_museum_layout.py"))["verify_layout"](world, MAPS[state["map_index"]][0], state["authored_actors"])
            row["gallery"] = gallery_verify(world, MAPS[state["map_index"]][0], state["authored_actors"])
            unreal.log_warning("MH_GALLERY_VERIFY=" + json.dumps(row["gallery"]))
            row.update(runtime_world=world.get_path_name(), runtime_game_seconds=float(unreal.GameplayStatics.get_time_seconds(world)),
                       runtime_guards=len(state["runtime_guards"]), strict_runtime=result)
            unreal.log_warning("MH_STRICT_STABLE_MAP=" + json.dumps({"map": row["map"], **result}))
            flush()
            levels.editor_request_end_play()
            change_phase("wait_end")
            return
        if state["phase"] == "wait_end" and editor.get_game_world() is None:
            state["map_index"] += 1
            change_phase("load")
    except Exception:
        report["error"] = traceback.format_exc()
        unreal.log_error(report["error"])
        finish("Exception:" + state["phase"])


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state["callback"] = unreal.register_slate_post_tick_callback(tick)
unreal.log_warning("MH_STRICT_STABLE_STARTED=" + str(OUTPUT))
