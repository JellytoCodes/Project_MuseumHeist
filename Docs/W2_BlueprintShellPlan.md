# Project_MuseumHeist — W2 Blueprint Shell Plan

## Rev 4: First-Person PvE And Smoke / Trap Removal

기준 문서:

- `AGENTS.md` Rev 8
- `ClassManifest.md` Rev 7
- `Museum_Heist_GDD.docx` 최신 Revision

이 문서는 Blueprint, Widget Blueprint, DataTable 및 Map에서 구성해야 하는 **Asset Shell과 Presentation Contract**를 정리한다.

Gameplay Rule, Authority, Validation, Replication은 C++가 소유한다.

Blueprint는 다음만 담당한다.

- Asset Assignment
- Component Assembly
- Mesh
- Material
- Camera Offset
- Animation
- Audio
- Widget Layout
- Visual Hook

---

# 1. Current Project Direction

Project_MuseumHeist는 Unreal Engine 5.8 기반의 1~4인 온라인 협동 1인칭 잠입·유물 위조 하이스트 게임이다.

현재 Core Loop:

```text
Lobby
→ Infiltration
→ Painting Observation
→ Full-Screen Forgery
→ OpenCV Score
→ Replica / Original Swap
→ Guard Inspection
→ Alert / Lockdown
→ Shared Extraction
```

---

# 2. Scope Removal Notice

기획 변경에 따라 Smoke Grenade와 플레이어 설치형 Trap 기능은 프로젝트 범위에서 제거됐다.

다음 C++ Class와 Blueprint 계획은 더 이상 유효하지 않다.

```text
AHeistSmokeProjectile
AHeistSmokeCloudActor
AHeistTrapActor
AHeistGlueTrapActor
AHeistNoiseTrapActor

BP_HeistSmokeProjectile
BP_HeistSmokeCloud
BP_HeistGlueTrap
BP_HeistNoiseTrap
```

다음 Gameplay 기능도 제거됐다.

```text
Smoke Grenade
Smoke Sight Blocking
Glue Trap
Noise Trap
Trap Placement Cast
Smoke QuickSlot
Trap QuickSlot
NoiseTrap SoundPing
```

이 항목은 다음 상태로 간주하지 않는다.

- Legacy
- Deferred
- Stretch
- Post-v1.0
- Regression Baseline

향후 필요성이 다시 확정되면 신규 기획과 신규 Task로 재설계한다.

---

# 3. Blueprint Responsibility Rules

## C++가 소유

- Gameplay Rule
- Authority
- Validation
- Replication
- Server RPC
- State Machine
- Score 계산
- Inventory 확정 상태
- QuickSlot 확정 상태
- Display Case 확정 상태
- Alert
- Lockdown
- Extraction Result
- Team Result

## Blueprint가 소유

- Mesh Assignment
- Material Assignment
- Component Assembly
- Camera Socket Offset
- Widget Layout
- Animation
- Color
- Icon
- Sound
- VFX
- Presentation Hook

## Blueprint Graph에서 금지

- Forgery Score 계산
- Server Authority 판정
- Original / Replica 확정
- Inventory Mutation
- Alert 변경
- Lockdown 변경
- Extraction 성공 판정
- 신규 Server RPC
- Replicated State 직접 수정

---

# 4. Player Character Blueprint

## 권장 Asset

```text
BP_HeistPlayerCharacter
Parent: AHeistPlayerCharacter
```

## Required Components

C++ 기본 Subobject를 유지한다.

- Capsule Component
- Character Movement
- Full Body Skeletal Mesh
- First-Person Camera
- Inventory Component
- Interaction Component
- Action Component
- Forgery Component
- Vision Component
- Noise Emitter Component
- Status Component
- Tag Component
- Customization Component

Blueprint에서 동일한 Gameplay Component를 중복 생성하지 않는다.

## Camera

