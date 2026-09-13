"""Read back Box, visual size, identities and trim separation from saved maps."""
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
plans=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_text(encoding='utf-8'))['maps']
paths={'M01':'M01_ClassicalPrototype','M02':'M02_MoonlitPrototype','M03':'M03_GlasshousePrototype'}
results=[]
for plan in plans:
    code=plan['id']
    unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+paths[code])
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    labels={a.get_actor_label():a for a in actors}
    failures=[];boxes=[];trim=[]
    for p in plan['paintings']:
        if not p['active']:continue
        a=labels['LDV2_'+code+'_Painting_'+p['case_key']]
        box=a.get_component_by_class(unreal.BoxComponent)
        if box is None or a.get_component_by_class(unreal.SphereComponent):failures.append(a.get_actor_label()+':shape');continue
        ext=box.get_scaled_box_extent();offset=box.get_editor_property('relative_location')
        if (ext-unreal.Vector(60,60,80)).length()>.01 or (offset-unreal.Vector(-70,0,100)).length()>.01:failures.append(a.get_actor_label()+':volume')
        meshes={c.get_name():c for c in a.get_components_by_class(unreal.StaticMeshComponent)}
        original=meshes['OriginalVisualComponent'];replica=meshes['ReplicaVisualComponent']
        if meshes['VisualMeshComponent'].get_attach_parent()!=a.root_component:failures.append(a.get_actor_label()+':visual parent')
        if abs(original.get_world_location().z-p['z']*100)>.1:failures.append(a.get_actor_label()+':height')
        if abs(original.get_world_scale().x*100-(p['size']*100-15))>.1:failures.append(a.get_actor_label()+':size')
        if (original.get_world_location()-replica.get_world_location()).length()>.01:failures.append(a.get_actor_label()+':replica alignment')
        boxes.append(dict(case=a.get_actor_label(),extent=[ext.x,ext.y,ext.z],root=a.root_component.get_name()))
    for wall in plan['walls']:
        w=labels['LDV2_'+code+'_Gallery_'+wall['id']]
        wo,we=w.get_actor_bounds(False)
        for suffix in ('Skirt','Cornice'):
            t=labels[w.get_actor_label()+'_'+suffix];to,te=t.get_actor_bounds(False)
            protrusion=te.y-we.y if wall['axis']=='h' else te.x-we.x
            expected=wall['thickness']*100*.075
            if abs(protrusion-expected)>.01 or protrusion<=0:failures.append(t.get_actor_label()+':coplanar')
            trim.append(dict(label=t.get_actor_label(),protrusion_cm=round(protrusion,3)))
    results.append(dict(map=code,status='FAIL' if failures else 'PASS',failures=failures,boxes=boxes,trim=trim))
(ROOT/'Saved/Logs/ExhibitPolishReadback.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
assert all(r['status']=='PASS' for r in results),results
