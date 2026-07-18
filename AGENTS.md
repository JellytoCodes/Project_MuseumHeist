# Project_MuseumHeist — Codex Instructions
## Rev 4: W4 Forgery And Detection Baseline

기준일: 2026-07-17
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
- Smoke Throwable / Smoke Sight Blocking

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
- First-Person Camera는 Full Body Mesh의 Head Bone/Socket에 부착한다.
- Camera 위치 오프셋은 Blueprint에서 캐릭터 Mesh에 맞게 조정한다.
- Owning Player와 다른 플레이어 모두 Full Body Mesh와 자연스러운 그림자를 유지한다.
- Local Head를 자동으로 숨기지 않으며, 얼굴 클리핑은 Camera Socket Offset으로 해결한다.
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
- Reference Image는 직접 제작한 단순한 이미지와 Template별 2~8색 제한 팔레트를 사용한다.
- Template은 `None / Black / White` 배경 필터와 허용 오차를 DataTable에서 지정한다.
- `Black / White`는 Reference Image에서 해당 배경색을 제외하고, `None`은 별도 Reference Mask를 사용한다.
- 플레이어는 Template 팔레트에서 색을 직접 선택하며, 위치에 맞는 정답 색을 자동 선택하지 않는다.
- 각 Stroke는 임의 RGB가 아니라 서버가 제공한 `PaletteIndex`를 함께 전송한다.
- 데이터 크기, 좌표 범위, Stroke Limit를 서버가 검증한다.
- 서버는 Reference Image를 제한 팔레트로 양자화하고 형태 정확도와 팔레트 색 정확도를 함께 판정한다.
- Reference 대비 과도한 면적을 칠하면 Anti-Fill Score Cap을 적용한다.
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
- Editor 작업이 필요하면 사용자용 Blueprint/Data/Map 작업 절차를 현재 대화에서 바로 제공한다. 사용자가 명시적으로 요청하지 않는 한 별도 `.md` 파일을 만들지 않는다.
- C++ 빌드만 성공했다고 Editor 작업 포함 Task를 완료 처리하지 않는다.
- 멀티플레이·Ownership·Replication 주장은 사용자 실행 PIE 증거가 있을 때만 PASS 처리한다.

### Codex / Unreal Editor Work Ownership

Codex가 담당한다.

- C++ Gameplay Rule, Authority, Validation, Replication 구현과 수정
- Repository 코드, Config, Data Import JSON의 분석과 필요한 범위의 수정
- C++ 빌드와 정적 검증
- Task 판정에 필요한 `UHeistDebugFunctionLibrary` 로그와 Debug/Cheat Command 구현
- 사용자가 제출한 PIE Output Log를 근거로 `PASS`, `FAIL`, `BLOCKED` 판정

사용자가 Unreal Editor에서 담당한다.

- Blueprint / Widget Blueprint 구성과 수정
- DataTable / DataAsset 편집
- Map 배치, Scale, Collision, Lighting, Navigation 수정
- Asset Assignment와 Component Assembly
- Blueprint Compile / Save
- PIE 실행과 Debug Command 실행

Codex는 사용자가 명시적으로 직접 조작을 요청하지 않는 한 다음을 수행하지 않는다.

- Unreal Editor 실행 또는 종료
- Unreal Editor UI 직접 조작
- Unreal MCP 연결, 재연결, 복구 또는 직접 호출
- `.uasset` / `.umap` 직접 수정

Editor 작업 절차는 현재 대화에서 다음 항목만 간결하게 제공한다.

- 열 Asset / Map
- 선택할 Actor / Component
- 변경할 Property와 값
- Compile / Save 순서
- PIE Mode와 Player 수
- 실행할 Debug Command
- 제출할 Output Log

### Runtime Test And Log Handoff

