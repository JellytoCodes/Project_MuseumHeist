# Project_MuseumHeist Current Project Status

최종 갱신: 2026-08-22 KST
현재 문서 Revision: 14

이 문서는 새 Codex 작업이 Notion의 최신 진행 상태와 로컬 구현·검증 증거를 바로 이어받기 위한 오프라인 실행 캐시다.

- 설계와 구현 규칙은 `AGENTS.md` → `Museum_Heist_TDD.docx` → `Museum_Heist_GDD.docx` 순서를 따른다.
- Task 상태·우선순위·실행 순서는 Notion `주차별 작업보드`가 Live Source of Truth다.
- Notion에 아직 연결되지 않은 실질 작업은 `LOCAL_PROGRESS_INBOX.md`에 기록한다.
- 이 문서만 보고 Task 상태를 확정하지 않는다. 새 작업 시작 시 Notion을 다시 라이브 조회한다.

> Rev14는 2026-08-18에 확정한 **Public Release 방향 재기준화**다. 2026-08-22 기준 C++ Rule/Authority/Replication 기반은 정적 구현했지만, Editor Build·Blueprint Shell·맵·UI·에셋·PIE·Cook·멀티플레이 Gate가 완료됐다는 의미는 아니다. 기존 W6/W7의 Solo·1P·Surface/Object 증거는 당시 구현의 역사적 검증으로 보존하며 Rev14 출시 Gate 통과로 재해석하지 않는다.

---

## 0. Notion Live Progress Source

