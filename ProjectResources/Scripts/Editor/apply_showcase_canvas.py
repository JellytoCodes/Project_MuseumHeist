"""Editor migration: shared Canvas materials and authored painting placements.

Requires the single-canvas C++ build. Does not rebuild room geometry.
Backups and a machine-readable change report go to Saved/CanvasMigration.
"""
import json, math, shutil
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / 'Saved/CanvasMigration'
OUT.mkdir(parents=True, exist_ok=True)
META = json.loads((ROOT / 'ProjectResources/SourceArt/Canvas/canvas_uv.json').read_text())
ASSET_DIR = '/Game/Assets/Art/SurfaceForgery/Materials/Canvas'
MAPS = ['M01_ClassicalPrototype', 'M02_MoonlitPrototype', 'M03_GlasshousePrototype']
EL = unreal.EditorAssetLibrary
ML = unreal.MaterialEditingLibrary
ACT = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

def backup(package):
    rel = package.removeprefix('/Game/')
    for ext in ('.uasset', '.umap'):
        src = ROOT / 'Content' / (rel + ext)
        if src.exists():
            dst = OUT / 'Backup' / (rel + ext)
            if not dst.exists():
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dst)

def expr(mat, cls, **props):
    e = ML.create_material_expression(mat, cls)
    for k,v in props.items(): e.set_editor_property(k,v)
    return e

def connect(a, out, b, inp):
    assert ML.connect_material_expressions(a,out,b,inp), (a,out,b,inp)

def custom_inputs(names):
    result=[]
    for name in names:
        inp=unreal.CustomInput();inp.set_editor_property('input_name',name);result.append(inp)
    return result

