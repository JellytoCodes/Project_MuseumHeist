"""Capture the saved approved exhibition through a dedicated full Unreal Editor.

Use -ExecutePythonScript with a rendered RHI, not a Python commandlet/NullRHI.
MH_APPROVED_CAPTURE_MODE=base_color|night|both (default both)
MH_APPROVED_CAPTURE_SET=all|minimum|overviews (default all)
MH_APPROVED_CAPTURE_QUIT=1 closes the Editor; -unattended also implies quit.
MH_APPROVED_CAPTURE_SETTLE_SECONDS sets per-view warmup (default 6).

Only temporary SceneCapture/flashlight actors are created. No package is saved.
Base Color is a placement review, not lighting evidence. Night eye-level views
use the saved environment and a 12cd copy of the player flashlight settings.
Top views omit the flashlight and hide roof primitives in that capture only.
These static Editor captures do not prove natural PIE, spawning or navigation.
"""
import hashlib
import itertools
import json
import math
import os
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
PLAN_PATH = ROOT / "ProjectResources/SourceArt/Gallery/ApprovedExhibitionLayout.json"
OUT = ROOT / "Saved/Automation/ApprovedExhibition20261007/Captures"
MAPS = {"M01": "M01_ClassicalPrototype", "M02": "M02_MoonlitPrototype",
        "M03": "M03_GlasshousePrototype"}
MINIMUM = {"M01": {"A1", "A6", "A8", "R4"}, "M02": {"B5", "R2"},
           "M03": {"C4", "R3"}}
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EDITOR = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
STATE = {"index": 0, "phase": "load", "stamp": 0., "busy": False,
         "callback": None, "temporary": [], "rt": None, "map": None,
         "finished": False, "cameras": {}, "scene_before": {}}
REPORT = {"captures": [], "maps": {}, "errors": [], "cleanup_errors": [],
          "scope": {"base_color": "Static Editor Base Color placement/composition review",
                    "night": "Static Editor Lit reference with saved lights/exposure and 12cd player-style flashlight",
                    "overview": "Orthographic top view; roof hidden only in SceneCapture, flashlight absent",
                    "natural_user_pie": "NOT_TESTED", "navigation": "NOT_TESTED",
                    "runtime_random_loot": "NOT_TESTED", "multiplayer": "NOT_TESTED"}}


def _sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def _plan_views(plan, selection):
    result = []
    for code in MAPS:
        entry = plan["maps"][code]
        groups = list(entry.get("groups", [])) + list(entry.get("furnishings", []))
        ids = [group["id"] for group in groups]
        assert len(ids) == len(set(ids)), code + " has duplicate view ids"
        assert MINIMUM[code].issubset(ids), code + " is missing required representative views"
        result.append({"code": code, "id": "WholeOverview", "kind": "overview",
                       "data": {"b": entry["b"]}})
        if selection == "overviews":
            continue
        for group in groups:
            if selection == "all" or group["id"] in MINIMUM[code]:
                result.append({"code": code, "id": group["id"],
                               "kind": "furnishing" if group["id"].startswith("R") else "exhibition",
                               "data": group})
    return result


def _scene_snapshot():
    rows = []
    temporary_paths = {a.get_path_name() for a in STATE["temporary"] if a}
    for actor in ACTORS.get_all_level_actors():
        if actor.get_path_name() in temporary_paths:
            continue
        transform = actor.get_actor_transform()
        q = transform.rotation
        components = []
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            if not component.static_mesh:
                continue
            t = component.get_world_transform()
            r = t.rotation
            components.append({"name": component.get_name(), "mesh": component.static_mesh.get_path_name(),
                               "location": _vec(t.translation), "scale": _vec(t.scale3d),
                               "rotation": [r.x, r.y, r.z, r.w], "visible": component.is_visible(),
                               "materials": [m.get_path_name() if m else None for m in component.get_materials()]})
        rows.append({"name": actor.get_name(), "location": _vec(transform.translation),
                     "scale": _vec(transform.scale3d), "rotation": [q.x, q.y, q.z, q.w],
                     "components": sorted(components, key=lambda value: value["name"])})
    return sorted(rows, key=lambda value: value["name"])


