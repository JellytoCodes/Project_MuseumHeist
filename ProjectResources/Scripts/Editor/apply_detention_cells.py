"""Author the shared cell door shell, timing HUD and three security patrols in Editor."""
import json
import math
import runpy
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
BP_PATH = '/Game/Blueprints/World/Actors/Security/BP_DetentionDoor'


def author_shell():
    tasks=[]
    for source in sorted((ROOT/'ProjectResources/SourceArt/Audio/Detention').glob('*.wav')):
        task=unreal.AssetImportTask()
        task.filename=str(source); task.destination_path='/Game/Assets/Audio/Detention'
        task.automated=True; task.replace_existing=True; task.save=True
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    attenuation_path='/Game/Assets/Audio/Detention/SA_DetentionMetal'
    attenuation=unreal.load_asset(attenuation_path)
    if not attenuation:
        attenuation=unreal.AssetToolsHelpers.get_asset_tools().create_asset('SA_DetentionMetal','/Game/Assets/Audio/Detention',unreal.SoundAttenuation,unreal.SoundAttenuationFactory())
    settings=attenuation.get_editor_property('attenuation')
    settings.set_editor_property('attenuate',True)
    settings.set_editor_property('spatialize',True)
    settings.set_editor_property('attenuation_shape_extents',unreal.Vector(160,0,0))
    settings.set_editor_property('falloff_distance',2240)
    attenuation.set_editor_property('attenuation',settings)
    unreal.EditorAssetLibrary.save_loaded_asset(attenuation)
    bp = unreal.load_asset(BP_PATH)
    if not bp:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.HeistDetentionDoorActor)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_DetentionDoor', BP_PATH.rsplit('/', 1)[0], unreal.Blueprint, factory)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    objects = [(h, lib.get_object(lib.get_data(h))) for h in handles]
    hinge = next(h for h, obj in objects if obj and obj.get_name() == 'DoorHinge')
    cube = unreal.load_asset('/Engine/BasicShapes/Cube')
    steel = unreal.load_asset('/Game/Assets/StarterContent/Materials/M_Metal_Burnished_Steel')
    pieces = [('LeftStile', (3,0,0), (6,16,290)), ('RightStile', (177,0,0), (6,16,290)),
              ('TopRail',(90,0,141),(180,16,8)), ('BottomRail',(90,0,-141),(180,16,8)),
              ('MiddleRail',(90,0,-28),(180,12,8)), ('LockPlate',(154,-10,-8),(24,8,36)),
              ('LockHandle',(158,-18,-8),(6,10,24))]
    pieces += [('Bar%02d'%i, (18+i*18,0,0), (4,8,278)) for i in range(9)]
    for name, position, size in pieces:
        existing = next((obj for _, obj in objects if obj and obj.get_name().removesuffix('_GEN_VARIABLE') == name), None)
        if existing is None:
            handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=hinge, new_class=unreal.StaticMeshComponent, blueprint_context=bp))
            if not lib.is_valid(lib.get_data(handle)):
                raise RuntimeError(str(reason))
            subsystem.rename_subobject(handle, name)
            existing = lib.get_object(lib.get_data(handle))
        existing.set_static_mesh(cube)
        existing.set_material(0, steel)
        existing.set_relative_location(unreal.Vector(*position), False, False)
        existing.set_relative_scale3d(unreal.Vector(*[v/100 for v in size]))
        existing.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    for prop, sound in [('latch_sound','SW_DetentionLatch'),('failure_sound','SW_DetentionFailure'),('open_sound','SW_DetentionOpen')]:
        cdo.set_editor_property(prop, unreal.load_asset('/Game/Assets/Audio/Detention/'+sound))
    cdo.set_editor_property('sound_attenuation',attenuation)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)


def author_timing_hud():
    tools = unreal.get_default_object(unreal.UMGToolSet)
    paths = [p for p in unreal.EditorAssetLibrary.list_assets('/Game/Blueprints/UI', True, False) if p.split('/')[-1].split('.')[0] == 'WBP_InteractionPrompt']
    assert len(paths) == 1
    bp = unreal.load_asset(paths[0])
    widgets = {str(i.widget_name): i.widget for i in tools.call_method('GetWidgets',(bp,)).widgets}
    def add(cls, name, parent):
        return widgets.get(name) or tools.call_method('AddWidget',(bp,cls.static_class(),name,parent,-1)).widget
    size = add(unreal.SizeBox,'DetentionTimingContainer',widgets['Overlay_0'])
    size=tools.call_method('MoveWidget',(bp,size,widgets['Overlay_0'],-1)).widget
    timing_slot=size.slot
    timing_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    timing_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    size.set_render_translation(unreal.Vector2D(0,60))
    size.set_width_override(320); size.set_height_override(24)
    size.set_visibility(unreal.SlateVisibility.COLLAPSED)
    canvas = add(unreal.CanvasPanel,'DetentionTimingCanvas',size)
    for name, x, y, width, height, color in [
        ('DetentionTimingTrack',0,4,320,16,(.025,.024,.022,1)),
        ('DetentionTimingWindow',180,4,88,16,(.15,.5,.28,1)),
        ('DetentionTimingCursor',0,0,4,24,(.92,.82,.59,1))]:
        image = add(unreal.Image,name,canvas)
        image.set_color_and_opacity(unreal.LinearColor(*color))
        slot = image.slot
        slot.set_position(unreal.Vector2D(x,y)); slot.set_size(unreal.Vector2D(width,height))
    assert tools.call_method('CompileWidgetBlueprint',(bp,))
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)


