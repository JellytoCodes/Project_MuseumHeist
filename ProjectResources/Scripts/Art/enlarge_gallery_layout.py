"""Enlarge the approved hanging plan once, preserving its distinct compositions."""
import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PATH = ROOT / 'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json'


def enlarge():
    data = json.loads(PATH.read_text(encoding='utf-8'))
    if data.get('painting_scale_revision') == 2:
        return data
    for plan in data['maps']:
        old = copy.deepcopy(plan['paintings'])
        pieces = plan['paintings']
        bounds, support = [], []
        for p in pieces:
            # Match the existing supporting wall and keep artwork inside its room.
            axis = 1 if p['normal'][0] else 0
            normal_axis = 1-axis
            candidates = [w for w in plan['walls']
                if (w['axis']=='v') == bool(axis)
                and w['start']-.01 <= p['xy'][axis] <= w['end']+.01
                and abs(p['xy'][normal_axis]-w['fixed']) < w['thickness']/2+.08]
            assert candidates, (plan['id'], p['id'], 'support')
            w = min(candidates, key=lambda w: abs(p['xy'][normal_axis]-w['fixed']))
            support.append((w['id'], tuple(p['normal'])))
            p['size'] = round(p['size']*1.5, 4)
            half = p['size']/2 + (.04 if p['active'] else .015)
            room = plan['rooms'][p['room']]['bounds']
            lo = max(w['start'],room[axis]) + half + .16
            hi = min(w['end'],room[axis+2]) - half - .16
            floor = half + (.34 if p['active'] else .20)
            ceiling = w['height'] - .18 - half
            assert lo <= hi and floor <= ceiling, (plan['id'],p['id'],'space')
            bounds.append((lo,hi,floor,ceiling,axis))
        # Preserve the original pairwise arrangement: side by side stays side by side,
        # stacked stays stacked. Project only the spacing needed for larger frames.
        constraints=[]
        for i,a in enumerate(old):
            for j in range(i+1,len(old)):
                b=old[j]
                if support[i] != support[j]: continue
                axis=bounds[i][4]
                dx=b['xy'][axis]-a['xy'][axis]; dz=b['z']-a['z']
                dim = 0 if abs(dx)>=abs(dz) else 1
                if dim==1 and a['pattern']=='Salon' and b['pattern']=='Salon':
                    # Large salon centrepieces now occupy a full-height column;
                    # shift the smaller upper neighbour sideways instead of clipping the cornice.
                    lower_index,upper_index=(i,j) if dz>0 else (j,i)
                    minimum=(pieces[i]['size']+pieces[j]['size'])/2+.10
                    if bounds[lower_index][2]+minimum>bounds[upper_index][3]:dim=0
                delta = dx if dim==0 else dz
                lower,upper=(i,j) if delta>0 else (j,i)
                gap=.22 if dim==0 else .10
                if dim==1 and pieces[upper]['active']:gap=.40
                required=(pieces[i]['size']+pieces[j]['size'])/2+gap
                constraints.append((lower,upper,dim,required))
        coords=[[p['xy'][b[4]],p['z']] for p,b in zip(pieces,bounds)]
        for iteration in range(20000):
            error=0
            for i,b in enumerate(bounds):
                for dim,(lo,hi) in enumerate(((b[0],b[1]),(b[2],b[3]))):
                    value=max(lo,min(hi,coords[i][dim]));error=max(error,abs(value-coords[i][dim]));coords[i][dim]=value
            for a,b,dim,minimum in constraints:
                short=minimum-(coords[b][dim]-coords[a][dim])
                if short>0:
                    error=max(error,short);coords[a][dim]-=short/2;coords[b][dim]+=short/2
            if error<1e-7:break
        assert error<1e-5,(plan['id'],'hanging constraints infeasible',error,
            [(p['id'],p['group'],p['pattern'],c,b[:4]) for p,c,b in zip(pieces,coords,bounds)
             if not b[0]-.001<=c[0]<=b[1]+.001 or not b[2]-.001<=c[1]<=b[3]+.001])
        for p,coord,b in zip(pieces,coords,bounds):
            p['xy'][b[4]]=round(coord[0],4);p['z']=round(coord[1],4)
            # Navigation inspection point now lies at the centre of the front Box.
            p['approach']=[round(v+n*.7,4) for v,n in zip(p['xy'],p['normal'])]
        for g in plan['groups']:
            ps=[p for p in pieces if p['group']==g['id']]
            axis=1 if ps[0]['normal'][0] else 0
            g['span']=[round(min(p['xy'][axis]-p['size']/2 for p in ps),4),
                       round(max(p['xy'][axis]+p['size']/2 for p in ps),4)]
        print(plan['id'],len(pieces),'paintings enlarged; spacing converged in',iteration+1,'iterations')
    data['painting_scale_revision']=2
    data['painting_scale_from_original']=1.5
    data['wall_trim_depth_ratio']=1.15
    PATH.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return data


if __name__=='__main__':
    enlarge()