def _bounds(component):
    bounds = component.static_mesh.get_bounding_box()
    transform = component.get_world_transform()
    points = [_vec(unreal.MathLibrary.transform_location(transform, unreal.Vector(*point)))
              for point in itertools.product(*zip(_vec(bounds.min), _vec(bounds.max)))]
    return [[min(p[axis] for p in points) for axis in range(3)],
            [max(p[axis] for p in points) for axis in range(3)]]


def _flashlight_template():
    blueprint = unreal.load_asset("/Game/Blueprints/Player/BP_HeistPlayerCharacter")
    assert blueprint, "Player Blueprint is missing"
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    lights = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        component = library.get_object(library.get_data(handle))
        if isinstance(component, unreal.SpotLightComponent) and component.component_has_tag("Flashlight"):
            lights[component.get_path_name()] = component
    assert len(lights) == 1, "Expected one existing player flashlight component"
    return next(iter(lights.values()))


def _trace_clear(start, target, target_tolerance=85.):
    result = unreal.SystemLibrary.line_trace_single(
        EDITOR.get_editor_world(), start, target, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True, [], unreal.DrawDebugTrace.NONE, True)
    hit = result.to_dict() if result is not None else {}
    # Hitting the target exhibit is expected; nearer architecture is an obstruction.
    return not hit.get("blocking_hit", False) or (hit["impact_point"] - target).length() < target_tolerance


def _camera(view):
    key = (view["code"], view["id"])
    if key in STATE["cameras"]:
        return STATE["cameras"][key]
    data = view["data"]
    floor_z = 10. if view["code"] == "M02" else 0.
    x0, y0, x1, y1 = data["b"]
    center = unreal.Vector((x0 + x1) / 2., (y0 + y1) / 2., floor_z + 200.)
    if view["kind"] == "overview":
        width = max(x1 - x0, (y1 - y0) * 16. / 9.) * 1.08
        result = {"location": [center.x, center.y, floor_z + max(width, 4000.)],
                  "rotation": [-90., 90., 0.], "projection": "orthographic",
                  "ortho_width_cm": width, "target": [center.x, center.y, floor_z],
                  "line_of_sight_samples_clear": None}
    else:
        current = {actor.get_name(): actor for actor in ACTORS.get_all_level_actors()}
        targets = []
        for item in data.get("art", []):
            actor = current.get(item.get("id"))
            if actor:
                component = next((c for c in actor.get_components_by_class(unreal.StaticMeshComponent)
                                  if c.static_mesh and "SM_Canvas_Painting_" in c.static_mesh.get_name()), None)
                if component:
                    b = _bounds(component)
                    targets.append(unreal.Vector(*[(low + high) / 2. for low, high in zip(*b)]))
        for item in data.get("cases", []):
            actor = current.get(item.get("source"))
            if actor:
                targets.append(actor.get_actor_location() + unreal.Vector(0., 0., 45.))
        if not targets:
            targets = [unreal.Vector(center.x, center.y, floor_z + 100.)]
        # Prefer the open side opposite the first wall, then score alternatives.
        preferred = -90. + float(data.get("angle", 0.))
        if data.get("walls"):
            wall = data["walls"][0]["b"]
            dx, dy = center.x - (wall[0] + wall[2]) / 2., center.y - (wall[1] + wall[3]) / 2.
            preferred = math.degrees(math.atan2(dy, dx))
        candidates = []
        span = max(x1 - x0, y1 - y0)
        for offset in (0., 35., -35., 70., -70., 135., -135., 180.):
            angle = math.radians(preferred + offset)
            for distance in (max(450., span * .55), max(600., span * .8)):
                location = unreal.Vector(center.x + math.cos(angle) * distance,
                                         center.y + math.sin(angle) * distance, floor_z + 160.)
                # Two crossing eye-level rays reject camera points embedded in a wall.
                embedded = not all(_trace_clear(location + unreal.Vector(*delta), location, 0.)
                                   for delta in ((20., 0., 0.), (0., 20., 0.)))
                visible = 0 if embedded else sum(_trace_clear(location, target) for target in targets)
                candidates.append((visible, not embedded, -abs(offset), -distance, location))
        visible, free, _, _, location = max(candidates, key=lambda candidate: candidate[:4])
        assert free, view["code"] + "/" + view["id"] + " has no clear camera candidate"
        aim = unreal.Vector(center.x, center.y, floor_z + (80. if view["kind"] == "furnishing" else 220.))
        rotation = unreal.MathLibrary.find_look_at_rotation(location, aim)
        result = {"location": _vec(location), "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
                  "projection": "perspective", "fov_degrees": 90., "target": _vec(aim),
                  "line_of_sight_samples_clear": visible, "line_of_sight_samples": len(targets),
                  "framing": "Representative static viewpoint; sample LOS does not prove full object visibility"}
    STATE["cameras"][key] = result
    return result


