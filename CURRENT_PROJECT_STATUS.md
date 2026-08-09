# Project_MuseumHeist Current Project Status

최종 갱신: 2026-08-09 23:14 KST
현재 문서 Revision: 10

이 문서는 새 Codex/Claude 작업이 이전 대화 없이도 현재 진척과 다음 행동을 바로 파악하기 위한 단일 실행 인계 문서다.

- 설계와 구현 규칙의 Source of Truth는 `AGENTS.md` → `Museum_Heist_TDD.docx` → `Museum_Heist_GDD.docx` 순서다.
- Notion Task Database가 현재 Task, 상태, 우선순위와 실행 순서의 Live Source of Truth다.
- 이 문서는 설계 규칙이나 Notion Task Database를 대체하지 않는 오프라인 실행 캐시다.
- Notion에 아직 연결·반영되지 않은 실질 작업은 [`LOCAL_PROGRESS_INBOX.md`](LOCAL_PROGRESS_INBOX.md)에 기록한다.
- 현재 Git, Editor, Notion 상태와 충돌하면 역할별 라이브 상태를 우선하고 이 문서를 즉시 갱신한다.
- 확인하지 않은 완료, Build, PIE, Multiplayer, Notion 상태를 기록하지 않는다.

---

## 0. Notion Live Progress Source

다음 진입점을 모든 새 작업에서 먼저 조회한다.

