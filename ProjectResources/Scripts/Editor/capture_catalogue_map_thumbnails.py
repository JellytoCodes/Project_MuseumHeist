"""Capture current map scenes for lobby cards without saving level changes."""
import json,time,traceback
from pathlib import Path
import unreal

out=Path(unreal.Paths.project_dir()).resolve()/'ProjectResources/SourceArt/UI/Catalogue'
views=[('M01','M01_ClassicalPrototype',(-850,1300,200),(0,40,0)),
       ('M02','M02_MoonlitPrototype',(-3200,350,180),(0,35,0)),
       ('M03','M03_GlasshousePrototype',(-850,1100,180),(0,55,0))]
state={'i':0,'phase':'load','time':0,'cb':None}
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

def finish(error=None):
    unreal.unregister_slate_post_tick_callback(state['cb'])
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    (out/'MapThumbnailCapture.json').write_text(json.dumps({'ok':not error,'error':error,'views':views},indent=2),encoding='utf-8')

def tick(dt):
    try:
        if state['i']>=len(views):finish();return
        code,mapname,loc,rot=views[state['i']]
        if state['phase']=='load':
            state['world']=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+mapname)
            state.update(phase='capture',time=time.time());return
        if time.time()-state['time']<6:return
        if state['phase']=='capture':
            world=state['world']
            a=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(*loc),unreal.Rotator(pitch=rot[0],yaw=rot[1],roll=rot[2]))
            c=a.capture_component2d
            rt=unreal.RenderingLibrary.create_render_target2d(world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
            c.texture_target=rt;c.capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
            c.capture_every_frame=True;c.always_persist_rendering_state=True;c.fov_angle=90
            pp=c.get_editor_property('post_process_settings')
            exposure=0.0 if code=='M02' else 3.0
            for p,v in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),('auto_exposure_min_brightness',exposure),('auto_exposure_max_brightness',exposure),('override_auto_exposure_bias',True),('auto_exposure_bias',0.0)]:
                pp.set_editor_property(p,v)
            c.set_editor_property('post_process_settings',pp);c.post_process_blend_weight=1
            c.capture_scene();state.update(actor=a,rt=rt,phase='export',time=time.time());return
        state['actor'].capture_component2d.capture_scene()
        unreal.RenderingLibrary.export_render_target(state['world'],state['rt'],str(out),'T_Catalogue_Map_'+code+'.png')
        actors.destroy_actor(state['actor']);state.update(i=state['i']+1,phase='load')
    except Exception:finish(traceback.format_exc())

state['cb']=unreal.register_slate_post_tick_callback(tick)
