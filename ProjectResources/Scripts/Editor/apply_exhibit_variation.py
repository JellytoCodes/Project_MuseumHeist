"""Convert existing decorative canvases into instances of the common exhibit BP.

Explicit 2026-10-03 approval only. Coordinates, canvas world transforms, target
identity, laser connections and all architecture are preserved. No new uasset
is created. Requires the matching read-only audit and the rebuilt C++ classes.
"""
import hashlib
import json
import math
import sys
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Automation/ExhibitVariation20261003'
sys.path.insert(0,str(ROOT/'ProjectResources/Scripts/Editor'))
from refine_canvas_and_light_fixtures import painting_geometry
from apply_loot_exhibition_layout import snapshot,transform

A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EL=unreal.EditorAssetLibrary
SUB=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
BASE='/Game/Blueprints/World/Actors/'
PAINTING=BASE+'Loot/BP_PaintingDisplayCase'
LOOT=BASE+'Loot/BP_Loot'
PANEL_TAG='HeistExhibitSecurityPanel'

def xyz(v): return [float(getattr(v,k)) for k in 'xyz']

def same_transform(a,b):
    for av,bv in ((a.translation,b.translation),(a.scale3d,b.scale3d)):
        if max(abs(x-y) for x,y in zip(xyz(av),xyz(bv)))>.001: return False
    aq,bq=a.rotation,b.rotation
    return abs(abs(sum(getattr(aq,k)*getattr(bq,k) for k in 'xyzw'))-1)<.00001

def preserved(a):
    row=snapshot(a)
    row['links']={}
    for prop in ('protected_painting_case','linked_laser_barrier','laser_barrier'):
        try:
            value=a.get_editor_property(prop)
            row['links'][prop]=value.get_name() if value else None
        except Exception: pass
    if isinstance(a,unreal.HeistLaserBarrierActor):
        target=a.get_protected_painting_case()
        row['links']['verified_protected_case']=target.get_name() if target else None
        assert target, a.get_name()+' missing protected painting'
    if isinstance(a,unreal.HeistSecurityHoldButtonActor):
        laser=a.get_linked_laser_barrier()
        row['links']['verified_linked_laser']=laser.get_name() if laser else None
        assert laser, a.get_name()+' missing linked laser'
    row['lights']=[dict(name=c.get_name(),transform=transform(c.get_relative_transform()),
        intensity=float(c.get_editor_property('intensity')),visible=c.is_visible())
        for c in a.get_components_by_class(unreal.LightComponent)]
    if isinstance(a,unreal.PostProcessVolume):
        row['postprocess']={k:a.settings.get_editor_property(k) for k in
            ('auto_exposure_min_brightness','auto_exposure_max_brightness','auto_exposure_bias','ambient_cubemap_intensity','lumen_final_gather_screen_traces')}
    return row

def canvas(a):
    cs=[c for c in a.get_components_by_class(unreal.StaticMeshComponent)
        if c.static_mesh and c.static_mesh.get_name().startswith('SM_Canvas_Painting_')]
    assert len(cs)<=1,a.get_name()
    return cs[0] if cs else None

def tagged_region(a,p,bounds,code):
    if code=='M03': region=str(a.get_folder_path()).split('/')[-1]
    else:
        x0,x1,y0,y1=bounds
        column=min(2,max(0,int((p['center'].x-x0)/max((x1-x0)/3,1))))
        region=('West','Center','East')[column]+('South' if p['center'].y<(y0+y1)/2 else 'North')
    return 'HeistExhibitRegion_'+region

def panel_position(a,p):
    panel=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent)
               if c.get_name().startswith('SecurityPanelVisual'))
    tags=[str(t) for t in panel.component_tags]
    panel.set_editor_property('component_tags',tags+[PANEL_TAG] if PANEL_TAG not in tags else tags)
    target=p['center']-unreal.Vector(0,0,p['height']/2+16)+p['normal']*2
    if target.z<12:
        target=p['center']+p['horizontal']*(p['width']/2+16)+p['normal']*2
        target.z=p['center'].z-p['height']/2+16
    panel.set_world_location(target,False,True)
    panel.set_world_rotation(unreal.MathLibrary.make_rot_from_z(p['normal']),False,True)
    panel.set_relative_scale3d(unreal.Vector(.2,.2,.2))

