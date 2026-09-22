"""Quiet-state security exposure model using native paths and Unreal LOS probes.

prepare -> export_museum_security_probes.py in Unreal -> analyze.
Stops qualification at first confirmed sighting; never predicts chase or win rate.
"""
import argparse
import bisect
import collections
import hashlib
import html
import json
import math
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT/'Saved/Automation/SecuritySimulation'
DT = .05
SPACING = .5
PHASES = 32
HORIZON = 600
SCENARIOS = ('split2', 'split3', 'split4', 'coop2', 'pulse2')


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(name, obj):
    (OUT/name).write_text(json.dumps(obj, ensure_ascii=False, separators=(',', ':'))+'\n', encoding='utf-8')


def turn(yaw, target, amount):
    delta = (target-yaw+180) % 360-180
    return yaw+max(-amount, min(amount, delta))


def sample_path(points):
    rows, distances, distance = [points[0]], [0.0], 0.0
    for a, b in zip(points, points[1:]):
        length = math.dist(a, b)
        for i in range(1, max(1, math.ceil(length/SPACING))+1):
            alpha = i/max(1, math.ceil(length/SPACING))
            rows.append([a[k]+(b[k]-a[k])*alpha for k in range(3)])
            distances.append(distance+length*alpha)
        distance += length
    return rows, distances


