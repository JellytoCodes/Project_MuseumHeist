"""Offline, source-hash-bound plan for the authorized ceiling/frame alignment.

Reads saved Editor snapshots. Does not rebuild rooms or write Unreal packages.
"""
from pathlib import Path
import json,math,hashlib
from collections import Counter
def rotate(q,v):
 x,y,z,w=q;vx,vy,vz=v
 tx,ty,tz=2*(y*vz-z*vy),2*(z*vx-x*vz),2*(x*vy-y*vx)
 return [vx+w*tx+y*tz-z*ty,vy+w*ty+z*tx-x*tz,vz+w*tz+x*ty-y*tx]
R=Path(__file__).resolve().parents[3];O=R/'Saved/Automation/M01M02Alignment20260930';O.mkdir(parents=True,exist_ok=True)
META=json.loads((R/'ProjectResources/SourceArt/Canvas/canvas_uv.json').read_text())
def merge(xs):
 out=[]
 for l,u in sorted(xs):
  if out and l<=out[-1][1]+2:out[-1][1]=max(u,out[-1][1])
  else:out.append([l,u])
 return out
def subtract(xs,cuts):
 for l,u in cuts:
  ys=[]
  for a,b in xs:
   if u<=a or l>=b:ys.append([a,b]);continue
   if l>a:ys.append([a,l])
   if u<b:ys.append([u,b])
  xs=ys
 return xs