def _spawn_torch(location, rotation):
    source = STATE["flashlight"]
    transform = unreal.Transform(location=location, rotation=rotation)
    # Use the same camera-relative offset as the existing player component.
    position = unreal.MathLibrary.transform_location(transform, source.get_editor_property("relative_location"))
    torch = ACTORS.spawn_actor_from_class(unreal.SpotLight, position, rotation)
    assert torch, "Could not spawn temporary flashlight"
    STATE["temporary"].append(torch)
    light = torch.spot_light_component
    for key in ("attenuation_radius", "inner_cone_angle", "outer_cone_angle", "light_color",
                "indirect_lighting_intensity", "volumetric_scattering_intensity", "use_temperature",
                "temperature", "cast_shadows", "use_inverse_squared_falloff", "specular_scale",
                "source_radius", "soft_source_radius", "source_length"):
        light.set_editor_property(key, source.get_editor_property(key))
    light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    light.set_editor_property("intensity", 12.)
    light.set_visibility(True)
    return {"intensity_cd": 12., "source": source.get_path_name(), "position_cm": _vec(position),
            "radius_cm": float(light.attenuation_radius), "inner_half_angle": float(light.inner_cone_angle),
            "outer_half_angle": float(light.outer_cone_angle)}


def _hide_roof_in_capture(capture, code):
    floor_z = 10. if code == "M02" else 0.
    hidden = []
    for actor in ACTORS.get_all_level_actors():
        if actor in STATE["temporary"]:
            continue
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            if not component.static_mesh:
                continue
            b = _bounds(component)
            extent = [high - low for low, high in zip(*b)]
            name = component.static_mesh.get_name().lower()
            roof = any(word in name for word in ("roof", "ceiling", "truss"))
            horizontal_ceiling = min(extent[:2]) > 700. and extent[2] < 300.
            # M03 reuses its original floor tiles as small ceiling panels. Their
            # names omit "roof" and each panel is smaller than the broad-plate
            # test above. Identify only these two original assets at the saved
            # 800cm ceiling layers; floor instances and vertical panels remain.
            tile_package = component.static_mesh.get_path_name().split(".")[0]
            gallery_upper_facade = (code == "M03" and tile_package == "/Game/Assets/Environment/M03Gallery/SM_GalleryUpperFacade" and b[0][2] >= floor_z + 800. - .01)
            gallery_tile_ceiling = (
                code == "M03" and tile_package in (
                    "/Game/Assets/MapAssets/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_stone_tile_1_1",
                    "/Game/Assets/MapAssets/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_stone_tile_2_1")
                and abs(b[0][2] - floor_z - 800.) <= 8.
                and extent[2] <= 4. and min(extent[:2]) > 50.)
            if (b[0][2] >= floor_z + 690. and (roof or horizontal_ceiling)) or gallery_tile_ceiling or gallery_upper_facade:
                capture.hide_component(component)
                hidden.append(component.get_path_name())
    return hidden


def _clean():
    for actor in reversed(STATE["temporary"]):
        try:
            if actor and not ACTORS.destroy_actor(actor):
                REPORT["cleanup_errors"].append("Could not destroy " + actor.get_path_name())
        except Exception:
            REPORT["cleanup_errors"].append(traceback.format_exc())
    STATE["temporary"] = []
    if STATE["rt"]:
        unreal.RenderingLibrary.release_render_target2d(STATE["rt"])
        STATE["rt"] = None


