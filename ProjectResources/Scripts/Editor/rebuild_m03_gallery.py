"""Apply the user-approved M03 gallery plan through Unreal Editor only.

Preserves gameplay identities, M01/M02, and the imported source map. This is a
focused M03 authoring operation, not a replacement for the retired map generators.
Requires -EnablePlugins=GeometryScripting for the derived upper facade asset.
"""
import collections
import hashlib
import json
import math
import shutil
import time
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / 'Saved/Automation/M03Rebuild'
OUT.mkdir(parents=True, exist_ok=True)
MAP = '/Game/Maps/M03_GlasshousePrototype'
SOURCE = '/Game/AIUE5_vol10_01/maps/AIUE_vol10_01'
PACK = '/Game/AIUE5_vol10_01/Mesh/'
DERIVED = '/Game/Assets/Environment/M03Gallery'
TAG = 'M03GalleryRebuild20260927'
A = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
E = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
EL = unreal.EditorAssetLibrary
META = json.loads((ROOT/'ProjectResources/SourceArt/Canvas/canvas_uv.json').read_text())
WALLS = []
DOORS = []
REPORT = {'map': MAP, 'source': SOURCE, 'saved': False}
NEVER = unreal.PropertyAccessChangeNotifyMode.NEVER

def setp(o,k,v): o.set_editor_property(k,v,notify_mode=NEVER)
def vec(v): return [float(v.x),float(v.y),float(v.z)]
def integer(v): return unreal.Vector(*(math.trunc(q) for q in vec(v)))
def pos(x,y,z=0): return unreal.Vector(round((x-30)*100),round((22-y)*100),z)
def plan(v): return [v.x/100+30,22-v.y/100,v.z]
def move(a,p,yaw=0):
    a.modify(); a.set_actor_location(integer(p),False,True)
    a.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
    a.set_actor_scale3d(unreal.Vector(1,1,1))
def named(a,name,folder):
    a.set_actor_label(name);a.set_folder_path('M03Gallery/'+folder)
    a.tags=list(a.tags)+[TAG]
    return a
def mesh(name):
    m=unreal.load_asset(PACK+'SM_AI_vol10_01_'+name)
    assert m,name
    return m
def spawn_mesh(name,m,location,rotation=None,scale=None,collision=True,folder='Architecture'):
    a=A.spawn_actor_from_class(unreal.StaticMeshActor,integer(location),rotation or unreal.Rotator())
    named(a,'M03G_'+name,folder);c=a.static_mesh_component;c.set_static_mesh(m)
    if scale:c.set_world_scale3d(unreal.Vector(*scale))
    c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    return a
def fit(name,m,center,size,yaw=0,folder='Architecture',collision=True):
    b=m.get_bounds();ext=b.box_extent
    a=spawn_mesh(name,m,unreal.Vector(),unreal.Rotator(yaw=yaw),
        [size[i]/max(.001,2*q) for i,q in enumerate(vec(ext))],collision,folder)
    c,e=a.get_actor_bounds(False)
    a.set_actor_location(integer(center-c),False,True)
    return a
def wall(name,a,b,height=500,bottom=0,thickness=24):
    # A finished gallery wall panel, tiled along long runs instead of default cubes.
    length=math.dist(a,b);count=max(1,math.ceil(length/4.4))
    horizontal=abs(a[1]-b[1])<.001
    for i in range(count):
        u=(i+.5)/count;x=a[0]+(b[0]-a[0])*u;y=a[1]+(b[1]-a[1])*u
        fit(name+'_%02d'%i,mesh('walls_4_1'),pos(x,y,bottom+height/2),
            [thickness,length/count*100+1,height],90 if horizontal else 0)
    WALLS.append(dict(name=name,a=a,b=b,height=height,bottom=bottom,thickness=thickness))
def opening(name,a,b,height=500,clear_height=300):
    DOORS.append(dict(name=name,a=a,b=b,width=math.dist(a,b)*100,clear_height=clear_height))
    if height>clear_height:wall(name+'_Lintel',a,b,height-clear_height,clear_height)
def span(name,axis,fixed,start,end,gaps=(),height=500):
    pt=lambda v:[v,fixed] if axis=='x' else [fixed,v]
    at=start
    for i,(lo,hi) in enumerate(sorted(gaps)):
        if lo>at:wall(name+'_%02d'%i,pt(at),pt(lo),height)
        opening(name+'_Door%02d'%i,pt(lo),pt(hi),height)
        at=hi
    if at<end:wall(name+'_End',pt(at),pt(end),height)
