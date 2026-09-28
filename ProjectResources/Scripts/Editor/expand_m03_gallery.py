"""Apply the approved M03 extension and three integer painting sizes in Editor.

Reuses imported meshes and the existing gameplay instances. No asset duplication.
The source fingerprint makes this a one-shot authored change, not a map generator.
"""
import unreal, json, math, hashlib, shutil, time, traceback, itertools
from pathlib import Path

R=Path(unreal.Paths.project_dir()).resolve()
O=R/'Saved/Automation/M03Expansion';O.mkdir(parents=True,exist_ok=True)
P=json.loads((R/'ProjectResources/SourceArt/Gallery/M03/M03ExpandedLayout.json').read_text())
OLD=json.loads((R/'ProjectResources/SourceArt/Gallery/M03/M03GalleryLayout.json').read_text())
MAP='/Game/Maps/M03_GlasshousePrototype'; PACK='/Game/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_'
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
REPORT={'saved':False,'errors':[],'paintings':[],'walls':[],'lights':[]}
ROOMS={r['name']:dict(r) for r in P['rooms']}
for r in OLD['rooms']:
 x0,y0,x1,y1=r['bounds'];ROOMS[r['name']]={'name':r['name'],'bounds':[x0-30,22-y1,x1-30,22-y0],'ceiling':r['ceiling'] or 595,'decorations':P['core_decorations'].get(r['name'],0)}
TAG='M03Expansion20260928'
def v(q):return [float(getattr(q,k)) for k in 'xyz']
def V(x,y,z=0):return unreal.Vector(round(x*100),round(y*100),round(z))
def integer(q):return unreal.Vector(*(math.trunc(x) for x in v(q)))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def bnd(c):
 b=c.static_mesh.get_bounding_box();t=c.get_world_transform();pts=[v(unreal.MathLibrary.transform_location(t,unreal.Vector(*p))) for p in itertools.product(*zip(v(b.min),v(b.max)))];return [[min(p[k] for p in pts) for k in range(3)],[max(p[k] for p in pts) for k in range(3)]]
def sm(a):return next((c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and 'SM_Canvas_Painting_' in c.static_mesh.get_name()),None) if unreal.SystemLibrary.is_valid(a) else None
def move(a,point,yaw=None):
 a.modify();a.set_actor_location(integer(point),False,True)
 if yaw is not None:a.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
def spawn_mesh(label,asset,center,scale=(1,1,1),yaw=0,folder='Extension',collision=True):
 a=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(),unreal.Rotator(yaw=yaw));a.set_actor_label('M03X_'+label);a.tags=[TAG];a.set_folder_path('M03Gallery/'+folder)
 c=a.static_mesh_component;c.set_static_mesh(asset);a.set_actor_scale3d(unreal.Vector(*scale));c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
 b=bnd(c);mid=unreal.Vector(*[(lo+hi)/2 for lo,hi in zip(*b)]);move(a,a.get_actor_location()+center-mid);return a
def wall(axis,plane,lo,hi,height,bottom=0,label='Wall'):
 if hi-lo<.15:return
 # One finished wall panel per continuous span, using only 0.1 scale increments.
 # Ceiling-rounded lengths meet perpendicular wall bodies and never leave a gap.
 m=unreal.load_asset(PACK+'walls_4_1');b=m.get_bounding_box();length=(hi-lo)*100
 sy=math.ceil((length-0.01)/(b.max.y-b.min.y)*10)/10
 sz=math.ceil((height-bottom)/(b.max.z-b.min.z)*10)/10
 center=V(plane,(lo+hi)/2,bottom+(b.max.z-b.min.z)*sz/2) if axis==0 else V((lo+hi)/2,plane,bottom+(b.max.z-b.min.z)*sz/2)
 a=spawn_mesh(label,m,center,(.8,sy,sz),0 if axis==0 else 90,'Extension/Walls')
 REPORT['walls'].append({'name':a.get_name(),'axis':axis,'plane':plane,'lo':lo,'hi':hi,'height':height,'bottom':bottom});return a
def merge(intervals):
 out=[]
 for lo,hi in sorted(intervals):
  if out and lo<=out[-1][1]+.025:out[-1][1]=max(out[-1][1],hi)
  else:out.append([lo,hi])
 return out
def subtract(intervals,cuts):
 for l,h in cuts:
  out=[]
  for a,b in intervals:
   if h<=a or l>=b:out.append([a,b]);continue
   if l>a:out.append([a,l])
   if h<b:out.append([h,b])
  intervals=out
 return intervals