def _check_scene():
    code = STATE["map"]
    if code:
        same = _scene_snapshot() == STATE["scene_before"][code]
        REPORT["maps"][code]["actor_geometry_and_materials_unchanged"] = same
        assert same, code + " authored scene changed during capture"


def _finish(error=None):
    if STATE["finished"]:
        return
    STATE["finished"] = True
    if error:
        REPORT["errors"].append(error)
    try:
        _clean()
        _check_scene()
    except Exception:
        REPORT["errors"].append(traceback.format_exc())
    finally:
        if STATE["callback"]:
            unreal.unregister_slate_post_tick_callback(STATE["callback"])
        for code, name in MAPS.items():
            row = REPORT["maps"].setdefault(code, {})
            path = ROOT / "Content/Maps" / (name + ".umap")
            row["sha256_after"] = _sha(path) if path.is_file() else "missing"
            row["package_unchanged"] = row.get("sha256_before") == row["sha256_after"]
        REPORT["saved_maps_unchanged"] = all(row["package_unchanged"] for row in REPORT["maps"].values())
        if not REPORT["saved_maps_unchanged"]:
            REPORT["errors"].append("A saved map hash changed")
        REPORT["status"] = "FAIL" if REPORT["errors"] or REPORT["cleanup_errors"] else "PASS"
        REPORT["captured_count"] = len(REPORT["captures"])
        REPORT["expected_count"] = len(STATE.get("views", []))
        if REPORT["captured_count"] != REPORT["expected_count"]:
            REPORT["status"] = "FAIL"
        OUT.mkdir(parents=True, exist_ok=True)
        name = "captures_" + STATE.get("mode", "error") + "_" + STATE.get("selection", "unknown") + ".json"
        (OUT / name).write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
        unreal.log("HEIST_APPROVED_EXHIBITION_CAPTURE_" + REPORT["status"])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        if STATE.get("quit", False):
            # Discard temporary actor dirtiness; never save the reviewed map.
            unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
            unreal.SystemLibrary.quit_editor()