def panel_grid(name,bounds,z,ceiling=False):
    x0,y0,x1,y1=bounds
    nx=math.ceil((x1-x0)/4);ny=math.ceil((y1-y0)/4)
    m=mesh('stone_tile_1_1')
    for ix in range(nx):
        for iy in range(ny):
            width=(x1-x0)/nx;depth=(y1-y0)/ny
            a=fit(name+'_%02d_%02d'%(ix,iy),m,pos(x0+(ix+.5)*width,y0+(iy+.5)*depth,z),
                [width*100,depth*100,2],folder='Ceilings' if ceiling else 'Floors')
            if ceiling:
                c=a.static_mesh_component
                white=mesh('walls_4_1').get_material(0)
                c.set_material(0,white)
                # The slab includes both surfaces; no extra plane is layered over it.
def source_snapshot():
    assert unreal.EditorLoadingAndSavingUtils.load_map(SOURCE)
    rows=[]
    for a in A.get_all_level_actors():
        cls=a.get_class().get_path_name()
        if not isinstance(a,unreal.StaticMeshActor) and 'BP_AI_vo10_01_roof_' not in cls:continue
        t=a.get_actor_transform();r=t.rotation.rotator()
        row=dict(label=a.get_actor_label(),cls=cls,location=vec(t.translation),rotation=[r.pitch,r.yaw,r.roll],scale=vec(t.scale3d),components=[])
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            row['components'].append(dict(mesh=c.static_mesh.get_path_name() if c.static_mesh else '',materials=[m.get_path_name() if m else None for m in c.get_materials()]))
        rows.append(row)
    return rows
def upper_facade():
    path=DERIVED+'/SM_GalleryUpperFacade'
    if EL.does_asset_exist(path):return unreal.load_asset(path)
    raise RuntimeError('Missing existing upper facade: asset duplication requires explicit user approval before creating a replacement: '+path)
def core_roof(rows):
    keep=('_roof_','_ventilation_','_cable_')
    for row in rows:
        is_bp='BP_AI_vo10_01_roof_' in row['cls']
        paths=[c['mesh'] for c in row['components']]
        if not is_bp and not any(any(k in p for k in keep) for p in paths):continue
        l=row['location'];r=row['rotation'];p=unreal.Vector(l[0]+2513,l[1]+14,l[2])
        if is_bp:
            a=A.spawn_actor_from_class(unreal.load_class(None,row['cls']),integer(p),unreal.Rotator(pitch=r[0],yaw=r[1],roll=r[2]))
            named(a,'M03G_'+row['label'],'OriginalRoof');a.set_actor_scale3d(unreal.Vector(*row['scale']))
        else:
            co=row['components'][0];m=unreal.load_asset(co['mesh'])
            a=spawn_mesh(row['label'],m,p,unreal.Rotator(pitch=r[0],yaw=r[1],roll=r[2]),row['scale'],False,'OriginalRoof')
            for i,mat in enumerate(co['materials']):
                if mat:a.static_mesh_component.set_material(i,unreal.load_asset(mat))
    spawn_mesh('UpperFacade',upper_facade(),unreal.Vector(-2983+2513,-463+14,0),collision=False,folder='OriginalRoof')