def wall_components():
 return [(a,c,bnd(c)) for a in A.get_all_level_actors() for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and '_walls_' in c.static_mesh.get_name()]
def cut_core():
 for i,p in enumerate(P['core_portals']):
  axis=p['axis'];h=1-axis;lo=p['center']-p['width']/2;hi=p['center']+p['width']/2;selected=[]
  for a,c,b in wall_components():
   if b[0][2]>100 or b[1][axis]-b[0][axis]>100:continue
   if abs((b[0][axis]+b[1][axis])/200-p['plane'])>.35:continue
   if b[0][h]/100<hi and b[1][h]/100>lo:selected.append((a,c,b))
  assert selected,('core portal has no wall',p)
  start=min(b[0][h]/100 for _,_,b in selected);end=max(b[1][h]/100 for _,_,b in selected);height=max(b[1][2] for _,_,b in selected)
  for a,c,b in selected:assert isinstance(a,unreal.StaticMeshActor);A.destroy_actor(a)
  wall(axis,p['plane'],start,lo,height,label='CorePortal_%d_L'%i);wall(axis,p['plane'],hi,end,height,label='CorePortal_%d_R'%i)
  wall(axis,p['plane'],lo,hi,height,bottom=300,label='CorePortal_%d_Header'%i)
def architecture():
 # A single contiguous floor grid avoids overlaps between independently fitted rooms.
 for a in list(A.get_all_level_actors()):
  if not isinstance(a,unreal.StaticMeshActor):continue
  c=a.static_mesh_component
  if c.static_mesh and 'stone_tile_' in c.static_mesh.get_name() and bnd(c)[1][2]<40:A.destroy_actor(a)
 tile=unreal.load_asset(PACK+'stone_tile_1_1')
 for x in range(16):
  for y in range(12):spawn_mesh('Floor_%02d_%02d'%(x,y),tile,V(-52.8+(x+.5)*6.6,-36+(y+.5)*6,-1),(1,.9,4),folder='Extension/Floors')
 cut_core();old=wall_components();edges={}
 for r in P['rooms']:
  x0,y0,x1,y1=r['bounds']
  for axis,plane,lo,hi in [(0,x0,y0,y1),(0,x1,y0,y1),(1,y0,x0,x1),(1,y1,x0,x1)]:edges.setdefault((axis,plane),[]).append((lo,hi,r['ceiling']))
 for j,((axis,plane),spans) in enumerate(sorted(edges.items())):
  h=1-axis;breaks=sorted(set(q for s in spans for q in s[:2]));cuts=[(p['center']-p['width']/2,p['center']+p['width']/2) for p in P['doors']+P['core_portals'] if p['axis']==axis and abs(p['plane']-plane)<.01]
  old_spans=[(b[0][h]/100,b[1][h]/100) for _,_,b in old if b[0][2]<50 and b[1][axis]-b[0][axis]<100 and abs((b[0][axis]+b[1][axis])/200-plane)<.35]
  for k,(lo,hi) in enumerate(zip(breaks,breaks[1:])):
   heights=[s[2] for s in spans if s[0]<(lo+hi)/2<s[1]]
   if not heights:continue
   height=max(heights)
   for n,(l,u) in enumerate(subtract([[lo,hi]],cuts+old_spans)):wall(axis,plane,l,u,height,label='Wall_%03d_%02d_%02d'%(j,k,n))
   for n,(l,u) in enumerate(subtract([[max(lo,c[0]),min(hi,c[1])] for c in cuts if min(hi,c[1])>max(lo,c[0])],old_spans)):
    wall(axis,plane,l,u,height,bottom=300,label='Lintel_%03d_%02d_%02d'%(j,k,n))
 # Existing north boundary rises into the new high gallery; reuse wall panels for the cap.
 wall(1,17.95,-5,17,650,bottom=490,label='NorthGrand_UpperCap')
 white=unreal.load_asset(PACK+'walls_4_1').get_material(0);tb=tile.get_bounding_box()
 for i,r in enumerate(P['rooms']):
  x0,y0,x1,y1=r['bounds'];sx=math.ceil((x1-x0)*100/(tb.max.x-tb.min.x)*10)/10;sy=math.ceil((y1-y0)*100/(tb.max.y-tb.min.y)*10)/10
  # Small elevation offsets put overlaps above shared wall tops, without coplanar flicker.
  ceiling=spawn_mesh('Ceiling_'+r['name'],tile,V((x0+x1)/2,(y0+y1)/2,r['ceiling']+1+(i%3)*2),(sx,sy,4),folder='Extension/Ceilings')
  ceiling.static_mesh_component.set_material(0,white)
