"""Add one forward-facing guard light using the player flashlight's light settings.

Run in Unreal Editor with PIE stopped. Saves only the existing BP_Guard.
No map, mesh, animation, perception, or player asset is modified.
"""
import json
from pathlib import Path

import unreal


GUARD_PATH = '/Game/Blueprints/Guard/BP_Guard'
PLAYER_PATH = '/Game/Blueprints/Player/BP_HeistPlayerCharacter'
LIGHT_PROPERTIES = (
    'intensity_units', 'attenuation_radius', 'inner_cone_angle', 'outer_cone_angle',
    'use_inverse_squared_falloff', 'light_color', 'use_temperature', 'temperature',
    'cast_shadows', 'source_radius', 'soft_source_radius', 'source_length',
    'indirect_lighting_intensity', 'volumetric_scattering_intensity', 'specular_scale',
)


def apply():
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary

    def components(blueprint):
        return [(handle, library.get_object(library.get_data(handle)))
                for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint)]

    player = unreal.load_asset(PLAYER_PATH)
    guard = unreal.load_asset(GUARD_PATH)
    assert player and guard, 'Required character Blueprint missing'
    player_lights = [obj for _, obj in components(player)
                     if isinstance(obj, unreal.SpotLightComponent) and obj.component_has_tag('Flashlight')]
    assert len(player_lights) == 1, 'Expected one player flashlight'
    source = player_lights[0]
    assert source.get_editor_property('intensity_units') == unreal.LightUnits.CANDELAS

    objects = components(guard)
    capsule = next(handle for handle, obj in objects if isinstance(obj, unreal.CapsuleComponent))
    existing = [(handle, obj) for handle, obj in objects
                if isinstance(obj, unreal.SpotLightComponent) and obj.component_has_tag('GuardFlashlight')]
    assert len(existing) <= 1, 'Duplicate guard flashlights'
    guard.modify()
    if existing:
        handle, light = existing[0]
    else:
        handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=capsule, new_class=unreal.SpotLightComponent, blueprint_context=guard))
        assert library.is_handle_valid(handle), str(reason)
        subsystem.rename_subobject(handle, 'GuardFlashlight')
        light = library.get_object(library.get_data(handle))

    for name in LIGHT_PROPERTIES:
        light.set_editor_property(name, source.get_editor_property(name))
    light.set_editor_property('component_tags', ['GuardFlashlight'])
    light.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    # Capsule-relative upper torso; clear of the body, stable through walk animation.
    # Guard perception uses ActorRotation too, so there is no separate aim or Tick.
    light.set_editor_property('relative_location', unreal.Vector(45, 18, 45))
    light.set_editor_property('relative_rotation', unreal.Rotator())
    light.set_editor_property('relative_scale3d', unreal.Vector(1, 1, 1))
    light.set_editor_property('intensity', 20.0)
    light.set_editor_property('visible', True)
    light.set_editor_property('hidden_in_game', False)
    unreal.BlueprintEditorLibrary.compile_blueprint(guard)
    assert unreal.EditorAssetLibrary.save_loaded_asset(guard)
    report = {
        'blueprint': GUARD_PATH, 'source': PLAYER_PATH, 'intensity_cd': 20,
        'inner_half_angle': light.get_editor_property('inner_cone_angle'),
        'outer_half_angle': light.get_editor_property('outer_cone_angle'),
        'radius_cm': light.get_editor_property('attenuation_radius'),
        'parent': 'CollisionCylinder', 'relative_location_cm': [45, 18, 45],
        'default_on': True, 'compiled_saved': True,
    }
    output = Path(unreal.Paths.project_saved_dir()) / 'Automation/GuardFlashlight'
    output.mkdir(parents=True, exist_ok=True)
    (output / 'applied.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return report


if __name__ == '__main__':
    apply()
