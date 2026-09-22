"""Replay native-path schedules with explicit, uncalibrated human-delay assumptions.

Run export_playtime_inputs.py in Unreal and analyze_museum_movement.py for both
ExplorationExpansion snapshots first. This is not an AI or multiplayer playtest.
"""
import collections
import argparse
import hashlib
import html
import json
import math
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Automation/PlaytimeEstimate'
SEED, RUNS = 20260921, 5000
PROFILES = {
    'cautious': dict(label='탐색·경계 가정 A', briefing=(30, 60), detour=(1.2, 1.6),
        per_art_handling=(15, 35), attempt_success=0.8, retry_reset=(5, 10),
        wait_chance_per_leg=0.5, wait_seconds=(10, 35)),
    'unfamiliar': dict(label='초행·재시도 가정 B', briefing=(60, 120), detour=(1.5, 2.2),
        per_art_handling=(30, 60), attempt_success=0.6, retry_reset=(10, 20),
        wait_chance_per_leg=0.75, wait_seconds=(20, 60)),
}


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def quantile(values, p):
    values = sorted(values)
    position = (len(values) - 1) * p
    lo, hi = math.floor(position), math.ceil(position)
    return values[lo] + (values[hi] - values[lo]) * (position - lo)


def inventory_fits(rects, occupied=0):
    # Exhaustive 5x5 placement, with rotation; no optimistic area-only check.
    if not rects:
        return True
    for w, h in sorted(set((rects[0], rects[0][::-1]))):
        for y in range(6 - h):
            for x in range(6 - w):
                bits = sum(1 << ((y + dy) * 5 + x + dx)
                           for dy in range(h) for dx in range(w))
                if not occupied & bits and inventory_fits(rects[1:], occupied | bits):
                    return True
    return False


