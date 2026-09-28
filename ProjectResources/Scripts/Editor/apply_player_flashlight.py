"""Assemble the player flashlight and its compact HUD status using Editor APIs."""
import json
from pathlib import Path
import unreal

bp = unreal.load_asset('/Game/Blueprints/Player/BP_HeistPlayerCharacter')
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
handles = sub.k2_gather_subobject_data_for_blueprint(bp)
objects = [(h, lib.get_object(lib.get_data(h))) for h in handles]
camera = next(h for h, o in objects if o.get_name() == 'FirstPersonCamera')
existing = [(h, o) for h, o in objects if isinstance(o, unreal.SpotLightComponent) and o.component_has_tag('Flashlight')]
bp.modify()
if existing:
    handle, light = existing[0]
else:
    handle, reason = sub.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=camera, new_class=unreal.SpotLightComponent, blueprint_context=bp))
    assert lib.is_handle_valid(handle), str(reason)
    sub.rename_subobject(handle, 'Flashlight')
    light = lib.get_object(lib.get_data(handle))
light.set_editor_property('component_tags', ['Flashlight'])
light.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
light.set_editor_property('relative_location', unreal.Vector(20, 8, -8))
light.set_editor_property('relative_rotation', unreal.Rotator())
light.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
light.set_editor_property('intensity', 12.0)
light.set_editor_property('attenuation_radius', 1500.0)
light.set_editor_property('inner_cone_angle', 16.0)
light.set_editor_property('outer_cone_angle', 27.0)
light.set_editor_property('indirect_lighting_intensity', 0.0)
light.set_editor_property('use_temperature', True)
light.set_editor_property('temperature', 4800.0)
light.set_editor_property('cast_shadows', True)
light.set_editor_property('volumetric_scattering_intensity', 0.0)
light.set_editor_property('visible', False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)

tools = unreal.get_default_object(unreal.UMGToolSet)
hud = unreal.load_asset('/Game/Blueprints/UI/HUD/WBP_HeistHUD')
widgets = {str(i.widget_name): i.widget for i in tools.call_method('GetWidgets', (hud,)).widgets}
hud.modify()
row = widgets.get('FlashlightStatusText') or tools.call_method('AddWidget', (
    hud, unreal.TextBlock.static_class(), 'FlashlightStatusText', widgets['HUDCanvas'], -1)).widget
assert row
font = widgets['InventoryShortcutKeyText'].get_editor_property('font')
font.set_editor_property('size', 15)  # Displayed 20 at the project's 72 DPI.
row.set_font(font)
row.set_text('[F] 손전등 OFF')
row.set_editor_property('justification', unreal.TextJustify.RIGHT)
row.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(.65, .63, .57, 1)))
row.set_shadow_color_and_opacity(unreal.LinearColor(0, 0, 0, .8))
row.set_shadow_offset(unreal.Vector2D(0, 0))
row.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
slot = row.slot
slot.set_anchors(unreal.Anchors(unreal.Vector2D(1, 1), unreal.Vector2D(1, 1)))
slot.set_alignment(unreal.Vector2D(1, 1))
slot.set_position(unreal.Vector2D(-24, -132))
slot.set_size(unreal.Vector2D(264, 32))
slot.set_auto_size(False)
assert tools.call_method('CompileWidgetBlueprint', (hud,))
assert unreal.EditorAssetLibrary.save_loaded_asset(hud)
out = Path(unreal.Paths.project_saved_dir()) / 'Flashlight'
out.mkdir(exist_ok=True)
(out / 'asset-application.json').write_text(json.dumps({
    'character': bp.get_path_name(), 'hud': hud.get_path_name(),
    'light_tag': 'Flashlight', 'attachment': 'FirstPersonCamera', 'intensity_cd': 12,
    'radius_cm': 1500, 'inner_half_angle': 16, 'outer_half_angle': 27,
    'indirect_lighting_intensity': 0, 'volumetric_scattering_intensity': 0,
    'default_on': False, 'hud_size': [264, 32], 'hud_offset': [-24, -132],
    'font_displayed': 20, 'font_stored': 15, 'compiled_saved': True}, indent=2), encoding='utf-8')
