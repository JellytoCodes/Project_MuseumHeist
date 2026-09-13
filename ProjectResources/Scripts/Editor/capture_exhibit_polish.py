"""Read-only eye-level captures of enlarged exhibits in each release map."""
import json
import math
import time
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Screenshots/ExhibitPolish';OUT.mkdir(parents=True,exist_ok=True)
plans=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_text(encoding='utf-8'))['maps']
paths={'M01':'M01_ClassicalPrototype','M02':'M02_MoonlitPrototype','M03':'M03_GlasshousePrototype'}
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
views=[]
for plan in plans:
    for pattern in ('Solo','Grid','Salon','Vertical'):
        group=next(g for g in plan['groups'] if g['pattern']==pattern)
        ps=[p for p in plan['paintings'] if p['group']==group['id']]
        p=next((p for p in ps if p['active']),ps[0]);n=p['normal']
        x=sum(p['xy'][0] for p in ps)/len(ps);y=sum(p['xy'][1] for p in ps)/len(ps)
        views.append((plan['id'],pattern,(x*100+n[0]*600,y*100+n[1]*600,170),math.degrees(math.atan2(-n[1],-n[0]))))
state={'i':0,'phase':'load','time':0,'map':None,'callback':None,'captures':[]}


def tick(dt):
    if time.monotonic()-state['time']<3:return
    if state['i']==len(views):
        unreal.unregister_slate_post_tick_callback(state['callback'])
        (OUT/'captures.json').write_text(json.dumps(state['captures'],indent=2),encoding='utf-8')
        unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        return
    code,pattern,location,yaw=views[state['i']]
    if state['phase']=='load':
        if state['map']!=code:
            unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+paths[code]);state['map']=code
        a=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(*location),unreal.Rotator(yaw=yaw))
        c=a.capture_component2d
        rt=unreal.RenderingLibrary.create_render_target2d(editor.get_editor_world(),1600,900,unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
        c.set_editor_property('texture_target',rt);c.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        c.set_editor_property('capture_every_frame',True);c.set_editor_property('always_persist_rendering_state',True)
        c.set_editor_property('fov_angle',90.0);c.capture_scene()
        state.update(actor=a,rt=rt,phase='capture',time=time.monotonic())
    else:
        name=code+'_'+pattern+'.png'
        unreal.RenderingLibrary.export_render_target(editor.get_editor_world(),state['rt'],str(OUT),name)
        actors.destroy_actor(state['actor']);unreal.RenderingLibrary.release_render_target2d(state['rt'])
        state['captures'].append(name);state.update(i=state['i']+1,phase='load',time=time.monotonic())


state['callback']=unreal.register_slate_post_tick_callback(tick)
