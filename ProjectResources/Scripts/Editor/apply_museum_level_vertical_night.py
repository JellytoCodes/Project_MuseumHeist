import importlib
import os
import sys

import unreal


script_directory = os.path.dirname(os.path.abspath(__file__))
if script_directory not in sys.path:
    sys.path.insert(0, script_directory)

level_build = importlib.import_module("build_museum_levels_v2")

for level_code in level_build.selected_level_codes:
    level_builder = level_build.LevelBuilder(level_code, level_build.MAPS[level_code])
    level_builder.apply_existing_vertical_night()
    level_builder.save()
    unreal.SystemLibrary.execute_console_command(level_builder.world, "MAP CHECK")
    unreal.log_warning("MH_VERTICAL_NIGHT_MAPCHECK_REQUESTED=" + level_code)

unreal.log_warning(
    "MH_VERTICAL_NIGHT_ALL_DONE={}".format(len(level_build.selected_level_codes))
)