def configure_loot_case_glass():
    # Reuse one existing glass pane mesh in the common BP. Functional BP parts
    # stay separate from authored furniture; no mesh cut, copy or derived asset.
    loot=unreal.load_asset(LOOT)
    loot.modify()
    unreal.BlueprintEditorLibrary.compile_blueprint(loot)
    lib=unreal.SubobjectDataBlueprintFunctionLibrary
    objects=[(h,lib.get_object(lib.get_data(h))) for h in SUB.k2_gather_subobject_data_for_blueprint(loot)]
    parent=next(h for h,c in objects if c.get_name()=='CaseShell')
    shell=next(c for h,c in objects if h==parent)
    shell.modify();shell.set_static_mesh(None)
    shell.set_relative_transform(unreal.Transform(),False,True)
    pane=unreal.load_asset('/Game/Assets/StarterContent/Props/SM_GlassWindow')
    glass=unreal.load_asset('/Game/Assets/MapAssets/Showcase/Materials/Instance/MI_Glass_01a')
    specs=[('Front',(20,20,0),unreal.Rotator(),(.1,.4,.4)),
           ('Back',(-20,20,0),unreal.Rotator(),(.1,.4,.4)),
           ('Left',(-20,-20,0),unreal.Rotator(yaw=90),(.1,.4,.4)),
           ('Right',(-20,20,0),unreal.Rotator(yaw=90),(.1,.4,.4)),
           ('Top',(-20,20,80),unreal.Rotator(pitch=-90),(.1,.4,.2))]
    for label,location,rotation,scale in specs:
        name='CaseGlass'+label
        component=next((c for h,c in objects if c.get_name().startswith(name)),None)
        if component is None:
            handle,reason=SUB.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.StaticMeshComponent,blueprint_context=loot))
            assert lib.is_handle_valid(handle),str(reason)
            assert SUB.rename_subobject(handle,name)
            component=lib.get_object(lib.get_data(handle))
        component.modify();component.set_static_mesh(pane);component.set_material(0,glass)
        component.set_relative_transform(unreal.Transform(location=unreal.Vector(*location),rotation=rotation,scale=unreal.Vector(*scale)),False,True)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_properties(dict(generate_overlap_events=False,mobility=unreal.ComponentMobility.MOVABLE,cast_shadow=False,visible=False))
    unreal.BlueprintEditorLibrary.compile_blueprint(loot)
    assert EL.save_loaded_asset(loot)

def configure_shells():
    bp=unreal.load_asset(PAINTING)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    for h in SUB.k2_gather_subobject_data_for_blueprint(bp):
        c=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
        if isinstance(c,unreal.StaticMeshComponent) and c.get_name().startswith('SecurityPanelVisual'):
            c.modify(); c.set_editor_property('component_tags',[PANEL_TAG])
    bp.modify(); unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert EL.save_loaded_asset(bp)
    loot=unreal.load_asset(LOOT)
    unreal.BlueprintEditorLibrary.compile_blueprint(loot)
    cdo=unreal.get_default_object(loot.generated_class())
    shell=next(c for c in cdo.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='CaseShell')
    shell.modify(); shell.set_static_mesh(None)
    shell.set_relative_transform(unreal.Transform(),False,True)
    shell.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    door=unreal.get_default_object(unreal.load_asset(BASE+'Security/BP_DetentionDoor').generated_class())
    for dst,src in (('case_latch_sound','latch_sound'),('case_failure_sound','failure_sound'),('case_open_sound','open_sound'),('case_sound_attenuation','sound_attenuation')):
        cdo.set_editor_property(dst,door.get_editor_property(src))
    loot.modify(); assert EL.save_loaded_asset(loot)
    configure_loot_case_glass()
    return bp.generated_class()

