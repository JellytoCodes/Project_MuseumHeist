"""Import approved originals and create shared presentation assets inside Editor."""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / 'ProjectResources/SourceArt/UI/Heist'
TARGET = '/Game/Assets/UI/Heist'
META = json.loads((SOURCE / 'ApprovedComposition.json').read_text(encoding='utf-8'))
TOOLS = unreal.get_default_object(unreal.UMGToolSet)
ASSETS = unreal.AssetToolsHelpers.get_asset_tools()
WHITE = unreal.LinearColor(1, 1, 1, 1)

def texture_brush(texture, width=64, height=64, sliced=False):
    b = unreal.WidgetLibrary.make_brush_from_texture(texture, width, height)
    b.set_editor_property('draw_as', unreal.SlateBrushDrawType.BOX if sliced else unreal.SlateBrushDrawType.IMAGE)
    b.set_editor_property('margin', unreal.Margin(.125, .125, .125, .125) if sliced else unreal.Margin(0, 0, 0, 0))
    return b

def import_art():
    tasks = []
    for path in SOURCE.glob('T_*.png'):
        task = unreal.AssetImportTask()
        for key, value in dict(filename=str(path), destination_path=TARGET, destination_name=path.stem,
                               automated=True, save=True, replace_existing=True).items():
            task.set_editor_property(key, value)
        tasks.append(task)
    ASSETS.import_asset_tasks(tasks)
    for task in tasks:
        texture = unreal.load_asset(TARGET + '/' + Path(task.filename).stem)
        assert texture
        texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property('never_stream', True)
        texture.set_editor_property('srgb', texture.get_name() != 'T_Heist_IconAtlas')
        texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
        texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
        assert unreal.EditorAssetLibrary.save_loaded_asset(texture)

