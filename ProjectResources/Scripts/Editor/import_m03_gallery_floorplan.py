"""Import only the approved M03 floor plan and merge only its presentation row."""
import json
from pathlib import Path
import unreal
R=Path(unreal.Paths.project_dir())
t=unreal.AssetImportTask()
for k,v in {'filename':str(R/'ProjectResources/SourceArt/Gallery/M03/T_FloorPlan_M03.png'),'destination_path':'/Game/Assets/UI/Map','destination_name':'T_FloorPlan_M03','replace_existing':True,'replace_existing_settings':True,'automated':True,'save':False}.items():t.set_editor_property(k,v)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
assert len(t.imported_object_paths)==1
tex=unreal.load_asset(t.imported_object_paths[0])
for k,v in {'compression_settings':unreal.TextureCompressionSettings.TC_DEFAULT,'mip_gen_settings':unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,'lod_group':unreal.TextureGroup.TEXTUREGROUP_UI,'address_x':unreal.TextureAddress.TA_CLAMP,'address_y':unreal.TextureAddress.TA_CLAMP,'never_stream':True,'srgb':True}.items():tex.set_editor_property(k,v)
assert unreal.EditorAssetLibrary.save_loaded_asset(tex,False)
dt=unreal.load_asset('/Game/Data/DataTable/DT_MapPresentation')
before=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt))
source=json.loads((R/'ProjectResources/DataTableImports/DT_MapPresentation.json').read_text(encoding='utf-8'))
merged=[next(x for x in source if x['Name']=='M03') if r['Name']=='M03' else r for r in before]
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt,json.dumps(merged,ensure_ascii=False))
after=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt))
assert [r for r in before if r['Name']!='M03']==[r for r in after if r['Name']!='M03']
assert unreal.EditorAssetLibrary.save_loaded_asset(dt,False)
(R/'Saved/Automation/M03Rebuild/map_presentation.json').write_text(json.dumps(after,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('M03 floor plan imported; other map rows unchanged')
