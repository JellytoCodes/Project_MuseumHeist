"""Deterministic route/timing analysis from export_museum_movement_paths.py.

This is a navigation and scheduling model, not a playtest or win-rate model.
It uses static complete capsule-clear paths; active beams are excluded unless
the scenario explicitly supplies a second player holding their button.
"""
import collections
import hashlib
import heapq
import itertools
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "Saved/Automation/MuseumMovement"
native = json.loads((OUT / "native-paths.json").read_text(encoding="utf-8"))
assert native["status"] == "PASS" and native["map_files_unchanged"]
plans = json.loads((ROOT / "ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json").read_text(encoding="utf-8"))["maps"]
artifacts = {r["ArtifactId"]:r for r in json.loads((ROOT / "ProjectResources/DataTableImports/DT_ArtifactDataRow.json").read_text(encoding="utf-8"))}
profiles = {r["GuardProfileId"]:r for r in json.loads((ROOT / "ProjectResources/DataTableImports/DT_GuardData.json").read_text(encoding="utf-8"))}
contract = json.loads((ROOT / "ProjectResources/DataTableImports/DT_ContractDataRow.json").read_text(encoding="utf-8"))[0]
result = dict(scope="Native paths plus deterministic movement/work schedule", maps=[],
    native_sha256=hashlib.sha256((OUT/"native-paths.json").read_bytes()).hexdigest(),
    assumptions={"forgery_seconds":40, "observation_seconds":1, "laser_activation_seconds":3, "escape_seconds":2,
       "max_paintings_per_player":3, "vent_unlock_seconds":180, "player_routes_ignore_guard_and_cctv":True,
       "excluded":"search/learning, drawing failure, UI handling, acceleration/cornering, noise, alert/chase, rescue and human coordination",
       "travel_times":"lower-bound schedule for the specified 40-second successful forgery assumption; not an absolute speedrun bound",
       "runtime_player_count":"SIE is only a navigation query world; 2/3/4-player results are modeled schedules, not multiplayer execution"})


def line_sample(points, spacing=1.0):
    rows=[]
    for a,b in zip(points,points[1:]):
        n=max(1,math.ceil(math.dist(a,b)/spacing))
        for i in range(n):
            t=i/n
            rows.append([a[j]+(b[j]-a[j])*t for j in range(3)])
    if points:rows.append(points[-1])
    return rows


