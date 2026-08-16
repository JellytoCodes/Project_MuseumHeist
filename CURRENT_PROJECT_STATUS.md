# Project_MuseumHeist Current Project Status

최종 갱신: 2026-08-16 KST
현재 문서 Revision: 13

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

마지막 라이브 재조회: `2026-08-16 KST`

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
TASK-W7-004  진행중  Stun 화면·오디오 구현/자동화 완료, Remote Stun Pose Asset 차단
TASK-W7-005  진행중  Arrest / Rescue 최종 UI·오디오 및 자동화 완료, Notion 완료 판정 대기
TASK-W7-006  진행중  Carry / Heavy Icon·Audio 구현/자동화 완료, Remote Pose Asset 차단
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
```

---

## 1. Current Focus

W7는 팀 가독성과 협동 피드백의 C++/복제/Input 기반과 Stun/Arrest/Carry 최종 HUD·Icon·Audio 구현을 마쳤다. 실제 HUD Widget Tree 동기화 후 2P TwoRuns와 전체 27/27 자동화를 통과했다. 현재 W7 마감 대상은 `TASK-W7-005`의 Notion 완료 판정과, `TASK-W7-004`·`006`의 실제 Remote Stun/Carry/Heavy Pose를 만들 Character/AnimBP Asset 결정이다.

```text
Editor Build        PASS / Win64 Development
Full Automation     PASS / 27 total / Failed 0 / Warning 0 / NotRun 0
Solo ContractRun    PASS / 2 consecutive runs / clean Lobby reset
2P Presentation     PASS / 2 consecutive runs / clean Lobby reset / PostLobbyFix
4P ContractRun      PASS / 2 consecutive runs / clean Lobby reset
W7 Variation        PASS / real M01·M02·M03 12 rows / deterministic 24 draws
HUD Widget Tree     PASS / actual StunOverlay·ArrestOverlay·Countdown·Reason nodes
```

범위 경계:

- NullRHI 자동화는 상태·복제·입력·Widget 생성 계약을 검증하지만 렌더링 화면과 실제 청음을 대신하지 않는다.
- `TASK-W7-005`는 로컬 구현과 자동화 증거가 완료됐지만 Notion 상태 판정 전에는 완료로 단정하지 않는다.
- `BP_HeistPlayerCharacter`는 `SkeletalCube` 기반 Data-only Blueprint이며 `Anim Class=None`이다. 따라서 `TASK-W7-004`·`006`의 Remote Pose는 실제 Character/AnimBP Asset 결정 전 완료 처리할 수 없다.
- 실제 2~3분 Escape 리듬은 W8-007로 통합했다.
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
5. 아래 W7 Final Presentation 증거를 확인하고 이미 통과한 HUD·Icon·Audio 기반을 재구현하지 않는다.
6. `TASK-W7-005`의 로컬 완료 증거를 Notion 완료 기준과 대조해 상태를 판정한다.
7. `TASK-W7-004`·`006`은 실제 Character/AnimBP Asset 방향을 확정한 뒤 Remote Stun/Carry/Heavy Pose를 검증한다.
8. W7 Gate 종료 후 W8은 M02 Gameplay 배치 → M03 Gameplay 배치 → M01 Final Pass → 세 맵 Route·Audio·UI 잔여 → 3-Map 9판 Gate 순서로 진행한다.
9. 이후 `TASK-W9-007`의 배포 권리·Notice 잔여를 재개한다.

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
Documents     Museum_Heist_GDD.docx / Museum_Heist_TDD.docx Rev 13 / changed-page Word render PASS
Balance       Docs/W7_PLAYER_COUNT_BALANCE.md
Result        W7 Final Presentation 2P TwoRuns + Full 27/27 / Failed 0 / Warning 0 / NotRun 0
Notion        TASK-W7-004·005·006 진행중 / 최신 로컬 증거 상태 판정 대기
```

### Remaining Evidence

- W7-005: 로컬 구현·자동화 완료, Notion 완료 기준 대조와 상태 판정 대기
- W7-004·006: `SkeletalCube` Data-only BP / `Anim Class=None` 때문에 Remote Stun/Carry/Heavy Pose Asset 결정 필요
- W8-007: 각 맵 Solo/2P/4P 총 9판에서 실제 2~3분 탐욕/탈출 선택 시점과 15~25분 Contract 지표 기록

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
