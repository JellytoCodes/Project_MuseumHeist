"""Run in a ticking Editor: move occluded canvas faces out of supporting walls.

Queries are offline authoring checks, not gameplay interaction traces.
"""
import unreal,time,json,traceback,math
from refine_canvas_and_light_fixtures import painting_geometry,MAPS,META,OUT,ACT,backup

def start():
 state={'index':0,'phase':'load','stamp':0,'world':None,'report':{}}
 def tick(dt):
  try:
   if state['phase']=='busy':return
   name=MAPS[state['index']]
   if state['phase']=='load':
    state['phase']='busy'
    backup('/Game/Maps/'+name)
    state['world']=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+name)
    state['phase']='wait';state['stamp']=time.monotonic();return
   if time.monotonic()-state['stamp']<3:return
   state['phase']='busy'
   rows=[];world=state['world']
   for a in ACT.get_all_level_actors():
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
     if not c.static_mesh or not c.static_mesh.get_name().startswith('SM_Canvas_Painting_'):continue
     p=painting_geometry(c);center=p['center'];normal=p['normal'];offsets=[]
     for dx,dz in [(0,0),(-.4,-.4),(.4,-.4),(-.4,.4),(.4,.4)]:
      point=center+p['horizontal']*(p['width']*dx)+unreal.Vector(0,0,p['height']*dz)
      hit=unreal.SystemLibrary.line_trace_single(world,point+normal*40,point-normal*40,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[a],unreal.DrawDebugTrace.NONE,True)
      if not hit:continue
      h=hit.to_tuple();other=h[9]
      if not isinstance(other,unreal.StaticMeshActor):continue
      mesh=other.static_mesh_component.static_mesh
      if not mesh or 'wall' not in (mesh.get_name()+' '+other.get_actor_label()).lower():continue
      if unreal.MathLibrary.dot_vector_vector(h[6],normal)<.9:continue
      offsets.append(unreal.MathLibrary.dot_vector_vector(h[4]-point,normal))
     if not offsets or max(offsets)<-1.5:continue
     i=int(c.static_mesh.get_name().split('_')[-1][:2])-1;m=META[i];axis=m['thin_axis']
     s=c.get_world_scale();depth=(m['front_max'][axis]-m['min'][axis])*abs([s.x,s.y,s.z][axis])
     amount=math.ceil(max(offsets)+max(depth+1,3))
     if not 0<amount<=32:continue
     a.modify();c.modify();c.set_world_location(c.get_world_location()+normal*amount,False,True)
     if isinstance(a,unreal.StaticMeshActor):
      pos=a.get_actor_location();a.set_actor_location(unreal.Vector(round(pos.x),round(pos.y),round(pos.z)),False,True)
     rows.append({'actor':a.get_actor_label(),'wall_offset_before':max(offsets),'moved_cm':amount})
   state['report'][name]=rows
   if rows:
    world.modify();assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Maps/'+name)
   (OUT/'wall_seating.json').write_text(json.dumps(state['report'],indent=2),encoding='utf-8')
   state['index']+=1
   if state['index']==len(MAPS):
    unreal.unregister_slate_post_tick_callback(state['handle']);unreal.SystemLibrary.quit_editor()
   else:state['phase']='load'
  except Exception:
   (OUT/'wall_seating_error.txt').write_text(traceback.format_exc());unreal.unregister_slate_post_tick_callback(state['handle']);unreal.SystemLibrary.quit_editor()
 state['handle']=unreal.register_slate_post_tick_callback(tick)
 return state

if __name__=='__main__':state=start()