def main():
    baseline=json.loads((OUT/'before.json').read_text(encoding='utf-8'))
    for m in baseline['maps']:
        assert hashlib.sha256((ROOT/'Content/Maps'/(m['name']+'.umap')).read_bytes()).hexdigest()==m['sha256'],m['name']+' changed since audit'
    painting_class=configure_shells()
    for table_name,source_name in (('DT_LootData','DT_LootDataRow.json'),('DT_ItemData','DT_ItemDataRow.json')):
        table=unreal.load_asset('/Game/Data/DataTable/'+table_name);table.modify()
        assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(table,str(ROOT/'ProjectResources/DataTableImports'/source_name),table.get_editor_property('row_struct'))
        assert EL.save_loaded_asset(table)
        (OUT/(table_name+'_after.json')).write_text(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table),encoding='utf-8')
    plan_path=ROOT/'ProjectResources/SourceArt/Gallery/LooseLootExhibitionLayout.json'
    plan=json.loads(plan_path.read_text(encoding='utf-8'))
    report={'maps':[],'asset_copies':0,'new_blueprints':0}
    for m in baseline['maps']:
        code=m['name'][:3];path='/Game/Maps/'+m['name']
        assert unreal.EditorLoadingAndSavingUtils.load_map(path)
        old=list(A.get_all_level_actors())
        paintings=[a for a in old if canvas(a)]
        assert len(paintings)==60
        decors=sorted([a for a in paintings if isinstance(a,unreal.StaticMeshActor)],key=lambda a:a.get_name())
        assert len(decors)==48
        unrelated={a.get_name():preserved(a) for a in old if a not in paintings}
        geometry={a.get_actor_label():(canvas(a).static_mesh.get_path_name(),canvas(a).get_world_transform()) for a in paintings}
        cases=[a for a in paintings if isinstance(a,unreal.HeistPaintingDisplayCaseActor)]
        identities={a.get_name():(str(a.get_editor_property('display_case_id')),str(a.get_editor_property('target_artifact_id')),a.get_actor_transform()) for a in cases}
        ps=[painting_geometry(canvas(a)) for a in paintings]
        bounds=(min(p['center'].x for p in ps),max(p['center'].x for p in ps),min(p['center'].y for p in ps),max(p['center'].y for p in ps))
        names={};converted=[]
        for index,old_actor in enumerate(decors):
            c=canvas(old_actor);t=c.get_world_transform();p=painting_geometry(c)
            tags=[str(v) for v in old_actor.tags if str(v)!='MuseumDecorativePainting']
            tags += ['MuseumPaintingCandidate',tagged_region(old_actor,p,bounds,code)]
            a=A.spawn_actor_from_class(painting_class,old_actor.get_actor_location(),old_actor.get_actor_rotation())
            a.modify();a.set_actor_scale3d(unreal.Vector(1,1,1))
            a.set_actor_label(old_actor.get_actor_label());a.set_folder_path(old_actor.get_folder_path())
            a.set_editor_properties(dict(tags=tags,display_case_id=unreal.Name(f'Case_{code}_Optional_Display_{index+1:02}'),target_artifact_id=unreal.Name(f'Artifact_Painting_{code}_Optional_{index%4+1:02}')))
            assert a.set_contract_exhibit_active(False)
            original=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='OriginalVisualComponent')
            original.set_static_mesh(c.static_mesh);original.set_world_transform(t,False,True)
            original.set_collision_profile_name(c.get_collision_profile_name(),False)
            original.set_collision_enabled(c.get_collision_enabled())
            for slot,material in enumerate(c.get_materials()): original.set_material(slot,material)
            variant=int(c.static_mesh.get_name().split('_')[-1][:2])
            material=unreal.load_asset(f'/Game/Assets/Art/SurfaceForgery/Materials/Canvas/MI_HeistCanvas_{variant:02}')
            a.set_editor_properties(dict(original_painting_material=material,replica_painting_material=material))
            box=a.get_component_by_class(unreal.BoxComponent)
            box.set_relative_scale3d(unreal.Vector(1,1,1));box.set_box_extent(unreal.Vector(60,60,80),False)
            focus=p['center']+p['normal']*90;focus.z=a.get_actor_location().z+100
            box.set_world_location(focus,False,True);box.set_world_rotation(unreal.MathLibrary.make_rot_from_x(-p['normal']),False,True)
            panel_position(a,painting_geometry(original))
            names[old_actor.get_name()]=a.get_name();converted.append(dict(old=old_actor.get_name(),new=a.get_name(),label=a.get_actor_label()))
            assert A.destroy_actor(old_actor)
        # Tags are cooked, unlike editor folders. Existing case IDs and references stay.
        for a in cases:
            a.modify();p=painting_geometry(canvas(a));tags=[str(t) for t in a.tags if not str(t).startswith('HeistExhibitRegion_')]
            a.set_editor_property('tags',tags+['MuseumPaintingCandidate',tagged_region(a,p,bounds,code)])
            panel=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('SecurityPanelVisual'))
            panel.set_editor_property('component_tags',[PANEL_TAG])
        # M03 artwork lights refer to actor names in tags: migrate only those tags.
        changed_light_names=[]
        for a in A.get_all_level_actors():
            tags=[str(t) for t in a.tags];new_tags=[('M03ArtLight_'+names[t[len('M03ArtLight_'):]]) if t.startswith('M03ArtLight_') and t[len('M03ArtLight_'):] in names else t for t in tags]
            if tags!=new_tags:
                a.modify();a.set_editor_property('tags',new_tags);changed_light_names.append(a.get_name())
                unrelated[a.get_name()]['tags']=new_tags
        def validate():
            actors=list(A.get_all_level_actors());arts=[a for a in actors if canvas(a)]
            assert len(arts)==60 and all(isinstance(a,unreal.HeistPaintingDisplayCaseActor) for a in arts)
            assert len({str(a.get_editor_property('display_case_id')) for a in arts})==60
            for a in arts:
                mesh,t=geometry[a.get_actor_label()]
                assert canvas(a).static_mesh.get_path_name()==mesh and same_transform(canvas(a).get_world_transform(),t),a.get_actor_label()+' canvas moved'
                assert a.get_actor_scale3d()==unreal.Vector(1,1,1)
                for c in a.get_components_by_class(unreal.SceneComponent):
                    assert all(abs(v*10-round(v*10))<.001 for v in xyz(c.get_relative_transform().scale3d)),(a.get_name(),c.get_name())
            byname={a.get_name():a for a in actors}
            for name,(case_id,artifact_id,t) in identities.items():
                a=byname[name];assert str(a.get_editor_property('display_case_id'))==case_id and str(a.get_editor_property('target_artifact_id'))==artifact_id and same_transform(a.get_actor_transform(),t)
            assert unrelated=={a.get_name():preserved(a) for a in actors if a.get_name() in unrelated},'Unrelated placement or security links changed'
        validate()
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        assert unreal.EditorLoadingAndSavingUtils.save_map(world,path)
        assert unreal.EditorLoadingAndSavingUtils.load_map(path);validate()
        for entry in plan['maps']:
            if entry['code']==code:
                for station in entry['stations']:
                    station['focus_painting']=names.get(station['focus_painting'],station['focus_painting'])
        report['maps'].append(dict(map=m['name'],candidates=60,converted=len(converted),converted_actors=converted,preserved_existing_identities=len(identities),preserved_other_actors=len(unrelated),light_reference_tags=changed_light_names,after_sha256=hashlib.sha256((ROOT/'Content/Maps'/(m['name']+'.umap')).read_bytes()).hexdigest()))
        (OUT/'apply.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    plan['runtime_rule']='All 12 existing stations display cases. Server selects Vault1 and Exhibition4 as active; other 7 remain decoration. Positions are fixed.'
    plan['world_visual_scope']='Existing original pack meshes, shared BP_Loot case shell; no per-instance asset copies. Active case must be opened before pickup.'
    plan_path.write_text(json.dumps(plan,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    unreal.log('EXHIBIT_VARIATION_APPLY_PASS maps=3 candidates=180 new_blueprints=0')

if __name__=='__main__': main()
