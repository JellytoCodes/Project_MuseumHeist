# Project_MuseumHeist — Codex Instructions
## Rev 2: First-Person Cooperative Forgery

기준일: 2026-07-11  
엔진: Unreal Engine 5.8  
현재 목표: 2026-09-20 W12 Final RC / 프로젝트 마무리

이 문서는 프로젝트 엔지니어링 정책의 최상위 Source of Truth다. 기존 경쟁형 Top-Down 버전은 Legacy로 보존한다.

---

## 1. Project Overview

Project_MuseumHeist는 Unreal Engine 5.8 C++ 기반 **1~4인 온라인 협동 1인칭 잠입·유물 위조 하이스트 게임**이다.

플레이어들은 박물관에 침입해 목표 유물을 관찰하고, 현장에서 Replica를 제작해 Original과 바꿔치기한 뒤, Guard가 위조품을 발견해 Lockdown을 완료하기 전에 Original과 Loose Loot을 가지고 탈출한다.

신규 방향에서 사용하지 않는 요소:

- 플레이어 간 공격·기절·전리품 강탈
- Piñata Drop
- 개인 Score 경쟁, Winner, Rank
- Gap Tracker와 Leader Reveal
- 선착순 Zero-Sum Extraction
- Top-Down Camera와 Cursor Aim

---

## 2. Design Source And Scope Priority

1. `AGENTS.md`
2. `ClassManifest.md`
3. `Museum_Heist_GDD.docx`
   - 본문: 게임 설계
   - Appendix A: Current Implementation Baseline
   - Appendix B: First-Person Conversion Audit
   - Appendix C: Pivot Migration Plan
   - Appendix D: Execution Roadmap
   - Appendix E: Blueprint / Widget Shell Plan

하위 문서가 상위 문서와 충돌하면 구현 전에 상위 문서를 먼저 수정한다.

---

## 3. Core Loop

```text
Lobby
→ First-Person Infiltration
→ Target Artifact 탐색
→ Painting Observation
→ Owner-only Full-Screen Forgery
→ 서버 Forgery Score 판정
→ Replica 배치 / Original 회수
→ Loose Loot 추가 루팅
→ Guard Inspection / Alert / Lockdown
→ Shared Extraction
→ Team Result / Player Contribution
```

---

## 4. v1.0 Required Scope

- 1~4인 Listen Server
- Full First-Person
- 고정 박물관 맵 1개
- 고정 계약 1개
- `Lobby → ReadyCountdown → InGame → End`
- Painting Target Artifact 1개
- Painting Template 3~5개
- Display Case State Machine
- Observation + Full-Screen Drawing Forgery
- 서버 권한 Forgery Score
- Replica / Original Swap
- Guard Patrol / Investigate / Chase / Search / InspectExhibit
- Alert Level / Lockdown
- Loose Loot + 4×5 Grid Inventory + Weight Penalty
- Coin Guard Distraction
- Smoke Guard Sight Blocking
- Shared Extraction
- Team Success / Partial Success / Failure
- Team Result / Player Contribution
- 1인 및 2~4인 완주
- Development Build 패키징

### Excluded

- 조각상·도자기·보석·문서·화석 복제
- 외부 AI 이미지 판정
- Steam Voice, PCG, Security Room, Cinematic
- 추가 맵, 고급 Loadout, Progression
- PvP, 배신, 경쟁 랭킹
- 전용 서버, Skill Matchmaking
- Perspective Toggle, Third-Person
- 별도 First-Person Arms와 복잡한 Hand Interaction

### Stretch

필수 기능과 멀티플레이 Gate가 모두 PASS한 뒤에만 진행한다.

- Sculpture Assembly Forgery
- Glue Trap PvE
- Optional Rare Artifact
- Steam Session
- First-Person Hand Animation
- 추가 Painting Template / Loose Loot

---

## 5. Hard Rules