def architecture():
    # Exact approved topology. Openings are 2m, detention door is 1.8m.
    span('WestExterior','y',4,4,40,height=400)
    span('NorthExterior','x',4,4,47,height=500)
    span('EastStaffOuter','y',47,4,8,height=400)
    span('NorthSecurity','x',8,47,57,height=400)
    span('EastExterior','y',57,8,33,height=500)
    span('SouthHall','x',33,8,43,[(12,14)],height=595)
    span('SouthEast','x',33,43,57,height=500)
    span('EntrySouth','x',40,4,18,height=400)
    span('EntryEast','y',18,33,40,height=400)
    span('HallWest','y',8,20,33,[(25,27)],height=595)
    span('AnnexWest','y',8,8,20,height=500)
    span('EntryWest','y',8,33,40,[(36,38)],height=400)
    span('AnnexNorth','x',8,8,43,[(13,15),(27,29)],height=500)
    span('A_B','y',20,8,20,[(15,17)],height=500)
    span('B_C','y',32,8,20,[(11,13)],height=500)
    span('HallNorth','x',20,8,43,[(11,13)],height=595)
    span('AnnexEast','y',43,8,20,[(15,17)],height=500)
    span('HallEast','y',43,20,33,[(25,27)],height=595)
    span('SecurityEntry','y',47,8,20,[(10,12),(16,18)],height=400)
    span('VaultEntry','y',47,20,33,[(25,27)],height=500)
    span('GuardEvidence','x',14,47,57,height=400)
    span('SecurityVault','x',20,47,57,height=500)
    # A true separated cell: closed sides + barred door, no low climb-over gap.
    span('EvidenceCell','y',52,14,20,[(16.1,17.9)],height=400)
    wall('HallPartition',[23.5,23],[23.5,30],450,thickness=40)
    wall('HallEndNorth',[33.5,20],[33.5,24.5],495,thickness=40)
    wall('HallEndSouth',[33.5,29],[33.5,33],495,thickness=40)
    wall('SpecialExhibition_L1',[25,14],[29,14],320)
    wall('SpecialExhibition_L2',[29,14],[29,16],320)
    wall('VaultAlcove_North',[55,24.58],[57,24.58],320)
    wall('VaultAlcove_South',[55,28.42],[57,28.42],320)
    rooms=[('Hall',[8,20,43,33],None),('A',[8,8,20,20],500),('B',[20,8,32,20],500),('C',[32,8,43,20],500),
        ('StaffNorth',[4,4,47,8],400),('StaffWest',[4,8,8,40],400),('StaffEast',[43,8,47,33],400),
        ('Entry',[8,33,18,40],400),('Guard',[47,8,57,14],400),('Evidence',[47,14,52,20],400),('Cell',[52,14,57,20],400),('Vault',[47,20,57,33],500)]
    for name,bounds,ceiling in rooms:
        panel_grid('Floor_'+name,bounds,-2)
        if ceiling:panel_grid('Ceiling_'+name,bounds,ceiling+1,True)
    REPORT['rooms']=[dict(name=n,bounds=b,ceiling=c) for n,b,c in rooms]
    REPORT['walls']=WALLS;REPORT['doors']=DOORS
def canvas_component(a):
    return next((c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name().startswith('SM_Canvas_Painting_')),None)
def canvas_meta(c):return META[int(c.static_mesh.get_name().split('_')[-1][:2])-1]
def canvas_size(c,scale):
    m=canvas_meta(c);return [(m['max'][1-m['thin_axis']]-m['min'][1-m['thin_axis']])*scale,(m['max'][2]-m['min'][2])*scale]
