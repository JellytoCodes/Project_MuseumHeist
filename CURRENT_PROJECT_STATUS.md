# Project_MuseumHeist Current Project Status

최종 갱신: 2026-08-15 KST
현재 문서 Revision: 12

이 문서는 새 Codex 작업이 Notion의 최신 진행 상태와 로컬 구현·검증 증거를 바로 이어받기 위한 오프라인 실행 캐시다.

- 설계와 구현 규칙은 `AGENTS.md` → `Museum_Heist_TDD.docx` → `Museum_Heist_GDD.docx` 순서를 따른다.
- Task 상태·우선순위·실행 순서는 Notion `주차별 작업보드`가 Live Source of Truth다.
- Notion에 아직 연결되지 않은 실질 작업은 `LOCAL_PROGRESS_INBOX.md`에 기록한다.
- 이 문서만 보고 Task 상태를 확정하지 않는다. 새 작업 시작 시 Notion을 다시 라이브 조회한다.

---

## 0. Notion Live Progress Source

- [Museum Heist — Project Leaderboard](https://app.notion.com/p/3831d26a5dfb81bfa7edeb4974818714)
- [주차별 작업보드](https://app.notion.com/p/884ad464d3334cdf890e595db0065fcf)  
  Data Source: `collection://c0d35883-8f6b-4467-b5be-62a1236073c6`
- [테스트 로그](https://app.notion.com/p/ec9727a40d9541e6a2e4ee6096b1c678)  
  Data Source: `collection://2f308111-75f1-4b68-bd45-f94361e855af`

마지막 라이브 재조회: `2026-08-15 KST`

```text
TASK-W6-000  완료    Contract Foundation / Required Target / Loot Value Quota
TASK-W6-006  완료    Player Contribution Capture
TASK-W6-007  완료    Team Result ViewModel / Widget
TASK-W6-008  완료    End Phase / Lobby Return
TASK-W6-009  완료    3-Map Shared Exit Placement — W6 Slice
TASK-W6-010  완료    W6 End-to-End Mission Gate
TASK-W6-011  완료    Shared Loose Loot Content 5+
W6 Gate      Pass    Contract Run Feature Complete

TASK-W7-001  진행중  Balance Runtime 소비·1P/4P 회귀 완료 / 2P·수동 튜닝 잔여
TASK-W7-002  검토중  Remote Nameplate / Crew Status 4P 동기화
TASK-W7-003  완료    Main HUD Team Status
TASK-W7-004  진행중  Stun Runtime / Input / HUD 완료, 최종 화면·오디오 잔여
TASK-W7-005  진행중  Arrest / Rescue Runtime 완료, Cuffed/RESCUE 연출 잔여
TASK-W7-006  진행중  Carry / Heavy Runtime 완료, Pose/Icon/Audio 잔여
TASK-W7-007  검토중  Owner-only Floor Plan Map
TASK-W7-008  검토중  Walk/Sprint/Weight/Footstep 계약
TASK-W7-009  검토중  Alert 작업 화면 강제 종료 / Main HUD 계약
TASK-W7-010  진행중  자동 E2E 완료 / 실제 2~3분 리듬 테스트 잔여
TASK-W7-011  완료    Surface/Map/Optional Variation
```

신규 Test Log:

```text
TEST-W6-008  Pass  TASK-W6-009 / M01 Exit·Nav·Deposit + M02/M03 W8 Checklist
TEST-W6-009  Pass  TASK-W6-011 / Loose Loot Definition + 2P Lifecycle
TEST-W6-010  Pass  TASK-W6-000·010 / Contract Foundation + Solo·4P Two-Run Gate
TEST-W7-001  Pass  W7 Team Readability / Map / Alert / Movement / Variation 통합 회귀
```

---

## 1. Current Focus

W7는 팀 가독성과 협동 피드백의 C++/복제/Input 기반을 구현하고 전체 자동화 회귀를 통과했다. 자동화로 완료 기준을 닫은 `TASK-W7-003`, `TASK-W7-011`은 완료 처리했고, 실제 화면·오디오·플레이 리듬 확인이 필요한 항목은 `진행중` 또는 `검토중`으로 유지했다.

```text
Editor Build        PASS / Win64 Development / Unity-safe
Full Automation     PASS / 21 total / Failed 0 / NotRun 0
Solo ContractRun    PASS / 2 consecutive runs / clean Lobby reset
4P ContractRun      PASS / 2 consecutive runs / clean Lobby reset
W7 Variation        PASS / real M01·M02·M03 12 rows / deterministic 24 draws
Notion Test Log     TEST-W7-001 / Pass
```

범위 경계:

- NullRHI 자동화는 상태·복제·입력·Widget 생성 계약을 검증하지만 렌더링 화면과 실제 청음을 대신하지 않는다.
- Stun/Arrest/Carry 최종 Presentation Asset, Nameplate/Map 화면 가독성, Alert 음악, 실제 2~3분 Escape 리듬은 후속 검토 대상이다.
- W7 Gate는 아직 닫지 않는다. Notion의 `진행중`/`검토중` Task를 완료 기준별로 마감한 뒤 판단한다.

---

## 2. Locked Direction

### Contract / Forgery

- Forgery Quality는 Alert 또는 Lockdown을 직접 올리지 않는다.
- 서버 확정 Quality 70 이상만 Replica를 승인한다.
- Timeout은 Replica 없이 작업을 폐기하고 근처 Guard 한 명에게 1회 조사만 요청한다.
- Surface/Object는 한글 중심 공통 UI 계약을 따르며 별도 `InstructionText`와 `ModeStatusText`를 사용하지 않는다.

### Contract Run

- Required Target 확보와 Loot Value Quota 달성은 서로 별도 조건이다.
- Player Count별 Optional Exhibit는 1/2/3/4P에서 2/3/4/5개를 서버가 활성화한다.
- 비선택 Level Case를 파괴하지 않고 replicated Contract 활성 상태로 숨김·충돌·상호작용을 비활성화한다.
- Shared Exit에서 각 Player의 Original과 Loose Loot를 서버가 Deposit하고 Result를 확정한다.

### Loose Loot

- 공용 `BP_Loot` Shell과 Data Row 기반 WorldMesh/Material/Transform을 사용한다.
- Pickup 승인 시 서버와 모든 Peer에서 Visual/Collision을 비활성화한다.
- Drop은 새 replicated Loot Actor를 만들고 Re-pickup과 Shared Exit Deposit을 지원한다.

---

## 3. 009–011 Implemented Artifacts

### Gameplay / Replication

- `Source/Project_MuseumHeist/Private/Core/HeistGameMode.cpp`
  - Player Count 기반 Optional Exhibit 2/3/4/5 선택·활성화
  - M01 4P Quota를 충족할 수 있는 5개 Optional Object Case 계약
- `Source/Project_MuseumHeist/Public/World/Actors/Loot/HeistPaintingDisplayCaseActor.h`
- `Source/Project_MuseumHeist/Private/World/Actors/Loot/HeistPaintingDisplayCaseActor.cpp`
- `Source/Project_MuseumHeist/Public/World/Actors/Loot/HeistObjectDisplayCaseActor.h`
- `Source/Project_MuseumHeist/Private/World/Actors/Loot/HeistObjectDisplayCaseActor.cpp`
  - `bContractExhibitActive` RepNotify
  - 비활성 Case의 표시·충돌·Tick·상호작용·Session·Inspection 차단
- `Source/Project_MuseumHeist/Private/Core/HeistHUD.cpp`
  - seamless transition 중 `ULocalPlayer` 미부착 Controller의 Widget 생성 차단
- `Source/Project_MuseumHeist/Private/World/Actors/Loot/HeistLootActor.cpp`
  - Pickup availability RepNotify와 Host/Client Visual·Collision 동기화
- `Source/Project_MuseumHeist/Private/Character/HeistPlayerController.cpp`
  - Loot Pickup 성공 팝업

### Automation

- `Source/Project_MuseumHeist/Private/Tests/HeistContractRunTests.cpp`
  - `ProjectMuseumHeist.ContractRun.SoloTwoRuns`
  - `ProjectMuseumHeist.ContractRun.FourPlayerTwoRuns`
- `Source/Project_MuseumHeist/Private/Tests/HeistLootDefinitionTests.cpp`
  - `ProjectMuseumHeist.Loot.Definition`
- `Source/Project_MuseumHeist/Private/Tests/HeistLootLifecycleTests.cpp`
  - `ProjectMuseumHeist.Loot.LifecycleTwoPlayer`

### Content

```text
Painting          Value 350   Grid 2x3
Crown             Value 1000  Grid 2x2
Ancient Sword     Value 450   Grid 1x3
Golden Vase       Value 600   Grid 2x1
Jewel Necklace    Value 700   Grid 1x1
```

5종은 Value, Grid, Weight, SpawnWeight와 World Visual Signature가 서로 구분된다.

---

## 4. Verification Evidence

### Build

```text
Target                 Project_MuseumHeistEditor Win64 Development
Engine                 Unreal Engine 5.8
ForceUnity Build       PASS
Final Errors           0
```

### TASK-W6-009

```text
M01 Exit Actor         BP_Vent / (0, 2800, 50)
Pre-Escape State       Inactive
Overlap                Ready
Map Contract           Exit 1 / PlayerStart 4 / RecastNavMesh 1 / PASS
Target→Exit Nav        Valid / NonPartial / 2816.5 cm
Actual Deposit         Solo 2 runs + 4P 2 runs / PASS
Log                    Saved/Logs/TASK-W6-009-Exit-Placement-Final-Retry1.log
```

### TASK-W6-010

```text
Solo                   2/2 Runs PASS / Secured 6200 / Quota 4000 / Error 0
Four Player            2/2 Runs PASS / Secured 12200 / Quota 11200 / Error 0
Second-Run Reset       Contribution 0 / Gameplay Input / Cursor·Move·Look unlocked
Terminal State         Originals 2/2 or 4/4 / Escaped all / Result Widget 1
Solo Report            Saved/Automation/TASK-W6-010-Solo-TwoRuns-Retry4/index.json
Solo Log               Saved/Logs/TASK-W6-010-Solo-TwoRuns-Retry4.log
4P Report              Saved/Automation/TASK-W6-010-4P-TwoRuns-Retry1/index.json
4P Log                 Saved/Logs/TASK-W6-010-4P-TwoRuns-Retry1.log
```

### TASK-W6-011

```text
Definition             Success / Warning 0 / Error 0
LifecycleTwoPlayer     Success / Warning 2 / Error 0
Rows                   5 required rows / unique definition and visual signatures
Lifecycle              5 Pickup + 5 Drop + 5 Re-pickup + 10 feedback
Deposit                Secured 3100 / Inventory Empty / PASS
Report                 Saved/Automation/TASK-W6-011-Loot-Final/index.json
Log                    Saved/Logs/TASK-W6-011-Loot-Final.log
```

Warning은 Title/Lobby RecastNavMesh 부재 및 테스트 Guard Noise의 OutsideRadius 진단이며 각 Gate의 실패 조건은 아니다.

---

## 5. Working Tree Safeguards

- W7 기반 구현은 `2395cdb Add W7 readability systems and contract safety`에 보존돼 있다.
- 후속 검증 보강은 같은 W7 범위의 최소 변경으로만 유지한다.
- `.uasset`과 `.umap`은 Unreal Editor 또는 승인된 MCP 경로로만 수정한다.
- M02/M03 실제 맵 배치는 W8 요청 전 임의로 진행하지 않는다.

---

## 6. Resume Here

1. `AGENTS.md`를 읽는다.
2. Notion 작업보드의 `진행중`/`검토중` Task와 사용자가 지정한 Task를 라이브 조회한다.
3. `LOCAL_PROGRESS_INBOX.md`의 `UNLINKED`/`READY_TO_SYNC` Entry를 확인한다.
4. `git status --short`와 관련 Diff를 확인한다.
5. `TEST-W7-001`과 아래 W7 Remaining Evidence를 확인하고 이미 통과한 기반을 재구현하지 않는다.
6. W7 Presentation 검토는 렌더링 Editor에서 화면·청음 증거를 수집하고 Task별 완료 기준만 닫는다.

---

## 7. W7 Implementation And Evidence

### Implemented Foundation

- `AHeistPlayerState` 공통 `CrewStatus` 복제: Active / Forging / Assembling / CarryingOriginal / Heavy / Stunned / Arrested / Escaped
- Remote-only `UHeistNameplateWidget`과 Main HUD Team Status가 동일 PlayerState 상태를 소비
- 서버 Walk 300 / Sprint 600, Weight 감속, Pace 기반 Footstep 500 / 1000
- Guard 접촉 Stun → 지연 Arrest → Teammate Rescue와 Owner Input/UI 복원
- `IA_Map`, `IMC_Map`, Owner-only `UHeistFloorPlanMapWidget` 및 Gameplay Input 복원
- Surface/Object 작업 중 Alarmed 진입 시 Widget 강제 종료, Session 해제, Gameplay Input 복원과 정상 재진입
- Random Map Shuffle Bag, Surface Template 최근 3개 보호, 고정 Seed 결정성, Optional Exhibit 조합 변화

### Verification

```text
Build       Saved/Logs/W7-Implementation-Build-Final.log
Full Log    Saved/Logs/W7-FinalRegression-Retry.log
Full Report Saved/Automation/W7-FinalRegression-Retry/index.json
Variation   Saved/Automation/W7-Variation-Final/index.json
Result      21/21 Success or SuccessWithWarnings / Failed 0
Notion      TEST-W7-001 Pass / live re-fetch confirmed
```

### Remaining Evidence

- W7-001: GuardCountMultiplier의 실제 Spawn 소비, 2P ContractRun, 수동 밸런스 표
- W7-002: 최종 아이콘과 렌더링 화면에서 Nameplate 거리 Fade/색상 가독성
- W7-004~006: Vignette/Low-pass, Cuffed/RESCUE, Carry Pose/Icon/Audio Asset
- W7-007: Floor Plan 최종 렌더링 화면
- W7-008: 실제 이동 입력 기반 2P Guard Investigate 가시 증거
- W7-009: Main HUD 색/Countdown과 실제 음악 레이어 청음
- W7-010: 실제 2~3분 단위 탐욕/탈출 리듬 테스트 시트

---

## 8. Update Contract

다음 경우 같은 작업 안에서 이 문서를 갱신한다.

- Notion Task 상태·우선순위·실행 순서가 바뀐 경우
- 구현 파일 또는 Asset 범위가 바뀐 경우
- Build, Blueprint Compile/Save, Automation, PIE, Multiplayer 증거가 생긴 경우
- Test Log 또는 Task 완료 상태를 실제로 갱신하고 재조회한 경우
- Blocker 또는 다음 재개 지점이 바뀐 경우
