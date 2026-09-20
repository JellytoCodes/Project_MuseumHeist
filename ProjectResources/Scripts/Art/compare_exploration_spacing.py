"""Compare saved native paths and draw measured before/after floor plans.

Distances use complete capsule-clear paths with enabled laser beams excluded.
Walking seconds are geometric estimates, not observed gameplay duration.
"""
import heapq
import hashlib
import json
import math
import statistics
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Automation/ExplorationExpansion'


def metrics(world):
    graph={key:[] for key in world['nodes']}
    for key,path in world['paths'].items():
        if not path['complete'] or not path['capsule_clear'] or path['lasers']:continue
        a,b=key.split('|');length=path['length_m']
        graph[a].append((b,length));graph[b].append((a,length))
    def distances(start):
        best={start:0};queue=[(0,start)]
        while queue:
            dist,node=heapq.heappop(queue)
            if dist!=best[node]:continue
            for dest,length in graph[node]:
                value=dist+length
                if value<best.get(dest,math.inf):
                    best[dest]=value;heapq.heappush(queue,(value,dest))
        return best
    pieces=[k for k,n in world['nodes'].items() if n['kind']=='painting']
    target=next(k for k in pieces if world['nodes'][k]['case_key']=='Target')
    start=distances('SP1');assert all(p in start for p in pieces)
    nearest=[min(distances(p)[q] for q in pieces if p!=q) for p in pieces]
    return dict(first_painting_m=round(min(start[p] for p in pieces),2),
        target_approach_m=round(start[target],2),
        target_return_m=round(distances(target)['Vent'],2),
        median_nearest_painting_m=round(statistics.median(nearest),2),
        active_paintings=len(pieces),native_pairs=world['native_pair_count'])


def main():
    before=json.loads((OUT/'native-before.json').read_text(encoding='utf-8'))
    after=json.loads((OUT/'native-after.json').read_text(encoding='utf-8'))
    assert before['status']==after['status']=='PASS'
    assert before['map_files_unchanged'] and after['map_files_unchanged']
    assert after['layout_sha256'] == hashlib.sha256((ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_bytes()).hexdigest(), 'Measure paths again after changing the layout'
    assert before['layout_sha256'] == hashlib.sha256((OUT/'layout-before.json').read_bytes()).hexdigest()
    assert all(hashlib.sha256((ROOT/'Content/Maps'/(name+'.umap')).read_bytes()).hexdigest() == value for name,value in after['map_sha256_after'].items()), 'Saved maps changed after measurement'
    plans=[json.loads((OUT/'layout-before.json').read_text(encoding='utf-8'))['maps'],
           json.loads((ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_text(encoding='utf-8'))['maps']]
    rows=[]
    for old,new in zip(before['maps'],after['maps']):
        a,b=metrics(old),metrics(new)
        rows.append(dict(map=new['id'],before=a,after=b,
            increase_percent={k:round((b[k]/a[k]-1)*100,1) for k in a if k.endswith('_m')}))
    report=dict(status='PASS',scope='Saved Recast paths and capsule sweeps; laser-free walking distances',
        excluded='Guard/CCTV reactions, actual discovery time, manual drawing, coordination and playtest duration',maps=rows)
    (OUT/'comparison.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    from PIL import Image, ImageDraw, ImageFont
    canvas=Image.new('RGB',(1920,1900),'#111519');draw=ImageDraw.Draw(canvas)
    font_path='C:/Windows/Fonts/segoeui.ttf'
    title=ImageFont.truetype(font_path,30);label=ImageFont.truetype(font_path,23)
    small=ImageFont.truetype(font_path,19)
    draw.text((60,30),'EXPLORATION SPACE / SAME SCALE COMPARISON',font=title,fill='#ece6d8')
    for index in range(3):
        for stage in range(2):
            plan=plans[stage][index];bounds=plans[1][index]['bounds']
            left,top=60+stage*960,115+index*550
            scale=min(840/(bounds[2]-bounds[0]),450/(bounds[3]-bounds[1]))
            def xy(p):return (left+(p[0]-bounds[0])*scale,top+65+(bounds[3]-p[1])*scale)
            def rect(b,color):
                x0,y1=xy(b[:2]);x1,y0=xy(b[2:]);draw.rectangle((x0,y0,x1,y1),fill=color)
            for room in plan['rooms'].values():rect(room['bounds'],'#263b42' if room['kind']=='security' else '#242b2c')
            for wall in plan['walls']:
                f,a,b,t=[wall[k] for k in ('fixed','start','end','thickness')]
                rect((a,f-t/2,b,f+t/2) if wall['axis']=='h' else (f-t/2,a,f+t/2,b),'#c7bca7')
            for prop in plan['props']:rect(prop['bounds'],'#64afb8' if prop['id'].startswith('DISCOVERY_') else '#636965')
            for p in plan['paintings']:
                x,y=xy(p['xy']);r=3 if p['active'] else 1.5
                draw.ellipse((x-r,y-r,x+r,y+r),fill='#edbd63' if p['active'] else '#8b8c84')
            for text,key in [('START','entry'),('EXIT','vent')]:
                x,y=xy(plan[key]);draw.text((x+4,y+5),text,font=small,fill='#99cab0')
            b=plan['bounds'];w,h=b[2]-b[0],b[3]-b[1]
            draw.text((left,top),f"{plan['id']}  {'BEFORE' if stage==0 else 'AFTER'}  |  {w:g} x {h:g} m",font=label,fill='#ece6d8')
            y=top+530;draw.line((left,y,left+10*scale,y),fill='#9ca6a8',width=3)
            draw.text((left+10*scale+12,y-13),'10 m',font=small,fill='#9ca6a8')
    draw.text((60,1800),'Gold: stealable artworks (20/map)   Grey: display-only art   Cyan: new sight-blocking partitions',font=label,fill='#b7c0bd')
    draw.text((60,1840),'Plan geometry. Actual discovery time and full play duration still require in-game playtesting.',font=small,fill='#b7c0bd')
    canvas.save(OUT/'comparison.png')
    print(json.dumps(report,ensure_ascii=False,indent=2))


if __name__=='__main__':main()
