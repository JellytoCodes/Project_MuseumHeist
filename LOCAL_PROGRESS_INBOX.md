# Project_MuseumHeist Local Progress Inbox

최종 갱신: 2026-08-15 KST

이 문서는 Notion에 아직 연결되지 않았거나 반영되지 않은 실질 작업을 잃지 않기 위한 Reconciliation Queue다.

- 공식 Task 상태, 우선순위와 실행 순서의 Source of Truth는 Notion `주차별 작업보드`다.
- 이 문서의 Entry는 Notion Task 진행률 또는 완료를 의미하지 않는다.
- 새 작업은 Notion을 먼저 라이브 조회한 뒤 Active Entry를 대조한다.
- 코드, Asset, 문서, Gameplay 방향 또는 검증 증거가 실질적으로 바뀐 경우만 기록한다.
- 단순 조회, 변경 없는 진단과 반복 상태 확인은 기록하지 않는다.

## State Definitions

```text
UNLINKED
- 정확히 대응하는 Notion Task가 확인되지 않았다.

READY_TO_SYNC
- 대응 Notion Task는 확인됐지만 진행 증거가 아직 Notion에 반영되지 않았다.

RECONCILED
- Notion Relation과 필요한 진행 증거 반영을 라이브로 확인했다.

NO_TASK_REQUIRED
- 프로젝트 운영 기록이며 별도 Notion Task가 필요 없다고 확정했다.
```

---

## Active Queue

### LOCAL-20260809-01

