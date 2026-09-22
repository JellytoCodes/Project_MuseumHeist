"""Build expanded review maps; promote only after an explicit validation report.

Use -MuseumExpansionStage=build (default) or promote in a dedicated Editor.
Binary maps and DataTables are saved only through Unreal Editor APIs.
"""
import json
import hashlib
import runpy
import time
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT/'Saved/Automation/ExplorationExpansion'
OUT.mkdir(parents=True, exist_ok=True)
MAPS = ['M01_ClassicalPrototype', 'M02_MoonlitPrototype', 'M03_GlasshousePrototype']


def run():
    _, _, parameters = unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())
    parameters = {str(k).lower(): str(v) for k,v in parameters.items()}
    output = Path(parameters.get('museumexpansionreportdirectory', str(OUT)))
    stage = parameters.get('museumexpansionstage', 'build')
    if stage == 'build':
        table = unreal.load_asset('/Game/Data/DataTable/DT_MapPresentation')
        assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(table, str(ROOT/'ProjectResources/DataTableImports/DT_MapPresentation.json'))
        assert unreal.EditorAssetLibrary.save_loaded_asset(table)
        builder = runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/build_approved_museum_layout.py'))
        builder['rebuild_saved_navigation'](builder['build']())
    elif stage == 'promote':
        report = json.loads((output/'review-verification.json').read_text(encoding='utf-8'))
        assert report['status'] == 'PASS', 'Review geometry/navigation must pass before promotion'
        assert len(report['maps']) == 3
        assert report['layout_sha256'] == hashlib.sha256((ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_bytes()).hexdigest(), 'Layout changed after validation'
        for name in MAPS:
            path = ROOT/'Content/Maps/Review'/(name+'_LayoutReview.umap')
            assert hashlib.sha256(path.read_bytes()).hexdigest() == report['map_sha256_after'][name[:3]], 'Review map changed after validation'
        for name in MAPS:
            world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/Review/'+name+'_LayoutReview')
            for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
                if isinstance(actor, unreal.HeistGuardCharacter):
                    movement = actor.get_component_by_class(unreal.CharacterMovementComponent)
                    unreal.log_warning('MH_GUARD_AVOIDANCE='+actor.get_actor_label()+' enabled='+str(movement.get_editor_property('use_rvo_avoidance')))
            assert world and unreal.EditorLoadingAndSavingUtils.save_map(world, '/Game/Maps/'+name)
        unreal.log_warning('MH_EXPLORATION_PROMOTED=3')
        runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/export_floor_plan_geometry.py'))
        unreal.SystemLibrary.quit_editor()
    else:
        raise ValueError('Unknown stage: '+stage)


def run_when_ready(_):
    if time.monotonic() - STARTED < 3:
        return
    unreal.unregister_slate_post_tick_callback(CALLBACK)
    try:
        run()
    except Exception:
        import traceback
        unreal.log_error(traceback.format_exc())
        unreal.SystemLibrary.quit_editor()


if __name__ == '__main__':
    STARTED = time.monotonic()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    CALLBACK = unreal.register_slate_post_tick_callback(run_when_ready)