def build_materials():
    path = ASSET_DIR + '/M_HeistCanvas'
    master = unreal.load_asset(path) if EL.does_asset_exist(path) else EL.duplicate_asset('/Game/Assets/MapAssets/Showcase/Materials/Masters/M_Surface_Basic', path)
    if not EL.does_asset_exist(ASSET_DIR + '/MI_HeistCanvas_07'):
        original = ML.get_material_property_input_node(master, unreal.MaterialProperty.MP_BASE_COLOR)
        uv = expr(master, unreal.MaterialExpressionTextureCoordinate)
        ur = expr(master, unreal.MaterialExpressionVectorParameter, parameter_name='CanvasURow')
        vr = expr(master, unreal.MaterialExpressionVectorParameter, parameter_name='CanvasVRow')
        ellipse = expr(master, unreal.MaterialExpressionScalarParameter, parameter_name='CanvasEllipse', default_value=0.0)
        mapping = expr(master, unreal.MaterialExpressionCustom, code='return float2(dot(float3(UV,1),URow), dot(float3(UV,1),VRow));', output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2)
        mapping.set_editor_property('inputs',custom_inputs(('UV','URow','VRow')))
        connect(uv,'',mapping,'UV');connect(ur,'',mapping,'URow');connect(vr,'',mapping,'VRow')
        mask = expr(master, unreal.MaterialExpressionCustom, code='float box = step(0,P.x)*step(P.x,1)*step(0,P.y)*step(P.y,1); float oval=step(dot((P-0.5)/float2(0.46,0.46),(P-0.5)/float2(0.46,0.46)),1); return box*lerp(1,oval,Ellipse);', output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        mask.set_editor_property('inputs',custom_inputs(('P','Ellipse')));connect(mapping,'',mask,'P');connect(ellipse,'',mask,'Ellipse')
        painting = expr(master, unreal.MaterialExpressionTextureSampleParameter2D, parameter_name='PaintingTexture', texture=unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
        aspect=expr(master,unreal.MaterialExpressionScalarParameter,parameter_name='CanvasAspect',default_value=1.0)
        image_aspect=expr(master,unreal.MaterialExpressionScalarParameter,parameter_name='PaintingAspect',default_value=1.0)
        fit=expr(master,unreal.MaterialExpressionCustom,code='float a=CanvasAspect/max(PaintingAspect,0.001); return (P-0.5)*float2(min(a,1),min(1/a,1))+0.5;',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2)
        fit.set_editor_property('inputs',custom_inputs(('P','CanvasAspect','PaintingAspect')))
        connect(mapping,'',fit,'P');connect(aspect,'',fit,'CanvasAspect');connect(image_aspect,'',fit,'PaintingAspect');connect(fit,'',painting,'UVs')
        inside=expr(master,unreal.MaterialExpressionCustom,code='return step(0,P.x)*step(P.x,1)*step(0,P.y)*step(P.y,1);',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        inside.set_editor_property('inputs',custom_inputs(('P',)));connect(fit,'',inside,'P')
        paper=expr(master,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.94,.92,.84,1))
        matte=expr(master,unreal.MaterialExpressionLinearInterpolate)
        connect(paper,'',matte,'A');connect(painting,'RGB',matte,'B');connect(inside,'',matte,'Alpha')
        blend = expr(master, unreal.MaterialExpressionLinearInterpolate)
        connect(original,'',blend,'A');connect(matte,'',blend,'B');connect(mask,'',blend,'Alpha')
        assert ML.connect_material_property(blend,'',unreal.MaterialProperty.MP_BASE_COLOR)
        ML.layout_material_expressions(master);ML.recompile_material(master);EL.save_loaded_asset(master)
    mats=[]
    for row in META:
        i=row['variant'];p=ASSET_DIR+f'/MI_HeistCanvas_{i:02}'
        mat=unreal.load_asset(p) if EL.does_asset_exist(p) else EL.duplicate_asset(f'/Game/Assets/MapAssets/Showcase/Materials/Instance/MI_Canvas_{i:02}a',p)
        ML.set_material_instance_parent(mat,master)
        for name,values in [('CanvasURow',row['u_row']),('CanvasVRow',row['v_row'])]:
            ML.set_material_instance_vector_parameter_value(mat,name,unreal.LinearColor(*values,0))
        ML.set_material_instance_scalar_parameter_value(mat,'CanvasEllipse',0.0)
        ML.set_material_instance_texture_parameter_value(mat,'PaintingTexture',unreal.load_asset('/Game/Data/Forgery/Textures/M01/T_Forgery_M01_Portrait_01'))
        wide=1-row['thin_axis']
        ML.set_material_instance_scalar_parameter_value(mat,'CanvasAspect',(row['front_max'][wide]-row['front_min'][wide])/(row['front_max'][2]-row['front_min'][2]))
        EL.save_loaded_asset(mat);mats.append(mat)
    return mats

def mesh_for(i):
    return unreal.load_asset(f'/Game/Assets/MapAssets/Showcase/Meshes/SM_Canvas_Painting_{i+1:02}a')

def fit_canvas(component, i, center, normal, max_width, max_height):
    row=META[i];axis=row['thin_axis'];wide=1-axis
    width=row['max'][wide]-row['min'][wide];height=row['max'][2]-row['min'][2]
    scale=min(max_width/width,max_height/height)
    # Same wall-facing normal as the old image plane; Z remains vertical.
    yaw=math.degrees(math.atan2(normal.y,normal.x))-(90 if axis==1 else 0)
    rot=unreal.Rotator(pitch=0,yaw=yaw,roll=0)
    pivot=unreal.Vector(*[(a+b)*.5 for a,b in zip(row['min'],row['max'])])
    pivot.x=row['front_max'][0] if axis==0 else pivot.x
    pivot.y=row['front_max'][1] if axis==1 else pivot.y
    transform=unreal.Transform(rotation=rot, location=center, scale=unreal.Vector(scale,scale,scale))
    offset=unreal.MathLibrary.transform_direction(unreal.Transform(rotation=rot),pivot*scale)
    transform.translation=center-offset
    component.set_static_mesh(mesh_for(i))
    component.set_world_transform(transform,False,True)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property('generate_overlap_events',False)

def migrate():
    mats=build_materials()
    # Locate the existing common shell without assuming an old folder layout.
    candidates=[p for p in EL.list_assets('/Game/Blueprints',True,False) if p.split('/')[-1].split('.')[0]=='BP_PaintingDisplayCase']
    assert len(candidates)==1,candidates
    bp=unreal.load_asset(candidates[0]);backup(candidates[0].split('.')[0])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo=unreal.get_default_object(bp.generated_class())
    components=cdo.get_components_by_class(unreal.StaticMeshComponent)
    original=next(c for c in components if c.get_name()=='OriginalVisualComponent')
    backing=next(c for c in components if c.get_name()=='VisualMeshComponent')
    # Preserve the old inherited plane while reading authored instance transforms.
    original.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    original.set_relative_scale3d(unreal.Vector(3.8,3.8,1))
    original.set_relative_location(unreal.Vector(200,12,200),False,True)
    original.set_relative_rotation(unreal.Rotator(roll=-90),False,True)
    bp.modify()
    EL.save_loaded_asset(bp)
    report={}
    for name in MAPS:
        package='/Game/Maps/'+name;backup(package)
        unreal.EditorLoadingAndSavingUtils.load_map(package)
        actors=list(ACT.get_all_level_actors());changed=[];canvas=[]
        unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().modify()
        # Capture world positions before any attachment parent moves.
        targets={a:unreal.Vector(*(math.trunc(v) for v in (a.get_actor_location().x,a.get_actor_location().y,a.get_actor_location().z))) for a in actors}
        def depth(a):
            n=0;p=a.get_attach_parent_actor()
            while p: n+=1;p=p.get_attach_parent_actor()
            return n
        for a in sorted(actors,key=depth):
            old=a.get_actor_location();new=targets[a]
            if max(abs(old.x-new.x),abs(old.y-new.y),abs(old.z-new.z))>0.00001:
                a.modify();a.root_component.modify()
                a.set_actor_location(new,False,True);changed.append(a.get_actor_label())
        cases=sorted([a for a in actors if isinstance(a,unreal.HeistPaintingDisplayCaseActor)],key=lambda a:a.get_actor_label())
        decors=sorted([a for a in actors if isinstance(a,unreal.StaticMeshActor) and 'MuseumDecorativePainting' in [str(t) for t in a.tags]],key=lambda a:a.get_actor_label())
        assert len(cases)==20,(name,len(cases))
        for index,a in enumerate(cases+decors):
            a.modify()
            is_case=isinstance(a,unreal.HeistPaintingDisplayCaseActor)
            comp=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='OriginalVisualComponent') if is_case else a.static_mesh_component
            oldmesh=comp.static_mesh
            comp.modify()
            if oldmesh and oldmesh.get_name().startswith('SM_Canvas_Painting_'):continue
            assert oldmesh and oldmesh.get_name()=='Plane',(a.get_actor_label(),oldmesh)
            center=comp.get_world_location();normal=comp.get_up_vector();sc=comp.get_world_scale()
            max_width=abs(sc.x)*100;max_height=abs(sc.y)*100
            oldmat=comp.get_material(0)
            texture=ML.get_material_instance_texture_parameter_value(oldmat,'PaintingTexture') if isinstance(oldmat,unreal.MaterialInstanceConstant) else None
            if not texture and isinstance(oldmat,unreal.MaterialInstanceConstant):
                for param in oldmat.get_editor_property('texture_parameter_values'):
                    if param.parameter_value: texture=param.parameter_value;break
            i=index%7
            if is_case:
                backing=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='VisualMeshComponent')
                backing.set_static_mesh(None);backing.set_relative_transform(unreal.Transform(),False,True)
            fit_canvas(comp,i,center,normal,max_width,max_height)
            # Per-exhibit instance preserves the authored decorative print before a contract starts.
            mp=ASSET_DIR+'/Prints/MI_'+a.get_actor_label()
            mi=unreal.load_asset(mp) if EL.does_asset_exist(mp) else EL.duplicate_asset(mats[i].get_path_name(),mp)
            if texture:
                ML.set_material_instance_texture_parameter_value(mi,'PaintingTexture',texture)
                ML.set_material_instance_scalar_parameter_value(mi,'PaintingAspect',texture.blueprint_get_size_x()/max(texture.blueprint_get_size_y(),1))
            EL.save_loaded_asset(mi);comp.set_material(0,mi)
            if is_case:
                a.set_editor_property('original_painting_material',mats[i]);a.set_editor_property('replica_painting_material',mats[i])
            else:
                # Mesh corner pivots can move the actor origin; truncate the final placement too.
                p=a.get_actor_location();a.set_actor_location(unreal.Vector(math.trunc(p.x),math.trunc(p.y),math.trunc(p.z)),False,True)
            canvas.append({'actor':a.get_actor_label(),'variant':i+1,'interactive':is_case,'old_center':str(center),'mesh':comp.static_mesh.get_path_name()})
        assert unreal.EditorLoadingAndSavingUtils.save_current_level()
        report[name]={'positions_truncated':len(changed),'paintings':canvas,'interactive':len(cases),'decorative':len(decors)}
        (OUT/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    backing.set_static_mesh(None);backing.set_relative_transform(unreal.Transform(),False,True)
    fit_canvas(original,4,unreal.Vector(-4,0,148),unreal.Vector(-1,0,0),152,152)
    original.set_material(0,mats[4])
    cdo.set_editor_property('original_painting_material',mats[4]);cdo.set_editor_property('replica_painting_material',mats[4])
    bp.modify();EL.save_loaded_asset(bp)
    return report

if __name__=='__main__':
    print(json.dumps(migrate(),indent=2))

