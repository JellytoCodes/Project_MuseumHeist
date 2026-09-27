"""Create a reusable editor-authored floor PCG; no runtime map randomization."""
import unreal,json
from pathlib import Path
EL=unreal.EditorAssetLibrary
BL=unreal.BlueprintEditorLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
BP='/Game/Blueprints/Environment/BP_FloorTilePCG'
GRAPH='/Game/Assets/PCG/PCG_FloorTiles'

def make():
    bp=unreal.load_asset(BP) if EL.does_asset_exist(BP) else BL.create_blueprint_asset_with_parent(BP,unreal.Actor)
    assert bp
    sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles=sub.k2_gather_subobject_data_for_blueprint(bp)
    if not any(isinstance(unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)),unreal.PCGComponent) for h in handles):
        handle,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=handles[0],new_class=unreal.PCGComponent,blueprint_context=bp))
        assert not str(reason),reason
    vec=BL.get_struct_type(unreal.load_object(None,'/Script/CoreUObject.Vector'))
    meshes=BL.get_array_type(BL.get_object_reference_type(unreal.StaticMesh))
    existing=[str(n) for n in BL.list_member_variable_names(bp)]
    for name,typ in [('TileMeshes',meshes),('GridExtents',vec),('CellSize',vec),('TilePivotOffset',vec)]:
        if name not in existing:assert BL.add_member_variable(bp,name,typ)
        BL.set_blueprint_variable_instance_editable(bp,name,True)
        BL.set_blueprint_variable_category(bp,name,'Floor Tiles')
    BL.compile_blueprint(bp)
    default=unreal.get_default_object(bp.generated_class())
    default.set_editor_property('TileMeshes',[unreal.load_asset('/Game/Assets/MapAssets/Showcase/Meshes/SM_Floor_Panel_01'+c) for c in 'abcdefg'])
    floor_materials={}
    for mesh in default.get_editor_property('TileMeshes'):
        for slot in mesh.static_materials:
            mat=slot.material_interface
            while isinstance(mat,unreal.MaterialInstanceConstant):mat=mat.parent
            floor_materials[mat.get_path_name()]=mat
    for mat in floor_materials.values():
        unreal.MaterialEditingLibrary.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        unreal.MaterialEditingLibrary.recompile_material(mat);EL.save_loaded_asset(mat)
    default.set_editor_property('GridExtents',unreal.Vector(6000,4400,0))
    default.set_editor_property('CellSize',unreal.Vector(400,400,100))
    default.set_editor_property('TilePivotOffset',unreal.Vector(-200,-200,0))
    graph=unreal.load_asset(GRAPH) if EL.does_asset_exist(GRAPH) else TOOLS.create_asset('PCG_FloorTiles','/Game/Assets/PCG',unreal.PCGGraph,unreal.PCGGraphFactory())
    for existing_node in list(graph.nodes):graph.remove_node(existing_node)
    def node(cls,title,x,y):
        n,s=graph.add_node_of_type(cls);n.set_editor_property('node_title',title);n.set_node_position(x,y);return n,s
    def prop(name,x,y):
        n,s=node(unreal.PCGGetActorPropertySettings,name,x,y);s.set_editor_property('property_name',name)
        return n
    grid,gs=node(unreal.PCGCreatePointsGridSettings,'Tile grid',0,0)
    gs.set_editor_property('grid_extents',unreal.Vector(6000,4400,0));gs.set_editor_property('cell_size',unreal.Vector(400,400,100));gs.set_editor_property('coordinate_space',unreal.PCGCoordinateSpace.ORIGINAL_COMPONENT)
    ext=prop('GridExtents',-450,-200);cell=prop('CellSize',-450,0)
    graph.add_edge(ext,'Out',grid,'GridExtents');graph.add_edge(cell,'Out',grid,'CellSize')
    offset,os=node(unreal.PCGTransformPointsSettings,'Mesh pivot offset',350,0)
    pv=prop('TilePivotOffset',0,300)
    graph.add_edge(grid,'Out',offset,'In');graph.add_edge(pv,'Out',offset,'OffsetMin');graph.add_edge(pv,'Out',offset,'OffsetMax')
    choose,cs=node(unreal.PCGMatchAndSetAttributesSettings,'Random tile from actor list',700,0)
    cs.set_editor_property('match_attributes',False)
    mp=prop('TileMeshes',350,400)
    graph.add_edge(offset,'Out',choose,'In');graph.add_edge(mp,'Out',choose,'Match Data')
    spawn,ss=node(unreal.PCGStaticMeshSpawnerSettings,'Floor tile instances',1050,0)
    ss.set_mesh_selector_type(unreal.PCGMeshSelectorByAttribute)
    selector=ss.get_editor_property('mesh_selector_parameters');selector.set_editor_property('attribute_name','TileMeshes')
    descriptor=selector.get_editor_property('template_descriptor')
    body=descriptor.get_editor_property('body_instance')
    body.set_editor_property('collision_profile_name','BlockAll')
    descriptor.set_editor_property('body_instance',body)
    descriptor.set_editor_property('use_default_collision',False)
    selector.set_editor_property('template_descriptor',descriptor)
    ss.set_editor_property('synchronous_load',True)
    graph.add_edge(choose,'Out',spawn,'In');graph.add_edge(spawn,'Out',graph.get_output_node(),'Out')
    handles=sub.k2_gather_subobject_data_for_blueprint(bp)
    comp=next(o for o in [unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)) for h in handles] if isinstance(o,unreal.PCGComponent))
    comp.modify();comp.set_graph(graph);comp.set_editor_property('generation_trigger',unreal.PCGComponentGenerationTrigger.GENERATE_ON_DEMAND)
    comp.set_editor_property('is_component_partitioned',False);comp.set_editor_property('regenerate_in_editor',False)
    bp.modify();BL.compile_blueprint(bp)
    EL.save_loaded_asset(graph);EL.save_loaded_asset(bp)
    (Path(unreal.Paths.project_saved_dir())/'FloorPCGCreated.json').write_text(json.dumps({'blueprint':BP,'graph':GRAPH,'nodes':len(graph.nodes)}))

if __name__=='__main__':make()
