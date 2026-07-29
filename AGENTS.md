# Project_MuseumHeist — Codex Instructions

## Rev 10: W5 Surface And Object Forgery Expansion

기준일: 2026-07-27
엔진: Unreal Engine 5.8  
현재 목표: 2026-09-20 W8 Final RC / 프로젝트 마무리

이 문서는 프로젝트 엔지니어링 정책의 최상위 Source of Truth다.

현재 프로젝트는 기존 경쟁형 Top-Down 구조에서 **1~4인 온라인 협동 1인칭 잠입·유물 위조 하이스트 게임**으로 전환됐다.

Forgery Gameplay는 다음 두 축으로 구분한다.

```text
Surface Forgery
- Painting 중심 2D Reference / Stroke / Palette / OpenCV 판정

Object Assembly Forgery
- Sculpture / Ceramic 중심 3D Modular Part / Socket / Orientation 판정
```

기존 경쟁형 Top-Down 구현은 아직 참조가 남아 있는 범위에서만 Legacy로 취급한다. 현재 기획에서 명시적으로 제거된 Smoke 및 플레이어 설치형 Trap 기능은 Legacy, Deferred, Stretch 또는 회귀 기준으로 유지하지 않는다.

---

# 1. Project Overview

Project_MuseumHeist는 Unreal Engine 5.8 C++ 기반의 **1~4인 온라인 협동 1인칭 잠입·유물 위조 하이스트 게임**이다.

플레이어들은 박물관에 침입해 목표 유물을 관찰하고, 현장에서 Replica를 제작해 Original과 바꿔치기한 뒤, Guard가 위조품을 발견하고 Lockdown을 완료하기 전에 Original과 Loose Loot을 가지고 탈출한다.

## Core Fantasy

```text
박물관 침입
→ 목표 그림 탐색
→ 그림 관찰
→ 제한된 시간 안에 위조 그림 제작
→ 서버 OpenCV 유사도 판정
→ Replica와 Original 교체
→ Guard 검사 및 Alert 상승
→ Original과 Loot을 들고 공동 탈출
```

## 현재 방향에서 사용하지 않는 요소

- 플레이어 간 공격
- 플레이어 간 기절
- 플레이어 간 전리품 강탈
- Piñata Drop
- 개인 Score 경쟁
- Winner
- Rank
- Gap Tracker
- Leader Reveal
- 선착순 Zero-Sum Extraction
- Top-Down Gameplay Camera
- Cursor World Aim
- Smoke Grenade
- Smoke Projectile
- Smoke Cloud
- Smoke Sight Blocking
- Glue Trap
- Noise Trap
- 플레이어 설치형 Trap
- Trap Placement Cast
- Trap 전용 QuickSlot
- Player-facing SoundPing Marker
- SoundPing Direction Widget

---

# 2. Design Source And Scope Priority

프로젝트 설계 문서의 우선순위는 다음과 같다.

1. `AGENTS.md`
2. `ClassManifest.md`
3. `Museum_Heist_GDD.docx`
   - 본문: 게임 설계
   - Appendix A: Current Implementation Baseline
   - Appendix B: First-Person Conversion Audit
   - Appendix C: Pivot Migration Plan
   - Appendix D: Execution Roadmap
   - Appendix E: Blueprint / Widget Shell Plan
4. `Docs/W2_BlueprintShellPlan.md`

하위 문서가 상위 문서와 충돌하면 구현 전에 상위 문서를 먼저 수정한다.

Notion Task 기록은 개별 Task 상태와 테스트 로그 번호의 Source of Truth로 사용한다. 단, 아키텍처와 구현 정책은 Repository의 `AGENTS.md`와 `ClassManifest.md`를 우선한다.

---

# 3. Core Loop

```text
Title Menu
→ Host Session 또는 Join Code 입력
→ Lobby
→ Ready Countdown
→ First-Person Infiltration
→ Target Artifact 탐색
→ Painting Observation
→ Owner-only Full-Screen Forgery
→ 서버 Forgery Score 판정
→ Replica 배치
→ Original 회수
→ Loose Loot 추가 루팅
→ Guard Inspection
→ Alert / Lockdown
→ Shared Extraction
→ Team Result / Player Contribution
```

---

# 4. v1.0 Required Scope

- 1~4인 Listen Server
- Steam Online Session
- 별도 Title Menu Level
- 별도 Online Lobby Level
- Full First-Person
- 고정 박물관 맵 1개
- 고정 계약 1개
- `TitleMenu → Lobby → ReadyCountdown → InGame → End`
- Painting Target Artifact
- Surface Forgery Template 36개
  - M01 / M02 / M03 각 12개
- Server-selected Surface Template Pool / Shuffle Bag
- Painting Display Case State Machine
- Observation Cast
- Owner-only Full-Screen Drawing Forgery
- 서버 권한 Forgery Score
- Object Assembly Forgery Vertical Slice
- Sculpture / Ceramic Modular Kit 최소 2종
- Object Assembly Template 최소 12개
- Owner-only Full-Screen Object Assembly
- 서버 권한 Part / Socket / Orientation Score
- Replica Placement
- Original Removal
- Guard Patrol
- Guard Investigate
- Guard Chase
- Guard Search
- Guard InspectExhibit
- Alert Level
- Lockdown
- Loose Loot
- 4×5 Grid Inventory
- Weight Penalty
- Coin Guard Distraction
- Shared Extraction
- Team Success
- Partial Success
- Failure
- Team Result
- Player Contribution
- 1인 완주
- 2~4인 멀티플레이 완주
- Development Build 패키징

## Excluded

다음 기능은 현재 프로젝트 범위에 포함하지 않는다.

- 보석·문서·화석 복제 Gameplay
- 외부 AI 이미지 판정
- Smoke Grenade
- Smoke Projectile
- Smoke Cloud
- Smoke Sight Blocking
- Glue Trap
- Noise Trap
- 플레이어 설치형 Trap
- Trap Placement Cast
- Steam Voice
- PCG
- Security Room
- Cinematic
- 추가 맵
- 고급 Loadout
- Progression
- PvP
- 배신
- 경쟁 랭킹
- 전용 서버
- Skill Matchmaking
- Perspective Toggle
- Third-Person Gameplay
- 별도 First-Person Arms
- 복잡한 Hand Interaction

## Stretch

필수 기능과 멀티플레이 Gate가 모두 PASS한 뒤에만 검토한다.

- Optional Rare Artifact
- First-Person Hand Animation
- 추가 Surface Forgery Template
- 추가 Object Assembly Kit / Template
- 추가 Loose Loot

Smoke 및 Trap 계열 기능은 Stretch 목록에 포함하지 않는다.

필요성이 다시 확정될 경우 기존 삭제 코드를 복구하지 않고, 당시의 기획과 현재 아키텍처를 기준으로 신규 Task를 작성한다.

---

# 5. Hard Rules