def _tick(delta_seconds):
    if STATE["busy"] or STATE["finished"]:
        return
    STATE["busy"] = True
    try:
        if STATE["index"] == len(STATE["views"]):
            _finish()
            return
        view = STATE["views"][STATE["index"]]
        if STATE["phase"] == "load":
            if STATE["map"] != view["code"]:
                _clean()
                _check_scene()
                world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/" + MAPS[view["code"]])
                assert world, "Map load failed: " + view["code"]
                STATE["map"] = view["code"]
                STATE["scene_before"][view["code"]] = _scene_snapshot()
                STATE.update(phase="map_settle", stamp=time.monotonic())
                return
            STATE["phase"] = "prepare"
        if STATE["phase"] == "map_settle":
            if time.monotonic() - STATE["stamp"] < STATE["settle"]:
                return
            STATE["phase"] = "prepare"
        if STATE["phase"] == "prepare":
            camera = _camera(view)
            location = unreal.Vector(*camera["location"])
            rotation = unreal.Rotator(pitch=camera["rotation"][0], yaw=camera["rotation"][1], roll=camera["rotation"][2])
            actor = ACTORS.spawn_actor_from_class(unreal.SceneCapture2D, location, rotation)
            assert actor, "Could not spawn temporary SceneCapture"
            STATE["temporary"].append(actor)
            component = actor.capture_component2d
            world = EDITOR.get_editor_world()
            rt = unreal.RenderingLibrary.create_render_target2d(world, 1600, 900, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
            assert rt, "Could not create transient render target"
            STATE["rt"] = rt
            component.set_editor_properties({"texture_target": rt,
                "capture_source": unreal.SceneCaptureSource.SCS_BASE_COLOR if view["mode"] == "base_color" else unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,
                "capture_every_frame": True, "always_persist_rendering_state": True, "fov_angle": 90.})
            hidden = []
            flashlight = None
            if view["kind"] == "overview":
                component.set_editor_property("projection_type", unreal.CameraProjectionMode.ORTHOGRAPHIC)
                component.set_editor_property("ortho_width", camera["ortho_width_cm"])
                hidden = _hide_roof_in_capture(component, view["code"])
            else:
                EDITOR.set_level_viewport_camera_info(location, rotation)
                if view["mode"] == "night":
                    flashlight = _spawn_torch(location, rotation)
            if view["mode"] == "night":
                pp = component.get_editor_property("post_process_settings")
                pp.set_editor_property("override_dynamic_global_illumination_method", True)
                pp.set_editor_property("dynamic_global_illumination_method", unreal.DynamicGlobalIlluminationMethod.LUMEN)
                pp.set_editor_property("override_reflection_method", True)
                pp.set_editor_property("reflection_method", unreal.ReflectionMethod.LUMEN)
                component.set_editor_property("post_process_settings", pp)
            component.capture_scene()
            STATE.update(phase="export", stamp=time.monotonic(), capture=component,
                         metadata={"map": view["code"], "view": view["id"], "kind": view["kind"],
                                   "mode": view["mode"], "camera": camera, "flashlight": flashlight,
                                   "hidden_roof_components_in_capture_only": hidden})
            return
        if STATE["phase"] == "export":
            if time.monotonic() - STATE["stamp"] < STATE["settle"]:
                return
            path = OUT / view["mode"] / (view["code"] + "_" + view["id"] + ".png")
            path.parent.mkdir(parents=True, exist_ok=True)
            STATE["capture"].capture_scene()
            unreal.RenderingLibrary.export_render_target(EDITOR.get_editor_world(), STATE["rt"], str(path.parent), path.name)
            assert path.is_file() and path.stat().st_size > 100, "Screenshot export missing: " + str(path)
            with path.open("rb") as stream:
                assert stream.read(8) == b"\x89PNG\r\n\x1a\n", "Export is not PNG: " + str(path)
            REPORT["captures"].append(dict(STATE["metadata"], path=str(path), sha256=_sha(path), bytes=path.stat().st_size))
            unreal.log("HEIST_APPROVED_CAPTURE " + view["mode"] + " " + view["code"] + "/" + view["id"])
            _clean()
            STATE.update(index=STATE["index"] + 1, phase="load", stamp=time.monotonic())
    except Exception:
        _finish(traceback.format_exc())
    finally:
        STATE["busy"] = False


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    command_line = unreal.SystemLibrary.get_command_line().lower()
    STATE["quit"] = os.environ.get("MH_APPROVED_CAPTURE_QUIT", "1" if "-unattended" in command_line else "0") == "1"
    try:
        mode = os.environ.get("MH_APPROVED_CAPTURE_MODE", "both")
        selection = os.environ.get("MH_APPROVED_CAPTURE_SET", "all")
        assert mode in ("base_color", "night", "both"), "Invalid capture mode"
        assert selection in ("all", "minimum", "overviews"), "Invalid capture set"
        assert "-nullrhi" not in command_line, "Scene capture requires a rendered RHI"
        STATE.update(mode=mode, selection=selection,
                     settle=max(2., float(os.environ.get("MH_APPROVED_CAPTURE_SETTLE_SECONDS", "6"))))
        plan = json.loads(PLAN_PATH.read_text(encoding="utf-8"))
        assert set(MAPS).issubset(plan["maps"]), "Expected all three approved maps"
        REPORT.update(plan_path=str(PLAN_PATH), plan_sha256=_sha(PLAN_PATH), mode=mode, selection=selection,
                      render_target={"width": 1600, "height": 900, "format": "RGBA8_SRGB"})
        for code, name in MAPS.items():
            REPORT["maps"][code] = {"path": "/Game/Maps/" + name,
                                     "sha256_before": _sha(ROOT / "Content/Maps" / (name + ".umap"))}
        modes = ("base_color", "night") if mode == "both" else (mode,)
        STATE["views"] = [dict(view, mode=selected_mode) for view in _plan_views(plan, selection) for selected_mode in modes]
        STATE["flashlight"] = _flashlight_template() if "night" in modes and selection != "overviews" else None
        unreal.EditorPythonScripting.set_keep_python_script_alive(True)
        STATE["callback"] = unreal.register_slate_post_tick_callback(_tick)
        unreal.log("HEIST_APPROVED_EXHIBITION_CAPTURE_BEGIN views=" + str(len(STATE["views"])))
    except Exception:
        _finish(traceback.format_exc())


if __name__ == "__main__":
    main()