- First-Person Camera는 Full Body Mesh Head Bone 또는 Socket에 부착한다.
- Camera Offset은 실제 Character Mesh에 맞게 조정한다.
- 기본 FOV는 90이다.
- 별도 First-Person Arms는 사용하지 않는다.
- Local Head를 자동으로 숨기지 않는다.
- 얼굴 Clipping은 Camera Offset으로 해결한다.
- SpringArm Gameplay Camera를 사용하지 않는다.
- Top-Down Camera를 활성화하지 않는다.

## Mesh

- Owning Player에서도 Full Body Mesh를 유지한다.
- Remote Player에서도 동일한 Character Mesh가 보여야 한다.
- 자연스러운 Shadow가 유지돼야 한다.
- First-Person Camera가 Head 내부로 들어가지 않도록 Socket 위치를 조정한다.

## Compile Check

- Missing Component 없음
- 중복 Camera 없음
- 중복 Gameplay Component 없음
- Parent Class 정상
- Compile
- Save

---

# 5. Player Controller And Input

## Input Modes

```text
Gameplay
Inventory
Forgery
```

## Gameplay Mode

- Mouse Cursor 비활성
- Game Input
- Movement 활성
- Look 활성
- Interaction 활성
- QuickSlot 활성

## Inventory Mode

- Mouse Cursor 활성
- Game And UI 또는 UI 중심 입력
- Look 제한
- Inventory Widget 표시
- Forgery Widget 숨김

## Forgery Mode

- Mouse Cursor 활성
- Forgery Widget 표시
- World View 완전 차단
- Movement 차단
- Look 차단
- Jump 차단
- Sprint 차단
- Coin Throw 차단
- Inventory 차단
- World Interaction 차단
- Draw / Erase / Submit / Cancel 허용

Input Context를 중복 추가하지 않는다.

Mode 전환 시 기존 Context를 명시적으로 제거한다.

---

# 6. HUD Blueprint

## 권장 Asset

```text
WBP_HeistHUD
Parent: UHeistHUDWidget
```

## Required Presentation

- Crosshair
- Interaction Prompt
- Action Progress
- Tool / QuickSlot
- Weight
- Objective
- Player Status
- Alert
- Popup Feedback Layer

## Optional Widget Names

C++ `BindWidgetOptional` 또는 이름 탐색 계약을 유지한다.

```text
ScoreText
ToolText
WeightText
ActionText
ObjectiveText
StatusText
AlertText
InteractionPromptWidget
ActionProgressWidget
CrosshairContainer
CrosshairIdleIndicator
CrosshairFocusIndicator
PopupFeedbackLayer
```

경쟁형 Score UI는 숨기거나 제거한다.

```text
GapTracker
RankText
WinnerText
```

## HUD Action Presentation

현재 Action Progress는 다음 두 Cast만 표시한다.

```text
Observation
Escape
```

표시 예시:

```text
OBSERVING
ESCAPING
```

삭제된 표시:

```text
PLACING TRAP
ACTION PLACING TRAP
MOVE TO CANCEL
TrapPlacement
```

## `BP_RefreshHUDPresentation`

Trap 상태 Parameter를 사용하지 않는다.

현재 HUD Blueprint Event는 C++ 최신 시그니처에 맞춰 Refresh한다.

C++ 시그니처 변경 후:

1. WBP 열기
2. Event Graph 이동
3. `Refresh All Nodes`
4. 삭제 Pin 제거
5. Trap 분기 제거
6. Compile
7. Save

---

# 7. Interaction Prompt Widget

## 권장 Asset

```text
WBP_HeistInteractionPrompt
Parent: UHeistInteractionPromptWidget
```

## Prompt Presentation

- Target Label
- Interaction Key
- Available / Unavailable
- Action Type
- Progress Bar
- Remaining Time
- Cancel Hint
- Observation Reference 표시

## Suggested Bindings

```text
InteractionPromptContainer
ActionProgressContainer
TargetText
KeyText
AvailabilityText
ActionTypeText
ActionProgressBar
ActionRemainingText
CancelHintText
ObservationReferenceContainer
ObservationReferenceText
```