class Model:
    def __init__(self,m,p):
        self.m,self.p=m,p
        self.art={k:artifacts[v['artifact_id']] for k,v in m['nodes'].items() if v['kind']=='painting'}
        self.active={l['id'] for l in p['lasers'] if self.art[l['cases'][0]]['ItemGrade']=='FourStar'}
        self.graph=collections.defaultdict(list)
        for key,edge in m['paths'].items():
            a,b=key.split('|')
            if edge['complete'] and edge['capsule_clear']:
                self.graph[a].append((b,edge,False));self.graph[b].append((a,edge,True))
        self.cache={}

    def path(self,a,b,opened=()):
        key=(a,b,tuple(sorted(opened)))
        if key in self.cache:return self.cache[key]
        if a==b:return dict(length_m=0,points=[self.m['nodes'][a]['nav']],chain=[a])
        queue=[(0,a)];best={a:0};prev={}
        while queue:
            dist,x=heapq.heappop(queue)
            if dist!=best[x]:continue
            if x==b:break
            for y,e,rev in self.graph[x]:
                if (set(e['lasers'])&self.active)-set(opened):continue
                nd=dist+e['length_m']
                if nd<best.get(y,math.inf)-1e-6:
                    best[y]=nd;prev[y]=(x,e,rev);heapq.heappush(queue,(nd,y))
        if b not in best:raise ValueError((self.p['id'],a,b,'no safe path'))
        items=[];x=b;chain=[b]
        while x!=a:
            x,e,rev=prev[x];items.append(list(reversed(e['points'])) if rev else e['points']);chain.append(x)
        points=[]
        for pts in reversed(items):points.extend(pts if not points else pts[1:])
        out=dict(length_m=best[b],points=points,chain=chain[::-1]);self.cache[key]=out
        return out

    def speed(self,weight,sprint=False):
        v=self.m['movement'];pace='sprint' if sprint else 'walk'
        return max(v['minimum_'+pace+'_move_speed'],v[pace+'_move_speed']-weight*v[pace+'_weight_speed_penalty'])/100

    def track(self,start='SP1'):
        return dict(start=start,current=start,time=0,weight=0,value=0,distance_m=0,events=[],paintings=[])

    def move(self,t,b,opened=()):
        path=self.path(t['current'],b,opened);sec=path['length_m']/self.speed(t['weight'])
        t['events'].append(dict(kind='move',a=t['current'],b=b,start=t['time'],end=t['time']+sec,
                                points=path['points'],length_m=path['length_m'],weight=t['weight'],opened=list(opened)))
        t['time']+=sec;t['distance_m']+=path['length_m'];t['current']=b

    def wait(self,t,sec,kind):
        if sec<=0:return
        t['events'].append(dict(kind=kind,node=t['current'],start=t['time'],end=t['time']+sec,
                                points=[self.m['nodes'][t['current']]['nav']]))
        t['time']+=sec

    def work(self,t,art):
        self.wait(t,41,'forge');t['paintings'].append(art)
        t['value']+=self.art[art]['ArtifactValue'];t['weight']+=self.art[art]['Weight']

    def finalize(self,tracks,escape=True):
        for t in tracks:
            self.move(t,'Vent')
            t['arrival_seconds']=t['time']
            if escape:
                self.wait(t,max(0,180-t['time']),'vent_wait');self.wait(t,2,'escape')
        return dict(tracks=tracks,total_distance_m=sum(t['distance_m'] for t in tracks),
            arrival_seconds=max(t['arrival_seconds'] for t in tracks),finish_seconds=max(t['time'] for t in tracks),
            value=sum(t['value'] for t in tracks),painting_count=sum(len(t['paintings']) for t in tracks))