def decorate(builder, plan):
    """Called by the canonical full builder and the focused security-wing update."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    gallery = runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/refine_museum_galleries.py'))
    wing = plan['security_wing']; code = plan['id']
    x,y = wing['opening']; left, bottom, right, _ = plan['rooms']['DT']['bounds']
    def box(name, x0, x1, z, height):
        return gallery['box'](builder,name,((x0+x1)*50,y*100,z*100+height*50),
            ((x1-x0)*100,24,height*100),'steel',folder='Theme/Security',collision='BlockAll')
    # Complete both side returns. Their gaps are narrower than a player capsule.
    spans = [(wing['grille'][2],x-.9),(x+.9,right)]
    for side,(a,b) in enumerate(spans):
        if b-a <= .01: continue
        for i in range(math.ceil((b-a)/.28)+1):
            bx=a+(b-a)*i/math.ceil((b-a)/.28)
            box('SEC_DoorReturn_%d_%d'%(side,i),bx-.025,bx+.025,0,2.9)
        box('SEC_DoorReturnTop_%d'%side,a,b,2.78,.12)
    box('SEC_DoorLintel',x-.94,x+.94,2.9,plan['walls'][0]['height']-2.9)
    label='LDV2_'+code+'_DetentionDoor'
    door=builder.by_label.get(label)
    if not door:
        cls=unreal.load_class(None,BP_PATH+'.BP_DetentionDoor_C')
        assert cls, 'Author BP_DetentionDoor before rebuilding levels'
        door=actors.spawn_actor_from_class(cls,unreal.Vector(x*100,y*100,145))
        door.set_actor_label(label); builder.register(door)
    builder.mark_generated(door); builder.folder(door,'Gameplay/Security')
    door.set_actor_location(unreal.Vector(x*100,y*100,145),False,False)
    bounds=door.get_editor_property('cell_bounds')
    bounds.set_box_extent(unreal.Vector((right-left)*50-16,(y-bottom)*50-12,220),False)
    bounds.set_relative_location(unreal.Vector(((left+right)/2-x)*100,((bottom+y)/2-y)*100,55),False,False)
    # Preserve the authored guard count; pin this guard ahead of player-count culling.
    index=wing['guard']; route=unreal.Name('LDV2_{}_Route_{:02}'.format(code,index+1))
    guard=builder.by_label['LDV2_{}_Guard_{:02}'.format(code,index+1)]
    builder.add_tags(guard,'HeistDetentionPatrol')
    patrol=guard.get_component_by_class(unreal.HeistPatrolPathComponent)
    patrol.set_editor_property('waypoint_wait_duration',.4)
    patrol.set_editor_property('acceptance_radius',35)
    patrol.set_editor_property('loop_patrol',True)
    px,py=plan['guards'][index]['polyline'][0]
    guard.set_actor_location(unreal.Vector(px*100,py*100,guard.get_actor_location().z),False,False)
    waypoints=sorted([a for a in builder.actors if isinstance(a,unreal.HeistGuardWaypoint) and a.get_editor_property('patrol_route_id')==route],key=lambda a:a.get_editor_property('patrol_order'))
    for wp in waypoints: wp.set_editor_property('wait_duration_override',-1)
    waypoints[-1].set_editor_property('wait_duration_override',2.5)
    unreal.log_warning('MH_DETENTION_CELL_APPLIED='+code)


if __name__ == '__main__':
    try:
        author_shell()
        author_timing_hud()
        if '-DetentionAssetsOnly' in unreal.SystemLibrary.get_command_line():
            unreal.EditorPythonScripting.set_keep_python_script_alive(True)
        else:
            paths=runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/apply_security_wings.py'))['apply']()
            runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/build_approved_museum_layout.py'))['rebuild_saved_navigation'](paths)
    except Exception:
        import traceback
        unreal.log_error(traceback.format_exc())
        unreal.SystemLibrary.quit_editor()
