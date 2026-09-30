"""Refine M02's central planting island using existing original asset references."""
import json
import math
import time
from pathlib import Path
import unreal
from audit_common_map_rules import bounds

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation/M02GardenReference20260930'
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MAP = '/Game/Maps/M02_MoonlitPrototype'
OLD_TAG = 'HeistM02CentralGarden'
TAG = 'HeistM02GardenReference'
BED = '/Game/Assets/MapAssets/PCMHall/Meshes/Dis_Prop/SM_DIS_FLOWER_BED_1'
ROCK = '/Game/Assets/MapAssets/SunTemplate/Meshes/SM_Cave_Rock_Large02'
FOLIAGE = '/Game/Assets/MapAssets/SunTemplate/Meshes/Soul_FG01'
SMALL_ROCK = '/Game/Assets/StarterContent/Props/SM_Rock'
TREE = '/Game/Assets/MapAssets/SunTemplate/Meshes/S_BuildingSetA_Tree_02'
PLANTS = '/Game/Assets/MapAssets/ConferenceRoom/Meshes/Plants/'
BIG_PLANTS = '/Game/Assets/MapAssets/ConferenceRoom/Meshes/Big_Plants/'
SOIL = '/Game/Assets/MapAssets/PCMHall/Materials/MI_SOIL'


def place_mesh(mesh, label, scale, center, bottom, yaw=0, collision=True):
    actor = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_asset(mesh))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
    actor.set_actor_label(label)
    actor.tags = [TAG]
    actor.set_folder_path('M02/CentralGarden')
    b = bounds(component)
    actor.set_actor_location(unreal.Vector(math.trunc(center[0] - (b[0][0]+b[1][0])/2),
        math.trunc(center[1] - (b[0][1]+b[1][1])/2), math.trunc(bottom-b[0][2])), False, False)
    component.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    return actor


