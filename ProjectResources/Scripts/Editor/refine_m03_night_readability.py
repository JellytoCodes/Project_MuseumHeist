"""Apply the explicit CCTV/ambient correction to the reviewed M03 night map only.

This does not run the earlier 0.5 light multiplier again. Asset and placement
snapshots are transient QA output; the existing Engine cubemap is referenced.
"""
import hashlib
import json
import time
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
PROFILES = json.loads((ROOT / 'ProjectResources/SourceArt/Gallery/M03/M03NightLighting.json').read_text())
PROFILE = dict(PROFILES['readability_correction'])
MAP = '/Game/Maps/M03_GlasshousePrototype'
MAP_FILE = ROOT / 'Content/Maps/M03_GlasshousePrototype.umap'
DIMMING = hashlib.sha256(MAP_FILE.read_bytes()).hexdigest() == PROFILES['ambient_dimming']['source_map_sha256']
LOW_LIGHT = hashlib.sha256(MAP_FILE.read_bytes()).hexdigest() == PROFILES['low_light_tuning']['source_map_sha256']
WALL_LIGHT = hashlib.sha256(MAP_FILE.read_bytes()).hexdigest() == PROFILES['wall_lighting']['source_map_sha256']
if DIMMING:
    PROFILE.update(PROFILES['ambient_dimming'])
elif LOW_LIGHT:
    PROFILE.update(PROFILES['low_light_tuning'])
elif WALL_LIGHT:
    PROFILE.update(PROFILES['wall_lighting'])
OUT_DIR = ROOT / 'Saved/Automation' / ('M03WallLighting/Applied' if WALL_LIGHT else 'M03LowLight' if LOW_LIGHT else 'M03AmbientDimming' if DIMMING else 'M03CCTVAmbient')
if hashlib.sha256(MAP_FILE.read_bytes()).hexdigest() != PROFILE['source_map_sha256']:
    OUT_DIR = ROOT / 'Saved/Automation/M03NightUnmatched'
OUT_DIR.mkdir(parents=True, exist_ok=True)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
STATE = {'phase': 0, 'stamp': 0, 'busy': False}
RESULT = {'errors': [], 'saved': False}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def placements():
    return {a.get_name(): str(a.get_actor_transform()) for a in ACTORS.get_all_level_actors()}

def lights():
    return {(a.get_name(), c.get_name()): (a, c) for a in ACTORS.get_all_level_actors()
            for c in a.get_components_by_class(unreal.LightComponentBase)}

def values():
    return {key: (c.intensity, str(c.get_editor_property('intensity_units'))
                 if isinstance(c, unreal.LocalLightComponent) else None)
            for key, (a, c) in lights().items()}

def expected_light(key, before):
    if key in STATE['cctv']:
        return (PROFILE['cctv_intensity_cd'], str(unreal.LightUnits.CANDELAS))
    if key in STATE['fixtures']:
        return (PROFILE['fixture_intensity_cd'], str(unreal.LightUnits.CANDELAS))
    return before