- Unreal Engine 5.8을 유지한다.
- `Heist` 접두사와 `PROJECT_MUSEUMHEIST_API`를 유지한다.
- Gameplay Rule, Authority, Validation, Replication은 C++가 소유한다.
- Blueprint는 Asset Assignment, Component Assembly, Visual Presentation만 담당한다.
- Widget Blueprint는 Layout, Animation, Color, Icon, Binding만 담당한다.
- DataTable/DataAsset는 반복 데이터와 밸런스 값을 담당한다.
- Map은 배치와 공간 구성을 담당한다.
- `.uasset`은 Unreal Editor/MCP로만 수정한다.
- `.umap`은 사용자가 명시적으로 요청한 경우에만 수정한다.
- 불필요한 Manager, Service, Factory, Processor, Subsystem을 추가하지 않는다.
- Painting마다 Actor Class를 만들지 않는다.
- 외부 AI API로 Forgery Score를 계산하지 않는다.
- Render Target 전체를 매 프레임 복제하지 않는다.
- Manifest에 없는 타입을 활성 Task에서 임의 생성하지 않는다.
- 미래 주차의 전체 시스템을 선행 구현하지 않는다.

---

## 6. Server Authority Flow

```text
Local Input / Widget Request
→ AHeistPlayerController Server RPC
→ C++ Component Validation
→ Server State Mutation
→ Replicated State / Owner Client Response
→ ViewModel
→ Widget Presentation
```

Client가 직접 확정할 수 없는 항목:

- Forgery Score
- Replica Placement
- Original Removal
- Display Case State
- Objective State
- Alert Level
- Lockdown
- Guard Inspection Result
- Extraction Result
- Team Result
- Player Contribution 확정값

---

## 7. First-Person Camera Rules

- Camera는 머리 높이에 배치한다.
- Controller Yaw/Pitch가 시점을 제어한다.
- Character Yaw는 Controller Yaw를 따른다.
- Interaction은 Center Screen Line Trace를 사용한다.
- Flashlight와 Throw Direction은 Camera Forward를 기준으로 한다.
- Top-Down Camera, SpringArm Gameplay Camera, Cursor World Aim은 사용하지 않는다.
- 다른 플레이어에게는 Full Body Mesh를 표시한다.
- Owning Player의 Head는 로컬 화면에서 숨긴다.
- FOV 기본값은 90이다.
- Head Bob, Camera Roll, Sprint Camera Effect는 v1.0에서 사용하지 않는다.
- Inventory/Forgery 진입 시 Cursor와 UI Input Mode를 활성화한다.
- UI 종료 시 Mouse Capture, Look, Movement, Interaction Context를 복원한다.

---

## 8. Forgery Rules

### Session Ownership

- 한 Display Case는 동시에 한 명만 위조할 수 있다.
- 서버가 Session Owner, 거리, Match Phase, Player State, Case State를 검증한다.
- Disconnect, Arrest, Cancel, Timeout, Match End 시 Session Lock을 해제한다.

### Owner-only Full-Screen Mode

- Forgery Widget은 Owning Player에게만 표시한다.
- Forgery 중 World View는 완전히 가린다.
- World Audio와 팀 통신은 유지한다.
- Move, Look, Jump, Sprint, Throw, QuickSlot, Inventory, Loot, 다른 Interaction을 차단한다.
- Draw, Erase, Submit, Cancel, Push-To-Talk, Pause만 허용한다.

### Stroke Transport

- Client는 정규화된 Stroke Point를 수집한다.
- 데이터 크기, 좌표 범위, Stroke Limit를 서버가 검증한다.
- Submit 시 또는 제한된 Chunk 단위로 전달한다.
- Client는 최종 Score를 전송하지 않는다.

### Cleanup

중단 시 반드시 복원한다.

- Display Case Lock
- Forgery Owner
- Movement / Look / Interaction
- Cursor / Mouse Capture
- Input Mapping Context
- HUD / QuickSlot / Inventory 접근
- Forgery Widget Instance

중간 Drawing 진행도는 v1.0에서 저장하지 않는다.

---

## 9. Input Mode Rules

입력 모드는 상호 배타적으로 관리한다.

