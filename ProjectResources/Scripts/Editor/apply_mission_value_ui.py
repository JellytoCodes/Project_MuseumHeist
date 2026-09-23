"""Add the compact contract progress row to the existing mission panel only."""
import json
from pathlib import Path
import unreal

tools = unreal.get_default_object(unreal.UMGToolSet)
bp = unreal.load_asset('/Game/Blueprints/UI/HUD/WBP_HeistHUD')
widgets = {str(i.widget_name): i.widget for i in tools.call_method('GetWidgets', (bp,)).widgets}
target = widgets['RequiredTargetNameText']
parent = target.get_parent()
assert isinstance(parent, unreal.VerticalBox), str(parent)
row = widgets.get('ContractValueText') or tools.call_method(
    'AddWidget', (bp, unreal.TextBlock.static_class(), 'ContractValueText', parent, -1)).widget
font = target.get_editor_property('font')
font.set_editor_property('size', 12)  # UMG displayed 16 at project 72 DPI, stored at 96 DPI.
row.set_font(font)
row.set_text('운반·확보 0 / 6,400')
row.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(.82, .76, .64, 1)))
row.set_auto_wrap_text(True)
row.set_shadow_offset(unreal.Vector2D(0, 0))
row.slot.set_padding(unreal.Margin(0, 8, 0, 0))
row.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
assert tools.call_method('CompileWidgetBlueprint', (bp,))
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
result = {'asset': bp.get_path_name(), 'parent': parent.get_name(), 'stored_font_size': 12,
          'displayed_font_size': 16, 'padding_top': 8, 'compiled_saved': True}
out = Path(unreal.Paths.project_saved_dir()) / 'Logs/MissionValueUI.json'
out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log(str(result))