def apply():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world
    actors = list(ACTORS.get_all_level_actors())
    assert not any(TAG in [str(t) for t in a.tags] for a in actors), 'Reference garden already applied'
    original = {a.get_name():a for a in actors if OLD_TAG in [str(t) for t in a.tags]}
    assert len(original)==10 and all(a.get_actor_label().startswith('M02_Garden_') for a in original.values()), 'Expected small garden changed; inspect before applying'
    # Replace only the previous garden's scenery and nav exclusion.
    for actor in original.values():
        ACTORS.destroy_actor(actor)
    props=[]
    # A low continuous planting surface covers approximately 14.5 x 10 metres.
    # Original mesh/material references only; the saved wood PCG stays intact.
    bed=place_mesh(BED,'M02_Garden_SoilBed',(2.6,16.0,.5),(100,200),10)
    soil=unreal.load_asset(SOIL);assert soil,SOIL
    for slot in range(bed.static_mesh_component.get_num_materials()):bed.static_mesh_component.set_material(slot,soil)
    props.append(bed)
    for label,mesh,scale,center,bottom,yaw,collision in [
        ('Canopy',TREE,.3,(-100,400),27,20,False),
        ('CanopySide',TREE,.2,(480,420),27,-45,False),
        ('TallLeavesA',BIG_PLANTS+'SM_Big_Monstera',1.1,(180,420),-45,65,False),
        ('TallLeavesB',BIG_PLANTS+'SM_Big_Monstera',1.0,(360,280),-40,-45,False),
        ('MainStone',ROCK,.1,(-175,210),-5,35,True),
        ('LowStone',SMALL_ROCK,.3,(-80,140),15,110,True),
        ('SideStone',SMALL_ROCK,.2,(-250,140),15,-30,True)]:
        props.append(place_mesh(mesh,'M02_Garden_'+label,(scale,)*3,center,bottom,yaw,collision))
    # Ground cover forms connected masses with open patches between them.
    for i,(center,yaw) in enumerate([((-280,400),25),((0,420),-40),((330,460),80),((470,240),-80),((260,40),60),((-120,-60),-30),((-390,90),10),((-320,-130),-15),((-70,-170),60),((160,-150),-30),((420,-100),85),((530,110),-50),((560,400),15),((230,570),-90),((-80,580),55),((-380,500),-35),((-470,280),70),((0,80),-10),((260,300),40)]):
        props.append(place_mesh(FOLIAGE,'M02_Garden_GroundCover_'+str(i+1),(.1,)*3,center,27,yaw,False))
    # Pots are part of these original meshes. Bury their bases in the soil;
    # never cut, duplicate or modify the shared mesh to remove a pot.
    for i,(name,scale,center,bottom,yaw) in enumerate([
        ('SM_Monstera',1.5,(-330,270),-30,55),('SM_Monstera',1.4,(20,180),-30,-25),
        ('SM_Monstera',1.5,(280,110),-30,115),('SM_Monstera',1.2,(500,340),-25,-75),
        ('SM_Monstera',1.3,(-240,-50),-30,15),('SM_Monstera',1.4,(110,-80),-30,145),
        ('SM_Plant_03',3.0,(-360,410),-50,60),('SM_Plant_03',2.8,(20,510),-45,-30),
        ('SM_Plant_03',3.0,(380,440),-50,135),('SM_Plant_03',2.6,(-70,-130),-45,95),
        ('SM_Plant_04',3.0,(-470,270),-35,20),('SM_Plant_04',3.0,(-260,65),-35,150),
        ('SM_Plant_04',2.8,(170,-150),-35,70),('SM_Plant_04',3.0,(480,100),-35,-40),
        ('SM_Plant_04',2.8,(400,540),-35,120),('SM_Plant_04',3.0,(-440,-60),-35,25),
        ('SM_Plant_02',1.6,(-450,150),-30,80),('SM_Plant_02',1.5,(390,-70),-30,140),
        ('SM_Plant_02',1.6,(100,530),-30,-10),('SM_Plant_01',4.0,(-500,20),-25,95),
        ('SM_Plant_01',4.0,(-30,-200),-25,40),('SM_Plant_01',4.0,(300,-120),-25,-15),
        ('SM_Plant_01',4.0,(520,450),-25,140),
        ('SM_Monstera',1.5,(-400,-120),-30,60),('SM_Monstera',1.6,(-80,-180),-30,-45),
        ('SM_Monstera',1.5,(220,-160),-30,120),('SM_Monstera',1.4,(510,-35),-30,-80),
        ('SM_Monstera',1.5,(570,260),-30,35),('SM_Monstera',1.4,(-400,500),-30,100),
        ('SM_Plant_04',3.8,(-540,120),-40,20),('SM_Plant_04',3.6,(-470,-150),-40,125),
        ('SM_Plant_04',3.8,(-220,-225),-40,70),('SM_Plant_04',3.6,(80,-235),-40,-30),
        ('SM_Plant_04',3.8,(440,-185),-40,145),('SM_Plant_04',3.6,(660,120),-40,55),
        ('SM_Plant_04',3.6,(650,500),-40,-60),('SM_Plant_03',3.4,(-510,410),-55,20),
        ('SM_Plant_03',3.2,(500,560),-55,150),('SM_Plant_02',2.0,(570,80),-35,-20)]):
        props.append(place_mesh(PLANTS+name,'M02_Garden_Leaves_'+str(i+1),(scale,)*3,center,bottom,yaw,False))
    # The low bed is scenery; guards use its four clear outer approaches.
    nav = ACTORS.spawn_actor_from_class(unreal.NavModifierVolume, unreal.Vector(100,200,200))
    nav.set_actor_label('M02_Garden_NavExclusion')
    nav.tags = [TAG]
    nav.set_folder_path('M02/CentralGarden')
    nav.set_editor_property('area_class', unreal.NavArea_Null)
    extent = nav.get_actor_bounds(False)[1]
    assert all(abs(getattr(extent,k)-100)<.01 for k in 'xyz'), 'Unexpected default volume brush'
    # Include the foliage overhang so paths do not brush through visible leaves.
    nav.set_actor_scale3d(unreal.Vector(8.2,6.3,2.5))
    report = {'removed':list(original), 'props':[{'name':a.get_name(),'label':a.get_actor_label(),
        'mesh':a.static_mesh_component.static_mesh.get_path_name(),'bounds':bounds(a.static_mesh_component),
        'scale':str(a.get_actor_scale3d())} for a in props], 'lights':[],
        'navigation_actor':nav.get_name(), 'new_assets':0, 'lighting_changed':False}
    return world, props, [], report