- `Gameplay`
- `Inventory`
- `Forgery`

Context 전환 시 기존 Context를 명시적으로 제거하고 새 Context를 추가한다. 중복 Widget 생성과 Input Context 누적을 허용하지 않는다.

---

## 10. C++ / Blueprint / Data / Map Responsibility

| 영역 | 책임 |
|---|---|
| C++ | Rule, State, Authority, Validation, Replication, Stable API |
| Blueprint | Mesh, Material, Camera Position, Component Assembly, Visual Hook |
| Widget Blueprint | Layout, Binding, Animation, Presentation |
| ViewModel/C++ Widget | UI State Exposure, Request Routing |
| DataTable/DataAsset | Artifact, Template, Guard, Balance, Scaling Data |
| Map | Display Case, Guard Route, Loot, Exit, Lighting, Navigation |

Blueprint Graph 금지 항목:

- Score 계산
- Original/Replica 확정
- Alert/Lockdown 변경
- Extraction 성공 판정
- Team Result 확정
- 신규 Server RPC
- Replicated Gameplay State 직접 변경

---

## 11. Numbered Task Boundary

- 활성 `TASK-Wn-###`만 구현한다. PvE 피벗 이후에도 기존 프로젝트 주차 번호를 연속 사용하며 별도 `F` 태스크 체계를 만들지 않는다.
- Task 시작 전 관련 문서와 Manifest 상태를 확인한다.
- Editor 작업이 필요하면 사용자용 Blueprint/Data/Map 지침을 별도로 제공한다.
- C++ 빌드만 성공했다고 Editor 작업 포함 Task를 완료 처리하지 않는다.
- 멀티플레이·Ownership·Replication 주장은 사용자 실행 PIE 증거가 있을 때만 PASS 처리한다.

---

## 12. Verification Standard

각 Task 결과는 다음을 구분한다.

- `Implementation Complete`
- `Blueprint/Data/Map Pending`
- `User PIE Pending`
- `PASS`
- `FAIL`
- `BLOCKED`

PIE가 필요한 Task는 다음을 명시한다.

- PIE Mode와 Player 수
- 실행할 Window
- 입력 또는 Debug Command
- 기대 화면 동작
- 기대 Log
- PASS / FAIL 신호
- Task Test인지 Weekly Gate인지

Known Warning은 숨기지 않는다. Critical Replication, Ownership, Duplicate Artifact, Input Restore 문제는 Weekly Gate를 차단한다.

---

## 13. Legacy Preservation

다음 구현은 즉시 삭제하지 않는다.

- Gap Tracker
- PvP Stun
- Piñata Drop
- Winner / Rank
- Top-Down Camera 관련 Blueprint
- Cursor Aim 관련 입력

새 흐름에서 호출하지 않고, Reference Viewer와 회귀 확인 후 별도 Cleanup Task에서 제거한다.

기존 검증 기록은 재사용 가능한 Regression Baseline으로 보존한다.

---

## 14. Current Phase

피벗 문서와 Notion 일정 개편은 완료됐다. 다음 구현 단계는 `Museum_Heist_GDD.docx` Rev.7의 W3이며 2026-07-13에 시작한다.

첫 우선순위:

1. `TASK-W3-001` First-Person Camera / Rotation
2. `TASK-W3-002` Local Head Hide / Remote Full Body
3. `TASK-W3-003` Center Screen Interaction / Crosshair
4. `TASK-W3-004` Camera Forward Flashlight / Throw
5. `TASK-W3-005` Gameplay / Inventory Input Restore
6. `TASK-W3-006` Legacy PvP / Gap / Rank Invocation Block
7. `TASK-W3-007` Coin / Smoke Guard-only Conversion
8. `TASK-W3-008` M01 First-Person Scale / Collision Pass
9. `TASK-W3-009` First-Person HUD Rewire
10. `TASK-W3-010` W3 1~4 Player Integrated Gate

세부 구현 상태와 주차별 Task는 `Museum_Heist_GDD.docx` Rev.7을 기준으로 Repository에서 다시 확인한다.