def center(b):return [(a+c)/2 for a,c in zip(*b)]
def canvas(a):return next((c for c in a['components'] if 'SM_Canvas_Painting_' in c.get('mesh','')),None)
def plan(code):
 name=code+('_ClassicalPrototype' if code=='M01' else '_MoonlitPrototype')
 d=json.loads((R/f'Saved/Automation/CommonMapRules/{name}_after.json').read_text());source=R/f'Content/Maps/{name}.umap'
 assert hashlib.sha256(source.read_bytes()).hexdigest()==d['sha256']
 rows=d['actors'];floor=0 if code=='M01' else 10;ceiling=floor+800
 paintings=[a for a in rows if canvas(a)];cases=[a for a in paintings if a.get('case_id')];decos=[a for a in paintings if not a.get('case_id')]
 protected={a['protected_painting_case'] for a in rows if a.get('protected_painting_case')};protected|={a['name'] for a in cases if 'Target' in a['case_id']}
 kept=[a for a in cases if a['name'] in protected]
 while len(kept)<12:
  q=max((a for a in cases if a not in kept),key=lambda a:min(math.dist(a['transform']['location'][:2],b['transform']['location'][:2]) for b in kept));kept.append(q)
 # Keep most dispersed existing decorative prints, retaining their texture identities.
 decor=[]
 while len(decor)<48:
  q=max((a for a in decos if a not in decor),key=lambda a:min(math.dist(center(canvas(a)['bounds'])[:2],center(canvas(b)['bounds'])[:2]) for b in kept+decor));decor.append(q)
 removed=[a['name'] for a in paintings if a not in kept+decor]
 walls=[]
 for a in rows:
  for c in a['components']:
   mesh=c.get('mesh','');b=c.get('bounds')
   if not b or b[0][2]>40 or b[1][2]<250:continue
   if (code=='M01' and 'SM_Display_Wall_' in mesh) or (code=='M02' and 'SM_WALL_B_' in mesh and 'DOOR' not in mesh):
    size=[b[1][i]-b[0][i] for i in range(3)];axis=0 if size[0]<size[1] else 1
    if size[axis]<110 and size[1-axis]>90 and 'Bend' not in mesh and 'Corner' not in mesh:walls.append((a,c,b,axis))
 groups={}
 for a,c,b,axis in walls:
  h=1-axis
  for sign in [-1,1]:groups.setdefault((axis,round(b[1 if sign>0 else 0][axis]),sign),[]).append([b[0][h],b[1][h]])
 surfaces=[]
 for (axis,face,sign),spans in groups.items():
  h=1-axis
  cuts=[(b[0][h]-55,b[1][h]+55) for _,_,b,k in walls if k!=axis and b[0][axis]-1<face+sign*15<b[1][axis]+1]
  for l,u in subtract(merge(spans),cuts):
   if u-l>=360:surfaces.append(dict(axis=axis,plane=face,sign=sign,lo=l+35,hi=u-35,used=[]))
 allb=[b for _,_,b,_ in walls]
 boundsxy=([-6058,-4430],[6092,4420]) if code=='M01' else ([-5400,-4420],[5400,4400])
 def choices(a,variant,tier,lock=False):
  m=META[variant-1];s=([4,6,8] if variant==4 else [2,3,4])[tier-1];w=(m['max'][1]-m['min'][1])*s;hh=(m['max'][2]-m['min'][2])*s
  old=center(canvas(a)['bounds']);q=canvas(a)['world']['rotation'];normal=rotate(q,[1,0,0] if META[int(canvas(a)['mesh'].split('_')[-1][:2])-1]['thin_axis']==0 else [0,1,0]);oldaxis=0 if abs(normal[0])>abs(normal[1]) else 1;oldsign=1 if normal[oldaxis]>0 else -1
  opts=[]
  for i,t in enumerate(surfaces):
   axis=t['axis'];h=1-axis
   if lock and (axis!=oldaxis or t['sign']!=oldsign or abs(t['plane']-old[axis])>100):continue
   for l,u in subtract([[t['lo'],t['hi']]],t['used']):
    if u-l<w:continue
    positions={round(max(l+w/2,min(u-w/2,old[h]))),round((l+u)/2),round(l+w/2),round(u-w/2)}
    for mid in positions:
     p=[0,0,floor+[60,45,20][tier-1]+hh/2];p[axis]=t['plane']+t['sign']*4;p[h]=mid
     front=p[:];front[axis]+=t['sign']*230
     if any(not boundsxy[0][k]+55<front[k]<boundsxy[1][k]-55 for k in (0,1)):continue
     # Keep the complete front strip clear; detect walls and narrow passages.
     ob=False
     for b in allb:
      if min(mid+w/2+15,b[1][h])-max(mid-w/2-15,b[0][h])>1 and min(max(p[axis],front[axis]),b[1][axis])-max(min(p[axis],front[axis]),b[0][axis])>1:ob=True;break
     if ob:continue
     cost=math.dist(p[:2],old[:2])+(0 if axis==oldaxis and t['sign']==oldsign else 700)
     opts.append((cost,i,mid,p,s,w,hh))
  return sorted(opts,key=lambda x:x[0])
 # Match reference tier distribution; medium/large only for interactive cases.
 targetcounts={1:24,2:30,3:6};out=[];vpool=Counter({3:41,4:13,5:6})
 queue=[]
 largecases=sorted([a for a in kept if a['name'] in protected and 'Target' not in a['case_id']],key=lambda a:a['case_id'])[:2]
 queue += [(a,3) for a in largecases]
 queue += [(a,2) for a in kept if a not in largecases]
 queue += [(a,3 if i<4 else 2 if i<24 else 1) for i,a in enumerate(decor)]
 # Most constrained/protected works placed first, then larger decorative frames.
 for a,tier in queue:
  selected=[]
  for variant in [3,4,5]:
   if vpool[variant]:
    cs=choices(a,variant,tier,a['name'] in protected)
    if cs:selected.append((cs[0][0],variant,cs[0]))
  if not selected and a['name'] in protected:raise AssertionError(('Protected artwork needs reviewed wall space',code,a['name'],tier))
  if not selected:raise AssertionError(('Insufficient existing wall space',code,a['name'],tier))
  _,variant,(_,i,mid,p,s,w,hh)=min(selected,key=lambda x:x[0]);t=surfaces[i];t['used'].append([mid-w/2-45,mid+w/2+45]);vpool[variant]-=1;targetcounts[tier]-=1
  out.append(dict(name=a['name'],case_id=a.get('case_id'),artifact_id=a.get('artifact_id'),variant=variant,tier=tier,scale=s,center=p,width=w,height=hh,material=canvas(a)['materials'][0]['path'],surface={k:v for k,v in t.items() if k!='used'},original_center=center(canvas(a)['bounds'])))
 assert not any(targetcounts.values()) and not any(vpool.values())
 data=dict(code=code,map=name,source_sha256=d['sha256'],floor=floor,ceiling=ceiling,protected=sorted(protected),removed=removed,paintings=out)
 (O/f'{code}_plan.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
 print(code,'planned',len(out),'removed',len(removed),'sizes',Counter(p['tier'] for p in out),'max movement',round(max(math.dist(p['center'][:2],p['original_center'][:2]) for p in out)))
if __name__=='__main__':
 for code in ['M01','M02']:plan(code)