def finish(error=None):
    if error:
        RESULT['errors'].append(error)
    (OUT_DIR / 'applied.json').write_text(json.dumps(RESULT, indent=2))
    unreal.unregister_slate_post_tick_callback(STATE['callback'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    if STATE['busy']:
        return
    STATE['busy'] = True
    try:
        if STATE['phase'] == 0:
            assert sha(MAP_FILE) == PROFILE['source_map_sha256'], 'Map changed; audit before applying'
            STATE['protected'] = {str(p): sha(p) for p in (ROOT / 'Content').rglob('*')
                                  if p.suffix in ('.uasset', '.umap') and p != MAP_FILE}
            STATE['world'] = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
            STATE.update(phase=1, stamp=time.monotonic())
            return
        if time.monotonic() - STATE['stamp'] < 6:
            return
        if STATE['phase'] == 1:
            STATE['poses'] = placements()
            STATE['before'] = values()
            assert len(STATE['before']) == 102
            STATE['cctv'] = []
            STATE['fixtures'] = []
            for key, (actor, component) in lights().items():
                if WALL_LIGHT and actor.get_class().get_name().startswith('BP_HeistLight_'):
                    assert isinstance(component, unreal.SpotLightComponent)
                    STATE['fixtures'].append(key)
                    actor.modify()
                    component.modify()
                    component.set_editor_properties({'intensity_units': unreal.LightUnits.CANDELAS,
                                                     'intensity': PROFILE['fixture_intensity_cd']})
                    continue
                if actor.get_class().get_name() != 'BP_SecurityCamera_C':
                    continue
                assert isinstance(component, unreal.SpotLightComponent)
                STATE['cctv'].append(key)
                if DIMMING:
                    assert component.intensity == 20 and component.intensity_units == unreal.LightUnits.CANDELAS
                    continue
                if LOW_LIGHT:
                    assert component.intensity in (5, 20) and component.intensity_units == unreal.LightUnits.CANDELAS
                actor.modify()
                component.modify()
                # Batch properties: a Blueprint reconstruction between separate
                # writes can invalidate the component being edited.
                component.set_editor_properties({'intensity_units': unreal.LightUnits.CANDELAS,
                                                 'intensity': PROFILE['cctv_intensity_cd']})
            assert len(STATE['cctv']) == 8
            if WALL_LIGHT:
                assert len(STATE['fixtures']) == 92
            STATE['fixture_meshes'] = []
            if PROFILE.get('fixture_indirect_lighting') is False:
                for actor in ACTORS.get_all_level_actors():
                    if not actor.get_class().get_name().startswith('BP_HeistLight_'):
                        continue
                    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
                        if not component.static_mesh:
                            continue
                        actor.modify()
                        component.modify()
                        STATE['fixture_meshes'].append((actor.get_name(), component.get_name()))
                        component.set_editor_property('affect_dynamic_indirect_lighting', False)
                assert len(STATE['fixture_meshes']) == 92
            pp = next(a for a in ACTORS.get_all_level_actors() if a.get_name() == 'PostProcessVolume_0')
            pp.modify()
            settings = pp.settings
            if DIMMING:
                assert abs(settings.ambient_cubemap_intensity - PROFILES['readability_correction']['ambient_cubemap_intensity']) < .000001
            elif LOW_LIGHT:
                assert abs(settings.ambient_cubemap_intensity - PROFILES['ambient_dimming']['ambient_cubemap_intensity']) < .000001
            STATE['exposure'] = {k: settings.get_editor_property(k) for k in
                                ('auto_exposure_min_brightness', 'auto_exposure_max_brightness', 'auto_exposure_bias')}
            cube = unreal.load_asset(PROFILE['ambient_cubemap'])
            assert isinstance(cube, unreal.TextureCube)
            settings.set_editor_property('ambient_cubemap', cube)
            settings.set_editor_property('ambient_cubemap_intensity', PROFILE['ambient_cubemap_intensity'])
            settings.set_editor_property('override_ambient_cubemap_intensity', True)
            settings.set_editor_property('ambient_cubemap_tint', unreal.LinearColor(*PROFILE['ambient_cubemap_tint']))
            settings.set_editor_property('override_ambient_cubemap_tint', True)
            for key, value in PROFILE.get('lumen', {}).items():
                settings.set_editor_property(key, value)
                settings.set_editor_property('override_' + key, True)
            pp.set_editor_property('settings', settings)
            STATE.update(phase=2, stamp=time.monotonic())
            return
        if STATE['phase'] == 2:
            current = values()
            for key, before in STATE['before'].items():
                expected = expected_light(key, before)
                assert current[key] == expected, (key, current[key], expected)
            RESULT['saved'] = unreal.EditorLoadingAndSavingUtils.save_map(STATE['world'], MAP)
            assert RESULT['saved']
            STATE['world'] = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
            STATE.update(phase=3, stamp=time.monotonic())
            return
        if STATE['phase'] == 3:
            current = values()
            RESULT['lights'] = []
            assert set(current) == set(STATE['before'])
            for key, before in STATE['before'].items():
                expected = expected_light(key, before)
                assert current[key] == expected, (key, current[key], expected)
                RESULT['lights'].append({'actor': key[0], 'component': key[1], 'before': before, 'after': current[key]})
            RESULT['cctv_verified_count'] = len(STATE['cctv'])
            RESULT['cctv_intensity_cd'] = PROFILE['cctv_intensity_cd']
            RESULT['fixture_verified_count'] = len(STATE['fixtures'])
            actor_by_name = {a.get_name(): a for a in ACTORS.get_all_level_actors()}
            for actor_name, component_name in STATE['fixture_meshes']:
                component = next(c for c in actor_by_name[actor_name].get_components_by_class(unreal.StaticMeshComponent)
                                 if c.get_name() == component_name)
                assert component.affect_dynamic_indirect_lighting is False
            RESULT['fixture_meshes_indirect_disabled'] = len(STATE['fixture_meshes'])
            RESULT['other_lights_unchanged'] = len(current) - len(STATE['cctv']) - len(STATE['fixtures'])
            RESULT['placements_unchanged'] = placements() == STATE['poses']
            assert RESULT['placements_unchanged']
            pp = next(a for a in ACTORS.get_all_level_actors() if a.get_name() == 'PostProcessVolume_0')
            settings = pp.settings
            assert pp.unbound and pp.blend_weight == 1
            assert settings.ambient_cubemap.get_path_name().split('.')[0] == PROFILE['ambient_cubemap']
            assert abs(settings.ambient_cubemap_intensity - PROFILE['ambient_cubemap_intensity']) < .000001
            tint = settings.ambient_cubemap_tint
            assert all(abs(a-b) < .000001 for a,b in zip((tint.r,tint.g,tint.b,tint.a), PROFILE['ambient_cubemap_tint']))
            assert settings.override_ambient_cubemap_intensity and settings.override_ambient_cubemap_tint
            for key, value in PROFILE.get('lumen', {}).items():
                assert settings.get_editor_property('override_' + key)
                assert abs(settings.get_editor_property(key) - value) < .000001
            RESULT['exposure'] = {k: settings.get_editor_property(k) for k in STATE['exposure']}
            assert RESULT['exposure'] == STATE['exposure']
            RESULT['profile'] = PROFILE
            RESULT['protected_unchanged'] = all(Path(p).exists() and sha(Path(p)) == h for p,h in STATE['protected'].items())
            assert RESULT['protected_unchanged']
            current_files = {str(p) for p in (ROOT / 'Content').rglob('*') if p.suffix in ('.uasset', '.umap')}
            RESULT['asset_set_unchanged'] = current_files == set(STATE['protected']) | {str(MAP_FILE)}
            assert RESULT['asset_set_unchanged']
            RESULT['map_sha256'] = sha(MAP_FILE)
            finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        STATE['busy'] = False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
STATE['callback'] = unreal.register_slate_post_tick_callback(tick)