- [Museum Heist — Project Leaderboard](https://app.notion.com/p/3831d26a5dfb81bfa7edeb4974818714)
- [주차별 작업보드](https://app.notion.com/p/884ad464d3334cdf890e595db0065fcf)  
  Data Source: `collection://c0d35883-8f6b-4467-b5be-62a1236073c6`
- [테스트 로그](https://app.notion.com/p/ec9727a40d9541e6a2e4ee6096b1c678)  
  Data Source: `collection://2f308111-75f1-4b68-bd45-f94361e855af`

마지막 라이브 재조회: `2026-08-22 KST` (`TASK-W8-008`만 재조회, 상태 `미시작`; 이번 작업에서 Notion 쓰기 없음)

```text
TASK-W6-000  완료    Contract Foundation / Required Target / Loot Value Quota
TASK-W6-006  완료    Player Contribution Capture
TASK-W6-007  완료    Team Result ViewModel / Widget
TASK-W6-008  완료    End Phase / Lobby Return
TASK-W6-009  완료    3-Map Shared Exit Placement — W6 Slice
TASK-W6-010  완료    W6 End-to-End Mission Gate
TASK-W6-011  완료    Shared Loose Loot Content 5+
W6 Gate      Pass    Contract Run Feature Complete

TASK-W7-001  완료    1P·2P·4P Guard Scaling / TwoRuns / 밸런스 표
TASK-W7-002  완료    Remote Nameplate / Crew Status 실제 WBP 4P 동기화
TASK-W7-003  완료    Main HUD Team Status
TASK-W7-004  진행중  Stun 화면·오디오·상태 FX 슬롯 기반 완료, Remote Stun Pose·실제 FX·화면·청음 잔여
TASK-W7-005  진행중  Arrest / Rescue UI·오디오 및 자동화 완료, 2P 실제 화면·청음 확인 잔여
TASK-W7-006  진행중  Carry / Heavy Icon·Audio·상태 FX 슬롯 기반 완료, Remote Pose·실제 FX·화면·청음 잔여
TASK-W7-007  완료    Owner-only Floor Plan Map / Marker 정책 / Stun Cleanup
TASK-W7-008  완료    2P 실제 이동 입력 / Footstep→Guard Investigate 복제
TASK-W7-009  완료    Alert 0~4 HUD·음악 / Alarmed 서버 강제 종료
TASK-W7-010  취소    TASK-W8-007 3-Map 9판 Release Balance Gate로 통합
TASK-W7-011  완료    Surface/Map/Optional Variation

TASK-W9-007  진행중  Legacy/Reference/License Audit — Cleanup·Fresh Package 검증 완료, 배포 권리 근거 잔여
```

신규 Test Log:

```text
TEST-W6-008  Pass  TASK-W6-009 / M01 Exit·Nav·Deposit + M02/M03 W8 Checklist
TEST-W6-009  Pass  TASK-W6-011 / Loose Loot Definition + 2P Lifecycle
TEST-W6-010  Pass  TASK-W6-000·010 / Contract Foundation + Solo·4P Two-Run Gate
TEST-W7-001  Pass  W7 Team Readability / Map / Alert / Movement / Variation 통합 회귀
TEST-W7-002  Pass  Player Count Guard Scaling + 2P Footstep Guard Investigation
TEST-W7-003  Pass  Remote Nameplate / Team Status 실제 WBP 4P 동기화
TEST-W7-004  Pass  Owner-only Floor Plan Map / 입력·정보정책·강제 Cleanup
TEST-W7-005  Pass  Alert 0~4 HUD / 음악 / Alarmed 서버 강제 종료
TEST-W7-006  Pass  Stun·Arrest/Rescue·Carry/Heavy 자동 Presentation 계약 / 2P TwoRuns / Lobby Cleanup
```

---

## 1. Current Focus

### Rev14 Release Rebaseline — C++ Foundation Applied / Editor Integration Pending

- Public Contract 지원 인원은 `2~4인`으로 바꾼다. 1인 PIE/자동화는 개발 진단 경로로만 남기고 출시 지원 인원으로 취급하지 않는다.
- InGame에서 2인이 1인으로 줄어든 경우 즉시 실패·즉시 종료하지 않는 soft-fail을 사용한다. Match 시작 시 확정한 Quota/Assignment는 재계산하지 않고, 남은 Player가 계속 진행하거나 탈출할 수 있어야 한다.
- v1 잠입 압박은 기존 Patrol Guard에 `CCTV`와 고가 Painting용 `Laser Hold`를 추가한다. Laser는 한 Player가 Hold Switch를 유지하고 다른 Player가 진입하는 선택적 협동 기믹이다.
- v1 플레이 콘텐츠는 Painting `Surface Forgery`를 중심으로 한다.
- `Object Assembly`는 삭제하지 않고 **Deferred Expansion**으로 보존한다. 소스·C++·Enum·Struct·DataTable·Blueprint Shell·원본 에셋은 repository에 남기되, v1 Runtime Assignment·Release Map·Player-facing UI·Result·Cook·QA/Release Gate에서는 비활성화한다.
- C++ 서버 기반에는 Public 2~4 Lobby Start Gate, 목적지 월드에서 1회 소비되는 `ContractStartPlayerCount`, InGame Join 거부, Surface-only Assignment/Result, Object Assembly Runtime Entry 차단, CCTV, Laser/Hold Button과 공통 Security Incident→Alert/근처 Guard 조사 경로를 반영했다.
- 2→1 이탈은 Contract의 Player Count·Quota·Assignment를 재계산하지 않는다. 원격 Client Quit 시 Pawn 파괴 전 Loose Loot를 월드 드롭으로 복구하고, 기존 Logout 안전망이 Original·Case Lock·Security Hold를 해제한 뒤 Carried Value를 다시 계산한다. Laser는 Query-only다.
- 기존 Object Assembly 소스·데이터·Shell은 보존하되 정상 플레이의 Case 활성화·Session·Submit·Original·Inspection·Result 경로를 비활성화했다.
- 기존 자동화의 Object 성공 기대와 Public Solo Start 기대는 Rev14 경계에 맞게 재정렬했지만 실행하지 않았다.
- 같은 `.cpp` 안에서 Pending Start Count 초기화, Local Hold 입력 상태 초기화, GameState Delegate 해제, Loot 제거 후처리, Timer Handle 정리와 Laser 기본 상태 복구의 확정 중복만 통합했다. 파일 분할·새 Manager·공개 Gameplay 계약 변경은 하지 않았다.
- Widget, `.uasset`, `.umap`은 사용자 요청에 따라 변경하지 않았다.

```text
C++ Rule/Authority/Replication   UPDATED / static review only
Duplicate / Boilerplate Cleanup  APPLIED / same-file, behavior-preserving static review
Widget / uasset / umap           NOT CHANGED
Editor Build                     NOT RUN / user Editor required
Automation / PIE / Cook          NOT RUN
Blueprint Shell / Map Placement  NOT CHANGED / pending
Notion TASK-W8-008               미시작 / read only / no write
```

따라서 `TASK-W8-008`과 Rev14 Gate는 완료로 처리하지 않는다.

정적 검토에서 마지막 Player Disconnect 시 `Logout()`의 Outcome 판정이 `PlayerArray` 제거보다 먼저 실행되는 순서가 별도 기능 이슈 후보로 확인됐다. 이번 중복·보일러플레이트 정리에는 섞지 않았으며, 재현 테스트와 동작 수정이 필요한 후속 항목으로 남긴다.

### Pre-Rev14 Implementation Evidence

W7는 팀 가독성과 협동 피드백의 C++/복제/Input 기반과 Stun/Arrest/Carry 최종 HUD·Icon·Audio 구현을 마쳤다. 실제 HUD Widget Tree 동기화 후 2P TwoRuns와 전체 27/27 자동화를 통과했다. 2026-08-17 UE5 Manny와 `ABP_Unarmed`를 임시 Character/AnimBP 베이스로 연결했고, `/Game/Blueprints`의 Non-Blueprint Asset 29개를 `/Game/Assets`로 이동해 폴더 경계를 정리했다. 2026-08-18에는 상태별 Component를 늘리지 않고 재사용 Niagara/Audio Component 각 1개와 7개 non-Active 상태 Asset 슬롯을 추가했으며 W7 11/11 회귀를 통과했다. 현재 W7 마감 대상은 실제 Niagara/Sound Asset 할당, `TASK-W7-004`·`006`의 Remote Stun/Carry/Heavy Pose 연결과 `TASK-W7-004~006`의 2P 실제 화면·청음 확인이다.

```text
Editor Build        PASS / Win64 Development / pre-Rev14 implementation
Full Automation     PASS / 27 Success / WithWarnings 7 / Failed 0 / NotRun 0
Solo ContractRun    PASS / 2 consecutive runs / clean Lobby reset
2P Presentation     PASS / 2 consecutive runs / clean Lobby reset / PostLobbyFix
4P ContractRun      PASS / 2 consecutive runs / clean Lobby reset
W7 Variation        PASS / real M01·M02·M03 12 rows / deterministic 24 draws
HUD Widget Tree     PASS / actual StunOverlay·ArrestOverlay·Countdown·Reason nodes
Asset Boundary      PASS / Blueprints=27 BP·WBP only / NonBlueprintAfter=0 / 29 assets moved
Manny Base          PASS / SKM_Manny_Simple / ABP_Unarmed / CameraSocket=head
Manny Editor Build  PASS / Win64 Development / 10 of 10 actions
W7 Regression       PASS / 11 of 11 / Success 9 / WithWarnings 2 / Failed 0 / NotRun 0
Status FX Slots     PASS / 1 reusable Niagara + 1 reusable Audio / 7 state pairs / null=no-op
2P Camera Socket    PASS / Host·Client head resolved / FullBody visible / face clipping not observed
```

범위 경계:

- NullRHI 자동화는 상태·복제·입력·Widget 생성 계약을 검증하지만 렌더링 화면과 실제 청음을 대신하지 않는다.
- `TASK-W7-005`는 로컬 구현·자동화와 Notion 증거 동기화를 완료했지만 실제 2P 화면·청음 확인 전에는 완료로 단정하지 않는다.
- `BP_HeistPlayerCharacter`는 UE5 `SKM_Manny_Simple`과 `ABP_Unarmed` 베이스로 교체됐다. 다만 `BP_ApplyCrewStatusPresentation`의 실제 Remote Stun/Carry/Heavy Pose Layer는 아직 연결되지 않았으므로 `TASK-W7-004`·`006`은 완료 처리하지 않는다.
- 상태 VFX·전환 Sound 슬롯과 Lifecycle은 완료했지만 실제 Asset은 의도적으로 미할당이다. Asset 할당 후 late-join, 반복 전환, Escaped one-shot과 2P 화면·청음을 별도 검증한다.
- 실제 2~3분 Escape 리듬은 W8-007로 통합했다.
- W7 Gate는 아직 닫지 않는다. Notion의 `진행중`/`검토중` Task를 완료 기준별로 마감한 뒤 판단한다.

---

## 2. Locked Direction

### Contract / Forgery

- Forgery Quality는 Alert 또는 Lockdown을 직접 올리지 않는다.
- 서버 확정 Quality 70 이상만 Replica를 승인한다.
- Timeout은 Replica 없이 작업을 폐기하고 근처 Guard 한 명에게 1회 조사만 요청한다.
- v1에서는 Surface Forgery만 Player-facing 공통 UI 계약을 사용하며 별도 `InstructionText`와 `ModeStatusText`를 사용하지 않는다. Object Assembly UI 계약과 Asset은 Deferred Expansion 자료로만 보존한다.

### Contract Run

- Required Target 확보와 Loot Value Quota 달성은 서로 별도 조건이다.
- 현재 C++ 계약은 Public 2~4 Start와 Match 시작 인원 Snapshot을 사용하고 Drawing Painting만 Required/Optional Assignment에 포함한다. 다만 M01/M02/M03의 실제 Surface Case·Loose Loot 합산 Quota 도달 가능성과 기존 `MinimumOptionalExhibits` Authoring 기준은 Editor/Cook에서 재검증해야 한다.
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
Compile Mode           Adaptive Non-Unity / 변경 Translation Unit 개별 컴파일
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
5. `LOCAL-20260818-01`은 2026-08-22 C++ 구현 증거가 Notion에 아직 반영되지 않아 `READY_TO_SYNC`다. 사용자가 요청하기 전 Notion 상태를 쓰거나 완료 처리하지 않는다.
6. 현재 Revision으로 `Project_MuseumHeistEditor Win64 Development`를 먼저 Build하고 전체 Error를 보존한다. 과거 Build PASS는 현재 Revision의 컴파일 증거가 아니다.
7. Build PASS 뒤 Editor에서 CCTV/Laser/Hold Button의 비-Widget Blueprint Shell을 구성하고 Mesh·Material·Audio·Niagara 슬롯을 할당·컴파일·저장한다. 최종 외부 Asset은 출처·라이선스·Notice를 함께 기록한다.
8. M02 → M03 → M01 순으로 CCTV Coverage와 Laser/Button/Painting 연결을 배치한다. Required Target은 Laser 뒤에 두지 않고 2→1에서도 Required Target·최소 Quota·Egress가 가능해야 한다.
9. 세 맵의 Surface Painting + Loose Loot 도달 가능 금액, Object Case Release 배치/하드 참조 0, Object 전용 Cook Package 0을 확인한다.
10. 위 Editor 전제까지 확인된 뒤 2P Host/Client를 시작으로 CCTV Detection, Laser Hold/Release/Stun/Arrest/Disconnect, 사건당 Alert+경비 1회, Timeout Alert 미변경, 2→1 Snapshot 불변과 이탈 Player의 Loose Loot/Original 월드 복구를 PIE 검증한다.
11. 아래 W6/W7 Solo·1P·Surface/Object 증거는 삭제하지 않되 Rev14 출시 PASS로 보지 않는다. `TASK-W7-004~006`의 실제 Niagara/Sound·Remote Pose·2P 화면/청음 잔여도 별도 유지한다.
12. `TASK-W9-007`의 배포 권리·Notice는 Surface/TENADA/Epic 잔여와 Rev14 실제 Cook Manifest를 함께 검증한다.

---

## 7. W7 Implementation And Evidence

### Implemented Foundation

- `AHeistPlayerState` 공통 `CrewStatus` 복제: Active / Forging / Assembling / CarryingOriginal / Heavy / Stunned / Arrested / Escaped
- Remote-only `UHeistNameplateWidget`과 Main HUD Team Status가 동일 PlayerState 상태를 소비
- 서버 Walk 300 / Sprint 600, Weight 감속, Pace 기반 Footstep 500 / 1000
- Guard 접촉 Stun → 지연 Arrest → Teammate Rescue와 Owner Input/UI 복원
- `IA_Map`, `IMC_Map`, Owner-only `UHeistFloorPlanMapWidget` 및 Gameplay Input 복원
- Surface/Object 작업 중 Alarmed 진입 시 서버 Session 선취소, Widget/Input 정리, 위험 단계 재진입 거부와 Quiet 복귀 후 정상 재진입
- Random Map Shuffle Bag, Surface Template 최근 3개 보호, 고정 Seed 결정성, Optional Exhibit 조합 변화
- 실제 `WBP_HeistHUD` Tree에 Stun/Arrest Overlay, Countdown과 Reason Text를 추가하고 기존 Widget Size는 변경하지 않음
- Stun Vignette·Low-pass, Arrest/Rescue Edge Audio, Carry/Heavy Icon·Spatial One-shot Audio와 Match/Lobby Cleanup 구현
- 재사용 `CrewStatusVFXComponent`·`CrewStatusTransitionAudioComponent`와 Forging/Assembling/CarryingOriginal/Heavy/Stunned/Arrested/Escaped별 Asset 슬롯, late-join 전환 억제와 Escaped World one-shot 구현

### Verification

```text
Build         Saved/Logs/W7-Integration-Build-CooldownFix-Final.log
Contract Log  Saved/Logs/W7-ContractRuns-PerceptionFix-Final.log
Contract      Saved/Automation/W7-ContractRuns-PerceptionFix-Final/index.json
W7 Targeted   Saved/Automation/W7-Targeted-CooldownFix-Final/index.json
Full Log      Saved/Logs/W7-FullRegression-CooldownFix-Final.log
Full Report   Saved/Automation/W7-FullRegression-CooldownFix-Final/index.json
Nameplate     Saved/Automation/W7-002-Nameplate-PostContract-Final/index.json
Floor Plan    Saved/Automation/W7-007-FloorPlan-PostPolicy-Final/index.json
Alert 4P      Saved/Automation/W7-009-AlertPresentation-NoDC-Final/index.json
Asset Import  Saved/Logs/W7-Presentation-AssetReimport-NoDC-Final.log
4P TwoRuns    Saved/Automation/W7-002-007-4P-Integration-PostFix-Final/index.json
W7 Full       Saved/Automation/W7-Presentation-FullRegression-Final/index.json
HUD Tree      Saved/Logs/W7-FinalPresentation-HUDTreeSync2.log
2P Final      Saved/Automation/W7-FinalPresentation-2P-PostLobbyFix/index.json
2P Log        Saved/Logs/W7-FinalPresentation-2P-PostLobbyFix.log
Full Final    Saved/Automation/W7-FinalPresentation-FullRegression/index.json
Full Log      Saved/Logs/W7-FinalPresentation-FullRegression.log
Boundary      Saved/Logs/Project_MuseumHeist.log / ContentBoundary Errors=0 / NonBlueprintAfter=0
Manny Setup   Saved/Logs/Project_MuseumHeist.log / W7Mannequin Result=PASS
Manny Build   Saved/Logs/W7-Mannequin-AssetBoundary-EditorBuild.log
W7 Regression Saved/Automation/W7-Mannequin-AssetBoundary-Regression/index.json
W7 Test Log   Saved/Logs/W7-Mannequin-AssetBoundary-Regression.log
Status FX     Saved/Automation/W7-StatusEffectSlots/index.json / 1 of 1 / warnings 0
Status FX W7  Saved/Automation/W7-StatusEffectSlots-FullRegression/index.json / 11 of 11 / Failed 0
Documents     Museum_Heist_GDD.docx / Museum_Heist_TDD.docx Rev 13 / changed-page Word render PASS
Balance       Docs/W7_PLAYER_COUNT_BALANCE.md
Result        W7 Final Presentation 2P TwoRuns + Full 27/27 Success / WithWarnings 7 / Failed 0 / NotRun 0
Notion        2026-08-18 재조회·쓰기·재조회 PASS / Status FX Slot 증거 동기화 / TASK-W7-004·006 진행중 유지
Test Log      TEST-W7-006 Pass / 자동 계약 범위 / 실제 화면·청음 및 Remote Pose는 Task 잔여로 분리
```

### Remaining Evidence

- W7-005: 로컬 구현·자동화·Notion 증거 동기화 완료, 2P 실제 화면·청음 확인 필요
- W7-004·006: Manny/`ABP_Unarmed`와 상태 FX/Sound 슬롯 기반 완료, 실제 Niagara/Sound Asset·Remote Stun/Carry/Heavy Pose Layer 및 2P 실화면·청음 확인 필요
- W8-007: Rev14 기준 각 맵 2P/3P/4P 총 9판의 Completion Time, Peak Alert, Secured Value/Quota Margin, CCTV Detection, Laser Trip·Hold와 퇴각 선택 시점 기록

### 2026-08-15 Roadmap Optimization

- W7~W12 활성 잔여는 29개이며 이 중 필수 27개다. 취소·통합된 행은 활성 주차 Relation에서 제거하고 이력만 보존했다.
- `TASK-W7-010`은 자동 E2E 범위가 이미 통과했고 실제 리듬 검증이 `TASK-W8-007`과 중복되어 취소 후 W8-007에 통합했다.
- `TASK-W11-002`는 External Test 직후 수행할 Issue Triage가 `TASK-W11-001`과 분리될 이유가 없어 취소 후 W11-001에 통합했다.
- W8-003의 삭제 기능 `Smoke` 문구를 제거했다.
- W8-004~006은 이미 통과한 Guard Scaling, Alert Loop, HUD/Result/Forgery/Inventory/Popup Pool을 재구현하지 않고 맵 배치·누락 Audio·해상도/시각 회귀만 수행한다.
- W9~W12의 반복 QA는 대상 Build가 서로 다르므로 유지하되 Baseline/RC1/External/RC2/Steam-installed Hash 경계를 명시했다.
- W12 실행 순서는 RC2 → Final Regression → Steam RC Upload → Steam-installed Smoke → Store/Rollback 준비 → Publish로 고정했다.

---

## 8. W9 Legacy Cleanup And Fresh Development Package Evidence

### Post-Verify Verification

```text
Engine Verify       PASS / 2026-08-16 19:51~20:32 KST / 656 files / 40,274,808 bytes restored
Editor Build        PASS / Saved/Logs/W9-PostVerify-EditorBuild-Final.log
Full Regression     PASS / 27/27 / Failed 0 / NotRun 0
Full Report         Saved/Automation/W9-PostVerify-FullRegression-Retry1/index.json
Full Log            Saved/Logs/W9-PostVerify-FullRegression-Retry1.log
W7-008 Retry        PASS / Saved/Automation/W9-PostVerify-W7-008-Retry1/index.json
Nameplate Post-Fix  PASS / Saved/Automation/W9-PostVerify-Nameplate-PostGameGuard/index.json
```

최초 Post-Verify Full Run은 W7-008 Sprint가 Walk와 같은 전방으로 진행해 맵 Geometry에 막힌 테스트 Fixture 문제로 26/27이었다. Sprint 입력을 이미 통과한 경로의 역방향으로 바꾼 뒤 Targeted 1/1과 Full Retry 27/27을 통과했으며 Production Gameplay 코드는 변경하지 않았다.

### Fresh Development Package

```text
Clean Cook          PASS / PackageProject.ps1 -Clean -CleanCook / UAT -clean
Cook / Archive      PASS / 5 release maps / UAT BUILD SUCCESSFUL
Package Validator   PASS / Development 0.5.0 / required runtime artifacts present
Package             Build/Packages/MuseumHeist-0.5.0-Development-Win64 / 54 files / 1,213,791,985 bytes
BuildInfo           0.5.0 / Development / Win64 / SteamAppId 480 / 5 maps / gitDirty true
Packaged BuildDump  PASS / Version 0.5.0 / Windows / Packaged=true / STEAM / SessionBuild 55116800
Runtime Log         Saved/Logs/W9-PostVerify-PackagedBuildDump-Retry1.log
```

Cooked IoStore에는 Title/Lobby/M01/M02/M03이 모두 포함됐다. 제거한 `BP_DisplayCase`, `BP_LootRoyalCrown`, `DT_LootDataRow`, 중복 Painting Material, Object Assembly Prototype Mesh와 구 `/Game/StarterContent` 경로는 0건이며 Canonical Painting Material은 포함됐다. 실제 사용하는 StarterContent 57개 Chunk의 합계는 40.69 MiB다.

`AssetSizeQuery`는 Cook 시 `WriteBackMetadataToAssetRegistry=Disabled`여서 Size Metadata를 읽지 못하고 종료 코드 1을 반환했다. 이는 Cook/Package 실패가 아니며 실제 IoStore 목록과 크기는 `UnrealPak -List`로 별도 확인했다.

### Remaining Release Gates

- M01 원화 12개의 source-specific rights 값이 누락돼 있다.
- M03 네 항목은 `No Copyright - United States`만 기록돼 있어 Global Steam 배포 근거 확인 또는 교체가 필요하다.
- `T_Forgery_SunArchWave`의 Creator/Source/Rights provenance가 필요하다.
- Epic StarterContent 적용 약관·검토일·배포 범위를 Release Manifest에 기록해야 한다.
- TENADA OFL 원문과 Notice를 사용자 열람 가능한 Package 위치에 포함해야 한다.
- 따라서 [`TASK-W9-007`](https://app.notion.com/p/39a1d26a5dfb8192940dd5cdee4c1a07)은 `진행중`을 유지한다.

---

## 9. Update Contract

다음 경우 같은 작업 안에서 이 문서를 갱신한다.

- Notion Task 상태·우선순위·실행 순서가 바뀐 경우
- 구현 파일 또는 Asset 범위가 바뀐 경우
- Build, Blueprint Compile/Save, Automation, PIE, Multiplayer 증거가 생긴 경우
- Test Log 또는 Task 완료 상태를 실제로 갱신하고 재조회한 경우
- Blocker 또는 다음 재개 지점이 바뀐 경우