def surfaces(room):
 r=ROOMS[room];x0,y0,x1,y1=r['bounds'];out=[]
 for axis,plane,lo,hi,sign in [(0,x0,y0,y1,1),(0,x1,y0,y1,-1),(1,y0,x0,x1,1),(1,y1,x0,x1,-1)]:
  h=1-axis;groups={}
  for a,c,b in wall_components():
   if b[0][2]>40 or b[1][2]<250 or b[1][axis]-b[0][axis]>100:continue
   if abs((b[0][axis]+b[1][axis])/200-plane)>.45:continue
   l=max(lo*100,b[0][h]);u=min(hi*100,b[1][h])
   if u<=l:continue
   face=b[1 if sign>0 else 0][axis];groups.setdefault(round(face),[]).append([l,u])
  for face,intervals in groups.items():
   obstacles=[(w[0][h]-65,w[1][h]+65) for _,_,w in wall_components() if w[0][2]<100 and w[1][h]-w[0][h]<100 and w[1][axis]-w[0][axis]>100 and w[0][axis]<face+sign*8<w[1][axis]]
   for l,u in subtract(merge(intervals),obstacles):
    if u-l>=290:out.append({'room':room,'axis':axis,'plane':face,'sign':sign,'lo':l+65,'hi':u-65,'used':[]})
 return out
def dimensions(a,tier):
 c=sm(a);variant=int(c.static_mesh.get_name().split('_')[-1][:2]);scale=([2,3,4] if variant!=4 else [4,6,8])[tier-1];b=c.static_mesh.get_bounding_box();return variant,scale,(b.max.y-b.min.y)*scale,(b.max.z-b.min.z)*scale
def painting(a,tier,s,center):
 c=sm(a);variant,scale,width,height=dimensions(a,tier);ceiling=ROOMS[s['room']]['ceiling'];bottom=[60,45,20][tier-1]
 assert bottom+height<ceiling-5,(a.get_name(),tier,height,ceiling)
 axis=s['axis'];h=1-axis;n=[0,0,0];n[axis]=s['sign'];normal=unreal.Vector(*n);yaw=math.degrees(math.atan2(normal.y,normal.x));rot=unreal.Rotator(yaw=yaw)
 rear=[0,0,bottom+height/2];rear[axis]=s['plane']+s['sign']*3;rear[h]=center;rear=unreal.Vector(*rear)
 b=c.static_mesh.get_bounding_box();offset=unreal.MathLibrary.transform_direction(unreal.Transform(rotation=rot),unreal.Vector(b.min.x,(b.min.y+b.max.y)/2,(b.min.z+b.max.z)/2)*scale)
 a.modify();c.modify()
 if isinstance(a,unreal.HeistPaintingDisplayCaseActor):
  a.set_actor_scale3d(unreal.Vector(1,1,1));move(a,unreal.Vector(rear.x,rear.y,0),yaw+180);c.set_world_transform(unreal.Transform(location=rear-offset,rotation=rot,scale=unreal.Vector(scale,scale,scale)),False,True)
  box=a.get_component_by_class(unreal.BoxComponent);box.modify();box.set_world_scale3d(unreal.Vector(1,1,1));box.set_box_extent(unreal.Vector(60,60,80),False);box.set_world_location(integer(unreal.Vector(rear.x,rear.y,100)+normal*90),False,True)
 else:a.set_actor_transform(unreal.Transform(location=integer(rear-offset),rotation=rot,scale=unreal.Vector(scale,scale,scale)),False,True)
 c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_folder_path('M03Gallery/Paintings/'+s['room']);a.tags=[t for t in a.tags if str(t) not in ['M03PaintingPlacement60'] and not str(t).startswith('M03PaintingTier')]+[TAG,'M03PaintingTier%d'%tier]
 s['used'].append([center-width/2-100,center+width/2+100])
 REPORT['paintings'].append({'name':a.get_name(),'label':a.get_actor_label(),'case':str(a.get_display_case_id()) if isinstance(a,unreal.HeistPaintingDisplayCaseActor) else None,'room':s['room'],'tier':tier,'variant':variant,'scale':scale,'center':v(rear),'normal':n,'box':bnd(c),'surface':{k:v for k,v in s.items() if k!='used'}})