## Action Priority

```text
Observation
→ Escape
→ None
```

Trap Placement 분기는 존재하지 않는다.

## Observation

표시:

```text
OBSERVING
```

취소 Hint 예시:

```text
RELEASE E, MOVE, TAKE DAMAGE OR ARREST TO CANCEL
```

## Escape

표시:

```text
ESCAPING
```

취소 Hint 예시:

```text
MOVE OR TAKE DAMAGE TO CANCEL
```

## Removed

```text
PLACING TRAP
MOVE TO CANCEL
TrapPlacement
```

---

# 8. Inventory Widget

## 권장 Asset

```text
WBP_HeistInventory
Parent: UHeistInventoryWidget
```

## Inventory Contract

- 4×5 Grid
- Slot Widget
- Item Widget
- Drag And Drop
- Rotation
- Valid Placement Preview
- Invalid Placement Preview
- Drop Request
- QuickSlot Assignment
- Server Confirmed Snapshot 표시

Widget은 InventoryComponent를 직접 Mutation하지 않는다.

Widget 요청 흐름:

```text
Widget
→ AHeistPlayerController Request
→ Server RPC
→ UHeistInventoryComponent
→ Replication
→ ViewModel
→ Widget Refresh
```

## Grid

```text
Columns: 4
Rows: 5
```

## Item Presentation

- Display Name
- Icon
- Quantity
- Grid Size
- Rotation
- Drag Preview

DataTable의 삭제된 Trap Row를 표시하는 Widget 분기를 남기지 않는다.

---

# 9. QuickSlot Widget

## 권장 Asset

```text
WBP_HeistQuickSlot
Parent: UHeistQuickSlotWidget
```

## Current Contract

QuickSlot은 Coin 하나만 지원한다.

```text
SlotType: Coin
Input: Q
ItemId: Throwable_Coin
```

표시 상태:

```text
Coin Assigned
Coin Empty
Coin Quantity
```

표시 예시:

```text
Q
COIN
x3
```

## Removed Slots

```text
Smoke Grenade
Glue Trap
Noise Trap
E Key Smoke Slot
R Key Trap Slot
```

Blueprint에서 다음을 제거한다.

- Smoke Enum Switch
- GlueTrap Enum Switch
- Smoke Icon
- Trap Icon
- E Slot Container
- R Slot Container
- Trap Assignment Animation

C++ Enum 변경 후:

1. Blueprint 열기
2. `Refresh All Nodes`
3. 깨진 Enum Pin 제거
4. Coin 분기만 유지
5. Compile
6. Save

---

# 10. Forgery Widget

## 권장 Asset

```text
WBP_HeistForgery
Parent: UHeistForgeryWidget
```

## Full-Screen Contract

- Owning Player에게만 표시
- World View 완전 차단
- 하나의 Widget Instance만 유지
- Session 종료 시 숨김
- Input Mode 복원
- Mouse Capture 복원
- HUD 복원
- Inventory 접근 복원
- QuickSlot 접근 복원

## Required Areas

- Reference Image
- Drawing Canvas
- Palette
- Selected Color
- Brush Preview
- Eraser
- Remaining Time
- Local Preview Score
- Submit
- Cancel
- Server Result

## Layout

Reference Image와 Drawing Canvas는 동일한 정사각형 영역을 사용한다.

권장 표시 크기:

```text
400×400
```

중요한 것은 실제 픽셀 크기보다 두 영역이 동일한 Aspect Ratio와 정규화 좌표계를 사용하는 것이다.

## Palette

- Template DataTable에서 2~8색 로드
- Mouse Click 선택
- Number Key 1~8 선택 가능
- 임의 RGB Picker 사용 금지
- Stroke마다 Palette Index 저장

## Drawing