- Workspace: `쩰리또`
- [Museum Heist — Project Leaderboard](https://app.notion.com/p/3831d26a5dfb81bfa7edeb4974818714)
- [주차별 작업보드](https://app.notion.com/p/884ad464d3334cdf890e595db0065fcf)  
  Data Source: `collection://c0d35883-8f6b-4467-b5be-62a1236073c6`
- [테스트 로그](https://app.notion.com/p/ec9727a40d9541e6a2e4ee6096b1c678)  
  Data Source: `collection://2f308111-75f1-4b68-bd45-f94361e855af`

시작 시 `주차별 작업보드`에서 `상태 = 진행중 또는 검토중`을 조회하고, 사용자가 Task ID를 지정했다면 해당 페이지를 추가 Fetch한다. SQL 조회가 제한되면 Search/Fetch를 사용한다. Notion 자체 조회가 실패하면 이 문서를 `OFFLINE CACHE`로 표시하고 Task 상태·우선순위·다음 작업을 확정하지 않는다.

마지막 라이브 조회: `2026-08-09 23:14 KST`

```text
TASK-W6-000  검토중  Contract Foundation / Required Target / Loot Value Quota
TASK-W6-006  완료    Player Contribution Capture
TASK-W6-007  완료    Team Result ViewModel / Widget
TASK-W6-008  완료    End Phase / Lobby Return
```

---

## 1. Current Focus

사용자 지정 범위 [`TASK-W6-007`](https://app.notion.com/p/39a1d26a5dfb81dd9c33ef462175be83)과 [`TASK-W6-008`](https://app.notion.com/p/39a1d26a5dfb81d49b3cc22a67ad68a2)은 구현·검증·Notion 완료 처리가 끝났다.

Notion에서 현재 별도 확인이 필요한 항목은 [`TASK-W6-000`](https://app.notion.com/p/3af1d26a5dfb819199d9e3b1be2eeaf5) `검토중`이다. 다음 구현 Task는 사용자가 지정하거나 작업보드를 다시 라이브 조회한 뒤 확정한다.

로컬 Resume Work Package: [`LOCAL-20260809-01`](LOCAL_PROGRESS_INBOX.md#local-20260809-01) — Replica Acceptance / Forgery Timeout / Shared Forgery UI 방향 변경

`TASK-W6-007` / `TASK-W6-008` 최종 결론:

```text
Result Presentation     PASS / Outcome·실패 사유·상세 점수·최대 4인 표·실제 Replica Recap
Winner / Rank           NONE / 비경쟁 Mission Result만 사용
WBP_Result              PASS / MCP Compile·Save 및 Native Binding 확인
C++ Editor Build        PASS / 2026-08-09 23:05 KST 최종 빌드
Result Automation       PASS / ProjectMuseumHeist.Result 8/8
2 Player Replay PIE     PASS / 동일 HeistSession에서 Result→Lobby→두 번째 M01
Residual Cleanup        PASS / Case·Timer·Action·Forgery·Inventory·Audio·Widget·Input 잔여 없음
Softlock                NONE / Host·Client 두 번째 Gameplay 입력 정상
Notion Test Logs        TEST-W6-006 / TEST-W6-007 PASS
Notion Task Status      TASK-W6-007 완료 / TASK-W6-008 완료
```

`LOCAL-20260809-01`의 Forgery UI/Timeout 패키지는 별도 범위이며 기존 검증 상태를 유지한다.

### TASK-W6-007 변경 및 검증 체크포인트

- `FHeistTeamResult`가 서버 확정 `RequiredTargetDisplayName`을 포함하고 모든 Client에 복제한다.
- `FHeistReplicaRecapEntry`가 작품 표시명, 실제 Painting Thumbnail Payload 또는 실제 Object Assembly Recipe를 전달한다.
- `UHeistResultViewModel`이 Full/Partial/Failure, 필수 목표 확보 여부, Secured/Quota/Extra와 Reward 식을 한글로 설명한다.
- `UHeistResultWidget`이 Painting Texture 생성과 Object Part Anchor/크기/각도 재구성 Helper를 제공하고, `WBP_Result`의 가로 Card Container를 Native에서 실제 Payload로 채운다.
- `WBP_Result`에 `ReplicaRecapVisualPanel → ReplicaRecapScrollBox → ReplicaRecapVisualContainer`를 추가하고 기존 결과·기여 텍스트와 겹치지 않게 재배치했다.
- `WBP_Result`의 TextBlock 13개는 기존 크기와 배치를 유지하면서 Runtime Font를 `F_TENADA`로 통일했다. Native 생성 Card Text도 `ReplicaRecapTextBlock`의 Font Style을 복사한다.
- `WBP_Result` MCP Compile은 성공했고 Asset Save 후 Dirty=false를 확인했다. `BP_RefreshReplicaRecap`은 추가 연출용 Optional Hook이며 실제 시각 Recap의 필수 경로가 아니다.
- `HeistResultVisualSeed`는 서버에서 Painting/Object 시각 Payload를 결정론적으로 주입하고, `HeistResultDump`는 Server/Client 표시 계약을 한 줄로 판정한다.
- `ProjectMuseumHeist.Result` 8개 자동화는 최종 Source 기준 모두 PASS다.
- 2 Player Listen Server Result 표시 감사와 WBP Compile 성공은 [`TEST-W6-006`](https://app.notion.com/p/3b71d26a5dfb81b49a73e907e5e21e45)에 기록했다.

### TASK-W6-008 End Phase / Lobby Return

- Result 진입 시 Main HUD Alert 오디오와 숨은 경고 Text를 정리하고 Result 상세 패널·동적 Recap/Contribution 자식을 초기화한다.
- 숨겨진 Main HUD는 ViewModel 변경을 받아도 Alert 오디오를 재초기화하지 않으며, 다시 표시할 때만 최신 상태를 갱신한다.
- Seamless Travel 이후 Local Presentation Source, Input Mode와 Session World Ready 통지를 재결합한다.
- 현재 Source/Blueprint에는 활성 PostProcess·AudioFilter 경로가 없으며, HUD Alert Audio Component 잔여 0을 런타임에서 검증했다.
- Return Lobby 사전 정리 뒤 Case Lock/Timer, Gameplay Action, Forgery/Assembly, Inventory와 Match Timer의 실제 사후 잔여값을 재검사한다.
- `ProjectMuseumHeist.Replay.EndLobbyReturnTwoPlayer`가 동일 `HeistSession`의 첫 M01 종료, 2인 Lobby 복귀, 두 번째 M01 재입장과 입력 복구를 검증한다.
- 최종 증적은 [`TEST-W6-007`](https://app.notion.com/p/3b71d26a5dfb81f6bbc2e4a9a9dc1335)에 기록했다.

---

## 2. Locked Direction

Surface Forgery와 Object Assembly는 다음 공통 계약을 사용한다.

```text
Forgery Quality
- Alert, Lockdown, Contract Value 또는 Quota를 변경하지 않는다.

Replica Acceptance
- Local Preview는 안내값이다.
- Server가 최종 품질을 판정한다.
- 프로젝트 공통 하한과 Artifact MinimumForgeryScore 중 큰 값 이상만 승인한다.
- 현재 공통 하한과 모든 Artifact Row는 70점이다.
- 70점 미만은 QualityBelowMinimum으로 거부한다.
- 거부 시 Replica를 만들지 않고 기존 Full-Screen Session과 남은 Timer를 유지한다.

Timeout
- 현재 Stroke, Assembly Entry와 Preview를 폐기한다.
- Replica를 만들지 않는다.
- Alert를 변경하지 않는다.
- Case 반경 1,500cm 안의 Patrol/ReturnToPatrol Guard 중 가장 가까운 한 명에게 한 번만 InvestigateNoise를 요청한다.

Shared UI
- Mode Title
- 예상 품질 `{점수}/100 · 제출 가능 70+` 단일 표시
- Remaining Time
- Submit / Enter
- Cancel / Escape
- 통합 하단 조작 안내
- 별도 InstructionText와 ModeStatusText는 사용하지 않는다. 작업 설명은 Tutorial과 통합 하단 안내가 담당한다.
- 별도 서버 점수, QualityRequirement, Alert Warning, Lockdown Countdown은 표시하지 않는다.
- Alert가 Quiet을 벗어나면 서버 취소를 한 번만 요청하고 작업 화면을 즉시 닫아 Gameplay Input을 복원한다.
- Surface는 Palette/Canvas와 Drawing Content 안의 시각적 Brush 소·중·대 선택을 사용한다.
- Object는 Original/Template Name/정답 실루엣/UViewport/순회 버튼을 제거하고 2D Part Tray와 Assembly Canvas Drag & Drop을 사용한다.
- Object Drop은 가장 가까운 호환 Socket Anchor로 양자화하며 Wheel 회전, 우클릭 제거, R Reset을 사용한다. 중간 Drag 좌표는 복제하지 않는다.
```

Guard의 시야 발각, 추격, 교체 소음과 일반 Alert/Lockdown 흐름은 유지한다. 제거된 것은 Forgery Quality가 Alert를 누적시키던 경로뿐이다.

---

## 3. Implemented Artifacts

### C++ / Data

- `Source/Project_MuseumHeist/Public/Data/HeistArtifactDataTypes.h`
  - `HeistReplicaAcceptance`
  - 공통 하한 70점과 서버 판정 Helper
- `Source/Project_MuseumHeist/Private/Character/Components/HeistForgeryComponent.cpp`
  - Surface 서버 70점 Gate
  - 기준 미달 Session 유지
  - Timeout 조사 Event
- `Source/Project_MuseumHeist/Private/Character/Components/HeistObjectAssemblyComponent.cpp`
  - Object 서버 70점 Gate
  - Local Preview와 Server가 공유하는 결정론적 Score 계산
  - Timeout 조사 Event
- `Source/Project_MuseumHeist/Private/Core/HeistGameMode.cpp`
  - Timeout 순간에만 Guard Iterator 1회 실행
  - 반경 내 가장 가까운 유효 Guard 한 명 선택
- `Source/Project_MuseumHeist/Private/AI/HeistGuardAIController.cpp`
  - Patrol/ReturnToPatrol 상태만 Timeout 조사 수락
  - Alert 변경 없이 InvestigateNoise 1회 진입
- `Source/Project_MuseumHeist/Private/World/Actors/Loot/HeistPaintingDisplayCaseActor.cpp`
- `Source/Project_MuseumHeist/Private/World/Actors/Loot/HeistObjectDisplayCaseActor.cpp`
  - 승인된 70점 이상 Replica만 검사
  - 검사 Delay와 결과를 Quality와 분리
  - 검사 완료 시 Alert 변화 없음
- `Source/Project_MuseumHeist/Private/UI/Widgets/HeistForgeryWidget.cpp`
- `Source/Project_MuseumHeist/Private/UI/Widgets/HeistObjectAssemblyWidget.cpp`
  - Alert 진입 시 1회 Cancel + 강제 종료 / Gameplay Input 복원
  - 한글 공통 UI, 단일 예상 품질, 통합 Footer
  - Surface 시각적 Brush 소·중·대
  - Object 2D Part Tray / Canvas Drag & Drop, 숨은 Socket Anchor 양자화, Wheel/우클릭/R 입력
- `Source/Project_MuseumHeist/Private/Tests/HeistContractTests.cpp`
  - `ProjectMuseumHeist.Forgery.ReplicaAcceptanceContract` 추가
- `DataTableImports/DT_ArtifactDataRow.json`
  - 14개 Row 모두 `MinimumForgeryScore=0.7`

### TASK-W6-007 Result Presentation

- `Source/Project_MuseumHeist/Public/Core/HeistTypes.h`
- `Source/Project_MuseumHeist/Private/Core/HeistTypes.cpp`
  - Team Result의 Required Target 표시명과 Replica별 작품 표시명 추가
  - 실제 Painting 64x64 Palette Payload와 Object Assembly Recipe의 제한된 NetSerialize 유지
- `Source/Project_MuseumHeist/Private/Core/HeistGameMode.cpp`
  - 서버 결과 Snapshot에서 Artifact DataTable 표시명과 실제 제출 Replica Payload 캡처
- `Source/Project_MuseumHeist/Public/UI/ViewModels/HeistResultViewModel.h`
- `Source/Project_MuseumHeist/Private/UI/ViewModels/HeistResultViewModel.cpp`
  - 계약 결과, 필수 목표, Secured/Quota/Extra, Reward 식과 Replica Metadata의 한글 요약
- `Source/Project_MuseumHeist/Public/UI/Widgets/HeistResultWidget.h`
- `Source/Project_MuseumHeist/Private/UI/Widgets/HeistResultWidget.cpp`
  - Painting Payload를 실제 Texture로 복원
  - Object Part/Socket/Orientation을 현재 2D Assembly 규칙과 같은 Anchor/Size/Angle로 재구성하는 Blueprint Helper
- `Source/Project_MuseumHeist/Private/Tests/HeistResultTests.cpp`
  - `ScreenPresentation`, `ReplicaRecapPayload` 자동화 추가
- `Source/Project_MuseumHeist/Public/Debug/HeistCheatManager.h`
- `Source/Project_MuseumHeist/Private/Debug/HeistCheatManager.cpp`
- `Source/Project_MuseumHeist/Public/Debug/HeistDebugFunctionLibrary.h`
- `Source/Project_MuseumHeist/Private/Debug/HeistDebugFunctionLibrary.cpp`
  - 서버 시각 Recap Seed와 Server/Client Result Presentation Audit 추가

### Unreal Assets

- `Content/Blueprints/UI/Forgery/WBP_HeistForgery.uasset`
- `Content/Blueprints/UI/ObjectAssembly/WBP_HeistObjectAssembly.uasset`
- `Content/Blueprints/UI/Fonts/FF_HeistKorean_Regular.uasset`
- `Content/Blueprints/UI/Fonts/F_HeistKorean.uasset`
- `Content/Blueprints/UI/Fonts/FF_TENADA.uasset`
- `Content/Blueprints/UI/Fonts/F_TENADA.uasset`
- `Content/Data/DataTable/DT_ArtifactData.uasset`

공식 Unreal Editor/MCP의 `UMGToolSet`, `ObjectTools`, `AssetTools`와 Editor Python Import 경로로 수정했다. 두 Widget Blueprint는 Compile/Save PASS다. 제목·제출·취소 6개 TextBlock에는 SIL OFL 1.1 TENADA 기반 `F_TENADA`를 적용하고, 점수·시간·통합 하단 안내와 Brush Label은 NanumGothic 기반 `F_HeistKorean`을 유지한다. 기존 WBP에 저장돼 있던 `??` 한글 19개 문구를 UTF-8 MCP 경로로 복구했고, Regular만 존재하는 Font에서 요청하던 `Bold` Typeface 17개를 `Regular`로 정규화했다. 사용자 편집본에서 `WBP_HeistForgery.ModeStatusText`, 두 WBP의 `InstructionText`는 실제 Widget Tree에서 이미 제거된 상태를 확인했으며, C++의 상속 바인딩과 갱신 코드만 추가 정리했다. 이 확인 과정에서는 WBP 크기·위치·패딩·Slot 값을 수정하지 않았다.

TENADA 원본과 배포 안내는 `SourceArt/UI/Fonts/TENADA/`에 보존했다. Designer 캡처는 `Saved/FontQA/TENADA/`에 저장했다.

### Documents

- `AGENTS.md` Rev 12
- `Museum_Heist_GDD.docx` Rev 12
- `Museum_Heist_TDD.docx` Rev 12

GDD 29쪽과 TDD 40쪽을 Microsoft Word PDF로 다시 렌더해 전 페이지 Contact Sheet와 `InstructionText`/`ModeStatusText` 변경 페이지를 원본 크기로 검수했다. 페이지 수, 표 경계, 줄바꿈과 잘림은 정상이다. 본문뿐 아니라 남아 있던 버튼식 Object Assembly/ViewModel 표도 2D Drag & Drop 계약으로 교체했다. 이번 최종 QA 산출물은 `Saved/DocQA/UIInstructionRemoval/Final_GDD`와 `Final_TDD`에 있다.

---

## 4. Verification Evidence

### Build

```text
UnrealEditor-Project_MuseumHeist.dll  2026-08-09 23:05 KST
Project_MuseumHeistEditor Build       PASS
Final Source Revision Build           PASS
```

### MCP / Asset Save

```text
WBP_HeistForgery               Compile=true / Save=true
WBP_HeistObjectAssembly        Compile=true / Save=true
Korean Static Text             QuestionMark=0 / TextBlocks=27
TENADA Display TextBlocks      Updated=6 / Title+Submit+Cancel
F_HeistKorean Runtime Font     FontFace=NanumGothic / Saved=true
F_TENADA Runtime Font          FontFace=TENADA / Source=SourceArt/UI/Fonts/TENADA/Tenada.ttf / Saved=true
Object Assembly KoreanUIFont   WBP CDO assignment / Saved=true
Python Remote Execution        DisabledAfterImport=true
Font Asset Dirty State         false / FF_TENADA, F_TENADA, Forgery WBP, Assembly WBP
WBP_Result Visual Recap Tree   MCP COMPILE PASS / SAVED / Dirty=false
WBP_Result Korean Runtime Font F_TENADA / 13 TextBlocks / 기존 Size 유지
WBP_Result Native Binding      PASS / 최종 Editor DLL 반영
```

### Static Checks

```text
git diff --check                         PASS
DT_ArtifactDataRow.json Row Count        14
MinimumForgeryScore unique values        [0.7]
Inspection score Alert escalation path   REMOVED
Timeout Guard selection                  EVENT-ONLY / NEAREST ONE
ReplicaAcceptanceContract                PASS / 2026-08-09 15:42 KST
TASK-W6-007 source diff check             PASS
Required Target player-facing name       PASS
Painting/Object actual recap payload      PASS
Result presentation automation            PASS / 8 of 8
TASK-W6-008 replay automation             PASS / 2 Player same-session replay
Shutdown residual verification            PASS / all remaining counts 0
Lobby/second-match presentation reset     PASS
Replay clean log                          Saved/Logs/TASK-W6-008-Replay-2P-Clean.log
Result clean log                          Saved/Logs/TASK-W6-008-Result-Regression-Clean.log
```

---

## 5. Working Tree Safeguards

- 현재 변경은 아직 Stage/Commit하지 않았다.
- `WBP_HeistForgery.uasset`에는 이번 작업 전에 존재하던 사용자 변경이 있었으며, 이를 되돌리지 않고 공통 UI 변경을 추가했다.
- 관련 Asset 백업: `Saved/AssetBackups/20260809-Rev12/`
- 관련 문서 백업: `Saved/DocBackups/20260809-Rev12/`
- 사용자가 실행한 Unreal Editor가 열려 있을 수 있다. 명시적 요청 없이 종료하지 않는다.
- `.uasset`을 Git/Filesystem Script로 직접 수정하지 않는다.

---

## 6. Resume Here

TASK-W6-007과 TASK-W6-008 변경을 다시 구현하거나 같은 검증을 반복하지 않는다.

1. 새 대화에서 Notion 작업보드의 `진행중`/`검토중`과 사용자가 지정한 Task를 라이브 조회한다.
2. 현재 확인된 `검토중` 항목은 TASK-W6-000이며, 다음 구현 번호는 사용자 지정 또는 최신 Notion 순서를 따른다.
3. 007 증적은 TEST-W6-006, 008 증적은 TEST-W6-007을 기준으로 한다.
4. 008 이후 별도 코드가 Result/Travel/HUD 수명주기를 변경한 경우에만 `ProjectMuseumHeist.Result`와 `ProjectMuseumHeist.Replay`를 재실행한다.

현재 사용자 지정 범위 007/008은 `완료`다. 새 Task를 임의로 시작하지 않는다.

---

## 7. New Chat Bootstrap

새 작업은 다음 순서로 시작한다.

1. `AGENTS.md`를 읽는다.
2. `Notion Live Progress Source`의 작업보드에서 `진행중`/`검토중` Task와 사용자가 지정한 Task를 라이브 조회한다.
3. `LOCAL_PROGRESS_INBOX.md`의 `UNLINKED`/`READY_TO_SYNC` Entry를 Notion 결과와 대조한다.
4. 이 문서의 `Current Focus`, `Working Tree Safeguards`, `Resume Here`를 Notion과 Inbox 결과에 대조한다.
5. `git status --short`와 관련 Diff를 확인한다.
6. Unreal Editor/Build 정보가 필요한 요청이면 라이브 상태를 확인한다.
7. 이 문서보다 새로운 Notion 상태 또는 사용자 지시가 있으면 해당 라이브 정보를 우선하고 `Current Focus`를 갱신한다.
8. 완료된 구현을 반복하지 않고 미검증 항목부터 이어간다.

---

## 8. Update Contract

다음 경우 이 문서를 같은 작업 안에서 갱신한다.

- Active Task 또는 방향이 변경됐을 때
- Notion 라이브 조회 시각 또는 `진행중`/`검토중` Task가 바뀌었을 때
- `LOCAL_PROGRESS_INBOX.md`의 Active Entry 또는 Reconciliation 상태가 바뀌었을 때
- 구현 파일이나 Asset 범위가 바뀌었을 때
- Build, Blueprint Compile/Save, PIE, Multiplayer 또는 Automation 증거가 새로 생겼을 때
- Notion Task/Test Log 상태를 실제로 갱신했을 때
- Blocker 또는 다음 재개 지점이 바뀌었을 때

오래된 세션 서술을 누적하지 않는다. 현재 상태와 가장 최근의 검증 가능한 Resume Point만 유지한다.
