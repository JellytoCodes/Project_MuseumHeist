"""Read-only saved-map lighting inventory and matching eye-level review captures."""
import hashlib
import json
import math
import os
import runpy
import time
import traceback
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
STAGE = os.environ.get("MH_NIGHT_STAGE", "before")
CAPTURE_MODE = os.environ.get("MH_NIGHT_CAPTURE_MODE", "viewport")
OUT = ROOT / "Saved/Screenshots/NightLighting" / STAGE
OUT.mkdir(parents=True, exist_ok=True)
PLANS = json.loads((ROOT / "ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json").read_text(encoding="utf-8"))["maps"]
MAPS = {"M01": "M01_ClassicalPrototype", "M02": "M02_MoonlitPrototype", "M03": "M03_GlasshousePrototype"}
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EDITOR = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
VIEWS = []
for plan in PLANS:
    for pattern in ("Solo", "Salon", "Grid"):
        group = next(g for g in plan["groups"] if g["pattern"] == pattern)
        paintings = [p for p in plan["paintings"] if p["group"] == group["id"]]
        n = paintings[0]["normal"]
        x = sum(p["xy"][0] for p in paintings) / len(paintings)
        y = sum(p["xy"][1] for p in paintings) / len(paintings)
        VIEWS.append((plan["id"], pattern, (x * 100 + n[0] * 550, y * 100 + n[1] * 550, 170), math.degrees(math.atan2(-n[1], -n[0]))))
    if STAGE != "before":
        hub = plan["rooms"][{"M01": "H", "M02": "G", "M03": "C2"}[plan["id"]]]
        x0, y0, x1, y1 = hub["bounds"]
        x, y = x0+(x1-x0)*0.1, y0+(y1-y0)*0.12
        yaw = math.degrees(math.atan2((y0+y1)*0.5-y, (x0+x1)*0.5-x))
        VIEWS.append((plan["id"], "Hub", (x*100, y*100, 170), yaw))
STATE = dict(index=0, phase="load", time=0, map=None, callback=None, busy=False)
REPORT = dict(stage=STAGE, capture_mode=CAPTURE_MODE, maps={}, captures=[])


def prop(obj, name):
    try:
        v = obj.get_editor_property(name)
        return v if isinstance(v, (bool, int, float, str)) else str(v)
    except Exception:
        return None


def inventory(code):
    lights, pp = [], []
    for actor in ACTORS.get_all_level_actors():
        label = actor.get_actor_label()
        cls = actor.get_class().get_name()
        light = actor.get_component_by_class(unreal.LightComponent)
        if light:
            lights.append(dict(label=label, type=cls, location=str(actor.get_actor_location()), **{k: prop(light, k) for k in ("intensity", "intensity_units", "attenuation_radius", "cast_shadows", "visible", "indirect_lighting_intensity", "light_color", "inner_cone_angle", "outer_cone_angle")}))
        elif isinstance(actor, unreal.PostProcessVolume):
            settings = actor.get_editor_property("settings")
            pp.append(dict(label=label, **{k: prop(settings, k) for k in ("auto_exposure_min_brightness", "auto_exposure_max_brightness", "auto_exposure_bias", "bloom_intensity", "local_exposure_shadow_contrast_scale")}))
    path = ROOT / "Content/Maps" / (MAPS[code] + ".umap")
    stable = runpy.run_path(str(ROOT / "ProjectResources/Scripts/Editor/apply_museum_night_lighting.py"))["non_lighting_snapshot"]()
    if STAGE != "before":
        applied = json.loads((OUT.parent / "apply.json").read_text(encoding="utf-8"))
        expected = next(m["after"] for m in applied["maps"] if m["map"] == code)
        if stable != expected:
            raise RuntimeError("Saved non-lighting transforms changed: " + code)
    return dict(lights=lights, post_process=pp, non_lighting_snapshot=stable, map_sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def finish(error=None):
    unreal.unregister_slate_post_tick_callback(STATE["callback"])
    REPORT["status"] = "FAIL" if error else "PASS"
    REPORT["error"] = error
    (OUT / "audit.json").write_text(json.dumps(REPORT, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log_warning("MH_NIGHT_CAPTURE_DONE=" + REPORT["status"])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(dt):
    if STATE["busy"] or time.monotonic() - STATE["time"] < 4:
        return
    STATE["busy"] = True
    try:
        if STATE["index"] == len(VIEWS):
            finish()
            return
        code, pattern, location, yaw = VIEWS[STATE["index"]]
        if STATE["phase"] == "load":
            if STATE["map"] != code:
                if not unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/" + MAPS[code]):
                    raise RuntimeError("Map load failed: " + code)
                STATE["map"] = code
                REPORT["maps"][code] = inventory(code)
            world = EDITOR.get_editor_world()
            # Reproduce the reported 512-page budget in both comparison runs.
            unreal.SystemLibrary.execute_console_command(world, "r.Shadow.Virtual.MaxPhysicalPages 512")
            EDITOR.set_level_viewport_camera_info(unreal.Vector(*location), unreal.Rotator(yaw=yaw))
            if CAPTURE_MODE == "viewport":
                camera = ACTORS.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(*location), unreal.Rotator(yaw=yaw))
                camera.camera_component.set_editor_property("field_of_view", 90.0)
                name = code + "_" + pattern + ".png"
                task = unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, str(OUT / name), camera=camera, delay=3.0)
                if not task.is_valid_task():
                    raise RuntimeError("Viewport capture task was not created")
                STATE.update(actor=camera, task=task, phase="capture", time=time.monotonic())
                return
            actor = ACTORS.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(*location), unreal.Rotator(yaw=yaw))
            capture = actor.capture_component2d
            rt = unreal.RenderingLibrary.create_render_target2d(world, 1600, 900, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
            capture.set_editor_property("texture_target", rt)
            capture.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
            capture.set_editor_property("capture_every_frame", True)
            capture.set_editor_property("always_persist_rendering_state", True)
            capture.set_editor_property("fov_angle", 90.0)
            settings = capture.get_editor_property("post_process_settings")
            settings.set_editor_property("override_dynamic_global_illumination_method", True)
            settings.set_editor_property("dynamic_global_illumination_method", unreal.DynamicGlobalIlluminationMethod.LUMEN)
            settings.set_editor_property("override_reflection_method", True)
            settings.set_editor_property("reflection_method", unreal.ReflectionMethod.LUMEN)
            capture.set_editor_property("post_process_settings", settings)
            STATE.update(actor=actor, rt=rt, phase="capture", time=time.monotonic())
        else:
            name = code + "_" + pattern + ".png"
            if CAPTURE_MODE == "viewport":
                if not STATE["task"].is_task_done():
                    if time.monotonic() - STATE["time"] > 45:
                        raise RuntimeError("Viewport capture timed out")
                    return
                if not (OUT / name).is_file():
                    raise RuntimeError("Viewport screenshot file is missing: " + name)
            else:
                unreal.RenderingLibrary.export_render_target(EDITOR.get_editor_world(), STATE["rt"], str(OUT), name)
            ACTORS.destroy_actor(STATE["actor"])
            if CAPTURE_MODE != "viewport":
                unreal.RenderingLibrary.release_render_target2d(STATE["rt"])
            REPORT["captures"].append(name)
            STATE.update(index=STATE["index"] + 1, phase="load", time=time.monotonic())
    except Exception:
        finish(traceback.format_exc())
    finally:
        STATE["busy"] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
STATE["callback"] = unreal.register_slate_post_tick_callback(tick)