- Unreal Engine 5.8을 유지한다.
- `Heist` 접두사를 유지한다.
- Export 대상 타입에는 `PROJECT_MUSEUMHEIST_API`를 유지한다.
- Gameplay Rule은 C++가 소유한다.
- Authority는 C++가 소유한다.
- Validation은 C++가 소유한다.
- Replication은 C++가 소유한다.
- Stable Gameplay API는 C++가 소유한다.
- Blueprint는 Asset Assignment를 담당한다.
- Blueprint는 Component Assembly를 담당한다.
- Blueprint는 Mesh, Material, Animation, Audio, Visual Hook을 담당한다.
- Widget Blueprint는 Layout, Animation, Color, Icon, Binding만 담당한다.
- DataTable과 DataAsset은 반복 데이터와 밸런스 값을 담당한다.
- Map은 Actor 배치, 공간 구성, Lighting, Navigation을 담당한다.
- `.uasset`은 Unreal Editor 또는 명시적으로 승인된 MCP 경로로만 수정한다.
- `.umap`은 사용자가 명시적으로 요청한 경우에만 수정한다.
- 불필요한 Manager, Service, Factory, Processor, Subsystem을 추가하지 않는다.
- Painting마다 별도 Actor Class를 만들지 않는다.
- Manifest에 없는 타입을 활성 Task에서 임의 생성하지 않는다.
- 미래 주차의 전체 시스템을 선행 구현하지 않는다.
- 현재 기획에서 삭제된 기능을 호환성 명목으로 다시 추가하지 않는다.

## Surface Forgery / Object Assembly Boundary

- Painting 전시품과 Surface Forgery는 `AHeistPaintingDisplayCaseActor`가 담당한다.
- Sculpture와 Ceramic을 포함한 3D 조립 전시품은 `AHeistObjectDisplayCaseActor`가 담당한다.
- `Object Assembly Forgery`는 Sculpture / Ceramic을 포괄하는 Gameplay System 명칭이다.
- Surface Forgery와 Object Assembly는 서로의 Template Row를 공유하지 않는다.
- Surface Forgery와 Object Assembly는 서로의 제출 Payload와 Replica Data를 공유하지 않는다.
- Surface Forgery와 Object Assembly는 서로의 State Machine과 상세 Result를 공유하지 않는다.
- 두 방식은 Owner-only Input Mode 원칙과 서버가 확정한 최종 0~100 Quality Score의 Guard Inspection Handoff만 공유할 수 있다.
- `AHeistDisplayCaseActor`는 기존 Asset 호환 전용 Deprecated Painting Alias다.
- 신규 Asset은 `AHeistDisplayCaseActor`를 부모로 사용하지 않는다.
- 기존 `BP_DisplayCase`는 `AHeistPaintingDisplayCaseActor`를 부모로 사용한다.
- `AHeistSculptureDisplayCaseActor`는 기존 `BP_SculptureDisplayCase` 호환 전용 Deprecated Alias로 전환한다.
- 신규 Sculpture / Ceramic Asset은 `AHeistObjectDisplayCaseActor`를 부모로 사용한다.

## Removed Feature Boundary

다음 기능은 프로젝트에서 제거됐다.

```text
AHeistSmokeProjectile
AHeistSmokeCloudActor
AHeistTrapActor
AHeistGlueTrapActor
AHeistNoiseTrapActor
Trap Placement Cast
Smoke QuickSlot
Glue Trap QuickSlot
Noise Trap SoundPing
```

이 기능들은 다음 상태로 남기지 않는다.

- Legacy
- Deferred
- Stretch
- Regression Baseline
- Post-v1.0
- Placeholder

---

# 6. Server Authority Flow

