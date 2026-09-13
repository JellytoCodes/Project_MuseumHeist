"""Import approved catalogue art and fonts through Unreal Editor only."""
from pathlib import Path
import json
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT/'ProjectResources/SourceArt/UI/Catalogue'
FONT_SOURCE = ROOT/'ProjectResources/SourceArt/UI/Fonts/Nanum'
TARGET = '/Game/Assets/UI/Catalogue'
REPORT = ROOT/'Saved/Logs/CatalogueImport.json'
tools = unreal.AssetToolsHelpers.get_asset_tools()
tasks=[]
for p in sorted(SOURCE.glob('T_*.png')):
    task=unreal.AssetImportTask()
    for key,value in dict(filename=str(p),destination_path=TARGET,destination_name=p.stem,automated=True,save=True,replace_existing=True).items():
        task.set_editor_property(key,value)
    tasks.append(task)
tools.import_asset_tasks(tasks)
result={'textures':[],'fonts':[]}
for task in tasks:
    for path in task.imported_object_paths:
        asset=unreal.load_asset(path)
        asset.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
        asset.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        asset.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        asset.set_editor_property('srgb',True)
        asset.set_editor_property('never_stream',True)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        result['textures'].append(path)
for p in sorted(FONT_SOURCE.glob('*.ttf')):
    task=unreal.AssetImportTask()
    factory=unreal.FontFileImportFactory()
    factory.set_editor_property('batch_create_font_asset',unreal.BatchCreateFontAsset.CREATE_IF_NO_FONT_EXISTS)
    name='F_'+p.stem.replace('-','_')
    for key,value in dict(filename=str(p),destination_path='/Game/Assets/UI/Fonts/Catalogue',destination_name=name,automated=True,save=True,replace_existing=True,factory=factory).items():
        task.set_editor_property(key,value)
    tools.import_asset_tasks([task])
    result['fonts']+=list(task.imported_object_paths)
result['font_assets']=list(unreal.EditorAssetLibrary.list_assets('/Game/Assets/UI/Fonts/Catalogue',recursive=True,include_folder=False))
for path in result['font_assets']:
    asset=unreal.load_asset(path)
    assert asset and unreal.EditorAssetLibrary.save_loaded_asset(asset,False),'Font package was not saved: '+path
REPORT.write_text(json.dumps(result,indent=2),encoding='utf-8')
unreal.log_warning('CATALOGUE IMPORT '+json.dumps(result))
