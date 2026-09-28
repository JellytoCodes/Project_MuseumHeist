"""Tune existing M03 fixtures and the shared flashlight; preserve all other assets.

Run in a saved, closed-editor workflow. The source map hash in the reviewed
profile protects subsequent user edits. No HUD, placement or CCTV edits.
"""
import hashlib
import json
import re
import time
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
PROFILE = json.loads((ROOT / 'ProjectResources/SourceArt/Gallery/M03/M03NightLighting.json').read_text())['fixture_flashlight_tuning']
MAP = '/Game/Maps/M03_GlasshousePrototype'
BP = '/Game/Blueprints/Player/BP_HeistPlayerCharacter'
MAP_FILE = ROOT / 'Content/Maps/M03_GlasshousePrototype.umap'
BP_FILE = ROOT / 'Content/Blueprints/Player/BP_HeistPlayerCharacter.uasset'
OUT = ROOT / 'Saved/Automation/M03FlashlightTuning'
OUT.mkdir(parents=True, exist_ok=True)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
STATE = {'phase': 0, 'stamp': 0, 'busy': False}
RESULT = {'errors': [], 'saved': False}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def assets():
    return {str(p): sha(p) for p in (ROOT / 'Content').rglob('*') if p.suffix in ('.uasset', '.umap')}

def snapshot():
    result = {}
    for a in ACTORS.get_all_level_actors():
        row = {'transform': str(a.get_actor_transform()), 'class': a.get_class().get_path_name()}
        row['lights'] = {c.get_name(): str(c.get_editor_property('intensity')) for c in a.get_components_by_class(unreal.LightComponentBase)}
        if isinstance(a, unreal.PostProcessVolume):
            row['settings'] = re.sub(r'0x[0-9A-Fa-f]+', 'PTR', str(a.settings))
        if a.get_class().get_name() == 'BP_SecurityCamera_C':
            row['cctv'] = [{p: str(c.get_editor_property(p)) for p in
                            ('intensity', 'attenuation_radius', 'inner_cone_angle', 'outer_cone_angle', 'light_color')}
                           for c in a.get_components_by_class(unreal.SpotLightComponent)]
        result[a.get_name()] = row
    return result

def flashlight_template(bp):
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    lights = [lib.get_object(lib.get_data(h)) for h in sub.k2_gather_subobject_data_for_blueprint(bp)]
    lights = [c for c in lights if isinstance(c, unreal.SpotLightComponent) and c.component_has_tag('Flashlight')]
    assert len(lights) == 1
    return lights[0]

def light_values(c):
    keys = tuple(PROFILE['flashlight']) + ('attenuation_radius', 'visible')
    return {k: c.get_editor_property(k) for k in keys}

def finish(error=None):
    if error:
        RESULT['errors'].append(error)
    (OUT / 'applied.json').write_text(json.dumps(RESULT, indent=2))
    unreal.unregister_slate_post_tick_callback(STATE['callback'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    if STATE['busy']:
        return
    STATE['busy'] = True
    try:
        if STATE['phase'] == 0:
            assert sha(MAP_FILE) == PROFILE['source_map_sha256'], 'Map changed; review current map first'
            STATE['assets'] = assets()
            (OUT / 'asset_hashes_before.json').write_text(json.dumps(STATE['assets']))
            STATE['world'] = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
            STATE.update(phase=1, stamp=time.monotonic())
            return
        if time.monotonic() - STATE['stamp'] < 4:
            return
        if STATE['phase'] == 1:
            STATE['before'] = snapshot()
            (OUT / 'scene_before.json').write_text(json.dumps(STATE['before'], indent=2))
            STATE['fixtures'] = []
            for a in ACTORS.get_all_level_actors():
                if not a.get_class().get_name().startswith('BP_HeistLight_'):
                    continue
                lights = a.get_components_by_class(unreal.SpotLightComponent)
                assert len(lights) == 1
                c = lights[0]
                assert c.intensity_units == unreal.LightUnits.CANDELAS
                a.modify()
                c.modify()
                c.set_editor_property('intensity', PROFILE['fixture_intensity_cd'])
                STATE['fixtures'].append(a.get_name())
            assert len(STATE['fixtures']) == 92
            bp = unreal.load_asset(BP)
            c = flashlight_template(bp)
            RESULT['flashlight_before'] = light_values(c)
            assert c.intensity_units == unreal.LightUnits.CANDELAS
            bp.modify()
            c.modify()
            c.set_editor_properties(PROFILE['flashlight'])
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
            STATE.update(phase=2, stamp=time.monotonic())
            return
        if STATE['phase'] == 2:
            assert unreal.EditorLoadingAndSavingUtils.save_map(STATE['world'], MAP)
            RESULT['saved'] = True
            STATE['world'] = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
            STATE.update(phase=3, stamp=time.monotonic())
            return
        if STATE['phase'] == 3:
            expected = STATE['before']
            for name in STATE['fixtures']:
                assert len(expected[name]['lights']) == 1
                expected[name]['lights'] = {k: str(float(PROFILE['fixture_intensity_cd'])) for k in expected[name]['lights']}
            after = snapshot()
            (OUT / 'scene_after.json').write_text(json.dumps(after, indent=2))
            assert after == expected, 'Unexpected scene transform, PP, light or CCTV changes'
            player = ACTORS.spawn_actor_from_class(unreal.EditorAssetLibrary.load_blueprint_class(BP), unreal.Vector(0, 0, -10000))
            c = next(c for c in player.get_components_by_class(unreal.SpotLightComponent) if c.component_has_tag('Flashlight'))
            RESULT['flashlight_after'] = light_values(c)
            for key, value in PROFILE['flashlight'].items():
                assert abs(c.get_editor_property(key) - value) < .000001, key
            assert c.intensity_units == unreal.LightUnits.CANDELAS and c.get_attach_parent().get_name() == 'FirstPersonCamera'
            assert c.attenuation_radius == RESULT['flashlight_before']['attenuation_radius'] and not c.visible
            ACTORS.destroy_actor(player)
            current = assets()
            assert set(current) == set(STATE['assets']), 'Asset set changed'
            changed = [p for p, h in current.items() if STATE['assets'][p] != h]
            assert set(changed) == {str(MAP_FILE), str(BP_FILE)}, changed
            RESULT.update(saved=True, fixtures_verified=92, unchanged_scene_except_fixture_intensity=True,
                          changed_assets=changed, map_sha256=sha(MAP_FILE), blueprint_sha256=sha(BP_FILE))
            finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        STATE['busy'] = False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
STATE['callback'] = unreal.register_slate_post_tick_callback(tick)