def prepare():
    native = read(OUT/'native.json')
    schedules = read(ROOT/'Saved/Automation/RoomCorridor/Playtime/after/simulation.json')
    previous = read(ROOT/'Saved/Automation/RoomCorridor/native-after.json')
    assert native['status'] == 'PASS' and native['map_files_unchanged']
    assert native['map_sha256_after'] == previous['map_sha256_after']
    assert schedules['native_sha256'] == digest(ROOT/'Saved/Automation/RoomCorridor/native-after.json')
    assert native['layout_sha256'] == digest(ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json')
    assert all(digest(ROOT/'Content/Maps'/(k+'.umap')) == v for k,v in native['map_sha256_after'].items())
    profiles = {p['GuardProfileId']: p for p in native['guard_profiles']}
    result = dict(native_sha256=digest(OUT/'native.json'), dt=DT, spatial_spacing_m=SPACING, maps=[])
    for m, sim in zip(native['maps'], schedules['maps']):
        assert m['id'] == sim['id']
        points, point_ids, observers, observer_ids = [], {}, [], {}
        def point_id(p):
            key = tuple(round(v, 3) for v in p)
            if key not in point_ids:
                point_ids[key] = len(points); points.append(list(key))
            return point_ids[key]
        def observer_id(p, radius):
            key = tuple(round(v, 3) for v in p)+(round(radius, 3),)
            if key not in observer_ids:
                observer_ids[key] = len(observers); observers.append(dict(xyz=list(key[:3]), radius=radius))
            return observer_ids[key]
        row = dict(id=m['id'], points=points, observers=observers, guards=[], scenarios={}, nodes={})
        for k, node in m['nodes'].items():
            if node['kind'] in ('painting', 'button', 'vent'):
                row['nodes'][k] = dict(point=point_id(node['nav']), kind=node['kind'], room=node.get('room'))
        for g in m['guards']:
            assert g['ping_pong'] and g['complete'] and g['capsule_clear']
            profile = profiles[g['profile']]
            radius = profile['SightRadius']/100*native['security_balance']['guard_perception_range_multiplier']
            eye = g['capsule_half_m']+profile['EyeHeight']/100
            legs = g['waypoint_paths'][:-1]  # runtime reverses, it never uses last->first shortcut
            sequence = [(i, False, i+1) for i in range(len(legs))]
            sequence += [(i, True, i) for i in range(len(legs)-1, -1, -1)]
            sections, now, yaw = [], 0.0, 0.0
            speed = profile['PatrolSpeed']/100
            for leg_index, reverse, target in sequence:
                pts = list(reversed(legs[leg_index])) if reverse else legs[leg_index]
                for a,b in zip(pts, pts[1:]):
                    duration = math.dist(a,b)/speed
                    if duration < 1e-8: continue
                    yaw = math.degrees(math.atan2(b[1]-a[1], b[0]-a[0]))
                    sampled, distances = sample_path([a,b])
                    ids = [observer_id([p[0],p[1],p[2]+eye],radius) for p in sampled]
                    sections.append(dict(start=now,end=now+duration,kind='move',yaw=yaw,
                        ids=ids,distances=distances,length=distances[-1]))
                    now += duration
                wait = g['waypoint_waits'][target]
                if wait:
                    p=pts[-1];oid=observer_id([p[0],p[1],p[2]+eye],radius)
                    sections.append(dict(start=now,end=now+wait,kind='wait',yaw=yaw,ids=[oid]))
                    now += wait
            assert now > 0
            states, si, scan_yaw, old_si = [], 0, 0.0, -1
            for tick in range(math.ceil(now/DT)):
                t=tick*DT
                while si+1<len(sections) and t>=sections[si]['end']: si+=1
                s=sections[si];alpha=(t-s['start'])/(s['end']-s['start'])
                if s['kind']=='move':
                    index=min(len(s['ids'])-1,bisect.bisect_left(s['distances'],alpha*s['length']))
                    oid=s['ids'][index];angle=s['yaw']
                else:
                    if si!=old_si:scan_yaw=s['yaw']
                    offset=(-g['look_around_degrees'] if alpha<1/3 else g['look_around_degrees'] if alpha<2/3 else 0)
                    if g['look_around_enabled']:scan_yaw=turn(scan_yaw,s['yaw']+offset,g['look_turn_rate']*DT)
                    oid=s['ids'][0];angle=scan_yaw
                states.append([oid,round(math.cos(math.radians(angle)),6),round(math.sin(math.radians(angle)),6)])
                old_si=si
            row['guards'].append(dict(id=g['id'],states=states,period=len(states)*DT,
                authored_period=now,half_angle=profile['SightAngle']/2,grace=profile['DetectionGrace'],
                radius=radius,detention=g['detention'],actor_path=g['actor_path']))
        for name in SCENARIOS:
            scenario=sim['scenarios'][name];tracks=[]
            for track in scenario['tracks']:
                states=[]
                for event in track['events']:
                    if event['kind']=='move':
                        sampled,distances=sample_path(event['points']);ids=[point_id(p) for p in sampled]
                    else:ids=[point_id(m['nodes'][event['node']]['nav'])]
                    for tick in range(math.ceil(event['start']/DT-1e-8),math.ceil(event['end']/DT-1e-8)):
                        alpha=max(0,min(1,(tick*DT-event['start'])/max(1e-8,event['end']-event['start'])))
                        index=min(len(ids)-1,bisect.bisect_left(distances,alpha*distances[-1])) if event['kind']=='move' else 0
                        states.append([ids[index],event['kind']])
                tracks.append(states)
            row['scenarios'][name]=dict(tracks=tracks,baseline=scenario['finish_seconds'])
        result['maps'].append(row)
        print(m['id'],'points',len(points),'guard observers',len(observers),'guards',len(row['guards']))
    write('probes.json',result)


def camera_sees(c, point, t, capsule_half):
    center=[point[0],point[1],point[2]+capsule_half]
    delta=[center[i]-c['origin'][i] for i in range(3)]
    distance=math.sqrt(sum(v*v for v in delta))
    if distance>c['range_m'] or distance<1e-8:return False
    volume=c['volume'];local=[center[i]-volume['origin'][i] for i in range(3)]
    for axis,extent in zip(volume['axes'],volume['extent']):
        # Capsule support on OBB axes; conservative candidate-volume approximation.
        support=.34+(capsule_half-.34)*abs(axis[2])
        if abs(sum(a*b for a,b in zip(local,axis)))>extent+support:return False
    angle=math.radians(math.sin(t/c['sweep_period']*2*math.pi)*c['sweep_half_angle'])
    u,v=c['up'],c['forward'];cross=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
    dot=sum(a*b for a,b in zip(u,v))
    forward=[v[i]*math.cos(angle)+cross[i]*math.sin(angle)+u[i]*dot*(1-math.cos(angle)) for i in range(3)]
    return sum(a*b for a,b in zip(forward,delta))/distance>=math.cos(math.radians(c['half_angle']))


def overview(plan, native, result):
    """Debug-only top view; never imported into the player's floor-plan UI."""
    bounds=plan['bounds'];scale=5
    width=(bounds[2]-bounds[0])*scale+60;height=(bounds[3]-bounds[1])*scale+60
    def xy(p):return ((p[0]-bounds[0])*scale+30,(bounds[3]-p[1])*scale+30)
    def line(a,b,color,stroke=1):
        x,y=xy(a);u,v=xy(b)
        return f'<line x1="{x:.1f}" y1="{y:.1f}" x2="{u:.1f}" y2="{v:.1f}" stroke="{color}" stroke-width="{stroke}"/>'
    def marker(p,label,color):
        x,y=xy(p)
        return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="4" fill="{color}"/><text x="{x+6:.1f}" y="{y-5:.1f}" fill="{color}" font-size="10">{html.escape(label)}</text>'
    pieces=[f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" style="width:100%;background:#202925">']
    for wall in plan['walls']:
        a,b=([wall['start'],wall['fixed']],[wall['end'],wall['fixed']]) if wall['axis']=='h' else ([wall['fixed'],wall['start']],[wall['fixed'],wall['end']])
        pieces.append(line(a,b,'#839189',2))
    for guard in native['guards']:
        for path in guard['waypoint_paths'][:-1]:
            for a,b in zip(path,path[1:]):pieces.append(line(a,b,'#428b98'))
        pieces.append(marker(guard['waypoint_paths'][0][0],guard['id'],'#79c2ce'))
    for c in native['cameras']:
        pieces.append(marker(c['origin'],c['id'],'#ffd477'))
        pieces.append(line(c['origin'],[c['origin'][i]+c['forward'][i]*3 for i in range(3)],'#ffd477',2))
    for laser in native['lasers']:
        a=[laser['origin'][i]-laser['right'][i]*laser['extent'][1] for i in range(3)]
        b=[laser['origin'][i]+laser['right'][i]*laser['extent'][1] for i in range(3)]
        pieces.append(line(a,b,'#ec82c6',4));pieces.append(marker(laser['origin'],laser['id'],'#ec82c6'))
    for s in result['stations']:
        color='#e79b75' if s['guard_visible_fraction']==s['camera_visible_fraction']==0 else '#b6dc87'
        pieces.append(marker(native['nodes'][s['node']]['nav'],s['node'],color))
    pieces.append('</svg>')
    return ''.join(pieces)


def analyze():
    native=read(OUT/'native.json');probes=read(OUT/'probes.json');los=read(OUT/'los.json')
    schedule_path=ROOT/'Saved/Automation/RoomCorridor/Playtime/after/simulation.json'
    schedules=read(schedule_path)
    plans=read(ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json')['maps']
    assert los['status']=='PASS' and los['map_files_unchanged']
    assert los['probes_sha256']==digest(OUT/'probes.json')
    assert probes['native_sha256']==digest(OUT/'native.json')
    assert all(digest(ROOT/'Content/Maps'/(k+'.umap'))==v for k,v in native['map_sha256_after'].items())
    assert native['layout_sha256']==digest(ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json')
    result=dict(status='PASS',scope='Quiet patrol/camera first-exposure model, not AI behavior or match outcome',
        dt=DT,spacing_m=SPACING,phase_cases=PHASES,maps=[],input_hashes={str(p.relative_to(ROOT)):digest(p) for p in
        [OUT/'native.json',OUT/'probes.json',OUT/'los.json',schedule_path,Path(__file__)]})
    balance=native['security_balance'];difficulty={x['player_count']:x for x in native['player_count_difficulty']}
    for m,p,l,sm in zip(native['maps'],probes['maps'],los['maps'],schedules['maps']):
        assert m['id']==p['id']==l['id']==sm['id']
        clear=[set(x) for x in l['guard_clear_points']]
        camera_cache={}
        def camera_mask(pid,t):
            # Sweep periods are equal in current maps; key remains per-camera for future changes.
            keys=tuple(round((t % c['sweep_period'])/DT) for c in m['cameras'])
            key=(pid,keys)
            if key not in camera_cache:
                camera_cache[key]=sum(1<<i for i,c in enumerate(m['cameras']) if
                    l['camera_los_masks'][pid] & (1<<i) and camera_sees(c,p['points'][pid],t,m['capsule_cm'][1]/100))
            return camera_cache[key]
        def guard_sees(g,pid,t):
            oid,dx,dy=g['states'][int(t/DT)%len(g['states'])]
            if pid not in clear[oid]:return False
            a=p['observers'][oid]['xyz'];b=p['points'][pid]
            x,y,z=b[0]-a[0],b[1]-a[1],b[2]+1.65-a[2]
            distance=math.sqrt(x*x+y*y+z*z)
            return distance<=g['radius'] and (dx*x+dy*y)/max(distance,1e-8)>=math.cos(math.radians(g['half_angle']))
        ordered=sorted(p['guards'],key=lambda g:(not g['detention'],g['actor_path']))
        def guards_for(count):
            size=max(1,math.floor(len(ordered)*difficulty[count]['guard_count_multiplier']+.5))
            chosen=ordered[:size];sources=[g for g in ordered if not g['detention']]
            chosen += [sources[i%len(sources)] for i in range(max(0,size-len(ordered)))]
            return chosen
        row=dict(id=m['id'],guards=[{k:g[k] for k in ('id','period','radius')} for g in p['guards']],
                 cameras=len(m['cameras']),lasers=len(m['lasers']),stations=[],scenarios=[],
                 laser_comparisons=[{k:v for k,v in x.items() if not k.endswith('_points')} for x in sm['laser_comparisons']],
                 laser_schedule_checks=[])
        # Validate the second player's actual scheduled hold and its 3 s activation.
        # These are static schedule checks, not runtime overlap/RPC tests.
        for name in SCENARIOS:
            tracks=sm['scenarios'][name]['tracks'];checked=0
            for player,track in enumerate(tracks):
                for event in track['events']:
                    if event['kind']!='move':continue
                    for laser in event['opened']:
                        button='B'+laser[1:];valid=False
                        for other,holder in enumerate(tracks):
                            if other==player:continue
                            for index,hold in enumerate(holder['events']):
                                if hold['kind']!='hold' or hold['node']!=button:continue
                                if hold['start']>event['start']+1e-6 or hold['end']<event['end']-1e-6:continue
                                activation=holder['events'][index-1] if index else {}
                                valid=(activation.get('kind')=='laser_activate' and activation['node']==button
                                    and activation['end']-activation['start']+1e-6>=balance['security_laser_hold_duration_seconds']
                                    and abs(activation['end']-hold['start'])<1e-6)
                                if valid:break
                            if valid:break
                        assert valid,(m['id'],name,player,laser,'Missing other-player activation/hold')
                        checked+=1
            row['laser_schedule_checks'].append(dict(scenario=name,validated_open_moves=checked))
        # 600 s quiet-state sample, one possible global phase; 41 s includes observation.
        for key,node in p['nodes'].items():
            pid=node['point'];flags=[];cg=cc=0
            for tick in range(int(HORIZON/DT)):
                t=tick*DT;gs=any(guard_sees(g,pid,t) for g in ordered);cs=bool(camera_mask(pid,t))
                cg+=gs;cc+=cs;flags.append(gs or cs)
            window=round(41/DT);busy=sum(flags[:window]);safe=not busy;total=1
            for end in range(window,len(flags)):
                busy+=int(flags[end])-int(flags[end-window]);safe+=not busy;total+=1
            row['stations'].append(dict(node=key,kind=node['kind'],room=node['room'],
                guard_visible_fraction=cg/len(flags),camera_visible_fraction=cc/len(flags),
                uninterrupted_41s_start_fraction=safe/total))
        for name,scenario in p['scenarios'].items():
            count=len(scenario['tracks']);guards=guards_for(count);trials=[]
            for run in range(PHASES):
                rng=random.Random(922000+run)
                offsets=[0 if run==0 else rng.random()*g['period'] for g in guards]
                camera_offset=0 if run==0 else rng.random()*60
                first=None;streak={};build={};camera_tick=0
                steps=max(len(t) for t in scenario['tracks'])
                for tick in range(steps):
                    t=tick*DT
                    evaluate_camera=t+1e-7>=camera_tick*balance['security_camera_evaluation_interval_seconds']
                    if evaluate_camera:camera_tick+=1
                    for player,track in enumerate(scenario['tracks']):
                        if tick>=len(track):continue
                        pid,kind=track[tick]
                        for gi,g in enumerate(guards):
                            visible=guard_sees(g,pid,t+offsets[gi]);k=(player,gi)
                            streak[k]=streak.get(k,0)+DT if visible else 0
                            if streak[k]+1e-8>=g['grace']/difficulty[count]['detection_multiplier']:
                                first=dict(time=round(t,2),sensor=g['id'],type='guard',action=kind,player=player+1);break
                        if first:break
                        if evaluate_camera:
                            mask=camera_mask(pid,t+camera_offset)
                            for ci,c in enumerate(m['cameras']):
                                k=(player,ci);build[k]=build.get(k,0)+balance['security_camera_evaluation_interval_seconds'] if mask&(1<<ci) else 0
                                if build[k]+1e-7>=balance['security_camera_detection_build_up_seconds']:
                                    first=dict(time=round(t,2),sensor=c['id'],type='camera',action=kind,player=player+1);break
                        if first:break
                    if first:break
                trials.append(first)
            failures=[x for x in trials if x]
            row['scenarios'].append(dict(name=name,players=count,guards=len(guards),baseline=scenario['baseline'],
                clear_phase_cases=PHASES-len(failures),phase_cases=PHASES,
                first_guard=sum(x['type']=='guard' for x in failures),first_camera=sum(x['type']=='camera' for x in failures),
                first_action=dict(collections.Counter(x['action'] for x in failures)),trials=trials))
        result['maps'].append(row)
        print(m['id'],'no visual coverage',sum(x['guard_visible_fraction']==x['camera_visible_fraction']==0 for x in row['stations'] if x['kind']=='painting'),'/20',flush=True)
        for s in row['scenarios']:print(s['name'],'clear',s['clear_phase_cases'],'/',PHASES,'first',s['first_guard'],s['first_camera'],s['first_action'],flush=True)
    result['limitations']=[
        'Static native geometry LOS, 0.5 m spatial sampling, 0.05 s modeled time; not full character simulation.',
        'Guard waypoint-center ping-pong motion at quiet speed; acceleration, acceptance radius, avoidance and movement turn lag excluded.',
        'Guard eye uses capsule half height plus profile BaseEyeHeight. Player standing eye assumed 1.65 m above path floor.',
        'CCTV uses saved cone/range/sweep and continuous build-up; candidate volume uses conservative capsule support test.',
        'Guard grace is approximated per player; runtime selects one nearest visible target and has perception update latency.',
        '32 initial phase cases are sensitivity cases, not empirical detection probability. Extra 3/4-player guards reuse runtime-selected source routes with phase uncertainty.',
        'First visual detection terminates route qualification. Noise/investigation/chase/capture/alert feedback and rescue are NOT simulated; no complete match-time prediction.',
        'Stations with zero visual coverage may still trigger footstep or swap noise investigations.',
        'Other-player 3 s activation and hold intervals are checked against each laser-open movement. RPC/overlap and rearm timing are not executed.',
        'Laser detours are alternatives within the sampled route graph, not proof of a globally shortest or unavoidable detour. Active laser is an alarm trigger, not a physical wall.']
    write('result.json',result)
    rows=[];station_rows=[];laser_rows=[]
    for m in result['maps']:
        for laser in m['laser_comparisons']:
            cells=[m['id'],laser['id'],laser['art'],f"{laser['closed_m']:.1f} m",f"{laser['opened_m']:.1f} m"]
            laser_rows.append('<tr>'+''.join('<td>'+html.escape(v)+'</td>' for v in cells)+'</tr>')
        for s in m['scenarios']:
            cells=[m['id'],s['name'],str(s['guards']),f"{s['clear_phase_cases']}/{PHASES}",str(s['first_guard']),str(s['first_camera']),str(s['first_action'])]
            rows.append('<tr>'+''.join('<td>'+html.escape(v)+'</td>' for v in cells)+'</tr>')
        for s in m['stations']:
            cells=[m['id'],s['node'],str(s['room'] or s['kind']),f"{s['guard_visible_fraction']:.1%}",f"{s['camera_visible_fraction']:.1%}",f"{s['uninterrupted_41s_start_fraction']:.1%}"]
            station_rows.append('<tr>'+''.join('<td>'+html.escape(v)+'</td>' for v in cells)+'</tr>')
    report='''<!doctype html><html lang="ko"><meta charset="utf-8"><title>보안 배치 시뮬레이션</title><style>body{background:#161b19;color:#ede7d8;font:16px/1.6 "Malgun Gothic";max-width:1100px;margin:40px auto;padding:20px}table{width:100%;border-collapse:collapse}th,td{padding:8px;border-bottom:1px solid #46544e;text-align:left}summary,h1,h2{color:#dfc594}li{margin:8px}</style><h1>현재 맵의 경비·CCTV·레이저 배치 검토</h1>
<p>실제 맵의 Unreal 시야 차폐 검사 + 저장된 보안 설정 + 시간에 따른 경로 재생. 첫 시야 발각 이전까지만 평가하며, 이후 조사·추격·체포를 포함한 실제 플레이시간이나 승률은 산출하지 않습니다.</p>
<p>split2/3/4: 레이저를 피하는 할당량 경로. coop2: 동료가 빔을 유지 해제하는 경로. pulse2: 통과할 때만 동료가 버튼을 잡는 경로. 32개는 초기 순찰·카메라 타이밍을 달리한 가정 사례이며 실제 적발 확률이 아닙니다.</p>
<table><tr><th>맵</th><th>일정</th><th>경비 수</th><th>시야 발각 없는 사례</th><th>첫 경비 발각</th><th>첫 CCTV 발각</th><th>당시 행동</th></tr>'''+''.join(rows)+'''</table><h2>정지 위치의 감시 범위</h2><p>10분 기본 순찰에서 시야 노출 비율과 관찰+위조 41초 동안 어떤 시야에도 노출되지 않는 시작 시간 비율입니다. 소음으로 유도되는 경비 조사는 제외하므로 안전 판정이 아닙니다. 버튼 위치도 비교를 위해 같은 41초로 표시합니다.</p><table><tr><th>맵</th><th>위치</th><th>방/용도</th><th>경비 시야</th><th>CCTV 시야</th><th>41초 비노출 시작</th></tr>'''+''.join(station_rows)+'''</table><h2>범위와 한계</h2><ul>'''+''.join('<li>'+html.escape(s)+'</li>' for s in result['limitations'])+'''</ul><p><a href="result.json">원본 결과</a> · <a href="los.json">Unreal LOS 검사</a></p></html>'''
    laser_html='<h2>레이저 우회와 협동</h2><p>현재 경로 그래프에서 빔을 피하는 경로와 동료가 해제한 경로의 거리입니다. 전자는 전역 최단 우회 거리로 보장하지 않습니다. 모든 협동 진입·복귀 일정은 다른 플레이어의 3초 활성화와 홀드 구간을 검사했습니다. 실제 레이저는 이동을 막지 않으며 무시하고 통과하면 경보와 경비 조사를 일으킵니다.</p><table><tr><th>맵</th><th>레이저</th><th>작품</th><th>빔 회피</th><th>동료 해제</th></tr>'+''.join(laser_rows)+'</table>'
    maps_html='<h2>보안 배치 TOP VIEW</h2><p>청색: 경비 왕복 경로 · 노랑: CCTV 위치/기본 방향 · 분홍: 레이저. 주황 점: 10분 모델에서 시야 감시가 없던 작업/버튼 위치 · 연두 점: 시야 감시가 있었던 위치. 전체 공간의 안전도 지도는 아닙니다.</p>'
    for plan,m,r in zip(plans,native['maps'],result['maps']):
        svg=overview(plan,m,r);(OUT/(m['id']+'-security.svg')).write_text(svg,encoding='utf-8')
        maps_html+='<h3>'+m['id']+'</h3>'+svg
    report=report.replace('<h2>정지 위치의 감시 범위</h2>',maps_html+'<h2>정지 위치의 감시 범위</h2>')
    report=report.replace('<h2>범위와 한계</h2>',laser_html+'<h2>범위와 한계</h2>')
    (OUT/'report.html').write_text(report,encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('stage',choices=('prepare','analyze'))
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    prepare() if args.stage=='prepare' else analyze()