- Mouse Down으로 Stroke 시작
- Mouse Move로 Point 추가
- Mouse Up으로 Stroke 종료
- Eraser는 별도 Drawing Mode 또는 Background Index 처리
- Payload에는 정규화 좌표 사용
- Local Preview는 Throttle 적용 가능
- Local Preview는 Server RPC를 발생시키지 않음

## Score Presentation

- Preview Score는 참고값
- Final Score는 Server Result만 표시
- Client 계산값을 서버 확정값처럼 표시하지 않음

---

# 11. Painting Display Case Blueprint

## 권장 Asset

```text
BP_DisplayCase
Parent: AHeistPaintingDisplayCaseActor
```

## Components

권장 구성:

- Root
- Interaction Collision
- Frame Mesh
- Original Plane
- Replica Plane
- Optional Glass Mesh
- Optional Highlight Mesh
- Optional Audio Component

## Original Plane

- Reference Painting Texture 표시
- 초기 상태에서 Visible
- Replica 배치 이후 State에 맞게 숨김
- Original이 World Actor로 따로 Spawn되지 않더라도 상호작용 상태는 C++ State로 관리

## Replica Plane

- 초기 Hidden
- Replica 확정 이후 Visible
- Dynamic Material 사용
- Material Parameter:

```text
PaintingTexture
```

## UV

Original Plane과 Replica Plane의 UV는 0~1 정규화 상태여야 한다.

제출된 Runtime Texture가 왜곡되지 않아야 한다.

## Material

권장 Material:

```text
M_HeistPaintingReplica
```

Parameter:

```text
PaintingTexture
```

필요하면 추가 Parameter:

```text
Score
Coverage
ColorAccuracy
Tier
```

C++ Custom Primitive Data와 Material Parameter 계약이 중복되지 않도록 한다.

## State Visual

C++가 State를 확정한다.

Blueprint는 State 변경 시 Visual만 적용한다.

예:

```text
Secured
- Original Visible
- Replica Hidden

ReplicaPlaced
- Original Hidden
- Replica Visible

OriginalRemoved
- Original Hidden
- Replica Visible

Suspected
- Replica Visible
- 경고 Visual 가능

Alarmed
- 경고 Light 또는 Material 가능
```

Blueprint에서 State를 직접 변경하지 않는다.

---

# 12. Sculpture Display Case Blueprint

## 권장 Asset

```text
BP_SculptureDisplayCase
Parent: AHeistSculptureDisplayCaseActor
```

현재는 Visual Shell만 담당한다.

허용:

- Interaction Collision 구성
- Display Mesh
- Pedestal Mesh
- Glass Mesh
- Material
- Lighting
- Scale

금지:

- Painting Forgery Graph 참조
- `FHeistReplicaPaintingData` 참조
- Painting Display Case State 참조
- Drawing Widget 호출
- Replica Texture 적용
- Original Carry Gameplay
- Gameplay Interaction 활성화

Sculpture Assembly가 별도 Stretch Gate를 통과하기 전에는 Gameplay를 추가하지 않는다.

---

# 13. Loot Actor Blueprint

## 권장 Asset

```text
BP_HeistLoot
Parent: AHeistLootActor
```

또는 Loot 종류별 Visual Child Blueprint를 사용할 수 있다.

Gameplay Rule은 C++와 DataTable이 소유한다.

Blueprint는 다음을 담당한다.

- Mesh
- Material
- Collision Shape
- Pickup Visual
- Highlight Visual
- Optional Audio

Painting마다 별도 Loot Gameplay Class를 만들지 않는다.

---

# 14. Coin Projectile Blueprint

## 권장 Asset

```text
BP_HeistCoinProjectile
Parent: AHeistCoinProjectile
```

## Blueprint Responsibility

- Coin Mesh
- Collision
- Trail
- Impact Effect
- Impact Sound
- Visual Rotation

## C++ Responsibility

- Server Spawn
- Owner
- Instigator
- Launch Direction
- Projectile Speed
- Impact Validation
- SoundPing Report
- Guard Distraction
- Lifetime

