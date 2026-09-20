"""Expand measured gallery plans while retaining human-scale exhibits and doors.

Run before the approved Editor builder. Re-running an applied revision is a no-op.
Wall endpoints at door jambs follow the preserved opening, not a scaled gap.
"""
import copy
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PATH = ROOT / 'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json'
SCALES = {'M01': (10/7, 11/8), 'M02': (3/2, 11/8), 'M03': (11/8, 10/7)}


def add_discovery_partitions(data):
    # Wall-connected returns separate the first exhibits and the target gallery.
    # They preserve broad walkable bypasses instead of introducing locked gates.
    specs = {
        'M01': [('N1', [-45.69, 21.5, -45.45, 38.5]),
                ('S1', [-45.69, -38.5, -45.45, -21.5]),
                ('N4', [27.88, 25.0, 28.12, 38.5])],
        'M02': [('W1', [-54, -2.12, -40, -1.88]),
                ('W2', [-30.12, 9.5, -29.88, 22])],
        'M03': [('N1', [-52.12, 17, -51.88, 34.28571]),
                ('C1', [-33, -4.62, -16.5, -4.38]),
                ('N4', [24.88, 25.5, 25.12, 34.28571])],
    }
    for plan in data['maps']:
        for room, rectangle in specs[plan['id']]:
            key = 'DISCOVERY_'+room
            existing = next((p for p in plan['props'] if p['id'] == key), None)
            if existing:
                existing['bounds'] = rectangle
            else:
                plan['props'].append(dict(id=key, bounds=rectangle, kind='wall', height=3.2))
    return data


def expand(data):
    if data.get('exploration_spacing_revision') == 1:
        return copy.deepcopy(data)
    data = copy.deepcopy(data)
    for plan in data['maps']:
        old = copy.deepcopy(plan)
        sx, sy = SCALES[plan['id']]
        factors = (sx, sy)

        def xy(p):
            return [round(p[0]*sx, 5), round(p[1]*sy, 5)]

        def bounds(b):
            return xy(b[:2]) + xy(b[2:])

        def translated(p, anchor):
            dest = xy(anchor)
            return [round(dest[i]+p[i]-anchor[i], 5) for i in range(2)]

        plan['bounds'] = bounds(old['bounds'])
        for key, room in plan['rooms'].items():
            room['bounds'] = bounds(old['rooms'][key]['bounds'])
        for door in plan['doors']:
            along_axis = 0 if door['axis'] == 'h' else 1
            door['fixed'] = round(door['fixed']*factors[1-along_axis], 5)
            door['along'] = round(door['along']*factors[along_axis], 5)
            door['xy'] = xy(door['xy'])
        for wall in plan['walls']:
            axis = 0 if wall['axis'] == 'h' else 1
            original_fixed = wall['fixed']
            for end in ('start', 'end'):
                value = wall[end]*factors[axis]
                for door in old['doors']:
                    if door['axis'] != wall['axis'] or abs(door['fixed']-original_fixed) > .001:
                        continue
                    for side in (-1, 1):
                        if abs(wall[end]-(door['along']+side*door['width']/2)) < .001:
                            value = door['along']*factors[axis]+side*door['width']/2
                wall[end] = round(value, 5)
            wall['fixed'] = round(original_fixed*factors[1-axis], 5)
        for prop in plan['props']:
            prop['bounds'] = bounds(prop['bounds'])
        # Keep each hanging composition intact. Move its supporting wall anchor;
        # never scale frames, the gap to plaster, or the front interaction point.
        old_groups = {g['id']: g for g in old['groups']}
        for piece in plan['paintings']:
            piece['xy'] = translated(piece['xy'], old_groups[piece['group']]['anchor'])
            piece['approach'] = [round(v+n*.7, 5) for v,n in zip(piece['xy'],piece['normal'])]
        for group in plan['groups']:
            group['anchor'] = xy(group['anchor'])
            pieces = [p for p in plan['paintings'] if p['group'] == group['id']]
            axis = 1 if group['normal'][0] else 0
            group['span'] = [round(min(p['xy'][axis]-p['size']/2 for p in pieces), 5),
                             round(max(p['xy'][axis]+p['size']/2 for p in pieces), 5)]
        for key in ('entry', 'vent', 'evidence', 'detention'):
            plan[key] = xy(old[key])
        for row in plan['player_starts']:
            row['xy'] = translated(row['xy'], old['entry'])
        for row in plan['detention_starts']:
            row['xy'] = translated(row['xy'], old['detention'])
        for row in plan['evidence_slots']:
            row['xyz'] = translated(row['xyz'], old['evidence']) + [row['xyz'][2]]
        for key, point in old.get('loot_spawn_overrides', {}).items():
            plan['loot_spawn_overrides'][key] = xy(point)
        for collection in ('guards', 'routes'):
            for route in plan[collection]:
                route['polyline'] = [xy(p) for p in route['polyline']]
                route['length_m'] = round(sum(math.dist(a,b) for a,b in zip(route['polyline'],route['polyline'][1:])), 3)
        for camera in plan['cameras']:
            doorway = next((d for d in old['doors'] if d['id'] == camera.get('door')), None)
            # Preserve the tested camera/doorway ray and coverage range.
            camera['xy'] = translated(camera['xy'], doorway['xy']) if doorway else xy(camera['xy'])
        for laser in plan['lasers']:
            laser['xy'] = xy(laser['xy'])
            laser['button'] = xy(laser['button'])
        wing = plan['security_wing']
        for key in ('baffle', 'grille'):
            wing[key] = bounds(wing[key])
        for key in ('opening', 'approach'):
            wing[key] = xy(wing[key])
        wing['camera'] = xy(wing['camera']) + [wing['camera'][2]]
        wing['views'] = [[xy(p)+[p[2]], yaw] for p,yaw in wing['views']]
        plan['exploration_spacing'] = dict(previous_bounds=old['bounds'], scale_xy=list(factors),
            retained_active_paintings=20, retained_hanging_patterns=True,
            retained_door_widths=True, retained_artwork_dimensions=True)
        # Historic numeric notes must not masquerade as the new measured plan.
        plan['notes'] = [n for n in old['notes'] if not any(c.isdigit() for c in n)]
        plan['notes'].append('전시 구역과 군집 사이 이동 거리를 확대한다. 문 폭·액자 크기·군집 내부 간격·상호작용 영역·천장 높이는 유지한다. 작품 수와 위조 시간은 늘리지 않는다.')
        assert sum(p['active'] for p in plan['paintings']) == 20
        assert all(a['size'] == b['size'] for a,b in zip(old['paintings'],plan['paintings']))
        assert all(a['width'] == b['width'] for a,b in zip(old['doors'],plan['doors']))
    data['exploration_spacing_revision'] = 1
    return data


if __name__ == '__main__':
    original = json.loads(PATH.read_text(encoding='utf-8'))
    result = add_discovery_partitions(expand(original))
    if result != original:
        archive = ROOT / 'Saved/Automation/ExplorationExpansion'
        archive.mkdir(parents=True, exist_ok=True)
        if not (archive/'layout-before.json').exists():
            (archive/'layout-before.json').write_text(json.dumps(original, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    for plan in result['maps']:
        print(plan['id'], plan['bounds'], 'active paintings', sum(p['active'] for p in plan['paintings']))
