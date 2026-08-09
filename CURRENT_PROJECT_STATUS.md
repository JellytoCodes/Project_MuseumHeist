# Project_MuseumHeist Current Project Status

최종 갱신: 2026-08-09 13:17 KST  
현재 문서 Revision: 3

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

마지막 라이브 조회: `2026-08-09 13:17 KST`

```text
TASK-W6-000  검토중  Contract Foundation / Required Target / Loot Value Quota
TASK-W6-006  진행중  Player Contribution Capture
```

---

## 1. Current Focus

Notion Active Task: [`TASK-W6-006`](https://app.notion.com/p/39a1d26a5dfb8132a835ee75d548e348) — `Player Contribution Capture` / `진행중`

로컬 Resume Work Package: [`LOCAL-20260809-01`](LOCAL_PROGRESS_INBOX.md#local-20260809-01) — Replica Acceptance / Forgery Timeout / Shared Forgery UI 방향 변경

주의: 사용자가 이전 대화에서 부른 `TASK 006` 작업 내용과 Notion의 `TASK-W6-006` 완료 기준은 일치하지 않는다. 아래 위조 변경 증거만으로 `TASK-W6-006`의 진행률 또는 완료 상태를 변경하지 않는다. 새 작업은 Notion의 현재 Task 정의를 우선 확인하고, 이 로컬 Work Package를 별도 후속 검증 대상으로 취급한다.

현재 결론:

```text
구현                   COMPLETE
C++ Editor Build       PASS
WBP Compile / Save      PASS
DataTable Save          PASS
문서 동기화 / 렌더 QA  PASS
자동화 테스트 실행     NOT RUN
User PIE                REQUIRED / NOT RUN
Notion Live Read        PASS / 2026-08-09 13:17 KST
Notion Task Update      NOT DONE
로컬 Work Package PASS  PENDING USER PIE
```

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
- 한 문장 Instruction
- Quality 70 Required
- Expected Score
- Remaining Time
- Submit / Enter
- Cancel / Escape
- Team / Alert 최소 정보
- Surface의 Palette/Brush/Canvas와 Object의 Part/Socket/Orientation/Material만 모드별로 다르다.
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
  - 공통 UI 계약과 Submit/Cancel 입력
- `Source/Project_MuseumHeist/Private/Tests/HeistContractTests.cpp`
  - `ProjectMuseumHeist.Forgery.ReplicaAcceptanceContract` 추가
- `DataTableImports/DT_ArtifactDataRow.json`
  - 14개 Row 모두 `MinimumForgeryScore=0.7`

### Unreal Assets

- `Content/Blueprints/UI/Forgery/WBP_HeistForgery.uasset`
- `Content/Blueprints/UI/ObjectAssembly/WBP_HeistObjectAssembly.uasset`
- `Content/Data/DataTable/DT_ArtifactData.uasset`

공식 Unreal MCP의 `UMGToolSet`, `DataTableTools`, `AssetTools`로 수정했다. 세 Asset 모두 저장됐고 두 Widget Blueprint Compile 요청이 완료됐다.

### Documents

- `AGENTS.md` Rev 12
- `Museum_Heist_GDD.docx` Rev 12
- `Museum_Heist_TDD.docx` Rev 12

GDD 29쪽과 TDD 40쪽을 렌더해 전 페이지를 검수했다. 최종 수정에서 남아 있던 `Rev 11` 머리글과 TDD의 `Replica Review / Atomic Swap` 번호 체계를 바로잡고 영향 페이지를 다시 렌더했다.

---

## 4. Verification Evidence

### Build

현재 `C:\Users\User\AppData\Local\UnrealBuildTool\Log.txt`:

```text
Target is up to date
Result: Succeeded
```

`Binaries/Win64/UnrealEditor-Project_MuseumHeist.dll` 시각은 2026-08-09 10:41:12이며 최신 변경 C++ 시각 10:39:47보다 이후다.

### MCP / Asset Save

근거 로그:

`Saved/Logs/Project_MuseumHeist-backup-2026.08.09-02.08.17.log`

```text
02:04:29 DT_ArtifactData Imported - 0 Problems
02:04:37 WBP_HeistForgery CompileWidgetBlueprint
02:04:37 WBP_HeistObjectAssembly CompileWidgetBlueprint
02:04:47 WBP_HeistForgery SavePackage
02:04:47 WBP_HeistObjectAssembly SavePackage
02:04:47 DT_ArtifactData SavePackage
```

### Static Checks

```text
git diff --check                         PASS
DT_ArtifactDataRow.json Row Count        14
MinimumForgeryScore unique values        [0.7]
Inspection score Alert escalation path   REMOVED
Timeout Guard selection                  EVENT-ONLY / NEAREST ONE
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

현재 변경을 다시 구현하지 않는다. 다음 단계는 User PIE 증거 수집이다.

권장 환경:

```text
PIE Mode: New Editor Window
Players: 2
Net Mode: Listen Server
```

최소 확인 항목:

1. Surface와 Object UI가 공통 설명/70점/예상 점수/시간/Submit/Cancel 구조인지 확인한다.
2. 작업 UI는 Owning Client에만 나타나야 한다.
3. 예상 점수 70 미만은 Submit이 비활성화되고 70 이상은 활성화돼야 한다.
4. 서버가 `QualityBelowMinimum`으로 거부하면 Replica 없이 같은 UI와 남은 Timer가 유지돼야 한다.
5. 70 이상 승인 시 UI가 닫히고 Case가 Owner 전용 ReplicaReady 상태가 돼야 한다.
6. Surface와 Object Timeout 모두 작업과 Preview를 폐기하고 Replica를 만들지 않아야 한다.
7. Timeout 전후 Alert 값이 같아야 한다.
8. 근처 유효 Guard가 한 명만 한 번 조사해야 한다.
9. 승인된 70/80/95점 Replica 검사로 Alert가 증가하지 않아야 한다.

고신호 로그:

```text
Forgery timeout investigation: ... AlertChanged=false OneShot=true ... Result=ASSIGNED
Guard forgery timeout investigation: ... AlertChanged=false ... Result=PASS
Forgery score commit rejected: ... Reason=QualityBelowMinimum
```

자동화 테스트도 실행한다.

```text
ProjectMuseumHeist.Forgery.ReplicaAcceptanceContract
```

User PIE와 자동화 결과가 모두 PASS하면 이 로컬 Work Package의 구현 상태만 완료 후보로 판단한다. Weekly Gate/Test Log와 Notion Task 상태는 해당 Task의 완료 기준과 증거를 별도로 대조한 뒤 갱신한다.

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
