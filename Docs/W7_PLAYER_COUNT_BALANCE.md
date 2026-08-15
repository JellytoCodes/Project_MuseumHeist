# W7 Player-count Balance Table

기준일: 2026-08-15
런타임 Source of Truth: `/Game/Data/DataAsset/DA_GameBalance`
소비 경로: `AHeistGameMode`(경비 수), `AHeistGuardAIController`(발각 유예/검사 시간), `HeistTeamReward::Calculate`(결과 보상)

이 문서는 TASK-W7-001의 인원별 기준값과 회귀 판정 근거를 한곳에서 비교하기 위한 스냅샷이다. 값 변경은 `DA_GameBalance`에서 수행하고 이 표와 `ProjectMuseumHeist.W7.Balance` 자동화를 함께 갱신한다.

## 불변 계약

- Required Target, Loot Value Quota, Quality 70 Replica 승인 Gate, Contract Outcome 판정은 이 표의 영향을 받지 않는다.
- Alert와 Arrest는 확보 가치나 할당량을 변경하지 않고 최종 Team Reward에만 적용된다.
- 인원별 난이도 계수는 서버가 확정하고 경비 Actor/상태 복제로 Client에 전달한다.
- 맵에 배치한 원본 경비는 삭제하지 않는다. 낮은 계수에서는 비활성화하고 높은 계수에서는 같은 Class/Profile/Patrol 설정을 복사한 보충 경비를 서버가 생성한다.

## 인원별 난이도

경비 목표 수는 `Max(1, RoundToInt(AuthoredGuardCount × GuardCountMultiplier))`다. 배치 경비가 0명이면 0명으로 유지하고 오류를 숨기기 위한 임의 Class Spawn은 하지 않는다.

| 플레이어 | Guard Count | Detection | 기본 0.35초 발각 유예 | Inspection Duration | 4명 배치 예시 | 현재 M01 1명 배치 |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.75 | 0.85 | 0.412초 | 2.40초 | 3명 | 1명 |
| 2 | 1.00 | 1.00 | 0.350초 | 2.00초 | 4명 | 1명 |
| 3 | 1.25 | 1.10 | 0.318초 | 1.80초 | 5명 | 1명 |
| 4 | 1.50 | 1.20 | 0.292초 | 1.60초 | 6명 | 2명 |

발각 유예는 `GuardData.DetectionGrace / DetectionMultiplier`, 검사 시간은 `2.0초 × InspectionDurationMultiplier`로 계산한다. 소수 경비 수는 가장 가까운 정수로 반올림하므로, 현재 M01처럼 기준 경비가 1명인 맵에서는 3인 1.25배가 1명으로 양자화된다.

## 결과 보상

| 항목 | 현재값 | 적용 범위 |
|---|---:|---|
| Minimum Forgery Reward Multiplier | 0.75 | Required Target Value만 |
| Maximum Forgery Reward Multiplier | 1.25 | Required Target Value만 |
| Alert Level Reward Penalty | 단계당 0.05 | Required Target Value의 Stealth 배율만 |
| Minimum Stealth Reward Multiplier | 0.75 | Alert 감점 하한 |
| Arrest Reward Penalty | 체포 1명당 0.10 | Target 보상과 Secured Loose Loot의 합계 |

```text
ForgeryMultiplier = Lerp(0.75, 1.25, Clamp(RequiredTargetQuality / 100))
StealthMultiplier = Clamp(1 - AlertLevelIndex * 0.05, 0.75, 1.00)
RewardSubtotal = Round(RequiredTargetValue * ForgeryMultiplier * StealthMultiplier)
               + SecuredLooseLootValue
ArrestPenalty = Round(RewardSubtotal * 0.10 * ArrestedCrewCount)
TeamReward = Max(0, RewardSubtotal - ArrestPenalty)
```

## 회귀 Gate

| 시나리오 | 자동화 | 확인 항목 | 최종 결과 |
|---|---|---|---|
| 1인 M01 2회 | `ProjectMuseumHeist.ContractRun.M01.SoloTwoRuns` | 1명 활성, Contract/Quota/Quality/Outcome 불변, Lobby Reset | PASS |
| 2인 M01 2회 | `ProjectMuseumHeist.ContractRun.M01.TwoPlayerTwoRuns` | 1명 활성, Stun/Arrest/Rescue, 반복 Assembly 소유권, Lobby Reset | PASS |
| 4인 M01 2회 | `ProjectMuseumHeist.ContractRun.M01.FourPlayerTwoRuns` | 서버 보충 경비 생성·활성·Ready·Nav 투영, Contract/Quota/Quality/Outcome 불변, Lobby Reset | PASS |
| 정적 수치 계약 | `ProjectMuseumHeist.W7.Balance` | 4개 Row, 경비 수 반올림, 보상 계수 범위 | PASS |

### 2026-08-15 최종 ContractRun 증거

실행 로그는 `Saved/Logs/W7-ContractRuns-PerceptionFix-Final.log`, 구조화 결과는 `Saved/Automation/W7-ContractRuns-PerceptionFix-Final/index.json`이다. 구조화 결과에서 Solo / TwoPlayer / FourPlayer TwoRuns가 모두 `Success`이며, 각 시나리오의 두 번째 실행 전 Lobby 복귀와 Gameplay Runtime 초기화가 확인됐다.

| 플레이어 | M01 활성 경비 | 발각 유예 | 검사 시간 | 2회 실행 / 초기화 |
|---:|---|---:|---:|---|
| 1 | 1명 (`Authored 1`, `Ready 1`) | 0.412초 | 2.40초 | `RunsCompleted=2`, `LobbyReturn=true`, `SecondRunCleanReset=true`, PASS |
| 2 | 1명 (`Authored 1`, `Ready 1`) | 0.350초 | 2.00초 | `RunsCompleted=2`, `LobbyReturn=true`, `SecondRunCleanReset=true`, PASS |
| 4 | 2명 (`Authored 1 + Supplemental 1`, `Ready 2`, `NavigationProjected 1`) | 0.292초 | 1.60초 | `RunsCompleted=2`, `LobbyReturn=true`, `SecondRunCleanReset=true`, PASS |

### 인원별 완료 결과 비교

아래 시간은 실제 플레이 시간이 아니라 동일 자동화 Fixture의 실행 시간이다. 실제 15~25분 체감 리듬은 TASK-W7-010의 별도 플레이테스트 범위로 유지한다.

| 플레이어 | 자동화 실행 시간 | Secured / Quota | Quota Margin | 최종 Outcome | 실패 원인 |
|---:|---:|---:|---:|---|---|
| 1 | 10.65초 | 6,200 / 4,000 | +2,200 | Success / ContractComplete | 없음 |
| 2 | 26.41초 | 9,200 / 6,400 | +2,800 | Success / ContractComplete | 없음 |
| 4 | 45.02초 | 12,200 / 11,200 | +1,000 | Success / ContractComplete | 없음 |

4인 실행은 두 Run 모두 보충 경비를 1명만 생성했고 `NavigationProjectionFailures=0`이었다. 모든 인원수에서 `AuthorityFlows=true`였으며, Required Target / Loot Value Quota / Quality 70 / Contract Outcome 계약은 인원별 경비·발각·검사 계수와 분리된 채 유지됐다.

자동화는 수치·권한·복제 회귀를 판정한다. 실제 15~25분 플레이의 체감 난이도와 경비 동선 밀도는 별도 수동 플레이테스트 결과로 이 표의 수치를 조정한다.