def paintings(old):
 cases={str(a.get_display_case_id()):a for a in old if isinstance(a,unreal.HeistPaintingDisplayCaseActor)};deco=[a for a in old if sm(a) and a not in cases.values()];assert len(cases)==12 and len(deco)==48
 spaces={name:surfaces(name) for name,r in ROOMS.items() if r.get('decorations') or any(c['room']==name for c in P['cases'])}
 for p in P['cases']:
  a=cases[p['case']];_,_,width,_=dimensions(a,p['tier']);choices=[s for s in spaces[p['room']] if s['axis']==p['axis'] and abs(s['plane']/100-p['plane'])<.5 and s['lo']<=p['center']*100-width/2 and s['hi']>=p['center']*100+width/2]
  assert choices,('case wall too short',p,width,spaces[p['room']]);painting(a,p['tier'],choices[0],p['center']*100)
 def place_deco(a,room,tier):
  _,_,w,h=dimensions(a,tier);options=[]
  for s in spaces[room]:
   if room=='Hall' and tier==3 and not (s['axis']==1 and s['plane']<0):continue
   for l,u in subtract([[s['lo'],s['hi']]],s['used']):
    if u-l>=w:options.append((u-l,s,(l+u)/2))
  assert options,('No wall space',room,a.get_name(),tier,w)
  _,s,center=max(options,key=lambda q:q[0]);painting(a,tier,s,center);deco.remove(a);ROOMS[room]['decorations']-=1
 for p in P['large_decorations']:
  a=next(a for a in deco if dimensions(a,1)[0]==p['variant']);place_deco(a,p['room'],3)
 medium=20
 while deco:
  for room,r in ROOMS.items():
   if r.get('decorations',0)<=0:continue
   tier=2 if medium>0 and r['ceiling']>=500 else 1
   a=sorted(deco,key=lambda a:a.get_actor_label())[0];place_deco(a,room,tier)
   if tier==2:medium-=1
 assert len(REPORT['paintings'])==60