- State: `UNLINKED`
- Created: `2026-08-09 KST`
- Last Updated: `2026-08-09 17:19 KST`
- Notion Relation: `NONE`
- Explicit Exclusion: [`TASK-W6-006`](https://app.notion.com/p/39a1d26a5dfb8132a835ee75d548e348) `Player Contribution Capture`와 완료 기준이 다르므로 연결하지 않는다.

#### User Request / Decision

- Forgery Quality 기반 Alert/Lockdown 패널티를 제거한다.
- 서버 최종 Quality 70 이상일 때만 Replica를 승인한다.
- Timeout은 작업을 폐기하고 Alert 변화 없이 근처 Guard 한 명의 1회 조사만 발생시킨다.
- Surface Forgery와 Object Assembly의 한글 Title, 70점 기준을 포함한 단일 예상 품질, Timer, Submit/Cancel과 통합 하단 안내 구조를 통일한다.
- 별도 `InstructionText`와 `ModeStatusText`는 사용하지 않고 작업 설명은 Tutorial과 통합 하단 안내가 담당한다.
- 작업 화면의 Alert/Lockdown 상세 Text는 제거한다. Suspicious/Searching에서는 작업을 유지하고, Alarmed/Lockdown에서만 서버가 Session을 선취소한 뒤 화면을 강제 종료해 탈출 판단을 우선한다.
- Object Assembly는 버튼식 3D Preview 대신 Original/정답을 숨긴 기억 기반 2D 조각 Drag & Drop으로 교체한다.
- 공통 UI의 제목·제출·취소에는 TENADA를 사용하고, 점수·시간·통합 하단 안내와 Brush Label은 가독성용 한글 본문 폰트를 유지한다.

#### Changed Scope

- C++ Authority / Validation / Timeout Investigation
- Surface/Object Widget C++ 및 Widget Blueprint
- 한글 Runtime Font Asset 및 WBP TextBlock 27개 적용
- TENADA FontFace/Composite Font 임포트, 제목·제출·취소 6개 적용, WBP의 `??` 정적 한글 19개 복구
- 사용자 편집본에서 `WBP_HeistForgery.ModeStatusText`와 두 WBP의 `InstructionText` 제거를 확인하고 C++ `BindWidgetOptional`/갱신 코드 정리. WBP Layout 수치는 변경하지 않음
- Object Assembly 2D Part Tray/Canvas, 숨은 Socket Anchor 양자화, Wheel 회전, 우클릭 제거, R Reset
- Surface 서버 `QualityBelowMinimum` 거절 시 동일 Drawing 재제출 차단 및 로컬 수정 후 제출 재활성화
- Artifact DataTable의 `MinimumForgeryScore=0.7`
- `AGENTS.md`, `Museum_Heist_GDD.docx`, `Museum_Heist_TDD.docx` Rev 12
- 상세 파일 목록과 변경 계약: [`CURRENT_PROJECT_STATUS.md`](CURRENT_PROJECT_STATUS.md#3-implemented-artifacts)

#### Verification

```text
C++ Editor Build        REBUILD REQUIRED / 마지막 PASS 이후 Instruction·ModeStatus C++ 정리
WBP Compile / Save      PASS / 두 WBP Compile=true, Dirty=false
Removed Widget Tree     PASS / Forgery Instruction·ModeStatus=None, Assembly Instruction=None
Korean Font Render      PASS
TENADA Import / Render  PASS / Compile+Save / QuestionMark=0 / TargetBlocks=6
DataTable Save          PASS
Document Render QA      PASS / Word PDF GDD 29p, TDD 40p, 전 페이지+변경 페이지 검수
git diff --check        PASS
Automation              PASS / ReplicaAcceptanceContract
User PIE                NOT RUN / REQUIRED
Multiplayer Runtime     NOT RUN / REQUIRED
Notion Write            NOT DONE
```

#### Remaining Evidence

- `ProjectMuseumHeist.Forgery.ReplicaAcceptanceContract` 자동화 PASS (`2026-08-09 15:42 KST`)
- 2 Player Listen Server PIE에서 Owner-only UI, 70점 Gate, Timeout 1회 조사, Alert 불변 확인
- Surface/Object 작업 중 Suspicious/Searching Session 유지, Alarmed/Lockdown 서버 선취소, Widget 강제 종료, 신규 재진입 거부, Gameplay Input 복원 확인
- Object 2D Drag & Drop, Socket 양자화, Wheel/우클릭/R 입력과 제출 후 Compact Payload 기반 3D Replica 확인

#### Next Reconciliation Action

1. User PIE 증거를 수집한다. 자동화 Gate는 PASS했다.
2. 2026-08-09 15:44 KST 라이브 검색에서는 정확히 일치하는 진행중 Task가 없고, 기존 `TASK-W5-013`은 버튼식 계약으로 이미 완료임을 유지한다.
3. 사용자가 신규 Task 생성 또는 별도 회귀 Task Relation을 결정한다.
4. 사용자가 Notion 반영을 요청하고 쓰기 성공을 재조회한 뒤 `RECONCILED`로 이동한다.

---

## Reconciled Archive

### LOCAL-20260815-02 — W7 Team Readability Foundation And Integration Gate

- State: `RECONCILED`
- Created / Reconciled: `2026-08-15 KST`
- Notion Relations: `TASK-W7-001`~`TASK-W7-011`
- Test Logs:
  - [`TEST-W7-001`](https://app.notion.com/p/3bc1d26a5dfb81f9a48ac7f4566fa7be)
  - [`TEST-W7-002`](https://app.notion.com/p/3bd1d26a5dfb81549291f537370bfd37)
  - [`TEST-W7-003`](https://app.notion.com/p/3bd1d26a5dfb813c8f05ee11c1ecc46c)
  - [`TEST-W7-004`](https://app.notion.com/p/3bd1d26a5dfb81e5aaabc7f01159e089)
  - [`TEST-W7-005`](https://app.notion.com/p/3bd1d26a5dfb81379889d4e36d76f8ee)
- Notion Status:
  - 완료: `TASK-W7-001`, `002`, `003`, `007`, `008`, `009`, `011`
  - 진행중: `TASK-W7-004`, `005`, `006`, `010`

#### Applied Work

- PlayerState 기반 Crew Status를 Remote Nameplate와 Main HUD가 함께 소비하도록 통합했다.
- 서버 Walk/Sprint/Weight, Pace 기반 Footstep, Guard Stun→Arrest→Rescue 상태를 복제·입력 계약과 연결했다.
- Owner-only Floor Plan Map과 Map Input Mode를 추가하고 Local/Team/Exit/Zone/발견 Target만 표시하도록 제한했다.
- Surface/Object 작업 중 Suspicious/Searching은 유지하고 Alarmed/Lockdown에서 서버가 Session을 선취소한 뒤 Widget과 Gameplay Input을 정리하며, 위험 단계의 신규 재진입과 Case Lock 누수를 막도록 검증했다.
- 실제 `WBP_HeistNameplate_C`와 Team Status의 8상태 색·글리프 및 Remote 3/Local 0을 4P TwoRuns 상태 전이에서 검증하고, 거리 Fade는 별도 수식 계약 테스트로 검증했다.
- M01/M02/M03 Floor Plan Data/Texture, 명시적 Marker whitelist, 4P Map 입력 잠금·복원과 Map-open Stun 강제 종료를 검증했다.
- 렌더·오디오 활성 4P에서 Alert 0~4 한글/색, Alarmed Countdown, click-free edge로 재임포트한 Suspense/Alarm Loop 재생과 Lockdown Cleanup을 검증했다.
- GDD/TDD를 Rev 13으로 맞추고 Suspicious/Searching 유지, Alarmed/Lockdown 서버 선취소·재진입 거부 계약으로 동기화했으며 변경 페이지를 Word PDF/PNG로 검증했다.
- 실제 M01/M02/M03 Surface Template 각 12개, 고정 Seed 결정성, 24회 Cycle 중복 방지, 최근 3개 보호, Random Map Bag, Optional Exhibit 조합을 검증했다.
- Player Count별 경비 수·발각 유예·검사 시간을 실제 Runtime에 적용하고 1P/2P/4P M01 TwoRuns 및 Lobby Reset을 검증했다.
- 2P Client의 실제 `IA_Move` 입력으로 Walk/Sprint Footstep 500/1000cm, 서버 Guard Investigate와 Client 상태 복제, Alert 불변을 검증했다.

#### Evidence

```text
Project_MuseumHeistEditor Build          PASS
ProjectMuseumHeist Full Automation       23/23 / Failed 0
Solo ContractRun Two Runs                PASS
Two Player ContractRun Two Runs          PASS
Four Player ContractRun Two Runs         PASS
2P Walk/Sprint Footstep→Guard Investigate PASS
Stun / Arrest / Rescue                    PASS
Surface / Object Alert Forced Close      PASS
Variation Real Data + Determinism        PASS
Notion Test Logs                         TEST-W7-001/002 Pass
W7 Presentation Test Logs                TEST-W7-003/004/005 Pass
W7 Presentation Full Regression          10/10 / Failed 0 / NotRun 0
GDD/TDD Rev 13 Alert Contract             PASS / changed-page Word render
Notion Live Re-fetch                     PASS
```

#### Remaining Scope

- Stun/Arrest/Carry 최종 화면·오디오·Pose Presentation은 개별 `진행중` Task의 완료 기준으로 남겼다.
- 실제 2~3분 Escape 리듬은 NullRHI 자동화 Pass로 대체하지 않았다.

---

### LOCAL-20260815-01 — W6 Contract Run Feature Complete

- State: `RECONCILED`
- Created / Reconciled: `2026-08-15 KST`
- Notion Relations:
  - [`TASK-W6-000`](https://app.notion.com/p/3af1d26a5dfb819199d9e3b1be2eeaf5)
  - [`TASK-W6-009`](https://app.notion.com/p/39a1d26a5dfb81b08540c8824de8dc30)
  - [`TASK-W6-010`](https://app.notion.com/p/39a1d26a5dfb810a8591d5d48ed7dd56)
  - [`TASK-W6-011`](https://app.notion.com/p/39a1d26a5dfb81b0a157d294b89edfc8)
- Test Logs:
  - [`TEST-W6-008`](https://app.notion.com/p/3bc1d26a5dfb810a9cb1f1462b3ee8ca)
  - [`TEST-W6-009`](https://app.notion.com/p/3bc1d26a5dfb816aa446e67ce0b1db5a)
  - [`TEST-W6-010`](https://app.notion.com/p/3bc1d26a5dfb81b281f0de6af59cddf2)
- Notion Status: `TASK-W6-000~011 완료` / `W6 Gate=Pass` / live re-fetch confirmed

#### Applied Work

- M01 Shared Exit의 실제 배치·Nav·Deposit을 검증하고 M02/M03 W8 배치 체크리스트를 확정했다.
- M01 Solo와 4P Contract Run을 각각 두 번 연속 수행해 Lobby Return 및 두 번째 Match clean reset을 검증했다.
- Loose Loot 5종의 정의·월드 비주얼을 구분하고 2P Pickup/Drop/Re-pickup/Deposit 전 과정을 자동화했다.
- TASK-W6-000의 독립 수동 검토를 TEST-W6-008/009/010과 TASK-W6-010 상위 E2E 증거로 대체해 완료 처리했다.
- replicated level actor 파괴로 발생하던 ActorChannelFailure를 Contract 활성 상태 RepNotify로 제거했다.
- seamless transition 중 LocalPlayer 미부착 Controller의 Widget 생성 오류를 차단했다.

#### Evidence

```text
Project_MuseumHeistEditor ForceUnity Build       PASS
TASK-W6-009 M01 Exit/Nav/Deposit                 PASS
TASK-W6-010 Solo Two Runs                        PASS / Errors=0
TASK-W6-010 Four Player Two Runs                 PASS / Errors=0
TASK-W6-011 Loot Definition                      PASS / Errors=0
TASK-W6-011 Loot LifecycleTwoPlayer              PASS / Errors=0
Notion Test Logs                                 TEST-W6-008/009/010 Pass
Notion Task Status                               TASK-W6-000~011 완료
Notion Week Gate                                 W6 Pass
```

#### Scope Boundary

- M02/M03 실제 `.umap` Shared Exit 배치와 최종 3-Map 조명·Lockdown·Original Carrier 동선은 W8 범위다.

---

### LOCAL-20260809-02 — TASK-W6-006 Progress Checkpoint

- State: `RECONCILED`
- Created / Reconciled: `2026-08-09 17:46 KST`
- Notion Relation: [`TASK-W6-006`](https://app.notion.com/p/39a1d26a5dfb8132a835ee75d548e348)
- Test Log: [`TEST-W6-005`](https://app.notion.com/p/3b71d26a5dfb81428a21f134d49cd06a) / `Integration Pass`
- Notion Status: `진행중`

#### Applied Work

- 기존 서버 누적·복제·Result Snapshot 경로를 완료 기준별로 재감사했다.
- Coin Distraction을 단순 반경 내 Guard 존재가 아닌 실제 `ReactToSoundPing` 수락 Guard 수로 누적하도록 수정했다.
- Result Row에 Surface/Assembly 최고 품질, Original 운반 시간과 `탈출/체포/미탈출` 설명을 추가했다.
- Winner/Rank 또는 정렬 기반 경쟁 결과는 추가하지 않았다.

#### Evidence

```text
Project_MuseumHeistEditor Build              PASS / 2026-08-09 17:42 KST
ProjectMuseumHeist.Result Automation         PASS / 6 of 6
git diff --check                             PASS
Notion Progress Note                         PASS / live re-fetch confirmed
2 Player Listen Server PIE                   NOT RUN / REQUIRED
```

#### Remaining Evidence

- Host/Client 각자의 기여 이벤트가 서버에서 해당 PlayerState에만 누적되는지 확인한다.
- `HeistContributionDump`를 Host와 Client에서 실행해 `DataContract=PASS`, `TerminalFlags=PASS`, `WinnerRank=None`과 동일 PlayerCount를 확인한다.
- Result 화면에서 각 Player Row의 최고 품질·운반 시간·기여 항목과 `탈출/체포/미탈출`이 일치하고 Winner/Rank가 없는지 확인한다.
