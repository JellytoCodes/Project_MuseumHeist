"""Author distinct room/corridor plans; binary maps are rebuilt separately in Editor."""
import copy
import heapq
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json'
OUT = ROOT/'Saved/Automation/RoomCorridor'


def wall(plan, key, axis, fixed, start, end):
    if end-start > .01:
        plan['walls'].append(dict(id=key, axis=axis, fixed=fixed, start=start, end=end,
                                 thickness=.24, height=plan['walls'][0]['height']))


def portal(plan, key, rooms, axis, fixed, start, end, along, width=3.2):
    wall(plan, key+'_A', axis, fixed, start, along-width/2)
    wall(plan, key+'_B', axis, fixed, along+width/2, end)
    plan['doors'].append(dict(id=key, rooms=rooms, axis=axis, fixed=fixed, along=along,
        width=width, xy=[along,fixed] if axis=='h' else [fixed,along], role='public'))


def split(plan, original, specs):
    # Rectangles tile the old room, so ceilings, room labels and loot anchors
    # describe the same spaces as the new physical partitions.
    old = plan['rooms'][original]['bounds']
    assert abs(sum((b[2]-b[0])*(b[3]-b[1]) for _,_,_,b in specs)-(old[2]-old[0])*(old[3]-old[1])) < .01
    for key,name,kind,bounds in specs:
        plan['rooms'][key] = dict(id=key,name=name,kind=kind,bounds=bounds)
    for door in plan['doors']:
        if original not in door['rooms']:
            continue
        x,y=door['xy']
        matches=[key for key,_,_,b in specs if b[0]-.001 <= x <= b[2]+.001 and b[1]-.001 <= y <= b[3]+.001]
        assert len(matches)==1,(door['id'],matches)
        door['rooms']=[matches[0] if r==original else r for r in door['rooms']]


def move_group(plan, key, room, anchor, normal):
    group=next(g for g in plan['groups'] if g['id']==key)
    old=group['anchor']; axis=1 if normal[0] else 0
    for piece in (p for p in plan['paintings'] if p['group']==key):
        delta=[piece['xy'][i]-old[i] for i in range(2)]
        depth=sum(delta[i]*group['normal'][i] for i in range(2))
        piece['xy']=[anchor[i]+(delta[i] if i==axis else normal[i]*depth) for i in range(2)]
        piece['normal']=normal[:];piece['room']=room
        piece['approach']=[piece['xy'][i]+normal[i]*.7 for i in range(2)]
    group.update(room=room,anchor=anchor,normal=normal)
    pieces=[p for p in plan['paintings'] if p['group']==key]
    group['span']=[min(p['xy'][axis]-p['size']/2 for p in pieces),max(p['xy'][axis]+p['size']/2 for p in pieces)]