def apply_garden_lights():
    """Add only the garden's three weak ceiling fixtures to the saved M02."""
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world
    actors = list(ACTORS.get_all_level_actors())
    light_tag = 'HeistM02GardenLights'
    assert sum(TAG in [str(t) for t in a.tags] for a in actors) == 67, 'Inspect the current garden before adding lights'
    assert not any(light_tag in [str(t) for t in a.tags] for a in actors), 'Garden lights already applied'
    fixture_class = unreal.load_class(None, '/Game/Blueprints/Environment/BP_HeistLight_Droplights_01b.BP_HeistLight_Droplights_01b_C')
    assert fixture_class
    fixtures = []
    for label, location, target in [
        ('LowLeaves', (-380, -180, 810), (-270, -60, 150)),
        ('Canopy', (-50, 600, 810), (-100, 400, 380)),
        ('TallLeaves', (650, 170, 810), (420, 300, 240)),
    ]:
        actor = ACTORS.spawn_actor_from_class(fixture_class, unreal.Vector(*location), unreal.Rotator())
        actor.set_actor_label('M02_GardenLight_' + label)
        actor.tags = [light_tag]
        actor.set_folder_path('M02/CentralGarden/Lights')
        mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
        assert mesh
        mesh.set_collision_profile_name('NoCollision')
        mesh.set_editor_property('affect_dynamic_indirect_lighting', False)
        # Editing the mesh may reconstruct Blueprint components. Acquire the
        # current light afterwards and persist its local rotation as a property.
        spot = actor.get_component_by_class(unreal.SpotLightComponent)
        assert spot and spot.get_name() == 'KeySpot'
        parent = spot.get_attach_parent().get_world_transform()
        aim = unreal.MathLibrary.find_look_at_rotation(spot.get_world_location(), unreal.Vector(*target))
        actor.modify()
        spot.modify()
        spot.set_editor_properties({
            'intensity_units': unreal.LightUnits.CANDELAS, 'intensity': 40.0,
            'relative_rotation': unreal.MathLibrary.inverse_transform_rotation(parent, aim),
            'use_inverse_squared_falloff': True, 'attenuation_radius': 1000.0,
            'inner_cone_angle': 24.0, 'outer_cone_angle': 38.0,
            'use_temperature': True, 'temperature': 3600.0,
            'indirect_lighting_intensity': 0.0, 'volumetric_scattering_intensity': 0.0,
        })
        fixtures.append(actor)
    return world, fixtures, {'fixtures': [a.get_actor_label() for a in fixtures],
        'count': 3, 'intensity_cd': 40.0, 'temperature_kelvin': 3600.0,
        'ceiling_z_cm': 810, 'new_assets': 0, 'existing_actors_modified': 0}


if __name__ == '__main__':
    world, props, fixtures, report = apply()
    OUT.mkdir(parents=True,exist_ok=True)
    state = {'phase':'load_wait', 'stamp':time.monotonic()}
    def save_when_navigation_ready(_):
        if time.monotonic()-state['stamp'] < 12:
            return
        if state['phase']=='load_wait':
            # An immediate build after LoadMap is rejected by AsyncLoadLock.
            unreal.SystemLibrary.execute_console_command(world,'RebuildNavigation')
            state.update(phase='build_wait',stamp=time.monotonic())
            return
        system = next(o for o in unreal.ObjectIterator(unreal.NavigationSystemV1) if o.get_outer()==world)
        if system.call_method('IsNavigationBeingBuiltOrLocked',(world,)):
            return
        assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
        (OUT/'apply_result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        unreal.unregister_slate_post_tick_callback(state['callback'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    state['callback'] = unreal.register_slate_post_tick_callback(save_when_navigation_ready)
