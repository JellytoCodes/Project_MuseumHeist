"""Apply approved slower CCTV detection and reduced authored ranges, idempotently."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
plans = json.loads((root/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_text(encoding='utf-8'))['maps']
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
balance = unreal.load_asset('/Game/Data/DataAsset/DA_GameBalance')
old_time = balance.get_editor_property('security_camera_detection_build_up_seconds')
balance.set_editor_property('security_camera_detection_build_up_seconds', 2.025)
assert unreal.EditorAssetLibrary.save_loaded_asset(balance)
report = dict(old_build_up=old_time, new_build_up=2.025, maps=[])
names = dict(M01='M01_ClassicalPrototype', M02='M02_MoonlitPrototype', M03='M03_GlasshousePrototype')
targets = [(names[p['id']], {'LDV2_{}_CCTV_{:02}'.format(p['id'], i+1): c['range']*100
                            for i, c in enumerate(p['cameras'])}) for p in plans]
targets.append(('SandBoxMap', {'W8_TEST_SecurityCamera': 1000.0}))
for name, ranges in targets:
    assert levels.load_level('/Game/Maps/'+name)
    entries = []
    for a in actors.get_all_level_actors():
        if not isinstance(a, unreal.HeistSecurityCameraActor):
            continue
        label = a.get_actor_label()
        assert label in ranges, (name, label)
        old_range = a.get_editor_property('detection_range')
        a.set_editor_property('detection_range', ranges[label])
        light = a.get_component_by_class(unreal.SpotLightComponent)
        assert abs(light.get_editor_property('attenuation_radius')-ranges[label]) < .1
        entries.append(dict(actor=label, old_range=old_range, new_range=ranges[label],
                            half_angle=a.get_editor_property('detection_half_angle_degrees')))
    assert len(entries) == len(ranges), name
    assert levels.save_current_level()
    report['maps'].append(dict(map=name, cameras=entries))
report['passed'] = True
(root/'Saved/Logs/CCTVTuning-Assets.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('CCTV_TUNING_PASS')
