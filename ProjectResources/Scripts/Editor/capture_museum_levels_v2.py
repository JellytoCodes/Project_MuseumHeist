import os
import time

import unreal


CAPTURES = (
    (
        "M01",
        "/Game/Maps/M01_ClassicalPrototype",
        (
            ("Overview", (0, 0, 18000), (-90, 0, 0)),
            ("Entry", (-6200, -1400, 170), (-1, 15, 0)),
            ("Feature", (-2200, -1800, 170), (-2, 39, 0)),
        ),
    ),
    (
        "M02",
        "/Game/Maps/M02_MoonlitPrototype",
        (
            ("Overview", (0, 0, 18000), (-90, 0, 0)),
            ("Entry", (-5400, -2500, 170), (-1, 25, 0)),
            ("Feature", (-2600, -2200, 170), (-2, 32, 0)),
        ),
    ),
    (
        "M03",
        "/Game/Maps/M03_GlasshousePrototype",
        (
            ("Overview", (0, 0, 19000), (-90, 0, 0)),
            ("Entry", (-7000, 0, 170), (-1, 0, 0)),
            ("Feature", (-3200, -650, 170), (-2, 12, 0)),
        ),
    ),
)


editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level_editor_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
output_directory = os.path.join(unreal.Paths.project_saved_dir(), "Screenshots", "MuseumLevels")
os.makedirs(output_directory, exist_ok=True)
capture_count = sum(len(entry[2]) for entry in CAPTURES)

state = {
    "map_index": 0,
    "view_index": 0,
    "phase": "load",
    "phase_started": time.time(),
    "callback": None,
}


def finish(message, is_error=False):
    if is_error:
        unreal.log_error(message)
    else:
        unreal.log_warning(message)
    if state["callback"] is not None:
        unreal.unregister_slate_post_tick_callback(state["callback"])
        state["callback"] = None
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def set_roof_hidden(hidden):
    for actor in actor_subsystem.get_all_level_actors():
        label = actor.get_actor_label()
        if "Ceiling_" not in label and "GlassRoof_" not in label and "RoofRib_" not in label and "RoofRail_" not in label:
            continue
        actor.set_is_temporarily_hidden_in_editor(bool(hidden))


def tick(delta_seconds):
    del delta_seconds
    try:
        if state["map_index"] >= len(CAPTURES):
            finish("MH_LEVEL_CAPTURE_ALL_DONE={} output={}".format(capture_count, output_directory.replace("\\", "/")))
            return

        code, map_path, views = CAPTURES[state["map_index"]]
        elapsed = time.time() - state["phase_started"]

        if state["phase"] == "load":
            world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
            if not world:
                finish("MH_LEVEL_CAPTURE_LOAD_FAILED=" + map_path, True)
                return
            state["view_index"] = 0
            state["phase"] = "wait_for_load"
            state["phase_started"] = time.time()
            return

        if state["phase"] == "wait_for_load" and elapsed >= 6.0:
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            level_editor_subsystem.editor_set_game_view(True)
            state["phase"] = "position"
            state["phase_started"] = time.time()
            return

        if state["phase"] == "position":
            view_name, location, rotation = views[state["view_index"]]
            set_roof_hidden(view_name == "Overview")
            editor_subsystem.set_level_viewport_camera_info(
                unreal.Vector(float(location[0]), float(location[1]), float(location[2])),
                unreal.Rotator(
                    pitch=float(rotation[0]),
                    yaw=float(rotation[1]),
                    roll=float(rotation[2]),
                ),
            )
            state["phase"] = "capture"
            state["phase_started"] = time.time()
            return

        if state["phase"] == "capture" and elapsed >= 1.5:
            world = editor_subsystem.get_editor_world()
            view_name = views[state["view_index"]][0]
            filename = os.path.join(output_directory, "{}_{}.png".format(code, view_name)).replace("\\", "/")
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1600x900 filename="{}"'.format(filename))
            unreal.log_warning("MH_LEVEL_CAPTURE_REQUESTED={} view={} file={}".format(code, view_name, filename))
            state["phase"] = "wait_for_capture"
            state["phase_started"] = time.time()
            return

        if state["phase"] == "wait_for_capture" and elapsed >= 4.0:
            state["view_index"] += 1
            if state["view_index"] >= len(views):
                state["map_index"] += 1
                state["phase"] = "load"
            else:
                state["phase"] = "position"
            state["phase_started"] = time.time()
    except Exception as exc:
        finish("MH_LEVEL_CAPTURE_EXCEPTION={}".format(exc), True)


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state["callback"] = unreal.register_slate_post_tick_callback(tick)
unreal.log_warning("MH_LEVEL_CAPTURE_STARTED={}".format(capture_count))