def replay(schedule, durations, balance, observation, rng=None, profile=None):
    briefing = rng.uniform(*profile['briefing']) if profile else 0
    tracks = []
    for track in schedule['tracks']:
        totals = collections.defaultdict(float, briefing=briefing)
        failed_attempts = 0
        for event in track['events']:
            if event['kind'] == 'move':
                base = event['end'] - event['start']
                totals['native_travel'] += base
                if profile:
                    totals['search_detour'] += base * (rng.uniform(*profile['detour']) - 1)
                    if rng.random() < profile['wait_chance_per_leg']:
                        totals['assumed_security_wait'] += rng.uniform(*profile['wait_seconds'])
            elif event['kind'] == 'forge':
                attempt = durations[event['node']] + observation
                totals['successful_forgery'] += attempt
                if profile:
                    totals['identification_and_ui'] += rng.uniform(*profile['per_art_handling'])
                    while rng.random() >= profile['attempt_success']:
                        totals['retry'] += attempt + rng.uniform(*profile['retry_reset'])
                        failed_attempts += 1
                        if failed_attempts > 1000:
                            raise RuntimeError('Invalid retry assumptions')
            else:
                assert event['kind'] in ('vent_wait', 'escape'), event['kind']
        arrival = sum(totals.values())
        totals['vent_wait'] = max(0, balance['vent_unlock_time'] - arrival)
        totals['escape'] = balance['escape_cast_time']
        tracks.append(dict(seconds=sum(totals.values()), components=dict(totals),
                           failed_attempts=failed_attempts))
    # Crew work happens concurrently; do not sum player completion times.
    critical = max(tracks, key=lambda t: t['seconds'])
    return dict(seconds=critical['seconds'], critical_components=critical['components'],
                team_failed_attempts=sum(t['failed_attempts'] for t in tracks))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUT)
    parser.add_argument('--tuning', type=Path, default=OUT/'runtime-inputs.json')
    parser.add_argument('--before-simulation', type=Path, default=OUT/'before/simulation.json')
    parser.add_argument('--after-simulation', type=Path, default=OUT/'after/simulation.json')
    parser.add_argument('--before-native', type=Path, default=ROOT/'Saved/Automation/ExplorationExpansion/native-before.json')
    parser.add_argument('--after-native', type=Path, default=ROOT/'Saved/Automation/ExplorationExpansion/native-after.json')
    parser.add_argument('--title', default='맵 개편 전후 플레이타임 모델')
    args = parser.parse_args()
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    tuning = read(args.tuning)
    assert tuning['status'] == 'PASS'
    assert all(sha(ROOT / name) == value for name, value in tuning['hashes'].items()), 'Stale saved tuning'
    tables, balance = tuning['tables'], tuning['balance']
    artifacts = {r['ArtifactId']: r for r in tables['DT_ArtifactData']}
    contract = tables['DT_ContractData'][0]
    # Old schedule generator reads import JSON. Validate the fields it consumes
    # against freshly exported saved assets before accepting its route timings.
    source_art = read(ROOT / 'ProjectResources/DataTableImports/DT_ArtifactDataRow.json')
    for r in source_art:
        for field in ('ArtifactValue', 'Weight', 'ItemGrade', 'GridWidth', 'GridHeight'):
            assert r[field] == artifacts[r['ArtifactId']][field], (r['ArtifactId'], field)
    source_contract = read(ROOT / 'ProjectResources/DataTableImports/DT_ContractDataRow.json')[0]
    for field in ('BaseLootValueQuota', 'PlayerCountQuotaMultipliers'):
        assert source_contract[field] == contract[field]
    saved_guards = {r['GuardProfileId']: r for r in tables['DT_GuardData']}
    for row in read(ROOT/'ProjectResources/DataTableImports/DT_GuardData.json'):
        assert row['PatrolSpeed'] == saved_guards[row['GuardProfileId']]['PatrolSpeed']
    simulation_paths = {'before': args.before_simulation, 'after': args.after_simulation}
    native_paths = {'before': args.before_native, 'after': args.after_native}
    data = {phase: read(path) for phase, path in simulation_paths.items()}
    natives = {phase: read(path) for phase, path in native_paths.items()}
    for phase in data:
        assert natives[phase]['status'] == 'PASS' and natives[phase]['map_files_unchanged']
        assert data[phase]['native_sha256'] == sha(native_paths[phase])
        assert [m['id'] for m in data[phase]['maps']] == [m['id'] for m in natives[phase]['maps']]
    assert [m['id'] for m in data['before']['maps']] == [m['id'] for m in data['after']['maps']]
    assert all(sha(ROOT / 'Content/Maps' / (name + '.umap')) == value
               for name, value in natives['after']['map_sha256_after'].items())
    assert natives['after']['layout_sha256'] == sha(ROOT / 'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json')
    results, checks = [], []
    for mi, current in enumerate(data['after']['maps']):
        map_id = current['id']
        templates = [r['ForgeryDuration'] for r in tables['DT_ForgeryTemplate'] if r['SurfacePoolId'] == map_id]
        assert len(templates) == 40
        for count in (2, 3, 4):
            key = 'split' + str(count)
            schedules = {phase: data[phase]['maps'][mi]['scenarios'][key] for phase in data}
            for phase, schedule in schedules.items():
                nodes = natives[phase]['maps'][mi]['nodes']
                picked = [n for t in schedule['tracks'] for n in t['paintings']]
                assert len(picked) == len(set(picked))
                assert any(nodes[n].get('case_key') == 'Target' for n in picked)
                value = sum(artifacts[nodes[n]['artifact_id']]['ArtifactValue'] for n in picked)
                quota = round(contract['BaseLootValueQuota'] * contract['PlayerCountQuotaMultipliers'][count - 1])
                assert value == schedule['value'] and value >= quota
                for track in schedule['tracks']:
                    rects = [(artifacts[nodes[n]['artifact_id']]['GridWidth'], artifacts[nodes[n]['artifact_id']]['GridHeight']) for n in track['paintings']]
                    assert inventory_fits(rects + [(1, 1)]), 'Painting inventory plus one utility cell does not fit'
                    weight, time = 0, 0
                    for event in track['events']:
                        assert abs(event['start'] - time) < 1e-6 and event['end'] >= event['start']
                        time = event['end']
                        if event['kind'] == 'move':
                            movement = natives[phase]['maps'][mi]['movement']
                            speed = max(movement['minimum_walk_move_speed'], movement['walk_move_speed'] - weight * movement['walk_weight_speed_penalty']) / 100
                            assert event['weight'] == weight and not event['opened']
                            assert abs((event['end'] - event['start']) * speed - event['length_m']) < 1e-5
                        elif event['kind'] == 'forge':
                            weight += artifacts[nodes[event['node']]['artifact_id']]['Weight']
                    assert weight == track['weight']
                baseline = replay(schedule, dict.fromkeys(picked, 40), balance, tuning['observation_seconds'])
                assert abs(baseline['seconds'] - schedule['finish_seconds']) < 1e-6
                checks.append(f'{phase}/{map_id}/{count}P: quota, target, uniqueness, grid, weight, timeline, baseline PASS')
            row = dict(map=map_id, players=count, quota=quota, painting_count=len(picked),
                       value=schedules['after']['value'], baseline_before=schedules['before']['finish_seconds'],
                       baseline_after=schedules['after']['finish_seconds'], profiles={})
            for pi, (profile_id, profile) in enumerate(PROFILES.items()):
                simulations = {}
                for phase, schedule in schedules.items():
                    rng = random.Random(SEED + mi * 100 + count * 10 + pi)
                    nodes = sorted(n for n, v in natives[phase]['maps'][mi]['nodes'].items() if v['kind'] == 'painting')
                    runs = [replay(schedule, dict(zip(nodes, rng.sample(templates, len(nodes)))), balance,
                                   tuning['observation_seconds'], rng, profile) for _ in range(RUNS)]
                    seconds = [r['seconds'] for r in runs]
                    median = quantile(seconds, .5)
                    simulations[phase] = dict(p10=quantile(seconds, .1), p50=median, p90=quantile(seconds, .9),
                        over_time_limit_samples=sum(t > contract['MatchDurationSeconds'] for t in seconds),
                        median_example=min(runs, key=lambda r: abs(r['seconds'] - median)))
                row['profiles'][profile_id] = simulations
            results.append(row)
    result = dict(status='PASS', meaning='Model consistency only; no gameplay-duration or multiplayer release gate verdict',
        seed=SEED, samples_per_map_player_profile_phase=RUNS, profiles=PROFILES,
        scope='InGame start to last scheduled crew deposit; no lobby/loading/results',
        limitations=['Uncalibrated assumptions, not measured human behavior or statistical confidence intervals.',
            'Known-location quota route with delay overlays; does not model first-visit discovery decisions.',
            'No AI/LOS/detection/noise/alert/lockdown/arrest simulation, no network contention or individual skill correlation.',
            'Each draw uses its full saved 35/40/45-second budget; early submission and actual scoring not simulated.',
            'No Loose Loot or optional greed; these are painting-only sufficient-quota routes, not absolute fastest routes.',
            'Geometric retries assume the same template and independent success. Per-leg waits are assumed, not guard observations.',
            'All players work concurrently; one utility grid cell reserved per player; other carried items excluded.'],
        runtime=dict(balance=balance, match_limit_seconds=contract['MatchDurationSeconds'], observation_seconds=tuning['observation_seconds']),
        input_hashes={str(p.resolve().relative_to(ROOT)): sha(p) for p in
            [args.tuning, *simulation_paths.values(), *native_paths.values(), Path(__file__)]},
        checks=checks, results=results)
    (output / 'estimate.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    def mmss(t):
        seconds = round(t)
        return f'{seconds//60}:{seconds%60:02}'
    rows = []
    for r in results:
        cols = [r['map'], str(r['players']) + '인', str(r['painting_count']), mmss(r['baseline_before']), mmss(r['baseline_after'])]
        for p in PROFILES:
            q = r['profiles'][p]['after']
            cols.append(f"{mmss(q['p50'])} ({mmss(q['p10'])}–{mmss(q['p90'])})")
        rows.append('<tr>' + ''.join('<td>' + html.escape(v) + '</td>' for v in cols) + '</tr>')
        print(' | '.join(cols))
    comparison_rows = []
    for r in results:
        cols = [r['map'], str(r['players']) + '인']
        for p in PROFILES:
            q = r['profiles'][p]
            cols.append(f"{mmss(q['before']['p50'])} → {mmss(q['after']['p50'])} (+{mmss(q['after']['p50']-q['before']['p50'])})")
        comparison_rows.append('<tr>' + ''.join('<td>' + html.escape(v) + '</td>' for v in cols) + '</tr>')
    route_rows = []
    for before, after in zip(data['before']['maps'], data['after']['maps']):
        route_rows.append('<tr><td>' + after['id'] + '</td><td>' + mmss(before['scenarios']['tour']['finish_seconds']) +
            ' → ' + mmss(after['scenarios']['tour']['finish_seconds']) + '</td><td>' +
            ', '.join(f"{d['id']}: {d['count']}/40" for d in after['door_use'][:3]) + '</td><td>' +
            ', '.join(f"{g['id']}: {mmss(g['seconds'])}" for g in after['guard_loops']) + '</td></tr>')
    report = '''<!doctype html><html lang="ko"><meta charset="utf-8"><title>맵 플레이타임 모델</title>
<style>body{background:#151917;color:#eee7d6;font:16px/1.7 "Malgun Gothic",sans-serif;max-width:1200px;margin:48px auto;padding:24px}h1{font-size:32px}p{max-width:960px}table{border-collapse:collapse;width:100%;font-variant-numeric:tabular-nums}td,th{text-align:left;padding:12px;border-bottom:1px solid #454c45}th{color:#d2b986}strong{color:#edc989}pre{white-space:pre-wrap;background:#202722;padding:24px;font-size:14px}a{color:#d2b986}</style>
<h1>''' + html.escape(args.title) + '''</h1><p>현재 파일 SHA 대조 · 2/3/4인 병렬 일정 · 조건별 5,000회, 전후 총 180,000회</p>
<p><strong>구조 개편 효과를 비교하는 모델이며 실제 플레이시간 측정이 아닙니다.</strong> 모든 작품 순회와 최소 할당량 달성은 서로 다른 동선입니다. 환풍구는 180초에 개방하며 위조 시간·할당량은 그대로 유지했습니다.</p>
<p>기본 일정은 현재 Recast 경로·무게별 걷기 속도·할당량으로 계산했습니다. A/B는 탐색·경계·재시도를 임의 가정한 민감도 분석이며 실제 플레이타임 측정이나 경비 AI 시뮬레이션이 아닙니다. 모든 수치는 인게임 시작부터 마지막 팀원의 반출까지입니다.</p>
<table><thead><tr><th>맵</th><th>인원</th><th>작품 수</th><th>개편 전 기본</th><th>개편 후 기본</th><th>A 중앙값 (P10–P90)</th><th>B 중앙값 (P10–P90)</th></tr></thead><tbody>''' + ''.join(rows) + '''</tbody></table>
<h2>같은 가정의 전후 중앙값 비교</h2><table><thead><tr><th>맵</th><th>인원</th><th>A 전 → 후 (증가)</th><th>B 전 → 후 (증가)</th></tr></thead><tbody>''' + ''.join(comparison_rows) + '''</tbody></table>
<h2>이동과 경로 집중</h2><table><thead><tr><th>맵</th><th>20작품 순회 이동</th><th>자주 지나는 문</th><th>정적 순찰 주기 모델</th></tr></thead><tbody>''' + ''.join(route_rows) + '''</tbody></table>
<p>문 통과 횟수는 시작→각 작품 20개와 각 작품→출구 20개의 경로에서 집계했습니다. 실제 교통량이나 적발 확률이 아닙니다. 순찰 주기는 경로 길이/기본 순찰 속도+정지 시간이며 조사·추격·회전·상호 회피를 제외합니다.</p>
<p>전체 작품을 순회하는 시간과 계약에 필요한 일부 작품만 노리는 시간을 분리해서 해석해야 합니다. 순회가 길어져도 숙련 팀의 계약 완료가 같은 비율로 늦어지지는 않습니다. 긴 순찰 경로는 감시 공백을 늘릴 수 있으므로 실제 경비 마주침 빈도를 별도로 확인해야 합니다.</p>
<h2>가정과 읽는 방법</h2><p>A: 시작 정리 30~60초, 이동 1.2~1.6배, 작품당 식별·UI 15~35초, 시도 성공 확률 80%, 이동 구간마다 50% 확률로 10~35초 대기.<br>B: 시작 정리 60~120초, 이동 1.5~2.2배, 작품당 식별·UI 30~60초, 시도 성공 확률 60%, 구간마다 75% 확률로 20~60초 대기.<br>실패마다 동일 작품의 35/40/45초 작업과 재시작 A 5~10초 / B 10~20초를 추가했습니다. 수치는 실측으로 보정되지 않았으며 P10~P90도 가정 내 분포입니다.</p>
<p>체포·구출·경보 상승·추격·추가 파밍은 제외했습니다. 초행 가정도 경로를 새로 발견하는 의사결정을 구현한 것은 아닙니다. 따라서 실제 초행 완주 예측으로 단정할 수 없습니다. 현재 계약 제한은 1,200초(20분)이므로 목표 15~25분과 상한도 맞지 않습니다.</p>
<p>다음 확인: 2인 실제 플레이에서 첫 작품 발견, 첫 성공 위조, 할당량 달성, 최종 반출 시각을 기록해 이 가정을 보정해야 합니다. 분석에서 게임 수치는 변경하지 않았습니다.</p><details><summary>모델 검증</summary><pre>''' + html.escape('\n'.join(checks)) + '''</pre></details><p><a href="estimate.json">원본 계산 결과 JSON</a></p></html>'''
    (output / 'report.html').write_text(report, encoding='utf-8')
    print('PASS:', len(checks), 'schedule checks;', len(results) * len(PROFILES) * 2 * RUNS, 'scenario samples')


if __name__ == '__main__':
    main()
