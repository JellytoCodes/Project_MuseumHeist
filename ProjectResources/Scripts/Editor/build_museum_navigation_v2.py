import time

import unreal


MAPS = (
    ("M01", "/Game/Maps/M01_ClassicalPrototype"),
    ("M02", "/Game/Maps/M02_MoonlitPrototype"),
    ("M03", "/Game/Maps/M03_GlasshousePrototype"),
)


level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {
    "map_index": 0,
    "phase": "load",
    "phase_started": time.time(),
    "callback": None,
}


def quit_editor(message, is_error=False):
    if is_error:
        unreal.log_error(message)
    else:
        unreal.log_warning(message)
    if state["callback"] is not None:
        unreal.unregister_slate_post_tick_callback(state["callback"])
        state["callback"] = None
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def tick(delta_seconds):
    del delta_seconds
    try:
        if state["map_index"] >= len(MAPS):
            quit_editor("MH_LEVEL_NAV_BUILD_ALL_DONE=3")
            return

        code, map_path = MAPS[state["map_index"]]
        elapsed = time.time() - state["phase_started"]

        if state["phase"] == "load":
            state["phase"] = "loading"
            world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
            if not world:
                quit_editor("MH_LEVEL_NAV_BUILD_LOAD_FAILED=" + map_path, True)
                return
            state["phase"] = "wait_for_async_loading"
            state["phase_started"] = time.time()
            return

        if state["phase"] == "wait_for_async_loading" and elapsed >= 8.0:
            state["phase"] = "building"
            world = editor_subsystem.get_editor_world()
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            unreal.SystemLibrary.execute_console_command(world, "BUILDPATHS")
            if not level_subsystem.save_current_level():
                quit_editor("MH_LEVEL_NAV_BUILD_SAVE_FAILED=" + map_path, True)
                return
            unreal.log_warning("MH_LEVEL_NAV_BUILD_SAVED={} map={}".format(code, map_path))
            state["map_index"] += 1
            state["phase"] = "load"
            state["phase_started"] = time.time()
    except Exception as exc:
        quit_editor("MH_LEVEL_NAV_BUILD_EXCEPTION={}".format(exc), True)


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state["callback"] = unreal.register_slate_post_tick_callback(tick)
unreal.log_warning("MH_LEVEL_NAV_BUILD_STARTED=3")