def gameplay(old):
 starts=sorted([a for a in old if isinstance(a,unreal.PlayerStart)],key=lambda a:a.get_name())
 for a,(x,y) in zip(starts,[(-47,-31),(-45,-31),(-47,-29),(-45,-29)]):move(a,V(x,y,98),45)
 vent=next(a for a in old if isinstance(a,unreal.HeistVentActor));move(vent,V(-49,-34,50),90)
 lasers=sorted([a for a in old if isinstance(a,unreal.HeistLaserBarrierActor)],key=lambda a:a.get_name());buttons=sorted([a for a in old if isinstance(a,unreal.HeistSecurityHoldButtonActor)],key=lambda a:a.get_name())
 for laser,button,y in zip(lasers,buttons,[-2,2]):
  move(laser,V(42,y,0),90);box=laser.get_editor_property('beam_trigger_component');box.modify();box.set_box_extent(unreal.Vector(10,120,120),False)
  for c in laser.get_components_by_class(unreal.NiagaraComponent):c.set_variable_vec3('User.BeamStart',unreal.Vector(0,-120,0));c.set_variable_vec3('User.BeamEnd',unreal.Vector(0,120,0))
  # Buttons sit on opposite corridor walls, outside their own beam trigger.
  move(button,V(40,-1.8 if y<0 else 1.8,105),90 if y<0 else -90)
 # Keep the dedicated detention patrol. Three existing gallery guards cover the new wings.
 routes=[ [(-43,-10),(-28,-20),(-28,0),(-43,0),(-43,10),(-42,24),(-18,24),(-18,16),(-24,16),(-24,-5),(-17,-14),(-17,-20),(-28,-20)],
 [(-18,16),(-18,24),(-17,34),(7,34),(31,34),(31,25),(22,22),(12,25),(0,27),(-12,23),(-18,24)],
 [(29,-20),(29,0),(29,25),(31,34),(48,34),(48,27),(35,27),(29,20),(29,0),(20,0),(15,-4),(5,-5),(0,-14),(3,-20),(27,-20),(42,-27),(27,-27),(3,-27),(3,-20)],
 [(15,7.5),(15,5),(19,5),(20.5,5),(19,5),(15,5),(15,0),(15,5)] ]
 guards=sorted([a for a in old if isinstance(a,unreal.HeistGuardCharacter)],key=lambda a:a.get_name())
 for wp in list(A.get_all_level_actors()):
  if isinstance(wp,unreal.HeistGuardWaypoint):A.destroy_actor(wp)
 for g,route in zip(guards,routes):
  pc=g.get_component_by_class(unreal.HeistPatrolPathComponent);rid=pc.get_editor_property('patrol_route_id')
  for i,(x,y) in enumerate(route):
   wp=A.spawn_actor_from_class(unreal.HeistGuardWaypoint,V(x,y,100));wp.set_actor_label(str(rid)+'_Point_%02d'%i);wp.set_folder_path('M03Gallery/Patrol');wp.set_editor_property('patrol_route_id',rid);wp.set_editor_property('patrol_order',i);wp.set_editor_property('wait_duration_override',.4)
  move(g,V(*route[0],98))
 REPORT['routes']=routes
 cameras=sorted([a for a in old if isinstance(a,unreal.HeistSecurityCameraActor)],key=lambda a:a.get_name())
 for a,(x,y,yaw) in zip(cameras,[(-31,-19,0),(-29,-1,135),(-18,31,-90),(18,21,30),(41,-1,-45),(41,3,45),(26,13,-150),(38,-23,-120)]):
  move(a,V(x,y,280));a.set_actor_rotation(unreal.Rotator(pitch=-14,yaw=yaw),False)
 nav=next(a for a in old if isinstance(a,unreal.NavMeshBoundsVolume));move(nav,V(0,0,350));nav.set_actor_scale3d(unreal.Vector(55,38,7))
 # Reuse loot sockets and their table/bench supports, pairing by horizontal proximity.
 loot=sorted([a for a in old if isinstance(a,unreal.HeistLootSpawnPoint)],key=lambda a:int(a.get_name().rsplit('_',1)[1]))
 furniture=[a for a in old if isinstance(a,unreal.StaticMeshActor) and a.get_actor_label() not in ['M03G_Table_00','M03G_Table_01'] and a.static_mesh_component.static_mesh and any(k in a.static_mesh_component.static_mesh.get_name() for k in ['table_','bench_'])]
 spots=[(-45,-12),(-40,12),(-22,27),(8,24),(33,27),(-22,-30),(2,-30),(27,-30),(20,12),(9,-7),(47,-12),(47,12)]
 used=set()
 for a,(x,y) in zip(loot,spots):
  p=a.get_actor_location();f=min((f for f in furniture if f not in used),key=lambda f:math.hypot(f.get_actor_location().x-p.x,f.get_actor_location().y-p.y));fp=f.get_actor_location();delta=V(x,y,0)-unreal.Vector(fp.x,fp.y,0);move(f,fp+delta);move(a,p+delta);used.add(f)
def lighting(old):
 existing=[a for a in old if a.get_actor_label().startswith('M03G_Light_Art_')];passage=[a for a in old if a.get_actor_label().startswith('M03G_Light_Passage_')]
 targets=[p for p in REPORT['paintings'] if p['case'] or p['tier']==3]
 targets += [p for p in REPORT['paintings'] if not p['case'] and p not in targets][::3]
 for i,p in enumerate(targets):
  a=existing.pop(0) if existing else A.spawn_actor_from_class(unreal.load_class(None,'/Game/Blueprints/Environment/BP_HeistLight_Droplights_01a.BP_HeistLight_Droplights_01a_C'),unreal.Vector())
  a.set_actor_label('M03X_Light_Art_%02d'%i);a.set_folder_path('M03Gallery/Lights/Art');n=unreal.Vector(*p['normal']);center=unreal.Vector(*p['center']);loc=center+n*250;loc.z=ROOMS[p['room']]['ceiling'];move(a,loc)
  c=a.get_component_by_class(unreal.SpotLightComponent);c.modify();source=unreal.MathLibrary.transform_location(a.get_actor_transform(),c.get_editor_property('relative_location'));direction=center-source;c.set_world_rotation(unreal.MathLibrary.make_rot_from_x(direction),False,True);c.set_editor_property('attenuation_radius',max(650,direction.length()+250));c.set_editor_property('outer_cone_angle',65);c.set_editor_property('inner_cone_angle',40);c.set_editor_property('intensity',35.0)
  REPORT['lights'].append({'name':a.get_name(),'painting':p['name'],'ceiling':loc.z})
 for a in existing:A.destroy_actor(a)
 # Keep old passage fixtures and add sparse fixtures at new junctions.
 points=[(-43,-25,400),(-28,-20,400),(-28,0,400),(-43,0,400),(-28,16,400),(-43,34,400),(-17,34,400),(7,34,400),(31,34,400),(48,34,400),(29,16,400),(42,0,400),(29,-20,400),(3,-20,400),(42,-27,500),(-21,-29,500),(3,-29,500),(27,-29,500)]
 for i,(x,y,z) in enumerate(points):
  path='/Game/Blueprints/Environment/BP_HeistLight_DropCeiling_01a.BP_HeistLight_DropCeiling_01a_C';a=A.spawn_actor_from_class(unreal.load_class(None,path),V(x,y,z));a.set_actor_label('M03X_Light_Passage_%02d'%i);a.set_folder_path('M03Gallery/Lights/Passage');c=a.get_component_by_class(unreal.SpotLightComponent);c.set_editor_property('intensity',12.5);c.set_editor_property('attenuation_radius',650)