def canvas_place(a,center,normal,scale):
    c=canvas_component(a);m=canvas_meta(c);yaw=math.degrees(math.atan2(normal.y,normal.x))-90*m['thin_axis']
    if isinstance(a,unreal.HeistPaintingDisplayCaseActor):
        move(a,unreal.Vector(center.x,center.y,0),math.degrees(math.atan2(-normal.y,-normal.x)))
        c.set_world_rotation(unreal.Rotator(yaw=yaw),False,True)
        c.set_world_scale3d(unreal.Vector(scale,scale,scale))
    else:
        move(a,unreal.Vector(),yaw);a.set_actor_scale3d(unreal.Vector(scale,scale,scale))
    local=unreal.Vector(*[(lo+hi)/2 for lo,hi in zip(m['front_min'],m['front_max'])])
    current=c.get_world_transform().transform_location(local)
    c.set_world_location(c.get_world_location()+center-current,False,True)
    if isinstance(a,unreal.HeistPaintingDisplayCaseActor):
        # Actor origin remains on the wall at floor height; box sits 70cm in front.
        box=a.get_component_by_class(unreal.BoxComponent)
        box.set_box_extent(unreal.Vector(60,60,80),False)
        box.set_world_location(unreal.Vector(center.x,center.y,100)+normal*70,False,True)
    else:a.set_actor_location(integer(a.get_actor_location()),False,True)
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    a.set_folder_path('M03Gallery/Paintings')
def pattern_positions(kind,count):
    # Coordinates in frame widths / heights. These differ structurally, not by labels.
    if kind=='Solo':return [(0,0)]
    if kind=='Serial':return [(i-(count-1)/2,0) for i in range(count)]
    if kind=='Grid':return [(i%3-1,i//3-.5) for i in range(count)]
    if kind=='TwinPairs':return [(-1.7,0),(-.7,0),(.7,0),(1.7,0)][:count]
    if kind=='Pendant':return [(-.65,0),(.65,0),(0,1)][:count]
    if kind=='Asymmetric':return [(-1,0),(.6,-.4),(.6,.7),(-1,1.1)][:count]
    if kind=='Triptych':return [(-1.15,0),(0,0),(1.15,0)][:count]
    if kind=='Baseline':return [(i-(count-1)/2,(-.25 if i%2 else .25)) for i in range(count)]
    if kind=='SideStack':return [(-.8,0),(.65,-.6),(.65,.6)][:count]
    if kind=='Vertical':return [(0,i-(count-1)/2) for i in range(count)]
    if kind=='Salon':return [(-1,0),(0,.65),(1,0),(-.65,-1),(.65,-1),(0,1.65)][:count]
    return [(i%2-.5,i//2-.5) for i in range(count)]
def paintings(old,lasers):
    cases=sorted([a for a in old if isinstance(a,unreal.HeistPaintingDisplayCaseActor)],key=lambda a:a.get_actor_label())
    assert len(cases)==20
    target=next(a for a in cases if a.get_actor_label().endswith('_Target'))
    high=next(a for a in cases if a.get_actor_label().endswith('_HighValue'))
    protected=[l.get_editor_property('protected_painting_case') for l in lasers]
    other=next(a for a in protected if a!=high and a!=target)
    remainder=[a for a in cases if a not in (high,other,target)]
    # x,y, inward normal in plan coordinates, hanging span, ceiling height.
    anchors=[(17,20.16,0,1,7,595),(29,20.16,0,1,7,595),(17,32.84,0,-1,7,595),(29,32.84,0,-1,7,595),
      (23.27,26.5,-1,0,6,595),(42.84,30,-1,0,5.2,595),
      (10.5,8.16,0,1,4,500),(17.5,8.16,0,1,4,500),(8.16,14,1,0,9,500),(19.84,11.5,-1,0,5.5,500),
      (24,8.16,0,1,5.6,500),(26,19.84,0,-1,9,500),(20.16,11.5,1,0,5.5,500),(31.84,16.5,-1,0,5.3,500),
      (42.84,11.5,-1,0,5.5,500),(37.5,19.84,0,-1,8,500),(32.16,16.5,1,0,5.3,500),
      (37.5,8.16,0,1,8,500),(52,32.84,0,-1,8,500),(56.84,26.5,-1,0,9,500)]
    ordered=remainder+[target,high,other]
    groups={}
    for a in old:
        if not canvas_component(a):continue
        tag=next((str(t) for t in a.tags if str(t).startswith('MuseumHangingGroup_')),None)
        if tag:groups.setdefault(tag,[]).append(a)
    used=[]; placed=[]
    for i,(case,anchor) in enumerate(zip(ordered,anchors)):
        x,y,nx,ny,span_m,ceiling=anchor;n=unreal.Vector(nx,-ny,0);h=unreal.Vector(-n.y,n.x,0)
        tag=next(str(t) for t in case.tags if str(t).startswith('MuseumHangingGroup_'))
        members=groups.get(tag,[case]);deco=sorted([a for a in members if a!=case],key=lambda a:a.get_actor_label())
        kind=next((str(t).replace('MuseumHangingPattern_','') for t in case.tags if str(t).startswith('MuseumHangingPattern_')),'Solo')
        count=min(len(deco)+1,6 if kind in ('Grid','Salon') else 4 if kind in ('TwinPairs','Asymmetric','Serial','Baseline') else 3 if kind!='Solo' else 1)
        points=pattern_positions(kind,count);members=[case]+deco[:len(points)-1]
        # One-decimal uniform scales; keep small meshes legible without stretching.
        scales=[]
        for a in members:
            c=canvas_component(a);w,hh=canvas_size(c,1)
            s=round(max(1,145/max(w,hh),92/min(w,hh)),1);scales.append(s)
        widths=[canvas_size(canvas_component(a),s)[0] for a,s in zip(members,scales)]
        heights=[canvas_size(canvas_component(a),s)[1] for a,s in zip(members,scales)]
        step_x=max(widths)+24;step_z=max(heights)+24
        # Room proportions can require fewer decorative frames, never smaller active art.
        while len(points)>1 and ((max(p[0] for p in points)-min(p[0] for p in points))*step_x+max(widths)>span_m*100 or (max(p[1] for p in points)-min(p[1] for p in points))*step_z+max(heights)>min(ceiling-160,390)):
            step_x*=.96;step_z*=.96
            if step_x<max(widths)+12 or step_z<max(heights)+12:
                # Compact the group by using 0.1 smaller uniform scales, bounded at 1m long side.
                changed=False
                for j,a in enumerate(members):
                    w,hh=canvas_size(canvas_component(a),scales[j]-.1)
                    if max(w,hh)>=115 and min(w,hh)>=70:scales[j]=round(scales[j]-.1,1);changed=True
                widths=[canvas_size(canvas_component(a),s)[0] for a,s in zip(members,scales)];heights=[canvas_size(canvas_component(a),s)[1] for a,s in zip(members,scales)]
                step_x=max(widths)+20;step_z=max(heights)+20
                if not changed:break
        zlow=min(p[1]*step_z-hh/2 for p,hh in zip(points,heights));zhigh=max(p[1]*step_z+hh/2 for p,hh in zip(points,heights))
        center_z=max(185,70-zlow);center_z=min(center_z,ceiling-115-zhigh)
        for a,(dx,dz),s in zip(members,points,scales):
            center=pos(x,y,center_z+dz*step_z)+h*dx*step_x
            if i==18:
                # A six-piece salon fits the low vault ceiling with an uneven
                # two-row composition; never push its bottom frames below floor.
                sx,sz=[(-200,155),(-20,170),(160,145),(-200,305),(-20,310),(160,280)][members.index(a)]
                center=pos(x,y,sz)+h*sx
            canvas_place(a,center,n,s);used.append(a)
            placed.append(dict(label=a.get_actor_label(),group=i,pattern=kind,center=vec(center),normal=vec(n),scale=s,active=a==case,size=canvas_size(canvas_component(a),s)))
        fixture(i,pos(x,y,ceiling)+n*140,pos(x,y,center_z),kind)
    # Independent decorative walls keep lighting from identifying every stealable work.
    spare=[a for a in old if canvas_component(a) and a not in used]
    extra=[(9.86,29,1,0),(38,20.16,0,1),(8.16,36,1,0),(49,8.16,0,1)]
    for i,a in enumerate(spare):
        if i>=len(extra)*3:A.destroy_actor(a);continue
        x,y,nx,ny=extra[i//3];n=unreal.Vector(nx,-ny,0);h=unreal.Vector(-n.y,n.x,0)
        # Free-standing source display wall for the independent group inside the hall.
        if i//3==0 and i%3==0:wall('IndependentExhibit',[9.7,27],[9.7,31],320)
        c=canvas_component(a);w,hh=canvas_size(c,1);s=round(max(1,120/max(w,hh)),1)
        canvas_place(a,pos(x,y,190)+h*((i%3-1)*160),n,s)
    REPORT['paintings']=placed
    REPORT['patterns']=sorted(set(r['pattern'] for r in placed))
    return ordered,high,other
def fixture(index,ceiling,target,kind='Solo',passage=False):
    typ='DropCeiling_01a' if passage else ('Droplights_01c' if kind in ('Salon','Grid') else 'Droplights_01b' if kind in ('Serial','TwinPairs','Triptych') else 'Droplights_01a')
    path='/Game/Blueprints/Environment/BP_HeistLight_'+typ
    a=A.spawn_actor_from_class(unreal.load_class(None,path+'.BP_HeistLight_'+typ+'_C'),integer(ceiling))
    named(a,'M03G_Light_%s_%02d'%('Passage' if passage else 'Art',index),'Lights')
    c=a.get_component_by_class(unreal.SpotLightComponent)
    # Type owns mesh and hanging offset. Only aim and light range are per placement.
    # SCS ComponentToWorld can still be identity on the spawn tick. Derive the
    # source from the fixed Blueprint suspension instead of that stale cache.
    source=ceiling+unreal.Vector(0,0,-156 if passage else -104);direction=target-source
    c.modify();c.set_relative_rotation(unreal.MathLibrary.make_rot_from_x(direction),False,True)
    setp(c,'intensity',12.5 if passage else 42.5 if kind in ('Grid','Salon') else 35.0)
    setp(c,'intensity_units',unreal.LightUnits.CANDELAS)
    setp(c,'light_color',unreal.Color(255,255,255,255))
    setp(c,'indirect_lighting_intensity',.65)
    setp(c,'attenuation_radius',max(600,direction.length()+180))
    setp(c,'outer_cone_angle',48 if passage else 52)
    setp(c,'inner_cone_angle',30 if passage else 34)
    setp(c,'use_temperature',True);setp(c,'temperature',3600 if passage else 4100)
    assert len(a.get_components_by_class(unreal.LightComponent))==1
def furniture():
    for i,(x,y,yaw) in enumerate([(16,28,0),(28,25,90),(17,17,0),(24,18,0),(37,17,0),(54,19,0)]):
        spawn_mesh('Bench_%02d'%i,mesh('bench_1_1'),pos(x,y,0),unreal.Rotator(yaw=yaw),folder='Furniture')
    for i,(name,x,y,yaw,scale) in enumerate([('sculpt_2_1',21.5,22.5,0,1),('sculpt_10_1',40,22,0,.7),('sculpt_4_1',35.5,30.8,0,.6),('sculpt_3_1',16,10,0,.7)]):
        spawn_mesh('Landmark_%02d'%i,mesh(name),pos(x,y,0),unreal.Rotator(yaw=yaw),[scale]*3,False,'InstallationArt')
    # Evidence uses actual table assets; anchors span two accessible tables.
    for i,(x,y) in enumerate([(49.5,15),(49.5,19),(50,10)]):
        spawn_mesh('Table_%02d'%i,mesh('table_1_1'),pos(x,y,0),scale=[2,2,1],folder='Furniture')
    for i,(x,y) in enumerate([(6,12),(6,22),(6,32),(12,6),(22,6),(33,6),(43,6),(45,13),(45,23),(13,36),(50,11),(50,17),(55,17),(52,23)]):
        fixture(i,pos(x,y,400 if i!=13 else 500),pos(x,y,0),passage=True)
def gameplay(old,ordered,high,other):
    guards=sorted([a for a in old if isinstance(a,unreal.HeistGuardCharacter)],key=lambda a:a.get_actor_label())
    routes=[[(15,25),(20.5,25),(20.5,31),(26,31),(29.5,27),(37,27),(40,30),(37,27),(29.5,27),(26,31),(20.5,31),(15,31)],
      [(12,14),(16,16),(23,16),(23,12),(35,12),(38,15),(35,12),(23,12),(23,16),(16,16)],
      [(6,27),(6,6),(14,6),(22,6),(28,6),(37,6),(45,6),(45,12),(45,6),(28,6),(14,6),(6,6)],
      [(45,14.5),(45,17),(49,17),(50.5,17),(49,17),(45,17),(45,22),(45,17)]]
    for a in list(A.get_all_level_actors()):
        if isinstance(a,unreal.HeistGuardWaypoint):A.destroy_actor(a)
    for g,route in zip(guards,routes):
        patrol=g.get_component_by_class(unreal.HeistPatrolPathComponent);rid=patrol.get_editor_property('patrol_route_id')
        for i,(x,y) in enumerate(route):
            wp=A.spawn_actor_from_class(unreal.HeistGuardWaypoint,pos(x,y,100));named(wp,str(rid)+'_Point_%02d'%i,'Patrol')
            setp(wp,'patrol_route_id',rid);setp(wp,'patrol_order',i);setp(wp,'wait_duration_override',2.5 if g==guards[-1] and i==3 else .4)
        move(g,pos(*route[0],98));g.set_folder_path('M03Gallery/Guards')
    starts=sorted([a for a in old if isinstance(a,unreal.PlayerStart)],key=lambda a:a.get_actor_label())
    for a,(x,y) in zip(starts,[(12,37),(14,37),(12,38.5),(14,38.5)]):move(a,pos(x,y,100),90)
    vent=next(a for a in old if isinstance(a,unreal.HeistVentActor));move(vent,pos(16.8,38.7,50),180)
    vent.set_folder_path('M03Gallery/Extraction')
    # Keep the functional vent shell; use the imported grille for its visible surface.
    vc=vent.get_component_by_class(unreal.StaticMeshComponent);vc.set_static_mesh(mesh('ventilation_2_1'));vc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    vb=vc.static_mesh.get_bounds();vc.set_relative_scale3d(unreal.Vector(*[95/max(1,2*q) for q in vec(vb.box_extent)]))
    door=next(a for a in old if isinstance(a,unreal.HeistDetentionDoorActor));move(door,pos(52,17,145),90);door.set_folder_path('M03Gallery/Detention')
    cb=door.get_editor_property('cell_bounds');cb.set_box_extent(unreal.Vector(280,230,200),False)
    cb.set_world_location(pos(54.5,17,200),False,True)
    ds=sorted([a for a in old if 'HeistDetentionSpawn' in [str(t) for t in a.tags]],key=lambda a:a.get_actor_label())
    for a,(x,y) in zip(ds,[(53.5,15.5),(55.3,15.5),(53.5,18.5),(55.3,18.5)]):move(a,pos(x,y,100),180)
    es=sorted([a for a in old if 'HeistEvidenceSlot' in [str(t) for t in a.tags]],key=lambda a:a.get_actor_label())
    for i,a in enumerate(es):move(a,pos(48.5+(i%5)*.5,14.6+(i//5)*.2 if i<15 else 18.6+((i-15)//5)*.4,82));a.set_folder_path('M03Gallery/Evidence')
    cameras=sorted([a for a in old if isinstance(a,unreal.HeistSecurityCameraActor)],key=lambda a:a.get_actor_label())
    camera_pos=[(18.9,20.3,-120),(42.6,28,155),(8.4,9,-35),(31.6,19.6,140),(42.6,8.5,-125),(6,8,-90),(45,19,90),(56.6,20.4,-135)]
    for a,(x,y,yaw) in zip(cameras,camera_pos):
        move(a,pos(x,y,280),yaw);a.set_actor_rotation(unreal.Rotator(pitch=-14,yaw=yaw),False);a.set_folder_path('M03Gallery/CCTV')
    lasers=sorted([a for a in old if isinstance(a,unreal.HeistLaserBarrierActor)],key=lambda a:a.get_actor_label())
    buttons=sorted([a for a in old if isinstance(a,unreal.HeistSecurityHoldButtonActor)],key=lambda a:a.get_actor_label())
    # Two independent optional protected paintings, matching the two-piece vault.
    for a in lasers[2:]+buttons[2:]:A.destroy_actor(a)
    for i,(laser,button,case,x,y) in enumerate(zip(lasers[:2],buttons[:2],[high,other],[47,55],[26,26.5])):
        move(laser,pos(x,y,0));setp(laser,'protected_painting_case',case)
        trigger=laser.get_editor_property('beam_trigger_component');trigger.set_box_extent(unreal.Vector(10,100 if i==0 else 180,120),False)
        move(button,pos(45.6 if i==0 else 53.5,24 if i==0 else 24.2,105),180 if i==0 else 0)
        setp(button,'linked_laser_barrier',laser);laser.set_folder_path('M03Gallery/Lasers');button.set_folder_path('M03Gallery/Lasers')
    loot=sorted([a for a in old if isinstance(a,unreal.HeistLootSpawnPoint)],key=lambda a:a.get_actor_label())
    spots=[(16,28),(28,25),(17,17),(24,18),(37,17),(15.5,9),(36,30),(40,22),(50,10),(16,35),(50,30),(54,30)]
    for i,(a,(x,y)) in enumerate(zip(loot,spots)):
        if i>=5 and i!=8:
            spawn_mesh('LootTable_%02d'%i,mesh('table_1_1'),pos(x,y,0),folder='Furniture')
        move(a,pos(x,y,52 if i<5 else 85));a.set_folder_path('M03Gallery/Loot')
    REPORT['routes']=routes
    REPORT['case_ids']=[str(a.get_display_case_id()) for a in ordered]
    REPORT['required_target']=next(dict(case=str(a.get_display_case_id()),position=vec(a.get_actor_location())) for a in ordered if a.get_actor_label().endswith('_Target'))
    REPORT['exit']=vec(vent.get_actor_location())
def apply():
    upper_facade()  # Fail before touching the map if the existing derived asset is missing.
    protected=['Content/Maps/M01_ClassicalPrototype.umap','Content/Maps/M02_MoonlitPrototype.umap','Content/AIUE5_vol10_01/maps/AIUE_vol10_01.umap']
    hashes={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in protected}
    backup=OUT/'Before/Content/Maps/M03_GlasshousePrototype.umap'
    assert backup.exists(),'Read-only inspection and backup required before applying.'
    rows=source_snapshot()
    world=unreal.EditorLoadingAndSavingUtils.load_map(MAP);old=list(A.get_all_level_actors())
    assert not any(isinstance(a,unreal.HeistPlayerCharacter) for a in old),'Do not run during PIE'
    assert not any(TAG in [str(t) for t in a.tags] for a in old),'Approved rebuild already applied. Make focused edits instead of regenerating this authored map.'
    # Keep authored gameplay instances and painting materials; replace the approved environment.
    preserve=[]
    for a in old:
        cls=a.get_class().get_name()
        keep=(canvas_component(a) is not None or cls in ['BP_Guard_C','BP_Vent_C','BP_DetentionDoor_C','BP_LootSpawnPoint_C','BP_PaintingDisplayCase_C','BP_SecurityCamera_C','BP_LaserBarrier_C','BP_SecurityHoldButton_C','TargetPoint','PlayerStart','NavMeshBoundsVolume','RecastNavMesh','DirectionalLight','SkyAtmosphere','SkyLight','ExponentialHeightFog','PostProcessVolume'])
        if keep:preserve.append(a)
        else:A.destroy_actor(a)
    lasers=sorted([a for a in preserve if isinstance(a,unreal.HeistLaserBarrierActor)],key=lambda a:a.get_actor_label())
    REPORT['identity_before']=[str(a.get_display_case_id()) for a in preserve if isinstance(a,unreal.HeistPaintingDisplayCaseActor)]
    core_roof(rows);architecture()
    ordered,high,other=paintings(preserve,lasers)
    furniture();gameplay(preserve,ordered,high,other)
    sun=next(a for a in preserve if isinstance(a,unreal.DirectionalLight));sun.set_actor_rotation(unreal.Rotator(pitch=-35,yaw=-25),False)
    setp(sun.light_component,'intensity',.25);setp(sun.light_component,'light_color',unreal.Color(150,180,235,255))
    sky=next(a for a in preserve if isinstance(a,unreal.SkyLight));setp(sky.light_component,'intensity',.12)
    setp(sky.light_component,'real_time_capture',False)
    pp=next(a for a in preserve if isinstance(a,unreal.PostProcessVolume));settings=pp.settings
    for k,v in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),('auto_exposure_min_brightness',1.0),('auto_exposure_max_brightness',1.0),('override_auto_exposure_bias',True),('auto_exposure_bias',0.0)]:settings.set_editor_property(k,v)
    pp.settings=settings
    nav=next(a for a in preserve if isinstance(a,unreal.NavMeshBoundsVolume));move(nav,pos(30.5,22,400));nav.set_actor_scale3d(unreal.Vector(28,20,6))
    recast=next(a for a in preserve if isinstance(a,unreal.RecastNavMesh));setp(recast,'max_simplification_error',.1)
    assert sorted(REPORT['identity_before'])==sorted(REPORT['case_ids'])
    assert len(REPORT['patterns'])>=10,REPORT['patterns']
    assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==v for p,v in hashes.items())
    REPORT['protected_hashes']=hashes
    (OUT/'applied.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    # Navigation is built by the editor before the map save; async completion is checked later.
    unreal.SystemLibrary.execute_console_command(world,'BUILDPATHS')
    return world

STATE={'busy':False,'phase':'start','time':0}
def finish(error=None):
    REPORT['error']=error
    (OUT/'applied.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    unreal.log_warning('M03_GALLERY_APPLY_'+('FAIL' if error else 'PASS'))
    unreal.unregister_slate_post_tick_callback(STATE['callback'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()
def tick(_):
    if STATE['busy']:return
    STATE['busy']=True
    try:
        if STATE['phase']=='start':
            STATE['world']=apply();STATE.update(phase='save',time=time.monotonic())
        elif time.monotonic()-STATE['time']>20:
            assert unreal.EditorLoadingAndSavingUtils.save_map(STATE['world'],MAP)
            REPORT['saved']=True;finish()
    except Exception:finish(traceback.format_exc())
    finally:STATE['busy']=False
if __name__=='__main__':
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    STATE['callback']=unreal.register_slate_post_tick_callback(tick)