probes=dict(maps=[])
for m,p in zip(native['maps'],plans):
    model=Model(m,p);target=next(k for k,v in m['nodes'].items() if v.get('case_key')=='Target')
    high=next(k for k,v in m['nodes'].items() if v.get('case_key')=='HighValue')
    laser_cases={l['cases'][0] for l in p['lasers'] if l['id'] in model.active}
    row=dict(id=p['id'],active_lasers_from_artifact_contract=sorted(model.active),scenarios={},laser_comparisons=[],guard_loops=[])
    t=model.track();model.move(t,target);model.work(t,target)
    row['scenarios']['required']=model.finalize([t],False)
    row['scenarios']['required']['label']='필수 작품 왕복 · 할당량 미포함'
    # Closest-next visiting tour, followed by 2-opt. No forgery/inventory implied.
    todo=set(model.art);order=['SP1']
    while todo:
        x=min(todo,key=lambda n:(model.path(order[-1],n)['length_m'],n));order.append(x);todo.remove(x)
    order.append('Vent')
    improved=True
    while improved:
        improved=False
        for i in range(1,len(order)-2):
            for j in range(i+1,len(order)-1):
                before=model.path(order[i-1],order[i])['length_m']+model.path(order[j],order[j+1])['length_m']
                after=model.path(order[i-1],order[j])['length_m']+model.path(order[i],order[j+1])['length_m']
                if after<before-1e-5:order[i:j+1]=reversed(order[i:j+1]);improved=True
    t=model.track()
    for n in order[1:-1]:model.move(t,n)
    row['scenarios']['tour']=model.finalize([t],False);row['scenarios']['tour']['label']='작품 20점 순회 · 이동만'
    row['scenarios']['tour']['order']=order
    # Two crew travel together; one holds while the other forges high value.
    a,b=model.track(),model.track('SP2')
    for t in (a,b):model.move(t,target)
    model.work(a,target);model.wait(b,max(0,a['time']-b['time']),'cover')
    for t in (a,b):model.move(t,'B01')
    sync=max(a['time'],b['time'])
    for t in (a,b):model.wait(t,sync-t['time'],'rendezvous');model.wait(t,3,'laser_activate')
    hold_begin=b['time'];model.move(a,high,('L01',));model.work(a,high);model.move(a,'B01',('L01',))
    model.wait(b,a['time']-b['time'],'hold')
    available=set(model.art)-{target,high}
    extra=min(available,key=lambda n:model.path('B01',n)['length_m']+model.path(n,'Vent')['length_m'])
    for t in (a,b):model.move(t,extra)
    model.work(b,extra);model.wait(a,b['time']-a['time'],'cover')
    coop=model.finalize([a,b]);coop.update(label='2인 계속 홀드 · 필수+고가+추가 1점',holder_seconds=next(e['end']-e['start'] for e in b['events'] if e['kind']=='hold'),quota=6400)
    row['scenarios']['coop2']=coop
    a,b=model.track(),model.track('SP2')
    model.move(a,target);model.work(a,target);model.move(a,high);model.work(a,high)
    extra=min(set(model.art)-{target,high},key=lambda n:model.path('SP2',n)['length_m']+model.path(n,'Vent')['length_m'])
    model.move(b,extra);model.work(b,extra)
    bypass=model.finalize([a,b]);bypass.update(label='2인 고가 우회 · 홀드 없이 분산',quota=6400)
    row['scenarios']['bypass2']=bypass
    # Release the button after entry, collect a nearby ordinary work in parallel,
    # then return to hold for the carrier's exit. This models an optional player
    # strategy; no game rule requires a continuous hold during forgery.
    a,b=model.track(),model.track('SP2')
    model.move(a,target);model.work(a,target);model.move(a,'B01');model.move(b,'B01')
    sync=max(a['time'],b['time'])
    for t in (a,b):
        model.wait(t,sync-t['time'],'rendezvous');model.wait(t,3,'laser_activate')
    model.move(a,high,('L01',));model.wait(b,a['time']-b['time'],'hold');model.work(a,high)
    extra=min(set(model.art)-laser_cases-{target},key=lambda n:(model.path('B01',n)['length_m']/model.speed(0)
        +model.path(n,'B01')['length_m']/model.speed(model.art[n]['Weight']),n))
    model.move(b,extra);model.work(b,extra);model.move(b,'B01')
    sync=max(a['time'],b['time'])
    for t in (a,b):
        model.wait(t,sync-t['time'],'rendezvous');model.wait(t,3,'laser_activate')
    model.move(a,'B01',('L01',));model.wait(b,a['time']-b['time'],'hold')
    pulse=model.finalize([a,b]);pulse.update(label='2인 진입·복귀만 홀드 · 작업 병행',quota=6400,
        holder_seconds=sum(e['end']-e['start'] for e in b['events'] if e['kind'] in ('hold','laser_activate')))
    row['scenarios']['pulse2']=pulse
    # Parallel quota collection: known locations, successful work, three works max/person.
    # Greedy finish-time assignment is an achievable schedule, not a global optimum.
    for count in (2,3,4):
        quota=round(contract['BaseLootValueQuota']*contract['PlayerCountQuotaMultipliers'][count-1])
        tracks=[model.track('SP'+str(i+1)) for i in range(count)]
        model.move(tracks[0],target);model.work(tracks[0],target)
        available=set(model.art)-laser_cases-{target}
        while sum(t['value'] for t in tracks)<quota:
            options=[]
            for i,t in enumerate(tracks):
                if len(t['paintings'])>=3:continue
                for n in available:
                    end=t['time']+model.path(t['current'],n)['length_m']/model.speed(t['weight'])+41
                    ret=model.path(n,'Vent')['length_m']/model.speed(t['weight']+model.art[n]['Weight'])
                    makespan=max([end+ret]+[x['time']+model.path(x['current'],'Vent')['length_m']/model.speed(x['weight']) for j,x in enumerate(tracks) if j!=i])
                    options.append((makespan,end+ret,n,i))
            _,_,n,i=min(options);model.move(tracks[i],n);model.work(tracks[i],n);available.remove(n)
        s=model.finalize(tracks);s.update(label=str(count)+'인 분산 · Laser 없이 할당량',quota=quota)
        row['scenarios']['split'+str(count)]=s
    # Team rescue travel only, work/AI excluded.
    t=model.track();model.move(t,'Detention');model.move(t,'Evidence')
    row['scenarios']['rescue']=model.finalize([t],False);row['scenarios']['rescue']['label']='구금실→증거→Vent · 이동만'
    for laser in p['lasers']:
        button='B'+laser['id'][1:];art=laser['cases'][0]
        closed=model.path(button,art);opened=model.path(button,art,(laser['id'],))
        row['laser_comparisons'].append(dict(id=laser['id'],enabled_by_grade=laser['id'] in model.active,art=art,
            closed_m=closed['length_m'],opened_m=opened['length_m'],detour_ratio=closed['length_m']/max(.01,opened['length_m']),
            closed_points=closed['points'],opened_points=opened['points']))
    for g in m['guards']:
        speed=profiles[g['profile']]['PatrolSpeed']/100
        row['guard_loops'].append(dict(id=g['id'],distance_m=g['length_m'],seconds=g['length_m']/speed+g['waypoint_count']*g['waypoint_wait_seconds'],
            waypoint_wait_seconds=g['waypoint_wait_seconds'],waypoint_count=g['waypoint_count'],speed_mps=speed))
    # Native start->art and art->Vent routes: authored door crossing frequency.
    use=collections.Counter();jobs=[]
    for art in model.art:
        jobs.extend([model.path('SP1',art),model.path(art,'Vent')])
    for path in jobs:
        crossed=set()
        for d in p['doors']:
            axis=1 if d['axis']=='h' else 0;other=1-axis
            for a,b in zip(path['points'],path['points'][1:]):
                da,db=a[axis]-d['fixed'],b[axis]-d['fixed']
                if da*db>0 or abs(b[axis]-a[axis])<1e-6:continue
                alpha=-da/(db-da);pos=a[other]+(b[other]-a[other])*alpha
                if abs(pos-d['along'])<=d['width']/2+.05:crossed.add(d['id'])
        use.update(crossed)
    row['door_use']=[dict(id=k,count=v,share=v/len(jobs)) for k,v in use.most_common()]
    row['direct_return_m']={k:model.path(k,'Vent')['length_m'] for k in model.art}
    # Engine LOS probes for player route samples and stationary work/hold positions.
    probe_points={}
    for key,s in row['scenarios'].items():
        for i,t in enumerate(s['tracks']):
            for j,e in enumerate(t['events']):
                if e['kind']=='move':
                    pts=line_sample(e['points'],1)
                    for q,pt in enumerate(pts):probe_points[f'{key}:{i}:{j}:{q}']=pt
    # Positions are quantized to 1cm only for deduplication, not a 1m visibility grid.
    unique={};refs={}
    for key,point in probe_points.items():
        coord=','.join(str(round(v,2)) for v in point)
        if coord not in unique:unique[coord]=len(unique)
        refs[key]=unique[coord]
    pts=[[float(v) for v in c.split(',')] for c in unique]
    probes['maps'].append(dict(id=p['id'],points=pts,refs=refs))
    result['maps'].append(row)
    print(p['id'],'active lasers',sorted(model.active),'doors',row['door_use'][:4])
    for k,s in row['scenarios'].items():print(k,round(s['total_distance_m'],1),round(s['arrival_seconds'],1),round(s['finish_seconds'],1),s['value'],[t['paintings'] for t in s['tracks']])
    print('Laser',[(l['id'],round(l['closed_m'],1),round(l['opened_m'],1)) for l in row['laser_comparisons']])

(OUT/'simulation.json').write_text(json.dumps(result,ensure_ascii=False,allow_nan=False,indent=2)+'\n',encoding='utf-8')
(OUT/'sightline-probe-input.json').write_text(json.dumps(probes,ensure_ascii=False,allow_nan=False)+'\n',encoding='utf-8')