현재 Coin은 유일한 QuickSlot Gameplay Item이다.

---

# 15. Vent Blueprint

## 권장 Asset

```text
BP_HeistVent
Parent: AHeistVentActor
```

## Components

- Interaction Collision
- Vent Mesh
- Optional Door Mesh
- Optional Indicator Light
- Optional Audio

## State Presentation

- Locked
- Active
- Casting
- Used

C++가 Escape 가능 여부와 성공을 확정한다.

Blueprint는 Visual과 Audio만 처리한다.

---

# 16. Guard Blueprint

## 권장 Asset

```text
BP_HeistGuard
Parent: AHeistGuardCharacter
```

## Components

- Character Mesh
- Collision
- AI Perception 또는 C++ 구성 요소
- Guard State Component
- Noise Reaction Component
- Patrol Path Component
- Optional Indicator Widget
- Optional Audio

## Blueprint Responsibility

- Mesh
- Animation Blueprint
- Material
- Audio
- Visual Indicator
- StateTree Asset Assignment

## C++ Responsibility

- State
- Authority
- Detection
- Noise Reaction
- Candidate Priority
- Inspection Target
- Alert Modifier
- Movement Rule
- StateTree Event

## StateTree

StateTree Editor 연결은 사용자가 담당한다.

주요 State:

```text
Patrol
InvestigateNoise
ChasePlayer
SearchLastKnownLocation
ReturnToPatrol
InspectExhibit
```

NoiseTrap 분기를 만들지 않는다.

---

# 17. Guard Waypoint And Patrol

## 권장 Asset

```text
BP_HeistGuardWaypoint
Parent: AHeistGuardWaypoint
```

Map에서 Patrol Route를 구성한다.

확인:

- Route ID
- 순서
- 연결
- Navigation
- Guard Spawn과 접근 가능성

Patrol Path Gameplay Rule은 C++가 소유한다.

---

# 18. SoundPing UI Removal

## 권장 Asset

```text
WBP_SoundPingMarker
Parent C++ Class removed. Delete this asset in Unreal Editor.
```

## Required Presentation

- Player-facing direction marker 없음
- SoundPing HUD Layer 없음
- Guard의 서버 소음 반응과 실제 공간 음향만 유지

현재 표시 가능한 Type:

```text
Player-facing SoundPing Type 없음
```

삭제된 표시:

```text
NOISE TRAP
```

`Ping_NoiseTrap` Data Row와 UI 분기를 제거한다.

---

# 19. Popup Feedback Widget

## 권장 Asset

```text
WBP_HeistPopupFeedback
Parent: UHeistUserWidgetBase
```

사용 예:

- Inventory Full
- Too Far Away
- Action Blocked
- Invalid Placement
- QuickSlot Empty
- Escape Not Available
- Loot Request Rejected

삭제 기능 관련 문구를 추가하지 않는다.

```text
Cannot Place Trap
Trap Cooldown
Smoke Unavailable
```

---

# 20. Result Widget

## 권장 Asset

```text
WBP_HeistResult
Parent: UHeistResultWidget
```

W6 기준으로 다음 정보를 표시한다.

## Team Result

- Mission Success
- Partial Success
- Failure
- Target Artifact Value
- Loose Loot Value
- Average Forgery Score
- Final Alert Level
- Extracted Player Count
- Arrested Player Count
- Final Team Reward

## Player Contribution

- Player ID
- Loose Loot Value
- Forgeries Completed
- Best Forgery Score
- Guards Distracted
- Teammates Rescued
- Alarms Triggered
- Escaped
- Arrested

개인 경쟁 Rank나 Winner를 표시하지 않는다.

---

# 21. DataTable Import Contract

## `DT_ItemDataRow`

허용 예시:

```text
Loot_RoyalCrown
Loot_Painting
Loot_AncientSword
Throwable_Coin
```

삭제 Row:

```text
Trap_Glue
Trap_Noise
Throwable_Smoke
```

## `DT_UsableItemDataRow`

현재 활성 Row:

```text
Throwable_Coin
```

삭제 Row:

```text
Trap_Glue
Trap_Noise
Throwable_Smoke
```

삭제 Class Reference:

```text
/Game/Blueprints/World/Actors/Trap/BP_HeistGlueTrap
/Script/Project_MuseumHeist.HeistNoiseTrapActor
/Game/Blueprints/World/Actors/Projectile/BP_HeistSmokeProjectile
```

## `DT_SoundPingDataRow`

UI 전용 `bShowDirectionOnly`, `bAffectsPlayers`, `MarkerIcon` 필드는 제거한다.

삭제 Row:

```text
Ping_NoiseTrap
```

## Unreal Editor DataTable Update

`DataTableImports/*.json`이 Source of Truth다.

JSON에서 Row를 제거한 뒤 실제 `.uasset` DataTable을 Reimport한다.

JSON 기반 DataTable의 Row를 Editor에서 직접 삭제하지 않는다.

순서:

1. `DataTableImports/DT_ItemDataRow.json`에서 삭제 Row가 없는지 확인
2. `DT_ItemData` 열기
3. Reimport
4. Save
5. `DataTableImports/DT_UsableItemDataRow.json`에서 삭제 Row가 없는지 확인
6. `DT_UsableItemData` 열기
7. Reimport
8. Save
9. `DataTableImports/DT_SoundPingDataRow.json`에서 `Ping_NoiseTrap`이 없는지 확인
10. `DT_SoundPingData` 열기
11. Reimport
12. Save
13. Data Validation 실행

---

# 22. GameplayTag Cleanup

다음 GameplayTag를 Blueprint 또는 DataTable에서 사용하지 않는다.

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

정리 순서:

1. C++ Native Tag Field 제거
2. Native Tag 등록 제거
3. DataTable Category Tag 제거
4. Blueprint GameplayTag Variable 확인
5. GameplayTag Query 확인
6. Compile
7. Save
8. PIE Log 확인

---

# 23. Removed Blueprint Asset Cleanup

Smoke 또는 Trap Blueprint가 Content Browser에 남아 있다면 다음 순서로 처리한다.

1. Reference Viewer 실행
2. 참조 Asset 목록 확인
3. DataTable Soft Class Reference 제거
4. Widget Enum 분기 제거
5. Blueprint Variable Type 제거
6. Spawn Class Property 제거
7. Compile
8. Save
9. 삭제 대상 Blueprint 제거
10. Folder에서 Fix Up Redirectors
11. Save All

C++ Parent가 먼저 삭제된 Blueprint는 다음 상태가 될 수 있다.

- Parent Class None
- Missing Class
- Load Error
- Broken Generated Class

Reference Viewer와 Output Log를 반드시 확인한다.

---

# 24. HUD And Widget Migration Checklist

## HUD

- Trap 상태 Pin 없음
- `ACTION PLACING TRAP` 없음
- Smoke 상태 없음
- Coin Tool만 표시
- Observation 표시 정상
- Escape 표시 정상

## Interaction Prompt

- Observation Progress 정상
- Escape Progress 정상
- Trap Progress 없음
- Prompt와 Progress Container Visibility 정상

## QuickSlot

- Coin Slot 1개
- Q Key
- Quantity 표시
- Smoke / Trap Slot 없음

## SoundPing

- Player-facing Marker / Direction Widget 없음
- Footstep / Glass Break / Coin Impact는 Guard 반응과 실제 공간 음향에만 사용
- Noise Trap 없음

---

# 25. Map Placement Contract

Map은 다음을 담당한다.

- Painting Display Case 배치
- Sculpture Visual Shell 배치
- Guard Spawn
- Guard Route
- Loot Spawn
- Vent
- Lighting
- Navigation
- Collision
- Audio Volume