def author(plan):
    code=plan['id']
    if code=='M01':
        camera=next(c for c in plan['cameras'] if c['id']=='C03')
        camera.update(door='D16',xy=[-28.57143,12.75],yaw=90)
    closed={'M01':['D08','D11','D18','D21','D26','D29'],
            'M02':['D02','D05','D12','D18','D23'],
            'M03':['D05','D06','D07','D22','D23','D25','D28','D31']}[code]
    for key in closed:
        door=next(d for d in plan['doors'] if d['id']==key)
        assert not any(x['door']==key for x in plan['lasers']+plan['cameras'])
        wall(plan,'ROOM_CLOSED_'+key,door['axis'],door['fixed'],door['along']-door['width']/2,door['along']+door['width']/2)
    plan['doors']=[d for d in plan['doors'] if d['id'] not in closed]
    if code=='M01':
        a,b=-37.14286,-17.14286
        split(plan,'W', [('WV','서측 굴절 복도','corridor',[a,-13.75,-31,13.75]),
            ('W','서측 연결 복도','corridor',[-31,-.25,b,5.75]),
            ('WN','초상 전실','hall',[-31,5.75,b,13.75]),('WS','드로잉 소전시실','gallery',[-31,-13.75,b,-.25])])
        portal(plan,'CW1',['WV','WS'],'v',-31,-13.75,-.25,-7.5)
        wall(plan,'CW2','h',-.25,-31,b)
        portal(plan,'CW3',['W','WN'],'h',5.75,-31,b,-25)
        wall(plan,'CW4','v',-31,5.75,13.75)
        a,b=17.14286,37.14286
        split(plan,'E',[('E','동측 연결 복도','corridor',[a,-5.75,31,.25]),
            ('EN','해양 소전시실','gallery',[a,.25,31,13.75]),('ES','남측 전실','hall',[a,-13.75,31,-5.75]),
            ('EVEST','보안 접근 복도','corridor',[31,-13.75,b,13.75])])
        portal(plan,'CE1',['E','EN'],'h',.25,a,31,24)
        portal(plan,'CE2',['E','ES'],'h',-5.75,a,31,26)
        wall(plan,'CE3','v',31,.25,13.75)
        wall(plan,'CE4','v',31,-13.75,-5.75)
        move_group(plan,'G04','WS',[-24,-13.75],[0,1])
        move_group(plan,'G17','EN',[17.14286,6.5],[1,0])
        plan.setdefault('loot_spawn_overrides',{})['Vault_01']=[22,-17]
        plan['concept']='중앙 Rotunda를 기준으로 굴절 복도와 크기가 다른 소전시실·전실·연속 전시실 묶음을 연결한다.'
        routes=[['V','N1','N2','WN','W','WV','V'],['H','N3','N4','N3','H'],
                ['H','S3','S2','S3','H'],['E','ES','S4','S5','BS','S1','V','W','H','E']]
    elif code=='M02':
        plan['concept']='정원 회랑을 방향 기준으로 삼고, 서측 막다른 전시 묶음·동측 특별전·남측 회귀 루프를 비대칭으로 연결한다.'
        routes=[['RW','RN','N2','N1','W2','N1','N2','RN','RW'],
                ['RE','E1','E2','N3','E2','E1','RE'],
                ['W1','RW','RS','S3','BS','V','W1'],None,
                ['RN','N2','N1','BN','N4','N3','E2','E1','RE','RN']]
        for key in ('RW','RN','RE','RS'):
            plan['rooms'][key]['kind']='corridor'
    else:
        lo,hi=-11.42857,11.42857
        split(plan,'C1',[('C1W','서측 진입 복도','corridor',[-46.75,lo,-39,hi]),
            ('C1N','북향 전이 복도','corridor',[-39,1.5,-16.5,hi]),
            ('C1','포스터 소전시실','gallery',[-39,lo,-16.5,1.5])])
        wall(plan,'CC1W','v',-39,lo,1.5)
        portal(plan,'CC1D',['C1','C1N'],'h',1.5,-39,-16.5,-33)
        split(plan,'C2',[('C2N','중앙 상부 복도','corridor',[-16.5,2.5,6.75,hi]),
            ('C2E','중앙 굴절 복도','corridor',[6.75,lo,13.75,hi]),
            ('C2','격자 전시실','gallery',[-16.5,lo,6.75,2.5])])
        portal(plan,'CC2D',['C2','C2N'],'h',2.5,-16.5,6.75,-5)
        wall(plan,'CC2E','v',6.75,lo,2.5)
        split(plan,'C3',[('C3S','동측 하부 복도','corridor',[13.75,lo,33.25,-2.5]),
            ('C3E','동측 굴절 복도','corridor',[33.25,lo,41.25,hi]),
            ('C3','색면 전시실','gallery',[13.75,-2.5,33.25,hi])])
        portal(plan,'CC3D',['C3','C3S'],'h',-2.5,13.75,33.25,24)
        wall(plan,'CC3E','v',33.25,-2.5,hi)
        plan['props']=[p for p in plan['props'] if p['id']!='DISCOVERY_C1']
        # Shorten the existing sheltered button return to join the new corridor
        # wall; otherwise it seals the south-to-north passage entirely.
        for p in plan['props']:
            if p['id']=='BUTTON_L02_COVER_1':p['bounds'][0]=36.5
            elif p['id']=='BUTTON_L02_COVER_2':p['bounds'][0],p['bounds'][2]=36.5,36.83
        plan['concept']='전시실을 관통하던 사거리 대신 위·아래로 교대해 꺾이는 복도에서 독립 소전시실과 북측 전시 날개로 진입한다.'
        routes=[['C0','C1W','C1N','N2','C1N','C2N','N3','C2N','C1N','C1W','C0'],
                ['C2E','C3S','C3','N4','N5','C4','C3E','C3S','C2E'],
                ['V','S1','S2','S3','C3S','C2E','C2N','C1N','C1W','C0','V']]
    plan['room_corridor_revision']=1
    plan['closed_gallery_portals']=closed
    return routes