```text
Local Input / Widget Request
→ AHeistPlayerController Server RPC
→ C++ Request Context Validation
→ Server Component Validation
→ Server State Mutation
→ Replicated State 또는 Owner Client Response
→ ViewModel
→ C++ Widget
→ Widget Blueprint Presentation
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
- Inventory Item Mutation
- QuickSlot Assignment
- Coin 사용 결과

Client Preview는 확정값으로 취급하지 않는다.

---

# 7. First-Person Camera Rules

- Camera는 머리 높이에 배치한다.
- Controller Yaw와 Pitch가 시점을 제어한다.
- Character Yaw는 Controller Yaw를 따른다.
- Interaction은 Center Screen Line Trace를 사용한다.
- Flashlight Direction은 Camera Forward를 기준으로 한다.
- Coin Throw Direction은 Camera Forward 또는 검증된 Camera Target을 기준으로 한다.
- Top-Down Gameplay Camera를 사용하지 않는다.
- SpringArm Gameplay Camera를 사용하지 않는다.
- Cursor World Aim을 사용하지 않는다.
- First-Person Camera는 Full Body Mesh의 Head Bone 또는 Socket에 부착한다.
- Camera 위치 Offset은 Blueprint에서 Character Mesh에 맞게 조정한다.
- Owning Player와 다른 플레이어 모두 Full Body Mesh를 유지한다.
- 자연스러운 Character Shadow를 유지한다.
- Local Head를 자동으로 숨기지 않는다.
- 얼굴 Clipping은 Camera Socket Offset으로 해결한다.
- 기본 FOV는 90이다.
- Head Bob은 v1.0에서 사용하지 않는다.
- Camera Roll은 v1.0에서 사용하지 않는다.
- Sprint Camera Effect는 v1.0에서 사용하지 않는다.
- Inventory와 Forgery 진입 시 Cursor와 UI Input Mode를 활성화한다.
- UI 종료 시 Mouse Capture, Look, Movement, Interaction Context를 복원한다.

---

# 8. Gameplay Item Rules

## Item Types

현재 지원하는 `EHeistItemType`:

```text
None
Loot
Throwable
KeyItem
```

현재 지원하지 않는 Item Type:

```text
Trap
```

## Use Types

현재 `EHeistUseType`은 다음 값을 유지한다.

```text
None
Throw
DeployArea
Consume
```

단, 현재 v1.0 활성 Gameplay Item은 Coin Throw뿐이다.

`DeployArea`와 `Consume`은 범용 Enum 값으로 남을 수 있지만, 승인된 Item Row와 Gameplay 실행 경로가 없으면 사용하지 않는다.

삭제된 Use Type:

```text
PlaceTrap
```

## QuickSlot

현재 QuickSlot은 Coin 하나만 지원한다.

```text
EHeistQuickSlotType::None
EHeistQuickSlotType::Coin
```

현재 QuickSlot Item:

```text
Slot: Coin
Input: Q
ItemId: Throwable_Coin
UseType: Throw
```

다음 QuickSlot은 존재하지 않는다.

- Smoke Grenade
- Glue Trap
- Noise Trap
- E Key Smoke Slot
- R Key Trap Slot

## DataTable Rules

다음 Row는 Item 및 UsableItem DataTable에 존재해서는 안 된다.

```text
Trap_Glue
Trap_Noise
Throwable_Smoke
```

삭제된 C++ 또는 Blueprint Class를 참조하는 Soft Class Reference를 남기지 않는다.

`AHeistGameMode::ValidateItemDataTables()`는 다음 조건을 만족해야 한다.

```text
Result=PASS
InvalidRows=0
OrphanExtensions=0
```

---

# 9. Action Component Rules

`UHeistActionComponent`가 현재 관리하는 Gameplay Cast:

- Observation Cast
- Escape Cast

제거된 Gameplay Cast:

- Trap Placement Cast

## Mutual Exclusion

Gameplay Cast는 상호 배타적이어야 한다.

```text
Observation 활성 중 Escape 시작 금지
Escape 활성 중 Observation 시작 금지
Forgery 활성 중 Observation / Escape 시작 금지
Inventory 활성 중 허용되지 않은 Cast 시작 금지
```

`IsGameplayCastActive()`는 현재 활성 Cast 전체를 나타내야 한다.

Cast 종료 시 Component Tick은 다른 활성 Cast가 없는 경우에만 비활성화한다.

## Cancellation

Observation 취소 조건:

- Input Release
- Movement
- Damage
- Arrest
- Session Invalid
- Display Case Invalid
- Match Phase 변경
- Owner EndPlay
- Disconnect

Escape 취소 조건:

- Movement
- Damage
- Arrest
- Vent Invalid
- Escape Phase 종료
- Match Phase 변경
- Owner EndPlay
- Disconnect

---

# 10. Surface Forgery Rules

## Match Template Selection

- 서버는 현재 Map Pool의 Surface Template을 매치당 하나만 확정한다.
- 선택은 Pool별 Shuffle Bag을 사용하며, 한 Cycle 안에서 같은 Template을 다시 선택하지 않는다.
- 재충전 시 직전 Cycle의 최근 3개 Template을 첫 선택 후보에서 제외한다.
- 선택된 Template Snapshot은 모든 Client에 복제한다.
- 선택된 Reference Image는 활성 Objective Target Painting Case의 Original World Visual에만 적용한다.
- 비목표 Painting Case의 Original World Visual을 현재 계약 Template으로 덮어쓰지 않는다.
- Lobby 복귀 또는 Selection Clear 시 Original World Visual은 Blueprint가 지정한 기준 Material로 복원한다.

## Session Ownership

- 한 Painting Display Case는 동시에 한 명만 위조할 수 있다.
- 서버가 Session Owner를 확정한다.
- 서버가 거리, Match Phase, Player State, Case State를 검증한다.
- Disconnect 시 Session Lock을 해제한다.
- Arrest 시 Session Lock을 해제한다.
- Cancel 시 Session Lock을 해제한다.
- Timeout 시 Session Lock을 해제한다.
- Match End 시 Session Lock을 해제한다.
- Owner 또는 Display Case EndPlay 시 Session을 정리한다.

## Owner-only Full-Screen Mode

- Forgery Widget은 Owning Player에게만 표시한다.
- Forgery 중 World View는 완전히 가린다.
- World Audio와 팀 통신은 유지한다.
- Move를 차단한다.
- Look을 차단한다.
- Jump를 차단한다.
- Sprint를 차단한다.
- Throw를 차단한다.
- QuickSlot을 차단한다.
- Inventory를 차단한다.
- Loot를 차단한다.
- 다른 Interaction을 차단한다.
- Draw를 허용한다.
- Erase를 허용한다.
- `R`은 현재 Client의 Local Stroke와 Preview만 초기화하며 Server RPC를 전송하지 않는다.
- 남은 Drawing Time은 Owner에게 복제된 `SessionEndServerTime`과 Server World Time의 차이로 표시한다.
- Drawing Time은 독립된 `DrawingTimeRemainingText`에 표시하며 키 가이드 또는 제목 Text와 결합하지 않는다.
- Submit을 허용한다.
- Cancel을 허용한다.
- Push-To-Talk를 허용한다.
- Pause를 허용한다.

## Stroke Transport

- Client는 정규화된 Stroke Point를 수집한다.
- Client는 Stroke별 Point Count를 수집한다.
- Client는 Stroke별 Palette Index를 수집한다.
- Client는 Template에서 승인된 Brush Size를 사용한다.
- Reference Image는 직접 제작한 단순한 이미지를 사용한다.
- Template별 Palette는 2~8색으로 제한한다.
- Template은 `None / Black / White` 배경 필터를 사용한다.
- `Black / White`는 Reference Image에서 해당 배경색을 제거한다.
- `None`은 별도 Reference Mask를 사용한다.
- 플레이어는 Template Palette에서 색을 직접 선택한다.
- 위치에 맞는 정답 색을 자동 선택하지 않는다.
- Stroke는 임의 RGB가 아니라 `PaletteIndex`를 전송한다.
- 서버는 Payload 크기를 검증한다.
- 서버는 좌표 범위를 검증한다.
- 서버는 Stroke Count를 검증한다.
- 서버는 Point Count를 검증한다.
- 서버는 Palette Index를 검증한다.
- 서버는 Brush Size를 검증한다.
- 서버는 Session Revision을 검증한다.
- Client는 최종 Score를 전송하지 않는다.

## OpenCV Score

서버와 Local Preview는 동일한 C++ Evaluator를 사용한다.

최종 확정값은 서버 결과뿐이다.

OpenCV 평가 구성:

```text
Reference 전처리
→ Player Stroke Palette Raster
→ Binary Foreground Mask
→ 3×3 Morphology Close
→ 양방향 Distance Transform
→ Mask Precision / Recall / Dice / IoU
→ Lab SSIM
→ Palette Histogram Similarity
→ Shape / Color 결합
→ Completeness
→ Anti-Fill
→ Final Score
```

Shape Score:

- Reference → Submitted 거리 기반 Coverage
- Submitted → Reference 거리 기반 Precision
- 양방향 Harmonic Mean
- Dice Similarity
- Response Curve 1.15

Color Score:

- Foreground Union ROI
- Lab SSIM
- Palette Histogram Similarity
- Response Curve 1.10

Completeness:

```text
min(SubmittedForeground / ReferenceForeground, 1)^0.65
```

Anti-Fill:

- Reference 대비 과도한 면적을 칠하면 Score Cap을 적용한다.
- 빈 배경 일치로 점수를 얻을 수 없도록 한다.

## Score Data Contract

Template Weight는 유효한 합계를 가져야 한다.

```text
CoverageWeight + MajorShapeWeight > 0
ShapeAccuracyWeight + ColorAccuracyWeight > 0
```

0으로 나누거나 NaN Score를 생성할 수 있는 Template Row를 허용하지 않는다.

Penalty 또는 Diagnostic Field가 Final Score에 직접 적용되지 않는 경우 문서와 필드 이름에서 그 사실을 명확히 한다.

## Alert Presentation

- `UHeistHUDViewModel`과 `UHeistForgeryViewModel`은 `AHeistGameState`의 복제 Alert Snapshot만 읽는다.
- Main HUD는 Quiet, Suspicious, Searching, Alarmed, Lockdown을 단계별 Text와 Color로 표시한다.
- 플레이어 표시는 `SECURITY LEVEL 0/4~4/4`와 4칸 별 Indicator를 사용한다.
- Guard의 확정 발각은 서버에서 최소 Suspicious를 요청하고, Painting 검사 결과는 Score Mapping 결과를 요청한다.
- Main HUD가 가려지는 Owner-only Forgery 화면에서도 동일한 Security Level Indicator를 표시한다.
- Alarmed의 Lockdown Countdown은 복제된 `AlertNextTransitionServerTime`과 Server World Time의 차이로 표시한다.
- HUD Lockdown Countdown은 독립된 `LockdownCountdownText`에 표시한다.
- Forgery 화면은 Quiet 이외 Alert에서 독립된 `ForgeryAlertWarningText`를 표시한다.
- Forgery 화면의 Lockdown Countdown은 `ForgeryLockdownCountdownText`에 별도로 표시한다.
- Suspicious/Searching은 Suspense Music Layer, Alarmed/Lockdown은 Alarm Music Layer를 사용한다.
- Forgery Full-Screen UI 중에도 Alert Music Layer는 유지한다.
- 실제 Music Asset 지정은 Widget Blueprint Class Default가 담당한다.

## Cleanup

Forgery 종료 시 반드시 복원한다.

- Painting Display Case Lock
- Forgery Owner
- Movement
- Look
- Interaction
- Cursor
- Mouse Capture
- Input Mapping Context
- HUD 접근
- QuickSlot 접근
- Inventory 접근
- Forgery Widget Instance

중간 Drawing 진행도는 v1.0에서 저장하지 않는다.

---

# 10A. Object Assembly Forgery Rules

## Supported Artifact Families

현재 Object Assembly Forgery의 활성 Family:

```text
Sculpture
Ceramic
```

Jewelry, Fossil 및 기타 Family는 별도 Numbered Task 승인 전 활성화하지 않는다.

## Modular Kit

- 하나의 Object를 Voxel 또는 파편 단위로 분해하지 않는다.
- 하나의 Template은 고정 Core와 조작 가능한 Part 3~5개를 기본으로 한다.
- 한 번의 조립에서 사용하는 전체 Static Mesh Component는 일반적으로 4~6개다.
- Part는 Family 안에서 여러 Template이 재사용할 수 있다.
- Core Mesh는 승인된 Socket을 소유한다.
- Part Pivot은 연결 지점을 기준으로 제작한다.
- Runtime Mesh Cutting, Geometry Collection, 자유 물리 조립을 사용하지 않는다.
- v1.0 조립은 Socket Snap과 승인된 회전 단계만 사용한다.
- Scale 자유 조절은 v1.0에서 사용하지 않는다.

## Owner-only Assembly Mode

- Object Assembly Widget은 Owning Player에게만 표시한다.
- Assembly 중 World View는 완전히 가린다.
- Move, Look, Jump, Sprint, Throw, QuickSlot, Inventory와 다른 Interaction을 차단한다.
- Part 선택, Socket 선택, 승인된 회전, Submit, Cancel, Push-To-Talk와 Pause를 허용한다.
- 조립 화면은 로컬 Preview Component를 사용하며 World Actor를 직접 변경하지 않는다.
- Session 종료 시 Gameplay Input Mode와 HUD 접근을 Surface Forgery와 동일한 원칙으로 복원한다.

## Server Authority And Payload

- 서버가 Object Assembly Session Owner, Revision, End Server Time과 선택 Template을 확정한다.
- Client는 Part Mesh 또는 임의 Transform을 전송하지 않는다.
- Client 최종 Payload는 승인된 `PartId`, `SocketId`, Quantized Orientation과 Material Id만 포함한다.
- 서버는 Session Revision, Part Count, 중복 Part, Part Family, Socket 호환, Orientation 범위, Material 범위와 Payload 크기를 검증한다.
- 중간 Drag 또는 Preview Transform은 복제하지 않는다.
- Submit 시 검증된 최종 Assembly Payload를 한 번만 확정한다.

## Deterministic Score

Object Assembly는 OpenCV를 사용하지 않는다.

```text
Required Part Match
→ Socket / Topology Match
→ Orientation Accuracy
→ Material Match
→ Completeness
→ Extra Part Score Cap
→ Final Quality Score
```

기본 Weight:

```text
Required Part Match: 35%
Socket / Topology Match: 30%
Orientation Accuracy: 25%
Material Match: 10%
```

- 누락 Part는 Completeness를 낮춘다.
- 불필요한 Part를 과도하게 추가하면 Final Score Cap을 적용한다.
- Client Preview는 확정값이 아니다.
- 최종 Quality Score는 서버만 확정한다.
- Guard Inspection은 서버가 확정한 0~100 Quality Score만 공통 입력으로 사용한다.

## Replica Data

- Object Assembly Replica는 Surface Forgery의 Palette Raster를 사용하지 않는다.
- Object Assembly Replica는 승인된 Part Id, Socket Id, Quantized Orientation, Material Id와 Revision만 복제한다.
- Client는 로컬 Template Data에서 Static Mesh와 Material을 해석해 Replica Component를 재구성한다.
- 전체 Mesh Data, Render Target, Physics State 또는 Preview Actor를 복제하지 않는다.
- 늦게 참가한 Client도 동일한 Assembly Replica를 재구성해야 한다.

## Cleanup

Session 종료, Cancel, Timeout, Arrest, Disconnect, Match End, Owner EndPlay 또는 Display Case EndPlay에서 다음을 정리한다.

- Object Display Case Lock
- Assembly Owner
- Assembly Session Revision / Timer
- Local Preview Components
- Assembly Widget
- Movement / Look / Interaction Block
- Cursor / Mouse Capture / Input Mapping Context
- QuickSlot / Inventory / HUD 접근

---

# 11. SoundPing Rules

현재 SoundPing 시스템은 Guard가 서버에서 소음에 반응하기 위한 Gameplay Event로 사용한다.

- Footstep
- Glass Break
- Coin Impact
- 현재 기획에 포함된 환경 소음

현재 제거된 SoundPing:

- Noise Trap
- Trap Trigger
- Smoke 관련 SoundPing

다음 값은 사용하지 않는다.

```text
EHeistSoundPingType::NoiseTrap
Event.SoundPing.NoiseTrap
AI.Stimulus.Trap
Ping_NoiseTrap
```

Guard Noise Reaction은 현재 활성 SoundPing Type만 처리한다.

Player-facing SoundPing Marker, Direction Widget 및 HUD Layer는 사용하지 않는다.

플레이어는 1인칭 공간 음향과 실제 Sound Cue에 의존해 소리 방향을 판단한다.

SoundPing Event는 Client HUD 표시를 위해 복제하지 않는다.

StunHit은 현재 PvE 기획에서 실제 사용 여부를 별도 점검한다. 미사용이 확정되면 관련 Enum, Tag, Data Row, UI 분기를 제거한다.

---

# 12. GameplayTag Rules

현재 GameplayTag는 실제 Runtime Rule, DataTable, UI 또는 AI에서 사용하는 값만 등록한다.

제거 대상 Tag:

```text
State.InSmoke
Action.PlacingTrap
Event.Trap.Placed
Event.Trap.Triggered
Event.SoundPing.NoiseTrap
AI.Stimulus.Trap
Item.Trap
Item.Trap.Glue
Item.Trap.Noise
Item.Throwable.Smoke
```

삭제 기능의 Tag를 향후 호환성 용도로 남기지 않는다.

GameplayTag를 삭제하기 전에 관련 DataTable, Blueprint, Config 참조를 확인한다.

---

# 13. Input Mode Rules

입력 모드는 상호 배타적으로 관리한다.

```text
Gameplay
Inventory
Forgery
```

Context 전환 시:

1. 기존 Context를 명시적으로 제거한다.
2. 새 Context를 추가한다.
3. 중복 Widget을 생성하지 않는다.
4. Input Context를 누적하지 않는다.
5. Cursor와 Mouse Capture를 현재 Mode에 맞게 설정한다.
6. 종료 시 이전 Gameplay Context를 복원한다.

---

# 14. C++ / Blueprint / Data / Map Responsibility

| 영역 | 책임 |
|---|---|
| C++ | Rule, State, Authority, Validation, Replication, Stable API |
| Blueprint | Mesh, Material, Camera Position, Component Assembly, Visual Hook |
| Widget Blueprint | Layout, Binding, Animation, Presentation |
| ViewModel / C++ Widget | UI State Exposure, Request Routing |
| DataTable / DataAsset | Artifact, Template, Guard, Balance, Scaling Data |
| Map | Painting/Sculpture Case, Guard Route, Loot, Exit, Lighting, Navigation |

## UI Copy Rules

- v1.0의 Source UI Copy는 영어를 사용한다.
- 플레이어에게 상태를 전달하는 문구는 대상, 현재 상태, 결과 또는 필요한 행동을 알 수 있는 문장형으로 작성한다.
- `LOCKDOWN`처럼 의미가 모호할 수 있는 단독 상태명 대신 `THE MUSEUM WILL ENTER LOCKDOWN IN {0}.`처럼 게임 내 대상을 명시한다.
- Forgery 제출 제한 시간과 Museum Lockdown 제한 시간은 서로 다른 문장으로 구분한다.
- Raw Enum, Data Row ID, Blueprint Class Name을 그대로 플레이어에게 노출하지 않는다.
- 화면 제목, 버튼 동사, 키 라벨, 수량처럼 문맥이 이미 분명한 짧은 UI Label은 간결하게 유지할 수 있다.
- `NSLOCTEXT` Key는 이후 영어 → 한국어 현지화 패치를 위해 안정적으로 유지한다.

## Blueprint Graph 금지 항목

- Forgery Score 계산
- Original 확정
- Replica 확정
- Alert 변경
- Lockdown 변경
- Extraction 성공 판정
- Team Result 확정
- 신규 Server RPC
- Replicated Gameplay State 직접 변경
- Inventory 확정 Mutation
- QuickSlot 확정 Mutation

---

# 15. Blueprint And Asset Cleanup Rules

C++ 타입을 삭제한 경우 다음 순서로 Asset 정리를 완료한다.

1. 관련 C++ 참조 제거
2. Development Editor Build
3. 관련 Blueprint 열기
4. `Refresh All Nodes`
5. 삭제된 Enum Pin과 Class Pin 제거
6. Blueprint Compile
7. Blueprint Save
8. JSON 기반 DataTable은 Import JSON 수정 후 재Import
9. 삭제 Class Reference Viewer 확인
10. Fix Up Redirectors
11. PIE 실행
12. Missing Class Log 확인
13. Invalid Enum Log 확인
14. Failed Import Log 확인

Smoke 및 Trap 관련 Blueprint와 C++ Class는 신규 Asset의 부모 또는 DataTable Class Reference로 사용하지 않는다.

`DataTableImports/*.json`을 Import Source로 사용하는 DataTable은 JSON을 Source of Truth로 취급한다.

이 DataTable의 Row를 정리할 때는 `.uasset`에서 직접 삭제하지 않고 JSON을 수정한 뒤 Unreal Editor에서 Reimport한다.

---

# 16. Numbered Task Boundary

- 활성 `TASK-Wn-###`만 구현한다.
- PvE 피벗 이후에도 기존 프로젝트 주차 번호를 연속 사용한다.
- 별도 `F` Task 체계를 만들지 않는다.
- Task 시작 전 관련 문서와 Manifest 상태를 확인한다.
- Editor 작업이 필요하면 사용자용 Blueprint/Data/Map 절차를 현재 대화에서 제공한다.
- 사용자가 명시적으로 요청하지 않는 한 별도 작업용 `.md` 파일을 추가하지 않는다.
- C++ Build만 성공했다고 Editor 작업 포함 Task를 완료 처리하지 않는다.
- 멀티플레이, Ownership, Replication 주장은 사용자 PIE 증거가 있을 때만 PASS 처리한다.

## Codex 담당

- C++ Gameplay Rule 구현
- C++ Authority 구현
- C++ Validation 구현
- C++ Replication 구현
- Repository 코드 분석
- Config 분석
- Data Import JSON 분석
- 승인된 범위의 코드 수정
- 사용자가 제출한 Build 오류 수정
- Task 판정용 최소 Debug Log 추가
- Debug 또는 Cheat Command 구현
- 사용자가 제출한 PIE Log 기반 PASS / FAIL / BLOCKED 판정

## 사용자 Unreal Editor 담당

- Blueprint 구성
- Widget Blueprint 구성
- DataTable 편집
- DataAsset 편집
- Map 배치
- Scale
- Collision
- Lighting
- Navigation
- Asset Assignment
- Component Assembly
- Development Editor Build
- Build Log 제출
- Blueprint Compile
- Save
- PIE 실행
- Debug Command 실행
- Output Log 제출

Codex는 Unreal C++ Build를 직접 실행하지 않는다.

사용자가 Build를 실행하고 오류가 발생하면 전체 오류 위치와 메시지를 전달한다. Codex는 해당 로그를 근거로 코드를 수정한다.

Codex는 사용자가 명시적으로 요청하지 않는 한 다음을 수행하지 않는다.

- Unreal Editor 실행
- Unreal Editor 종료
- Unreal Editor UI 직접 조작
- Unreal MCP 연결
- Unreal MCP 재연결
- Unreal MCP 복구
- `.uasset` 직접 수정
- `.umap` 직접 수정

## Editor 작업 절차 형식

Editor 작업 안내에는 다음만 포함한다.

- 열 Asset 또는 Map
- 선택할 Actor 또는 Component
- 변경할 Property와 값
- Compile / Save 순서
- PIE Mode
- Player 수
- 실행할 Debug Command
- 제출할 Output Log

---

# 17. Runtime Test And Log Handoff

- Runtime Task는 기존 `UHeistDebugFunctionLibrary`와 `UHeistCheatManager`를 우선 사용한다.
- 완료 판정용 로그가 부족하면 활성 Task 범위 안에서 최소 Debug Log를 추가한다.
- 사용자는 Unreal Editor PIE에서 Debug Command를 실행한다.
- PIE의 Disconnect, Session Cleanup, Owner EndPlay 또는 재접속 연속성 검증에서 Client 콘솔 `disconnect`를 사용하지 않는다.
- PIE Client를 종료하기 위해 `ESC`를 사용하지 않는다.
- 위 검증에서 원격 Client 연결 종료가 필요하면 Listen Server가 서버 권한 `KickPlayer` Debug Command로 대상 Player를 제거한다.
- 현재 공용 Kick 경로는 `HeistObjectAssemblyKickPlayer <PlayerId>`이며, 이름과 관계없이 `AGameSession::KickPlayer()`를 호출하는 서버 권한 진단 명령으로 사용한다.
- 테스트 안내에서 `disconnect`가 필요한 것처럼 보이는 경우에도 항상 위 Listen Server Kick 절차로 대체한다.
- 사용자는 관련 Output Log를 제출한다.
- 화면 동작이 완료 조건이면 관찰 결과도 제출한다.
- 제출된 로그와 관찰 결과를 Task 완료 조건에 대조한다.
- 결과는 `PASS`, `FAIL`, `BLOCKED`로 판정한다.
- Build 성공만으로 Runtime Task를 PASS 처리하지 않는다.
- 개별 Task PASS와 Weekly Gate PASS를 분리한다.
- Formal Test PASS를 별도로 관리한다.

## Debug Logging Policy

- Gameplay Class와 Component는 진단 및 테스트 목적으로 `UE_LOG`를 직접 호출하지 않는다.
- 진단 로그는 사건 이름이 드러나는 `UHeistDebugFunctionLibrary::Debug...` 함수로 기록한다.
- 로그 Category, Severity, 문장, 필드 순서와 `Result` Schema는 `UHeistDebugFunctionLibrary`가 소유한다.
- 호출부에 Format String을 남기는 범용 Logging Macro를 중앙화의 대체 수단으로 사용하지 않는다.
- 호출부는 Actor, Component, `FName`, 수치와 Boolean 같은 원시 Context만 전달한다.
- 문자열 조립, Soft Object Path 변환, 배열 요약과 Enum 문자열 변환은 Debug 함수의 Shipping Guard 내부에서 수행한다.
- 현재 Target의 기본 Shipping 설정에서는 `Fatal`이 아닌 `UE_LOG`가 자동 제거되므로, 단순 출력 차단만을 위한 호출부 `#if !UE_BUILD_SHIPPING`은 추가하지 않는다.
- Actor 탐색, 배열 순회, 화면 출력 또는 기타 Debug 전용 연산이 있는 함수는 `#if UE_BUILD_SHIPPING` 조기 반환 또는 동등한 Compile Guard를 유지한다.
- Runtime State를 변경하는 Debug/Cheat 함수는 로그 설정과 무관하게 Shipping에서 컴파일 경로가 활성화되지 않도록 명시적으로 Guard한다.
- 실제 Shipping 운영 로그가 필요해질 경우 Debug Log와 섞지 않고 별도 정책과 Task를 먼저 정의한다.

---

# 18. Verification Standard

각 Task 결과는 다음을 구분한다.

- `Implementation Complete`
- `Blueprint/Data/Map Pending`
- `User PIE Pending`
- `PASS`
- `FAIL`
- `BLOCKED`

PIE가 필요한 Task는 다음을 명시한다.

- PIE Mode
- Player 수
- 실행할 Window
- 입력
- Debug Command
- 기대 화면 동작
- 기대 Log
- PASS 신호
- FAIL 신호
- Task Test 또는 Weekly Gate 구분

Known Warning은 숨기지 않는다.

다음 문제는 Weekly Gate를 차단한다.

- Critical Replication
- Ownership 위반
- Duplicate Artifact
- Input Restore 실패
- Orphan Session Lock
- Missing Parent Class
- Invalid DataTable Enum
- Removed Class Soft Reference
- Forgery Score NaN
- Client-side Authoritative Mutation

---

# 19. Legacy Preservation

다음 구현은 아직 참조 감사가 끝나지 않은 경우에만 Legacy로 보존할 수 있다.

- Top-Down Camera 관련 Blueprint
- Cursor Aim 관련 입력

새 흐름에서는 호출하지 않는다.

Reference Viewer와 회귀 확인 후 별도 Cleanup Task에서 제거한다.

기존 검증 기록은 재사용 가능한 Regression Baseline으로 보존할 수 있다.

다음 기능은 Legacy Preservation 대상이 아니다.

- Smoke Grenade
- Smoke Projectile
- Smoke Cloud
- Glue Trap
- Noise Trap
- Trap Placement Cast
- Smoke / Trap QuickSlot
- Trap 전용 GameplayTag
- NoiseTrap SoundPing

---

# 20. Current Phase

W3는 완료됐다.

현재 실행 기준은 W5이며 기간은 2026-08-17부터 2026-08-30까지다.

W4는 `TASK-W4-001~020`까지 완료됐고 아래 W4 범위와 규칙은 완료 이력 및 회귀 기준으로 유지한다.

## W3 Closeout

- `TASK-W3-001~015`, `TASK-W3-017~028`: 완료
- `TASK-W3-016`: 취소
- Smoke Gameplay는 프로젝트에서 물리적으로 제거됐다.
- Glue Trap Gameplay는 프로젝트에서 물리적으로 제거됐다.
- Noise Trap Gameplay는 프로젝트에서 물리적으로 제거됐다.
- Trap Placement Cast는 프로젝트에서 제거됐다.
- QuickSlot은 Coin 전용으로 축소됐다.
- `TASK-W3-029`: 앞선 개별 검증과 중복되는 통합 Gate였으므로 제거
- 테스트 로그는 Task 번호와 독립된 연속 번호를 사용
- 기존 `TEST-W3-001~027`은 과거 검증 기록으로 유지

Smoke와 Trap은 W4 이후의 Gate, Regression Baseline, Stretch 또는 Deferred Scope로 사용하지 않는다.

## W4 Scope

### Forgery Vertical Slice

1. `TASK-W4-001` Forgery Session Lifecycle
2. `TASK-W4-002` Owner-only Full-Screen Forgery UI
3. `TASK-W4-003` Forgery Input Mode / Restore
4. `TASK-W4-004` Reference Template Load / Observation Handoff
5. `TASK-W4-005` Drawing Canvas / Stroke Collection
6. `TASK-W4-006` Stroke Transport / Server Validation
7. `TASK-W4-007` OpenCV Reference Similarity Forgery Score
8. `TASK-W4-008` Replica Placement / Original Removal
9. `TASK-W4-009` Submitted Replica World Visual
10. `TASK-W4-010` Forgery Recovery Edge Cases
11. `TASK-W4-011` Painting Frame / Submitted Texture Projection

### Detection / Alert / Lockdown

12. `TASK-W4-012` Inspection Target Registration
13. `TASK-W4-013` Guard InspectExhibit State
14. `TASK-W4-014` Score → Inspection Delay Mapping
15. `TASK-W4-015` Global Alert State Replication
16. `TASK-W4-016` Alert-driven Guard Modifiers
17. `TASK-W4-017` Lockdown Countdown / World Restriction
18. `TASK-W4-018` Alert HUD / Audio Layers
19. `TASK-W4-019` Duplicate Inspection / Timer Protection
20. `TASK-W4-020` Low / Medium / High Guard Profiles / Security Level Indicator

## W4 Execution Rules

- 활성 구현 범위는 `TASK-W4-001~020`이다.
- Forgery Session은 서버 권한을 유지한다.
- Forgery Score는 서버 권한을 유지한다.
- Replica 확정은 서버 권한을 유지한다.
- Original 확정은 서버 권한을 유지한다.
- Alert는 서버 권한을 유지한다.
- Lockdown은 서버 권한을 유지한다.
- 최종 위조 그림은 서버 Score용 Palette Raster에서 생성한다.
- 고정 해상도 Palette Index Data는 제출 시 한 번만 복제한다.
- 각 Client는 복제된 Index Data로 Transient Texture를 재구성한다.
- Render Target을 World Visual 목적으로 복제하지 않는다.
- 전체 Stroke Payload를 World Visual 목적으로 추가 복제하지 않는다.
- Owner-only UI는 Critical Gate다.
- Input Restore는 Critical Gate다.
- Case Lock Cleanup은 Critical Gate다.
- Duplicate 방지는 Critical Gate다.
- Timer 정리는 Critical Gate다.
- 멀티플레이, Ownership, Replication, Recovery 주장은 사용자 PIE와 Debug Log가 있을 때만 PASS 처리한다.
- 이미 증명된 동일 흐름을 같은 조건으로 반복하는 중복 Gate를 만들지 않는다.
- Guard Alert Profile은 `DT_GuardData`의 `Guard_Alert_Low / Medium / High` Row를 사용한다.
- 위치명 기반 `Guard_Default / Guard_Vault / Guard_SecurityRoom` Row는 사용하지 않는다.
- 신규 Guard의 기본 `GuardProfileId`는 `Guard_Alert_Medium`이다.
- 실제 맵의 Guard별 등급 배정, Patrol 영역, 공간 압박과 최종 수치 조정은 W8 Level Design / Map Balance에서 수행한다.

## W4 Current Handoff

- `TASK-W4-001~020`: 완료
- 정식 완료 상태와 테스트 로그 번호는 Notion 기록을 Source of Truth로 사용
- Low / Medium / High Guard Profile Data와 Security Level Indicator 구현 및 2 Player Listen Server PIE 검증 완료
- SandboxMap에서 세 등급 Guard의 독립 Patrol Route, Alert 반응과 Client Security Level 복제를 확인
- 실제 맵별 Guard 등급 배치와 Patrol 공간 조정은 W8 Level Design / Map Balance 범위
- W4 개별 Task는 완료됐으며 Weekly Gate와 Formal Test는 별도 판정
- OpenCV 유사도 판정 구현 완료
- 제한 Palette 구현 완료
- Local Preview와 Server Final 공통 Evaluator 구현 완료
- 제출 그림 Palette Index Data 복제 구현 완료
- Painting과 Sculpture Case C++ 타입 분리 완료
- `BP_DisplayCase` 부모는 `AHeistPaintingDisplayCaseActor`
- `BP_SculptureDisplayCase` 부모는 `AHeistSculptureDisplayCaseActor`
- Sculpture Case는 시각 Shell만 존재
- Sculpture Case는 v1.0 Gameplay에 사용하지 않음
- `TASK-W4-015~017`은 Global Alert Replication, Guard Modifier, Lockdown Countdown / World Restriction 검증 완료
- `TASK-W4-018~019`는 Alert HUD / Audio Layer와 Duplicate Inspection / Timer Protection 검증 완료

## Post-W4 Compressed Roadmap

### W5 — 2026-08-17 ~ 2026-08-30

- Steam Online Subsystem
- Session
- Travel
- Packaging
- Surface Forgery Template 36개
- Surface Template Pool / Shuffle Bag
- Object Assembly Forgery
- Sculpture / Ceramic Modular Kit과 Template
- Loose Loot 콘텐츠
- Tutorial 콘텐츠
- Steam Session을 먼저 진행

### W6 — 2026-08-31 ~ 2026-09-06

- Shared Extraction
- Team Result
- Player Contribution

### W7 — 2026-09-07

고정 Task가 없는 검토 체크포인트다.

W4~W6 결과와 실제 잔여 위험을 검토한 뒤 필요한 경우에만 `TASK-W7-###`을 생성한다.

### W8 — 2026-09-08 ~ 2026-09-20

- 레벨 디자인
- 라이팅
- 이펙트
- 오디오
- HUD Polish
- Result Polish
- Map Balance
- Feature Lock
- RC1 QA
- Final RC
- Public Release 준비

## Post-W4 Execution Priority

- Gameplay 구현 우선
- Authority 구현 우선
- Replication 구현 우선
- Online Session 구현 우선
- Extraction 구현 우선
- Result 구현 우선
- Level Design과 Polish는 W5/W6 필수 구현 이후 진행
- 미래 주차 번호는 `W5~W8`만 사용
- 기존 `W9~W12` 번호는 Rev.10 이전 이력 참조에만 사용
- W7 Task는 사전 생성하지 않음
- Public Release 목표일 `2026-09-20` 유지

## W5 Current Handoff

- `TASK-W5-001~010`은 완료됐다.
- `TEST-W5-001`은 2026-07-26 Editor `OnlineSubsystemNull` 2 Player Listen Server PIE에서 Host / Join Code / Find / Join / Lobby Travel을 PASS했다.
- `TEST-W5-002`는 2026-07-27 Editor `OnlineSubsystemNull` 3 Player Listen Server PIE에서 Client Leave / Rejoin / Empty Slot 재사용 / Host Quit / Title Return / Map Selection / Random / Lobby Roster Refresh를 PASS했다.
- `TEST-W5-005`는 2026-07-27 Development Editor 1 Player PIE에서 Local Settings 저장과 First-Person 적용을 PASS했다.
- `TEST-W5-007`은 2026-07-27 별도 PC·Steam 계정 2개 Development Package에서 Steam Host / Join / Lobby / M02 Travel / End / Lobby Return / Client Leave / Host Leave를 PASS했다.
- W5 Weekly Gate는 후속 필수 콘텐츠 작업 때문에 아직 PASS하지 않았다.
- `UHeistGameInstance`가 Online Subsystem 선택과 Create / Find / Join / Travel 상태를 단독 소유한다.
- Editor PIE는 로컬 다중 인스턴스 검증을 위해 `OnlineSubsystemNull`을 사용한다.
- 비 Editor 실행과 패키지 빌드는 기본 `OnlineSubsystemSteam`을 사용한다.
- 비 Editor 실행은 별도 Title Menu Level에서 시작한다.
- Title Menu는 Host Session, Join Code 입력과 Local Settings를 소유한다.
- Session 생성 또는 참가 성공 시 별도 Lobby Level로 이동한다.
- Lobby는 참가 코드 표시, Player Slot, Map 선택, Ready / Start, Leave만 소유한다.
- Session Leave 또는 Host Quit 시 Title Menu Level로 복귀한다.
- Editor 직접 Gameplay Map PIE는 기존 Gameplay 회귀 검증을 위해 InGame 시작을 유지한다.
- Session은 1~4인 Listen Server, Presence, Lobby, Join In Progress를 사용한다.
- Online Session의 로컬 이름은 PIE가 선점하는 `GameSession`과 분리된 `HeistSession`을 사용한다.
- Host는 혼동 문자를 제외한 6자리 참가 코드를 생성하고 Session Setting에 게시한다.
- Join은 참가 코드, Product Id, Build Unique Id, 공개 슬롯을 검증한 뒤 서버 주소로 이동한다.
- Host는 Lobby에서 `M01`, `M02`, `M03`, `Random`을 선택할 수 있고 선택 결과는 `AHeistGameState`를 통해 모든 Client에 복제한다.
- Lobby Player Id는 현재 `PlayerArray`에서 사용하지 않는 가장 낮은 `1~4` 번호를 할당한다. 퇴장한 Slot은 `EMPTY`가 되고 다음 참가자가 해당 번호를 재사용한다.
- `UHeistLobbyViewModel`은 Player 추가·제거뿐 아니라 각 `AHeistPlayerState`의 Identity 변경에도 반응해 모든 Client의 Slot 표시를 갱신한다.
- `TASK-W5-008`은 FOV, Mouse Sensitivity, Master Volume, Resolution / Window Mode의 로컬 저장과 First-Person 적용을 완료했다.
- PIE New Editor Window가 저장된 Resolution을 Editor 창 크기로 덮어쓰는 경우 Settings 진단은 `DisplayApply=EDITOR_OVERRIDE`로 구분한다.
- `TASK-W5-009 Packaging Pipeline`은 Development / Shipping Package 생성·검증·실행 증거로 완료됐다.
- 2026-07-27 기능 구현 선행 원칙에 따라 `TASK-W5-011~023`을 재번호화했고 기존 `보류` Task는 모두 `미시작`으로 변경했다.
- `TASK-W5-011~015`는 Object Assembly Data / State, Session / Score, Owner-only UI, Replica / Inspection / Cleanup과 Primitive Prototype Gate다.
- `TASK-W5-016`은 Surface Template Pool / Shuffle Bag 서버 기능 Task다.
- `TASK-W5-017~018`은 Sculpture / Ceramic Object Content Pack과 Object Assembly Two-Player Gate다.
- `TASK-W5-019~021`은 M01 / M02 / M03 각 12개, 총 36개 Surface Forgery Template Pack이다.
- `TASK-W5-022~023`은 Shared Loose Loot Content와 Tutorial / Onboarding Flow다.
- 생성된 M01 Surface Reference 후보, Palette 정규화 결과, Mask와 `Tools/Forgery/QuantizeForgeryReference.ps1`은 WIP로 보존한다.
- `TASK-W5-011~015 Object Assembly Vertical Slice`는 Data / State, Session / Payload / Score, Owner-only UI, Replica / Inspection / Cleanup과 Primitive Prototype Gate까지 완료했다.
- `TASK-W5-016 Surface Template Pool / Shuffle Bag`은 `TEST-W5-010`으로 완료했다. 12-slot Shuffle Bag 2회전, 최근 3개 반복 방지, Server/Client Match Selection 복제와 서버 권한 Kick 이후 Snapshot 유지가 PASS했다.
- 현재 활성 작업은 `TASK-W5-019 M01 Painting Template Pack`이다.
- Sculpture와 Ceramic의 통합 Gameplay System 명칭은 `Object Assembly Forgery`다.
- Surface Forgery와 Object Assembly는 Template, State, Payload, Result와 Replica Data를 공유하지 않는다.
- `TASK-W5-010 External Two-PC Online Gate`는 `TEST-W5-007` 증거로 완료됐다.
- `TASK-W5-009`의 Project Version Source of Truth는 `Config/DefaultGame.ini`의 `ProjectVersion`이다.
- Win64 Development / Shipping Package는 `Tools/Packaging/PackageProject.ps1`로 생성한다.
- Package 출력은 `Build/Packages`, Steam Depot 후보는 `Build/SteamCandidate` 아래에 생성한다.
- 프로젝트에서 사용하지 않는 기본 `ChaosCloth` Plugin은 비활성화하며, 그 의존성인 `Buoyancy`, `Water`, `Landmass`의 Editor Content를 Release Cook에 포함하지 않는다.
- `HeistBuildDump`는 Development Package에서 Version, Configuration, Platform, Cooked Runtime, Online Subsystem과 Session Build Id를 검증한다.
- `TASK-W5-009` 증거는 Development Package의 `HeistBuildDump Result=PASS`와 정상 종료, Shipping UAT `BUILD SUCCESSFUL / ExitCode=0`, 깨끗한 Shipping Stage 구성 및 두 Configuration 실행 화면이다.
- Editor Archive Directory를 Development와 Shipping에 재사용해 이전 Runtime Binary 또는 Log가 섞인 폴더는 Steam Depot 후보로 사용하지 않는다.
- `ValidatePackage.ps1`는 Development / Shipping Runtime Binary 혼합을 실패 처리하고 UE 5.8 Prerequisite의 `UEPrereqSetup_x64.exe` 또는 `vc_redist.x64.exe`를 허용한다.
- Steam Depot VDF는 `preview=1` 후보만 생성하며 Upload와 SetLive는 자동 수행하지 않는다.
- Editor `OnlineSubsystemNull` 검증은 구현 검증용이며 Steam 최종 PASS를 대체하지 않는다.
- `TASK-W5-001`의 Steam 패키지 2계정 검증은 `TASK-W5-010 External Two-PC Online Gate`로 이관됐으며 `TASK-W5-001`을 다시 열지 않는다.
- Steam 최종 PASS는 서로 다른 Steam 계정 2개와 Development Package 증거가 있을 때만 처리한다.
- Packaging Pipeline은 `TASK-W5-009`, Steam 2계정 통합 검증은 `TASK-W5-010`에서 수행한다.
- Development 검증 명령은 `HeistSessionHost`, `HeistSessionJoin <Code>`, `HeistSessionLeave`, `HeistSessionMap <M01|M02|M03|Random>`, `HeistSessionStart`, `HeistSessionComplete`, `HeistSessionReturn`, `HeistSessionDump`를 사용한다.
- `HeistSessionComplete`는 Listen Server의 활성 Steam Session과 2명 이상의 Player를 요구하며 현재 Player를 Escaped로 확정하고 Result를 재구성한 뒤 Match Phase를 `End`로 전환한다.
- `HeistSessionDump`는 Player / Identity / Slot / Roster / UI Snapshot과 Map Selection이 일치하고 Gameplay Phase가 `InGame` 또는 `End`일 때 `Result=PASS`를 출력한다.
- Package Client는 로컬 PlayerController `BeginPlay`에서 Session World Ready를 통지해 성공한 `TravelJoin`의 Pending 상태와 30초 감시 타이머를 해제한다.
- `TASK-W5-010` PASS에는 Host / Client 양쪽에서 `Subsystem=STEAM`, 동일 Join Code와 Build Id, `Players=2`, `Roster=PASS`, `Framework=PASS`, `NamedSession=true`, `SessionContinuity=PASS`, Gameplay / End / Lobby `Result=PASS`, Client와 Host의 Leave / Title Return 증거가 필요하다.
- `TEST-W5-007`에서 Client는 `Pending=false`, `Operation=None`, `TravelPending=false`를 유지했고 90초 이상 접속해 기존 30초 `TravelJoin` Timeout이 재발하지 않았다.
- Shipping은 Debug / Cheat Command가 제거되므로 `TASK-W5-010` Formal Test에 사용하지 않는다.
- `aqProf.dll` / VTune 선택적 Profiler 경고와 Title / Lobby 전환 중의 일시적 AI Perception / Recast 경고는 현재 확인된 비차단 Known Warning이다. Crash, Travel 실패 또는 Gameplay Map 회귀가 동반되면 다시 분류한다.

세부 설계와 주차별 Task 정의는 `Museum_Heist_GDD.docx` 최신 Revision과 Notion Task 기록을 함께 확인한다.
