"""Update only existing paintings/frames and wall trim in the three release maps."""
import json
import runpy
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
base=runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/build_museum_levels_v2.py'))
gallery=runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/refine_museum_galleries.py'))
data=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_text(encoding='utf-8'))
assert data['painting_scale_revision']==2
bp=unreal.load_asset('/Game/Blueprints/World/Actors/Loot/BP_PaintingDisplayCase')
assert bp
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo=unreal.get_default_object(bp.generated_class())
box=cdo.get_component_by_class(unreal.BoxComponent)
assert box and not cdo.get_component_by_class(unreal.SphereComponent),'Native Box migration failed'
assert cdo.root_component.get_name()=='PaintingRoot'
backing=next(c for c in cdo.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='VisualMeshComponent')
backing.attach_to_component(cdo.root_component, '', unreal.AttachmentRule.KEEP_RELATIVE,
    unreal.AttachmentRule.KEEP_RELATIVE, unreal.AttachmentRule.KEEP_RELATIVE, False)
box.set_editor_property('relative_location',unreal.Vector(-70,0,100))
box.set_box_extent(unreal.Vector(60,60,80),False)
unreal.EditorAssetLibrary.save_loaded_asset(bp)
results=[]
for plan in data['maps']:
    code=plan['id'];config=base['MAPS'][code]
    assert unreal.EditorLoadingAndSavingUtils.load_map(config['path'])
    builder=base['LevelBuilder'](code,config)
    cases={a.get_actor_label().split('_Painting_')[-1]:a for a in builder.actors
           if isinstance(a,unreal.HeistPaintingDisplayCaseActor)}
    assert len(cases)==20
    identities={k:(str(a.get_editor_property('display_case_id')),str(a.get_editor_property('target_artifact_id'))) for k,a in cases.items()}
    gallery['refine_exhibits'](builder,cases,gallery['mounting_walls'](builder))
    for wall in plan['walls']:
        for suffix in ('Skirt','Cornice'):
            a=builder.by_label['LDV2_'+code+'_Gallery_'+wall['id']+'_'+suffix]
            scale=a.get_actor_scale3d()
            if wall['axis']=='h':scale.y=wall['thickness']*1.15
            else:scale.x=wall['thickness']*1.15
            a.set_actor_scale3d(scale)
    for k,a in cases.items():
        assert identities[k]==(str(a.get_editor_property('display_case_id')),str(a.get_editor_property('target_artifact_id')))
        assert a.get_component_by_class(unreal.BoxComponent) and not a.get_component_by_class(unreal.SphereComponent)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,config['path'])
    results.append(dict(map=code,paintings=len(plan['paintings']),active=len(cases),trim_pieces=len(plan['walls'])*2,identities_preserved=True))
(ROOT/'Saved/Logs/ExhibitPolishApply.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