def route_grid(plan, specs):
    # 0.5 m planning grid with 0.65 m obstacle inflation. Native Unreal checks
    # remain the acceptance gate; this only places safe authored waypoints.
    step=.5; x0,y0,x1,y1=plan['bounds']; nx,ny=round((x1-x0)/step),round((y1-y0)/step)
    blocks=[]
    for w in plan['walls']:
        f,a,b,t=w['fixed'],w['start'],w['end'],w['thickness']/2
        blocks.append([a,f-t,b,f+t] if w['axis']=='h' else [f-t,a,f+t,b])
    blocks.extend(p['bounds'] for p in plan['props'])
    occupied=set()
    for a,b,c,d in blocks:
        for i in range(max(0,math.floor((a-.65-x0)/step)),min(nx,math.ceil((c+.65-x0)/step))+1):
            for j in range(max(0,math.floor((b-.65-y0)/step)),min(ny,math.ceil((d+.65-y0)/step))+1):
                occupied.add((i,j))
    def xy(q):return [x0+q[0]*step,y0+q[1]*step]
    def snap(point):
        q=(round((point[0]-x0)/step),round((point[1]-y0)/step))
        candidates=[(q[0]+i,q[1]+j) for i in range(-12,13) for j in range(-12,13)]
        return min((p for p in candidates if p not in occupied and 1<=p[0]<nx and 1<=p[1]<ny),key=lambda p:math.dist(xy(p),point))
    def path(a,b):
        a,b=snap(a),snap(b);q=[(0,a)];best={a:0};previous={}
        while q:
            _,u=heapq.heappop(q)
            if u==b:break
            for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
                v=u[0]+dx,u[1]+dy
                if v in occupied or not (1<=v[0]<nx and 1<=v[1]<ny):continue
                cost=best[u]+1
                if cost<best.get(v,math.inf):
                    best[v]=cost;previous[v]=u
                    heapq.heappush(q,(cost+abs(v[0]-b[0])+abs(v[1]-b[1]),v))
        if b not in best:raise ValueError((plan['id'],xy(a),xy(b),'Disconnected route'))
        out=[b]
        while out[-1]!=a:out.append(previous[out[-1]])
        out=out[::-1]
        return [xy(v) for i,v in enumerate(out) if i in (0,len(out)-1) or
                (v[0]-out[i-1][0],v[1]-out[i-1][1])!=(out[i+1][0]-v[0],out[i+1][1]-v[1])]
    def center(key):
        b=plan['rooms'][key]['bounds'];return [(b[0]+b[2])/2,(b[1]+b[3])/2]
    for guard,rooms in zip(plan['guards'],specs):
        if rooms is None:continue
        pts=[]
        for a,b in zip(rooms,rooms[1:]):
            seg=path(center(a),center(b));pts.extend(seg if not pts else seg[1:])
        guard.update(rooms=rooms,polyline=pts,length_m=round(sum(math.dist(a,b) for a,b in zip(pts,pts[1:])),3))
    for route in plan['routes']:
        old=route['polyline'];pts=[]
        for a,b in zip(old,old[1:]):
            seg=path(a,b);pts.extend(seg if not pts else seg[1:])
        route.update(polyline=pts,length_m=round(sum(math.dist(a,b) for a,b in zip(pts,pts[1:])),3))
    for p in plan['paintings']:
        if p['active']:path(plan['entry'],p['approach'])


def main():
    data=json.loads(SOURCE.read_text(encoding='utf-8'))
    if all(p.get('room_corridor_revision')==1 for p in data['maps']):
        print('Already applied');return
    OUT.mkdir(parents=True,exist_ok=True)
    before=OUT/'layout-before.json'
    if before.exists():
        assert json.loads(before.read_text(encoding='utf-8'))==data,'Source differs from the pre-edit snapshot'
    else:
        before.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    updated=copy.deepcopy(data)
    for plan in updated['maps']:
        routes=author(plan)
        route_grid(plan,routes)
        plan['case_rooms']=[p['room'] for p in plan['paintings'] if p['active']]
        plan['notes'].append(plan['concept'])
        print(plan['id'],'closed',plan['closed_gallery_portals'],'rooms',len(plan['rooms']),'doors',len(plan['doors']))
    SOURCE.write_text(json.dumps(updated,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
