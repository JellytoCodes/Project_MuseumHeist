"""Apply the authored security branches without rebuilding gallery content.

Canonical maps are edited through Unreal Editor only. The approved full builder
also calls decorate() so subsequent rebuilds retain the same presentation.
"""
import json
import math
import runpy
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
PLANS = json.loads((ROOT / 'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_text(encoding='utf-8'))['maps']


def decorate(builder, plan):
    if 'security_wing' not in plan:
        return
    gallery = runpy.run_path(str(ROOT / 'ProjectResources/Scripts/Editor/refine_museum_galleries.py'))
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    wing = plan['security_wing']

    def box(name, bounds, bottom, height, material='steel', collision='NoCollision'):
        x0,y0,x1,y1 = [v*100 for v in bounds]
        return gallery['box'](builder, name, ((x0+x1)/2,(y0+y1)/2,bottom*100+height*50),
                              (x1-x0,y1-y0,height*100), material,
                              folder='Theme/Security', collision=collision)

    def sign(name, location, yaw, caption, size):
        label = 'LDV2_'+plan['id']+'_SecuritySign_'+name
        actor = builder.by_label.get(label)
        if actor is None:
            actor = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(*location), unreal.Rotator(yaw=yaw))
            actor.set_actor_label(label)
            builder.register(actor)
        actor.set_actor_location(unreal.Vector(*location),False,False)
        actor.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
        builder.folder(actor,'Theme/Security')
        builder.mark_generated(actor)
        text = actor.get_component_by_class(unreal.TextRenderComponent)
        text.set_text(caption)
        text.set_world_size(size)
        text.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        text.set_text_render_color(unreal.Color(230,210,170,255))

    # Replace the visual floor finish only; walkable floor/collision stays intact.
    for key in ('EV','DT'):
        x0,y0,x1,y1 = plan['rooms'][key]['bounds']
        for ix in range(math.ceil((x1-x0)/4)):
            for iy in range(math.ceil((y1-y0)/4)):
                x,y=x0+ix*4,y0+iy*4
                box('SEC_Floor_'+key+'_'+str(ix)+'_'+str(iy),[x+.02,y+.02,min(x+4,x1)-.02,min(y+4,y1)-.02],.04,.02,'m03_floor')

    # Low opaque sill defines the map/collision footprint; narrow bars leave the
    # detained crew visible from inside the security wing, with a side opening.
    x0,y0,x1,y1=wing['grille']
    count=math.ceil((x1-x0)/.32)
    for i in range(count+1):
        x=x0+(x1-x0)*i/count
        box('SEC_Bar_'+str(i),[x-.025,y0,x+.025,y1],1.1,1.8,collision='BlockAll')
    for z in (1.08,2.78):
        box('SEC_Rail_'+str(z),[x0,y0-.02,x1,y1+.02],z,.12)
    # Bench against the rear wall, outside all detention capsule/rescue offsets.
    ax,ay,bx,by=plan['rooms']['DT']['bounds']
    box('SEC_Bench',[ax+1,ay+.2,bx-1,ay+.8],.4,.15,'worn_wood',collision='BlockAll')
    box('SEC_BenchBack',[ax+1,ay+.18,bx-1,ay+.3],.55,.65,'worn_wood')
    for x in (ax+1.2,bx-1.2):
        box('SEC_BenchLeg_'+str(x),[x-.06,ay+.3,x+.06,ay+.7],0,.4,collision='BlockAll')

    for key,caption in ((wing['entry'],'SECURITY'),(wing['inner'],'HOLDING')):
        d=next(d for d in plan['doors'] if d['id']==key)
        x,y=d['xy']; horizontal=d['axis']=='h'
        yaw=90 if horizontal else 180
        # Plate faces the approach, with a visible 4cm separation from plaster.
        b=[x-1.3,y+.17,x+1.3,y+.25] if horizontal else [x-.25,y-1.3,x-.17,y+1.3]
        box('SEC_SignBoard_'+key,b,3.08,.64,'tech')
        sign(key,(x*100 if horizontal else x*100-26,y*100+26 if horizontal else y*100,348),yaw,caption,24)
        sign(key+'_Small',(x*100 if horizontal else x*100-26,y*100+26 if horizontal else y*100,322),yaw,'STAFF ONLY' if key==wing['entry'] else 'AUTHORIZED ACCESS',12)
        for side in (-1,1):
            b=[x+side*1-.08,y-.18,x+side*1+.08,y+.18] if horizontal else [x-.18,y+side*1-.08,x+.18,y+side*1+.08]
            box('SEC_Frame_'+key+'_'+str(side),b,0,3.04,'burnished')
        b=[x-.88,y-.28,x+.88,y+.28] if horizontal else [x-.28,y-.88,x+.28,y+.88]
        box('SEC_Threshold_'+key,b,.065,.015,'copper')
        # Small local light; retains the night exposure and shadow-light budget.
        light=builder.point_light('LDV2_'+plan['id']+'_SecurityLight_'+key,
            (x*100,y*100,280),(150,190,235),{'M01':45,'M02':12,'M03':18}[plan['id']],360,'Lighting/Security')
        light.get_component_by_class(unreal.PointLightComponent).set_editor_property('cast_shadows',False)
        # Illuminate the sign face from the approach side, rather than relying
        # on light beneath the lintel to reveal lettering above it.
        light=builder.point_light('LDV2_'+plan['id']+'_SecuritySignLight_'+key,
            (x*100 if horizontal else x*100-100,y*100+100 if horizontal else y*100,340),
            (185,210,240),{'M01':65,'M02':20,'M03':28}[plan['id']],280,'Lighting/Security')
        light.get_component_by_class(unreal.PointLightComponent).set_editor_property('cast_shadows',False)
    gx=(wing['grille'][0]+wing['grille'][2])/2
    gy=wing['grille'][1]-.8
    light=builder.point_light('LDV2_'+plan['id']+'_SecurityHoldingLight',
        (gx*100,gy*100,260),(150,190,230),{'M01':110,'M02':35,'M03':45}[plan['id']],700,'Lighting/Security')
    light.get_component_by_class(unreal.PointLightComponent).set_editor_property('cast_shadows',False)


def apply():
    base=runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/build_museum_levels_v2.py'))
    gallery=runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/refine_museum_galleries.py'))
    actor_system=base['actor_subsystem']
    paths=[]
    for plan in PLANS:
        code=plan['id']; wing=plan['security_wing']
        builder=base['LevelBuilder'](code,base['MAPS'][code])
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

        def box(name,bounds,bottom,height,material,folder,collision='BlockAll'):
            x0,y0,x1,y1=[v*100 for v in bounds]
            return gallery['box'](builder,name,((x0+x1)/2,(y0+y1)/2,bottom*100+height*50),
                (x1-x0,y1-y0,height*100),material,folder=folder,collision=collision)

        for door_id in wing['close']:
            for stem in ('Door_','DoorTrim_'):
                label='LDV2_'+code+'_Gallery_'+stem+door_id
                a=builder.by_label.pop(label,None)
                if a:
                    if not actor_system.destroy_actor(a):raise RuntimeError('Could not remove closed lintel: '+label)
                    builder.actors.remove(a)
        for wall in (w for w in plan['walls'] if w['id'].startswith('SEC_')):
            f,a,b,t=[wall[k] for k in ('fixed','start','end','thickness')]
            bounds=[a,f-t/2,b,f+t/2] if wall['axis']=='h' else [f-t/2,a,f+t/2,b]
            mesh=box(wall['id'],bounds,0,wall['height'],'m01_wall','Architecture/Walls')
            builder.add_tags(mesh,'MuseumPlanWall_'+wall['id'])
            trim=[a,f-t*.575,b,f+t*.575] if wall['axis']=='h' else [f-t*.575,a,f+t*.575,b]
            for suffix,z,height in (('_Skirt',.02,.16),('_Cornice',wall['height']-.18,.12)):
                box(wall['id']+suffix,trim,z,height,{'M01':'gold','M02':'oak','M03':'nickel'}[code],'Architecture/Trim','NoCollision')
        for prop in (p for p in plan['props'] if p['id'].startswith('SEC_')):
            box(prop['id'],prop['bounds'],0,prop['height'],'m01_wall','Architecture/GalleryPartitions')
        for d in (d for d in plan['doors'] if d['id'] in (wing['entry'],wing['inner'])):
            x,y=d['xy'];w=d['width']
            b=[x-w/2,y-.2,x+w/2,y+.2] if d['axis']=='h' else [x-.2,y-w/2,x+.2,y+w/2]
            box('Door_'+d['id'],b,3,plan['walls'][0]['height']-3,'m01_wall','Architecture/ApprovedDoors')
            box('DoorTrim_'+d['id'],b,2.94,.06,{'M01':'gold','M02':'oak','M03':'nickel'}[code],'Architecture/Trim','NoCollision')

        # Change only the affected patrol. Other guard spawns/routes remain intact.
        index=wing['guard']; route_id=unreal.Name('LDV2_{}_Route_{:02}'.format(code,index+1))
        old=[a for a in builder.actors if isinstance(a,unreal.HeistGuardWaypoint) and a.get_editor_property('patrol_route_id')==route_id]
        for a in old:
            builder.by_label.pop(a.get_actor_label(),None)
            actor_system.destroy_actor(a);builder.actors.remove(a)
        points=plan['guards'][index]['polyline']
        if points[0]==points[-1]:points=points[:-1]
        for i,(x,y) in enumerate(points):
            a=actor_system.spawn_actor_from_class(unreal.HeistGuardWaypoint,unreal.Vector(x*100,y*100,25))
            a.set_actor_label('LDV2_{}_Route_{:02}_Point_{:02}'.format(code,index+1,i))
            a.set_editor_property('patrol_route_id',route_id);a.set_editor_property('patrol_order',i)
            builder.register(a);builder.mark_generated(a);builder.folder(a,'Gameplay/GuardRoutes')
        cam=plan['cameras'][-1]
        actor=builder.by_label['LDV2_{}_CCTV_{:02}'.format(code,len(plan['cameras']))]
        actor.set_actor_location(unreal.Vector(cam['xy'][0]*100,cam['xy'][1]*100,cam['z']*100),False,False)
        actor.set_actor_rotation(unreal.Rotator(pitch=cam['pitch'],yaw=cam['yaw']),False)
        actor.set_editor_property('detection_range',cam['range']*100)
        actor.set_editor_property('detection_half_angle_degrees',cam['angle']/2)
        # Table and all 25 pickup anchors move as one authored assembly.
        ex,ey=plan['evidence']
        table=builder.by_label['LDV2_'+code+'_Gallery_EvidenceTable']
        center=table.get_actor_bounds(False)[0]
        delta=unreal.Vector(ex*100-center.x,ey*100-center.y,0)
        for a in builder.actors:
            if a.get_actor_label().startswith(('LDV2_'+code+'_Gallery_EvidenceTable','LDV2_'+code+'_Gallery_EvidenceLeg_')):
                a.set_actor_location(a.get_actor_location()+delta,False,False)
        for slot in plan['evidence_slots']:
            a=builder.by_label['LDV2_'+code+'_Evidence_'+slot['id']]
            a.set_actor_location(unreal.Vector(*[v*100 for v in slot['xyz']]),False,False)
        decorate(builder,plan)
        if not unreal.EditorLoadingAndSavingUtils.save_map(world,base['MAPS'][code]['path']):
            raise RuntimeError('Security wing save failed: '+code)
        paths.append(base['MAPS'][code]['path'])
        unreal.log_warning('MH_SECURITY_APPLIED='+code)
    return paths


if __name__=='__main__':
    runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/build_approved_museum_layout.py'))['rebuild_saved_navigation'](apply())
