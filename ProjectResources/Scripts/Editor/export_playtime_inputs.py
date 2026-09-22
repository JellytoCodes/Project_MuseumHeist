"""Read saved tuning through Unreal Python commandlet; no asset or map saves."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
_, _, parameters = unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())
parameters = {str(k).lower(): str(v) for k, v in parameters.items()}
OUT = Path(parameters.get('museumplaytimedirectory', str(ROOT/'Saved/Automation/PlaytimeEstimate')))
OUT.mkdir(parents=True, exist_ok=True)
tables = {}
for name in ('DT_ArtifactData', 'DT_ContractData', 'DT_GuardData', 'DT_ForgeryTemplate'):
    table = unreal.load_asset('/Game/Data/DataTable/'+name)
    assert table
    tables[name] = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
balance = unreal.load_asset('/Game/Data/DataAsset/DA_GameBalance')
player = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Player/BP_HeistPlayerCharacter'))
door = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/World/Actors/Security/BP_DetentionDoor'))
payload = dict(status='PASS', scope='Saved asset tuning, not live multiplayer or playtest', tables=tables,
    balance={k: balance.get_editor_property(k) for k in ('vent_unlock_time','escape_cast_time','security_laser_hold_duration_seconds','detention_restraint_duration_seconds')},
    detention={k: door.get_editor_property(k) for k in ('latch_period_seconds','rescue_duration_seconds')},
    observation_seconds=player.get_component_by_class(unreal.HeistActionComponent).get_editor_property('observation_cast_duration_seconds'),
    hashes={str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()
        for p in [ROOT/'Content/Data/DataAsset/DA_GameBalance.uasset',
                  ROOT/'Content/Blueprints/Player/BP_HeistPlayerCharacter.uasset',
                  ROOT/'Content/Blueprints/World/Actors/Security/BP_DetentionDoor.uasset'] +
        [ROOT/'Content/Data/DataTable'/(name+'.uasset') for name in tables]})
(OUT/'runtime-inputs.json').write_text(json.dumps(payload,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
unreal.log_warning('MH_PLAYTIME_INPUTS=PASS '+str(OUT/'runtime-inputs.json'))