def create_icons():
    material = unreal.load_asset(TARGET + '/M_HeistIcon')
    if not material:
        material = ASSETS.create_asset('M_HeistIcon', TARGET, unreal.Material, unreal.MaterialFactoryNew())
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET + '/MI_HeistIcon_coin'):
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
        material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        library = unreal.MaterialEditingLibrary
        def node(kind, x, y):
            return library.create_material_expression(material, kind, x, y)
        def link(a, output, b, input_name):
            inputs = library.get_material_expression_input_names(b)
            if input_name not in inputs and len(inputs) == 1:
                input_name = inputs[0]
            assert library.connect_material_expressions(a, output, b, input_name), (a, output, b, input_name, inputs)
        uv = node(unreal.MaterialExpressionTextureCoordinate, -900, -300)
        rect = node(unreal.MaterialExpressionVectorParameter, -900, -100)
        rect.set_editor_property('parameter_name', 'UVRect')
        rect.set_editor_property('default_value', unreal.LinearColor(0, 0, .25, .25))
        offset = node(unreal.MaterialExpressionComponentMask, -700, -100)
        scale = node(unreal.MaterialExpressionComponentMask, -700, 100)
        for mask, r, g, b, a in [(offset, True, True, False, False), (scale, False, False, True, True)]:
            for key, value in dict(r=r, g=g, b=b, a=a).items():
                mask.set_editor_property(key, value)
            link(rect, 'RGBA', mask, 'Input')
        multiply = node(unreal.MaterialExpressionMultiply, -500, -300)
        add = node(unreal.MaterialExpressionAdd, -300, -300)
        link(uv, '', multiply, 'A'); link(scale, '', multiply, 'B')
        link(multiply, '', add, 'A'); link(offset, '', add, 'B')
        sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -100, -300)
        sample.set_editor_property('parameter_name', 'IconAtlas')
        sample.set_editor_property('texture', unreal.load_asset(TARGET + '/T_Heist_IconAtlas'))
        sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        link(add, '', sample, 'UVs')
        vertex = node(unreal.MaterialExpressionVertexColor, -100, 100)
        alpha = node(unreal.MaterialExpressionMultiply, 150, -100)
        link(sample, 'R', alpha, 'A'); link(vertex, 'A', alpha, 'B')
        assert library.connect_material_property(vertex, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        assert library.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
        library.recompile_material(material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    for glyph, cell in META['glyphs'].items():
        name = 'MI_HeistIcon_' + glyph
        instance = unreal.load_asset(TARGET + '/' + name)
        if not instance:
            instance = ASSETS.create_asset(name, TARGET, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, material)
        # UE 5.8's setter returns false even after applying the value; read back.
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, 'UVRect', unreal.LinearColor(*cell['uv']))
        actual = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(instance, 'UVRect')
        assert all(abs(a-b) < .000001 for a,b in zip([actual.r, actual.g, actual.b, actual.a], cell['uv']))
        unreal.MaterialEditingLibrary.update_material_instance(instance)
        assert unreal.EditorAssetLibrary.save_loaded_asset(instance)

def create_common(name, texture_name, keycap=False):
    folder = '/Game/Blueprints/UI/Common'
    if unreal.EditorAssetLibrary.does_asset_exist(folder + '/' + name):
        return unreal.load_asset(folder + '/' + name)
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property('parent_class', unreal.UserWidget.static_class())
    bp = ASSETS.create_asset(name, folder, unreal.WidgetBlueprint, factory)
    assert bp
    initial = TOOLS.call_method('GetWidgets', (bp,)).widgets
    if initial:
        assert TOOLS.call_method('RemoveWidget', (bp, initial[0].widget))
    root = TOOLS.call_method('AddWidget', (bp, unreal.Overlay.static_class(), 'Root', None, -1)).widget
    image = TOOLS.call_method('AddWidget', (bp, unreal.Image.static_class(), 'FrameImage', root, -1)).widget
    brush = texture_brush(unreal.load_asset(TARGET + '/' + texture_name), 32 if keycap else 64, 32 if keycap else 64, True)
    if keycap:
        brush.set_editor_property('margin', unreal.Margin(.25, .25, .25, .25))
    image.set_brush(brush)
    image.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    image.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    image.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    content = TOOLS.call_method('AddWidget', (bp, unreal.NamedSlot.static_class(), 'ContentSlot', root, -1)).widget
    content.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    content.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    tree = root.get_outer()
    assert TOOLS.call_method('CompileWidgetBlueprint', (bp,))
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return bp

def create_menu_backdrop():
    # Reuse the authored gallery thumbnail; no background texture is copied.
    name = 'M_HeistMenuBackdrop'
    material = unreal.load_asset(TARGET + '/' + name) if unreal.EditorAssetLibrary.does_asset_exist(TARGET + '/' + name) else ASSETS.create_asset(name, TARGET, unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    texture = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureObject, -400, 0)
    texture.set_editor_property('texture', unreal.load_asset('/Game/Assets/UI/Catalogue/T_Catalogue_Map_M03'))
    uv = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -400, 180)
    shader = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCustom, -100, 0)
    shader.set_editor_property('description', 'Approved menu background: gallery, soft blur, local readability gradients')
    shader.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for input_name in ['Backdrop', 'UV']:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property('input_name', input_name)
        inputs.append(custom_input)
    shader.set_editor_property('inputs', inputs)
    shader.set_editor_property('code', '''float3 c = float3(0,0,0);
float total = 0;
for (int y=-1; y<=1; ++y) {
    for (int x=-1; x<=1; ++x) {
        float weight = (x==0 ? 2.0 : 1.0) * (y==0 ? 2.0 : 1.0);
        c += Texture2DSample(Backdrop, BackdropSampler, UV + float2(x*4.0/1280.0,y*4.0/720.0)).rgb * weight;
        total += weight;
    }
}
c = pow(max(c / total, 0), 1.0/2.2);
c = lerp(dot(c, float3(0.2126,0.7152,0.0722)).xxx, c, 0.64) * 0.72;
float3 ink = float3(8,12,16)/255.0;
c = lerp(c, ink, lerp(0.73,0.50,UV.x));
c = lerp(c, ink, saturate((UV.y-0.28)/0.72)*0.75);
return pow(max(c,0), 2.2);''')
    assert unreal.MaterialEditingLibrary.connect_material_expressions(texture, '', shader, 'Backdrop')
    assert unreal.MaterialEditingLibrary.connect_material_expressions(uv, '', shader, 'UV')
    assert unreal.MaterialEditingLibrary.connect_material_property(shader, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)

def main():
    import_art()
    create_icons()
    create_common('WBP_HeistKeycap', 'T_Heist_Keycap', True)
    create_common('WBP_HeistPanelFrame', 'T_Heist_Panel')
    create_menu_backdrop()
    print('Approved UI originals imported: 8 textures, one UI master, 16 glyph instances, two shared WBP.')

if __name__ == '__main__':
    main()