Map에 삭제된 Trap Actor 또는 Smoke Actor를 배치하지 않는다.

검색 대상:

```text
BP_HeistGlueTrap
BP_HeistNoiseTrap
BP_HeistSmokeProjectile
BP_HeistSmokeCloud
```

World Outliner와 Reference Viewer에서 참조를 확인한다.

---

# 26. Build And Compile Sequence

Smoke / Trap 정리 후 권장 순서:

1. Unreal Editor 종료
2. C++ Development Editor Build
3. Unreal Editor 실행
4. Missing Class Log 확인
5. Item DataTable 정리
6. UsableItem DataTable 정리
7. SoundPing DataTable 정리
8. WBP_HeistHUD 열기
9. Refresh All Nodes
10. Compile
11. Save
12. WBP_HeistInteractionPrompt 열기
13. Refresh All Nodes
14. Compile
15. Save
16. WBP_HeistQuickSlot 열기
17. Refresh All Nodes
18. Compile
19. Save
20. 관련 Blueprint 전체 Compile
21. Fix Up Redirectors
22. Save All
23. PIE

---

# 27. PIE Validation

## Mode

```text
PIE
2 Players
Listen Server
Separate Windows 권장
```

## Required Checks

### Startup

- Missing Class 없음
- Invalid Enum 없음
- Failed Import 없음
- Item Data Validation PASS

### Inventory

- Inventory Open
- Inventory Close
- Item Drag
- Item Rotation
- Item Drop
- Coin QuickSlot Assignment

### Coin

- Q 입력
- Coin Projectile Spawn
- Coin Impact
- SoundPing Report
- Guard Investigate

### Observation

- Display Case Target
- Observation 시작
- Progress 표시
- 이동 시 취소
- 완료 시 Forgery 진입

### Forgery

- Owner Client만 Widget 표시
- Remote Client는 Widget 미표시
- Draw
- Palette
- Preview
- Submit
- Server Final
- Replica Texture 표시
- Session Cleanup

### Escape

- Escape Phase 확인
- Vent Interaction
- Escape Progress
- Movement Cancel
- Escape 완료

### Removed Feature Check

- Trap UI 없음
- Trap Actor 없음
- Smoke Actor 없음
- NoiseTrap Marker 없음
- E / R Trap Slot 없음
- `PLACING TRAP` 문구 없음

---

# 28. PASS Criteria

다음 조건을 모두 만족하면 Blueprint/Data Cleanup을 PASS로 본다.

```text
Development Editor Build 성공
Blueprint Compile Error 0
Missing Parent Class 0
Invalid Enum 0
Failed Import 0
Removed Class Reference 0
Item Data Validation PASS
Coin QuickSlot 정상
Coin Throw 정상
Guard Coin Distraction 정상
Observation 정상
Forgery 정상
Escape 정상
Trap UI 없음
Smoke Runtime 참조 없음
NoiseTrap SoundPing 없음
```

---

# 29. Final Active Blueprint Shell List

```text
BP_HeistPlayerCharacter
BP_DisplayCase
BP_SculptureDisplayCase
BP_HeistLoot
BP_HeistCoinProjectile
BP_HeistVent
BP_HeistGuard
BP_HeistGuardWaypoint

WBP_HeistHUD
WBP_HeistInteractionPrompt
WBP_HeistInventory
WBP_HeistInventorySlot
WBP_HeistInventoryItem
WBP_HeistQuickSlot
WBP_HeistForgery
WBP_SoundPingMarker
WBP_HeistPopupFeedback
WBP_HeistResult
WBP_HeistLobby
```

삭제된 Blueprint Shell:

```text
BP_HeistSmokeProjectile
BP_HeistSmokeCloud
BP_HeistGlueTrap
BP_HeistNoiseTrap
```

이 문서의 Active 목록에 없는 Gameplay Blueprint를 신규로 생성하기 전에는 `AGENTS.md`, `ClassManifest.md`, 활성 Task를 먼저 확인한다.