- Runtime Task는 기존 `UHeistDebugFunctionLibrary`와 `UHeistCheatManager` 경로를 우선 사용한다.
- 완료 조건을 판정할 로그가 부족하면 Codex가 활성 Task 범위 안에서 최소 DebugLibrary 로그 또는 Debug/Cheat Command를 C++로 추가한다.
- 사용자는 Unreal Editor PIE에서 안내된 Debug Command를 실행하고 관련 Output Log를 Codex에게 제출한다.
- 화면 동작이 완료 조건에 포함된 경우 사용자는 관찰 결과도 함께 전달한다.
- Codex는 제출된 로그와 관찰 결과를 Task 완료 조건에 직접 대조해 `PASS`, `FAIL`, `BLOCKED`를 판정한다.
- 빌드 성공만으로 Runtime Task를 PASS 처리하지 않는다.
- 개별 Task PASS와 Weekly Gate / Formal Test PASS를 분리한다.

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

- Top-Down Camera 관련 Blueprint
- Cursor Aim 관련 입력

새 흐름에서 호출하지 않고, Reference Viewer와 회귀 확인 후 별도 Cleanup Task에서 제거한다.

기존 검증 기록은 재사용 가능한 Regression Baseline으로 보존한다.

---

## 14. Current Phase

W3는 완료됐다. 현재 실행 기준은 W4 단일 범위이며 기간은 2026-08-03부터 2026-08-16까지다.

### W3 Closeout

- `TASK-W3-001~015`, `TASK-W3-017~028`: 완료
- `TASK-W3-016`: v1.0 Excluded / 취소. Smoke는 Legacy로만 보존하며 활성 기능이나 Gate 조건으로 사용하지 않는다.
- `TASK-W3-029`: 앞선 개별 검증과 중복되는 통합 Gate였으므로 제거했다.
- 테스트 로그는 Task 번호와 독립된 연속 번호를 사용하며 현재 `TEST-W3-001~027`이 PASS다.

### W4 Scope

#### Forgery Vertical Slice

1. `TASK-W4-001` Forgery Session Lifecycle
2. `TASK-W4-002` Owner-only Full-Screen Forgery UI
3. `TASK-W4-003` Forgery Input Mode / Restore
4. `TASK-W4-004` Reference Template Load / Observation Handoff
5. `TASK-W4-005` Drawing Canvas / Stroke Collection
6. `TASK-W4-006` Stroke Transport / Server Validation
7. `TASK-W4-007` Reference Mask / Palette Forgery Score
8. `TASK-W4-008` Replica Placement / Original Removal
9. `TASK-W4-009` Submitted Replica World Visual
10. `TASK-W4-010` Forgery Recovery Edge Cases
11. `TASK-W4-011` W4 3-Map Forgery Gate

#### Detection / Alert / Lockdown

12. `TASK-W4-012` Inspection Target Registration
13. `TASK-W4-013` Guard InspectExhibit State
14. `TASK-W4-014` Score → Inspection Delay Mapping
15. `TASK-W4-015` Global Alert State Replication
16. `TASK-W4-016` Alert-driven Guard Modifiers
17. `TASK-W4-017` Lockdown Countdown / World Restriction
18. `TASK-W4-018` Alert HUD / Audio Layers
19. `TASK-W4-019` Duplicate Inspection / Timer Protection
20. `TASK-W4-020` M01 / M02 / M03 Alert Profiles
21. `TASK-W4-021` W4 Detection Loop Gate

### W4 Execution Rules

- 활성 구현 범위는 `TASK-W4-001~021`이다.
- Forgery Session, Score, Replica/Original 확정, Alert, Lockdown은 서버 권한 C++ 경로를 유지한다.
- Owner-only UI, Input Restore, Case Lock Cleanup, Duplicate 방지, Timer 정리는 Critical Gate 조건이다.
- 멀티플레이·Ownership·Replication·Recovery 주장은 사용자 PIE와 DebugLibrary/Cheat Command 로그가 있을 때만 PASS 처리한다.
- 이미 증명된 동일 흐름을 같은 조건으로 반복하는 중복 Gate는 만들지 않는다. 새로운 위험을 검증할 때만 별도 Gate를 추가한다.

세부 설계와 주차별 Task 정의는 `Museum_Heist_GDD.docx` Rev.9를 기준으로 Repository에서 확인한다.