def validate_geometry():
 walls=wall_components();boxes=[]
 for p in REPORT['paintings']:
  b=p['box'];s=p['surface'];h=1-s['axis']
  for wa,_,w in walls:
   if min(min(b[1][k],w[1][k])-max(b[0][k],w[0][k]) for k in range(3))>1:REPORT['errors'].append([p['name'],'wall intersection',wa.get_actor_label(),w])
  for name,q in boxes:
   if min(min(b[1][k],q[1][k])-max(b[0][k],q[0][k]) for k in range(3))>1:REPORT['errors'].append([p['name'],'painting intersection',name])
  boxes.append((p['name'],b))
 assert not REPORT['errors'],REPORT['errors']
def apply():
 file=R/'Content/Maps/M03_GlasshousePrototype.umap';assert sha(file)==P['source_sha256'],'Map changed: review before applying'
 shutil.copy2(file,O/'Before.umap')
 protected=[R/'Content/Maps/M01_ClassicalPrototype.umap',R/'Content/Maps/M02_MoonlitPrototype.umap',R/'Content/AIUE5_vol10_01/maps/AIUE_vol10_01.umap']+list((R/'Content/AIUE5_vol10_01/Mesh').glob('*.uasset'))+list((R/'Content/Assets/MapAssets/Showcase/Meshes').glob('SM_Canvas_Painting_*.uasset'))
 REPORT['protected']={str(p):sha(p) for p in protected};REPORT['assets_before']=[str(p.relative_to(R)) for p in (R/'Content').rglob('*.uasset')]
 world=unreal.EditorLoadingAndSavingUtils.load_map(MAP);old=list(A.get_all_level_actors());assert not any(TAG in [str(t) for t in a.tags] for a in old)
 architecture();old=[a for a in old if unreal.SystemLibrary.is_valid(a)];REPORT['phase']='architecture';paintings(old);REPORT['phase']='paintings';gameplay(old);lighting(old);validate_geometry();return world
S={'busy':False,'phase':'apply','stamp':0}
def finish(error=None):
 if error:REPORT['errors'].append(error)
 (O/'applied.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(S['cb']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
def tick(dt):
 if S['busy']:return
 S['busy']=True
 try:
  if S['phase']=='apply':S['world']=apply();S.update(phase='save',stamp=time.monotonic());return
  if not S.get('nav_built'):
   if time.monotonic()-S['stamp']<5:return
   S['world']=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
   unreal.SystemLibrary.execute_console_command(S['world'],'BUILDPATHS');S.update(nav_built=True,stamp=time.monotonic());return
  if time.monotonic()-S['stamp']<25:return
  REPORT['protected_unchanged']=all(sha(Path(p))==h for p,h in REPORT['protected'].items());assert REPORT['protected_unchanged']
  REPORT['new_assets']=sorted({str(p.relative_to(R)) for p in (R/'Content').rglob('*.uasset')}-set(REPORT.pop('assets_before')));assert not REPORT['new_assets']
  assert unreal.EditorLoadingAndSavingUtils.save_map(S['world'],MAP);REPORT['saved']=True;finish()
 except:finish(traceback.format_exc())
 finally:S['busy']=False
if __name__=='__main__':
 unreal.EditorPythonScripting.set_keep_python_script_alive(True);S['cb']=unreal.register_slate_post_tick_callback(tick)
