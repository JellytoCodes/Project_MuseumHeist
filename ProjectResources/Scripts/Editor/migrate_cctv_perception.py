"""Compile the shared CCTV shell and verify placed cameras inherit Sight without Box sensors."""
import json
from pathlib import Path
import unreal

bp = unreal.load_asset('/Game/Blueprints/World/Actors/Security/BP_SecurityCamera')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = []
for name in ['SandBoxMap', 'M01_ClassicalPrototype', 'M02_MoonlitPrototype', 'M03_GlasshousePrototype']:
    assert levels.load_level('/Game/Maps/' + name)
    cameras = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.HeistSecurityCameraActor)]
    assert cameras, name
    entries = []
    for camera in cameras:
        boxes = camera.get_components_by_class(unreal.BoxComponent)
        perception = camera.get_component_by_class(unreal.AIPerceptionComponent)
        assert not boxes, (name, camera.get_name(), boxes)
        assert perception, (name, camera.get_name())
        light = camera.get_component_by_class(unreal.SpotLightComponent)
        assert light, (name, camera.get_name(), 'missing sight light')
        assert abs(light.get_editor_property('attenuation_radius') - camera.get_editor_property('detection_range')) < .1
        assert abs(light.get_editor_property('outer_cone_angle') - camera.get_editor_property('detection_half_angle_degrees')) < .1
        entries.append(dict(actor=camera.get_actor_label(), box_count=len(boxes),
                            perception=perception.get_name(),
                            range=camera.get_editor_property('detection_range'),
                            half_angle=camera.get_editor_property('detection_half_angle_degrees'),
                            light_radius=light.get_editor_property('attenuation_radius'),
                            light_half_angle=light.get_editor_property('outer_cone_angle')))
    report.append(dict(map=name, cameras=entries))
# Inherited components migrate on load. Map placement and packages are not saved here.
out = Path(unreal.Paths.project_saved_dir()) / 'Logs/CCTVPerception-Assets.json'
out.write_text(json.dumps(dict(passed=True, maps=report), indent=2), encoding='utf-8')
unreal.log('CCTV_PERCEPTION_ASSETS_PASS ' + str(out))
