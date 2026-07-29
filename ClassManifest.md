# Project_MuseumHeist — Class Manifest

## Rev 9: W5 Surface And Object Forgery Contract

Design Reference:

- `AGENTS.md` Rev 10
- `Museum_Heist_GDD.docx` 최신 Revision
- Notion W5 Task / Test 기록

---

# 0. Status Definitions

| 상태 | 의미 |
|---|---|
| **Keep** | 기존 책임을 유지한다. |
| **Modify** | 현재 방향에 맞게 책임 또는 계약을 수정한다. |
| **Add** | 활성 Task 범위 안에서 신규 생성이 허용된다. |
| **Deprecate** | 기존 Asset 또는 직렬화 호환을 위해 임시 유지한다. |
| **Deferred** | 현재 생성하지 않지만 확정된 미래 범위다. |
| **Remove** | 현재 프로젝트에서 제거됐으며 신규 참조를 금지한다. |
| **Excluded** | 현재 프로젝트 방향에 포함하지 않는다. |

Smoke 및 플레이어 설치형 Trap은 `Deferred` 또는 `Legacy`가 아니라 `Remove / Excluded`다.

---

# 1. Core Types

## `Core/HeistTypes.h` — Modify

| 타입 | 상태 | 현재 책임 |
|---|---|---|
| `EHeistMatchPhase` | Modify | Lobby / ReadyCountdown / InGame / End 중심 Match Flow |
| `EHeistInputMode` | Keep | Gameplay / Inventory / Forgery 로컬 입력 상태 |
| `EHeistObjectiveState` | Keep | Objective 진행 상태 |
| `EHeistForgeryType` | Modify | Drawing / Assembly Forgery 타입 |
| `EHeistDisplayCaseState` | Keep | Painting Display Case 상태 |
| `EHeistObjectAssemblyState` | Add | Object Assembly 전용 상태 |
| `EHeistAlertLevel` | Keep | Global Alert / Lockdown |
| `FHeistForgeryResult` | Keep | 서버 확정 Surface Forgery 결과 |
| `FHeistObjectAssemblyEntry` | Add | Part / Socket / Orientation / Material 제출 단위 |
| `FHeistObjectAssemblyResult` | Add | 서버 확정 Object Assembly 결과 |
| `FHeistObjectAssemblyReplicaData` | Add | 확정 Assembly Entry와 Revision 복제 데이터 |
| `FHeistPlayerResult` | Deprecate | 기존 결과 호환용 |
| `FHeistRareLootEventState` | Deferred | Optional Rare Artifact 승인 전 비활성 |
| `EHeistItemType` | Modify | None / Loot / Throwable / KeyItem |
| `EHeistLootGrade` | Keep | Loose Loot 등급 |
| `EHeistUseType` | Modify | None / Throw / DeployArea / Consume |
| `EHeistTargetType` | Keep | 사용 대상 분류 |
| `EHeistSpawnCategory` | Keep | Loose Loot Spawn 분류 |
| `EHeistSoundPingType` | Modify | 활성 환경 소음과 Coin 분류 |
| `EHeistGuardState` | Keep | Patrol / Investigate / Chase / Search / InspectExhibit |
| `EHeistCustomizationType` | Keep | 외형 타입 |
| `EHeistZoneId` | Keep | Zone 식별 |
| `EHeistQuickSlotType` | Modify | None / Coin |

## Removed Enum Values

```text
EHeistItemType::Trap
EHeistUseType::PlaceTrap
EHeistQuickSlotType::SmokeGrenade
EHeistQuickSlotType::GlueTrap
EHeistSoundPingType::NoiseTrap
```

## `FHeistForgeryResult` — Keep

기존 이름을 유지하지만 Surface Forgery 전용 결과다. Object Assembly 상세 지표를 이 Struct에 추가하지 않는다.

필드:

```text
ArtifactId
TemplateId
ForgeryType
SimilarityScore
CoverageScore
MajorShapeScore
ColorAccuracyScore
PaintToReferenceRatio
bAntiFillTriggered
MissingShapePenalty
ExtraStrokePenalty
TimeoutPenalty
CompletionTime
bReplicaPlaced
```

`MissingShapePenalty`, `ExtraStrokePenalty`, `TimeoutPenalty`가 진단용 값인지 Final Score 직접 차감값인지 구현과 문서에서 동일하게 정의한다.

## `FHeistObjectAssemblyEntry` — Add

필드:

```text
PartId
SocketId
QuantizedOrientation
MaterialId
```

Client가 Mesh, 임의 Transform 또는 Physics State를 전송하지 않도록 하는 compact final payload다.

## `FHeistObjectAssemblyResult` — Add

필드:

```text
ArtifactId
TemplateId
QualityScore
RequiredPartScore
SocketTopologyScore
OrientationScore
MaterialScore
Completeness
bExtraPartCapTriggered
CompletionTime
bReplicaPlaced
```

## `FHeistObjectAssemblyReplicaData` — Add

필드:

```text
Entries
Revision
```

서버에서 확정한 compact `FHeistObjectAssemblyEntry` 배열만 복제하며 Mesh, 임의 Transform, Physics State 또는 Preview Actor를 포함하지 않는다.

## `FHeistReplicaPaintingData` — Keep

필드:

```text
Resolution
Palette
PackedPaletteIndices
ScoreRevision
```

## Team Result Types

```text
FHeistTeamResult
FHeistPlayerContribution
```

상태: Add 또는 Modify

현재 W6 범위에서 최종 구현한다.

---

# 2. Core Framework

| 파일 / 클래스 | 상태 | 현재 책임 |
|---|---|---|
| `Core/HeistGameplayTags.*` / `FHeistGameplayTags` | Modify | 실제 활성 Gameplay Tag 등록 |
| `Core/HeistLogChannels.*` | Keep | 로그 채널 |
| `Core/HeistGameMode.*` / `AHeistGameMode` | Modify | Match, Objective, Data Validation, Surface Template Pool Match Initialization, Alert, Result |
| `Core/HeistGameState.*` / `AHeistGameState` | Modify | Replicated Objective / Alert / Team State, Lobby Map Selection / Surface Template Selection / Player Connection Revision, Server-only SoundPing Dispatch |
| `Core/HeistPlayerState.*` / `AHeistPlayerState` | Modify | 1~4 Player Identity, Contribution, Escape, Arrest, Carry Value |
| `Core/HeistPlayerController.*` / `AHeistPlayerController` | Modify | Input Mode, Server RPC, Session Leave / Map Selection Request, Coin Use, Surface Forgery / Object Assembly Request |
| `Core/HeistHUD.*` / `AHeistHUD` | Keep + Extend | Title / Lobby / HUD / Inventory / QuickSlot / Surface Forgery / Object Assembly / Result Widget와 ViewModel 생성 |
| `Core/HeistGameInstance.*` / `UHeistGameInstance` | Modify | Steam/NULL Subsystem 선택, Host/Find/Join/Leave, 6자리 참가 코드, Title/Lobby/Gameplay Travel, Map Selection, Server Surface Template Shuffle Bag, Session Timeout/Cancel/Retry와 Network/Travel Failure 수명주기 |
| `Core/HeistGameUserSettings.*` / `UHeistGameUserSettings` | Add | Local FOV, Mouse Sensitivity, Master Volume, Resolution / Window Mode 저장, 검증 및 적용 |

## Authority

```text
GameMode
- Server only
- Match Rule
- Data Validation
- Objective
- Alert / Lockdown
- Team Result

GameState / PlayerState
- Server Mutation
- Replication
- Client Read

PlayerController
- Local Input
- Server RPC Entry
- Request Routing

HUD
- Local Presentation

GameInstance
- Online Session State Machine
- Title / Lobby / Gameplay Travel
- Session Create / Find / Join / Leave

GameUserSettings
- Local-only Save / Load
- FOV / Mouse Sensitivity / Master Volume Validation
- Resolution / Window Mode Apply
```

## Online Session Runtime Contract

- Online Session 로컬 이름은 `HeistSession`이다.
- Editor PIE는 `OnlineSubsystemNull`, 비 Editor와 Package는 기본 `OnlineSubsystemSteam`을 사용한다.
- Title Menu는 Host, 6자리 Join Code 입력, Local Settings를 담당한다.
- Session 성공 후 `/Game/Maps/LobbyMap`으로 이동한다.
- Lobby는 Join Code, Player Slot, Map Selection, Ready / Start, Leave를 담당한다.
- Host Quit 또는 Leave 완료 후 `/Game/Maps/TitleMenuMap`으로 복귀한다.
- Player Id는 현재 접속자가 사용하지 않는 가장 낮은 `1~4` 번호를 서버가 할당한다.
- PlayerState Identity 복제는 Lobby ViewModel의 Slot Snapshot 갱신을 발생시킨다.
- Host Map Selection은 `M01`, `M02`, `M03`, `Random`이며 `AHeistGameState`가 복제한다.
- Lobby → Gameplay → Lobby Travel의 Session / PlayerState 보존 검증은 `TASK-W5-006` 범위다.
- Steam Package 2계정 최종 검증은 `TASK-W5-010` 범위다.
- `HeistSessionComplete`는 Development 전용 2 Player External Gate 명령이다.
- 이 명령은 Listen Server, 활성 Named Session, 선택 Gameplay Map과 `InGame` Phase를 검증한 뒤 현재 Player를 Escaped로 확정하고 Result를 재구성해 `End` Phase로 전환한다.
- `HeistSessionDump`는 선택 Gameplay Map의 `InGame`과 정상 완료 후 `End`를 모두 유효한 Phase로 판정한다.
- Package Client는 로컬 `AHeistPlayerController::BeginPlay()`에서 `UHeistGameInstance::NotifySessionWorldReady()`를 호출해 완료된 `TravelJoin` 감시 타이머를 해제한다.
- `TASK-W5-010` PASS에는 서로 다른 PC와 Steam 계정 2개, 동일 Development Package, `Subsystem=STEAM`, Host / Joined Session, 동일 Join Code / Build Id, 2 Player Roster, Gameplay / End / Lobby 연속성과 양쪽 Leave 증거가 필요하다.

## Packaging Runtime Contract

- Project Version의 Source of Truth는 `Config/DefaultGame.ini`의 `ProjectVersion`이다.
- Release Cook Map은 `TitleMenuMap`, `LobbyMap`, `M01_ClassicalPrototype`, `M02_MoonlitPrototype`, `M03_GlasshousePrototype`이다.
- `SandBoxMap`은 Release Cook Map에 포함하지 않는다.
- MCP / Toolset / Preview Plugin은 Editor Target에서만 활성화한다.
- 미사용 기본 `ChaosCloth` Plugin은 비활성화해 `Buoyancy → Water → Landmass`의 Editor Content가 Cook에 유입되지 않도록 한다.
- 재현 가능한 Win64 Package Entry는 `Tools/Packaging/PackageProject.ps1`이다.
- Package 출력은 `Build/Packages/MuseumHeist-<Version>-<Configuration>-Win64`을 사용한다.
- Package 완료 시 실행 파일 옆에 `BuildInfo.json`을 생성한다.
- Development Package에만 로컬 실행용 `steam_appid.txt`를 생성할 수 있다.
- Shipping Package와 Steam Depot 후보에는 `steam_appid.txt`를 포함하지 않는다.
- Package 검증은 `Tools/Packaging/ValidatePackage.ps1`가 EXE, Pak, IoStore, Steam, OpenCV, Prerequisite와 BuildInfo를 확인한다.
- UE 5.8 Prerequisite는 설치 엔진 구성에 따라 `UEPrereqSetup_x64.exe` 또는 `vc_redist.x64.exe`를 허용한다.
- Development / Shipping Runtime 실행 파일이 같은 Package Root에 섞이면 검증 실패로 처리한다.
- Steam Depot 후보는 `Tools/Packaging/PrepareSteamDepot.ps1`로 생성한다.
- 생성된 VDF는 `preview=1`, 빈 `setlive`를 유지하며 자동 Upload를 실행하지 않는다.
- 실제 App / Depot Id 확정, Steam Upload와 2계정 검증은 `TASK-W5-010` 범위다.

## GameplayTag Removal

다음 Tag Field 및 Native Tag 등록을 제거한다.

```text
State.InSmoke
Action.PlacingTrap
Event.Trap.Placed
Event.Trap.Triggered
Event.SoundPing.NoiseTrap
AI.Stimulus.Trap
```

DataTable 및 Blueprint에서 다음 Category Tag를 사용하지 않는다.

```text
Item.Trap
Item.Trap.Glue
Item.Trap.Noise
Item.Throwable.Smoke
```

---

# 3. Character

## `Character/HeistPlayerCharacter.*`

`AHeistPlayerCharacter : public ACharacter` — Modify

현재 책임:

- First-Person Camera Component 소유
- Controller Yaw 기반 Rotation
- Head Socket 기반 Full Body First-Person Camera
- Gameplay Component 기본 Subobject 생성
- Input Mode에 따른 Movement / Look 제한
- Forgery Movement Lock 반영
- Full Body Mesh 유지
- Owning Client와 Remote Client 표현 유지

SpringArm Gameplay Camera는 신규 흐름에서 사용하지 않는다.

기존 Top-Down Camera 참조는 별도 Cleanup 전까지 직렬화 호환 여부를 확인한다.

---

# 4. Character Components

Folder:

```text
Source/Project_MuseumHeist/Public/Character/Components
Source/Project_MuseumHeist/Private/Character/Components
```

| 클래스 | 상태 | 현재 책임 |
|---|---|---|
| `UHeistTagComponent` | Keep | Gameplay Tag 상태 |
| `UHeistStatusComponent` | Modify | 일반 Timed Status |
| `UHeistInventoryComponent` | Keep + Extend | Grid, FastArray, Coin QuickSlot, Original Carry |
| `UHeistInteractionComponent` | Modify | Center Screen Trace, Target Filter, Prompt Snapshot |
| `UHeistActionComponent` | Modify | Observation Cast, Escape Cast, Action Lock |
| `UHeistVisionComponent` | Modify | First-Person Flashlight |
| `UHeistCustomizationComponent` | Keep | 외형 |
| `UHeistNoiseEmitterComponent` | Keep | Footstep, Coin, 환경 소음 |
| `UHeistForgeryComponent` | Keep | Session, Stroke, Timeout, Submit, OpenCV Score, Cleanup |

## `UHeistInventoryComponent`

현재 책임:

- 4×5 Inventory Grid
- FastArray Replication
- Server-authoritative Add
- Move
- Rotate
- Remove
- Coin QuickSlot
- Inventory Open State
- Original Carry Entry
- Owner-only Inventory State Replication
- Inventory Changed Delegate

현재 QuickSlot:

```text
None
Coin
```

삭제된 QuickSlot:

```text
SmokeGrenade
GlueTrap
```

## `UHeistActionComponent`

현재 활성 Cast:

```text
Observation Cast
Escape Cast
```

제거된 Cast:

```text
Trap Placement Cast
```

삭제 대상 API:

```text
TryBeginTrapPlacementRequest
IsTrapPlacementCastActive
GetTrapPlacementCastEndServerTime
GetTrapPlacementCastCompletedDelegate
CancelTrapPlacementCast
ClearTrapPlacementCastState
HandleTrapPlacementCastTimerElapsed
HasMovedBeyondTrapPlacementCastTolerance
```

삭제 대상 Delegate:

```text
FHeistTrapPlacementCastCompleted
```

삭제 대상 Forward Declaration:

```text
AHeistTrapActor
```

삭제 대상 상태:

```text
bTrapPlacementCastActive
TrapPlacementCastEndServerTime
PendingTrapItemId
PendingTrapSourceInstanceId
PendingTrapTargetWorldLocation
PendingTrapEffectDurationSeconds
PendingTrapActorClass
bPendingTrapConsumesSourceItem
TrapPlacementCastTimerHandle
TrapPlacementCastStartLocation
```

Observation과 Escape는 상호 배타적으로 시작돼야 한다.

`ClearEscapeCastState()`와 `ClearObservationCastState()`는 다른 활성 Cast가 없을 때만 Component Tick을 끈다.

## `UHeistForgeryComponent`

현재 책임:

- Template Prepare
- Observation Handoff
- Session Begin
- Session Revision
- Owner Validation
- Stroke Payload Validation
- Palette Validation
- Brush Validation
- Reference Cache
- Player Stroke Raster
- OpenCV Metric
- Local Preview
- Server Final
- Replica Painting Data Build
- Display Case Commit
- Timeout
- Cancel
- Disconnect Cleanup
- Arrest Cleanup
- EndPlay Cleanup

별도 Forgery Manager를 만들지 않는다.

`UHeistForgeryComponent`는 Surface Forgery 전용으로 유지한다.

## `UHeistObjectAssemblyComponent` — Add

현재 승인 책임:

- Object Template Prepare
- Owner-only Session Begin / Revision / Timeout
- Part / Socket / Orientation / Material Payload Validation
- Server Deterministic Score
- Local Preview Request Routing
- Object Display Case Commit
- Cancel / Disconnect / Arrest / EndPlay Cleanup

Surface Forgery의 Stroke, Palette, OpenCV Cache와 Replica Painting Data를 소유하지 않는다.

별도 Object Assembly Manager 또는 Subsystem을 만들지 않는다.

---

# 5. Inventory And Data Types

유지 타입:

```text
FHeistInventoryItem
FHeistInventoryFastArrayItem
FHeistReplicatedInventory
FHeistQuickSlotState
FHeistOriginalCarryEntry
FHeistItemDataRow
FHeistLootDataRow
FHeistUsableItemDataRow
FHeistSoundPingDataRow
FHeistGuardDataRow
FHeistLootSpawnRow
FHeistVentDataRow
FHeistCustomizationRow
FHeistUITextRow
```

## Item Contract

현재 지원 Item Type:

```text
Loot
Throwable
KeyItem
```

현재 활성 Throwable:

```text
Throwable_Coin
```

현재 QuickSlot:

```text
Coin
```

삭제 Row:

```text
Trap_Glue
Trap_Noise
Throwable_Smoke
```

## `FHeistItemDataRow`

필드:

```text
ItemId
DisplayName
CategoryTag
ItemType
GridSize
Weight
bCanRotate
bCanUseQuickSlot
bAvailableInV1
Icon
```

## `FHeistLootDataRow`

Loose Loot 확장 데이터로 사용한다.

## `FHeistUsableItemDataRow`

현재 활성 Runtime Use:

```text
Throwable_Coin
UseType=Throw
```

삭제된 Use:

```text
PlaceTrap
```

삭제된 Row에서 삭제된 C++ Class나 Blueprint Class를 참조하면 안 된다.

## Data Validation

`AHeistGameMode::ValidateItemDataTables()`는 다음을 검증한다.

- Item Row Struct
- Loot Row Struct
- Usable Row Struct
- Row Name과 ItemId 일치
- Item Type 유효성
- Grid Size
- Weight
- Loot Extension 정합성
- Throwable Extension 정합성
- Orphan Extension
- Spawned Actor Class

PASS 조건:

```text
InvalidRows=0
OrphanExtensions=0
Result=PASS
```

---

# 6. Artifact And Forgery Data

## `FHeistArtifactDataRow` — Keep

필드:

```text
ArtifactId
DisplayName
ArtifactValue
Weight
GridWidth
GridHeight
ForgeryType
ForgeryTemplateId
MinimumForgeryScore
BaseInspectionDelay
VisualActorClass
```

## `FHeistForgeryTemplateRow` — Keep

필드:

```text
TemplateId
SurfacePoolId
ReferenceImage
ReferenceMask
BackgroundFilterMode
BackgroundColorTolerance
AllowedPalette
ObservationDuration
ForgeryDuration
StrokeLimit
BrushSize
CoverageWeight
MajorShapeWeight
ExtraStrokePenaltyWeight
TimeoutPenalty
ShapeAccuracyWeight
ColorAccuracyWeight
MaximumPaintToReferenceRatio
OverpaintScoreCap
```

## Template Validation

다음을 검증한다.

- TemplateId
- SurfacePoolId
- ReferenceImage
- BackgroundFilterMode
- ReferenceMask 조건
- AllowedPalette 개수
- ObservationDuration
- ForgeryDuration
- StrokeLimit
- BrushSize
- CoverageWeight
- MajorShapeWeight
- ShapeAccuracyWeight
- ColorAccuracyWeight
- MaximumPaintToReferenceRatio
- OverpaintScoreCap

필수 Weight 합:

```text
CoverageWeight + MajorShapeWeight > 0
ShapeAccuracyWeight + ColorAccuracyWeight > 0
```

`ReferenceMask`는 `BackgroundFilterMode == None`일 때 필수다.

`Black / White` Filter에서는 Reference Image에서 Foreground Mask를 생성할 수 있으므로 별도 ReferenceMask를 강제하지 않는다.

## `FHeistObjectAssemblyPartRow` — Add

필드:

```text
PartId
FamilyId
StaticMesh
CompatibleSocketIds
AllowedMaterialIds
AllowedOrientationSteps
```

## `FHeistObjectAssemblyTemplateRow` — Add

필드:

```text
TemplateId
FamilyId
DisplayName
CorePartId
RequiredParts
DecoyPartIds
AssemblyDuration
RequiredPartWeight
SocketTopologyWeight
OrientationWeight
MaterialWeight
ExtraPartScoreCap
```

Sculpture와 Ceramic은 같은 Object Assembly Row 계약을 사용한다.

Part 및 Template DataTable은 Surface Forgery DataTable과 분리한다.

필수 Weight 합:

```text
RequiredPartWeight + SocketTopologyWeight + OrientationWeight + MaterialWeight > 0
```

---

# 7. OpenCV Forgery Contract

UE 5.8 Runtime OpenCV의 다음 모듈을 사용한다.

```text
core
imgproc
quality
```

Stroke 수집과 Palette Raster 생성은 프로젝트 C++ 경로가 소유한다.

OpenCV는 최종 유사도 측정을 담당한다.

## Shape

```text
Binary Foreground Mask
→ 3×3 Morphology Close
→ Reference→Submitted Distance Transform
→ Submitted→Reference Distance Transform
→ Bidirectional Distance Similarity
→ Mask Precision / Recall / Dice / IoU
```

## Color

```text
Palette Raster
→ Foreground Union ROI
→ BGR
→ Lab
→ SSIM
→ Palette Histogram
→ Bhattacharyya Similarity
```

## Final Score Layer

```text
Shape Response Curve
Color Response Curve
Weighted Geometric Mean
Bottleneck
Paint Completeness
Palette Fidelity
Anti-Fill Score Cap
```

Local Preview와 Server Final은 동일한 Evaluator를 사용한다.

서버 결과만 확정값이다.

Render Target 또는 전체 Stroke Payload를 World Visual 목적으로 추가 복제하지 않는다.

---

# 8. World And Interactable

| 클래스 | 상태 | 현재 책임 |
|---|---|---|
| `IHeistInteractable` | Keep | 공통 상호작용 Interface |
| `AHeistInteractableActor` | Keep | 공통 Interactable 기반 |
| `AHeistLootActor` | Keep | Loose Loot |
| `AHeistPaintingDisplayCaseActor` | Modify | Painting Target, Session, Replica, Original, Inspection |
| `AHeistDisplayCaseActor` | Deprecate | 기존 Painting Asset 호환 Alias |
| `AHeistObjectDisplayCaseActor` | Add | Sculpture / Ceramic Object Assembly Session, Replica, Original, Inspection |
| `AHeistSculptureDisplayCaseActor` | Deprecate | 기존 Sculpture Visual Shell Asset 호환 Alias |
| `AHeistVentActor` | Modify | Shared Extraction |
| `AHeistThrowableProjectile` | Keep | Throwable 공통 기반 |
| `AHeistCoinProjectile` | Keep | Guard Distraction |
| `AHeistLootSpawnPoint` | Keep | Loose Loot Spawn |
| `AHeistPlayerStart` | Keep | Spawn |
| `AHeistGuardWaypoint` | Keep | Patrol |

## Removed World Classes

```text
AHeistSmokeProjectile
AHeistSmokeCloudActor
AHeistTrapActor
AHeistGlueTrapActor
AHeistNoiseTrapActor
```

삭제된 클래스의 다음 참조를 남기지 않는다.

- Header Include
- Forward Declaration
- Delegate Parameter
- UPROPERTY Class
- SpawnedActorClass
- DataTable Row
- Blueprint Parent
- Blueprint Variable Type
- GameplayTag
- Debug Function
- Documentation Active Scope

---

# 9. Painting Display Case Contract

`AHeistPaintingDisplayCaseActor`는 다음 책임을 가진다.

- Target Artifact 식별
- Session Lock
- Session Owner
- Observation State
- Forgery State
- Replica Ready
- Replica Placement
- Original Availability
- Original Removal
- Inspection Candidate Registration
- Inspection Result
- Replica Painting Data Replication
- Runtime Texture Reconstruction
- Original / Replica Plane Visibility
- Original Carry Handoff

## State

```text
Secured
Observed
ForgeryInProgress
ReplicaReady
ReplicaPlaced
OriginalAvailable
OriginalRemoved
Inspecting
Completed
Suspected
Alarmed
Failed
```

Surface Forgery와 Object Assembly State를 하나의 enum switch로 통합하지 않는다.

---

# 9A. Object Display Case Contract

`AHeistObjectDisplayCaseActor`는 다음 책임을 가진다.

- Object Family / Target Artifact 식별
- Assembly Session Lock / Owner / Revision / Timeout
- Template 확정
- Replica Ready / Placement
- Original Availability / Removal
- Inspection Candidate 등록과 결과 Handoff
- Compact Assembly Replica Data 복제
- Client Static Mesh Component 재구성
- Disconnect / Arrest / EndPlay Cleanup

## State

```text
Secured
Observed
AssemblyInProgress
ReplicaReady
ReplicaPlaced
OriginalAvailable
OriginalRemoved
Inspecting
Completed
Suspected
Alarmed
Failed
```

Object Assembly Actor는 Painting Palette Raster, Transient Painting Texture 또는 Painting State를 사용하지 않는다.

---

# 10. Replica World Visual Contract

- 서버에서 확정된 `FHeistForgeryResult`를 사용한다.
- 제출 Stroke를 Score와 동일한 Palette Raster로 변환한다.
- Background는 0으로 저장한다.
- Palette 색은 1~8로 저장한다.
- Pixel 두 개를 한 Byte에 4-bit Index로 패킹한다.
- 제출 시 한 번만 복제한다.
- Client는 RepNotify에서 Transient `UTexture2D`를 생성한다.
- Dynamic Material의 `PaintingTexture` Parameter에 적용한다.
- 늦게 참가한 Client도 동일한 그림을 재구성한다.
- Render Target을 복제하지 않는다.
- 전체 Stroke Payload를 World Actor에 복제하지 않는다.

Blueprint는 다음을 담당한다.

- Frame Mesh
- Original Plane
- Replica Plane
- Material
- UV
- Visual Animation

---

# 11. AI

| 클래스 | 상태 | 현재 책임 |
|---|---|---|
| `AHeistGuardCharacter` | Keep + Extend | Guard Pawn |
| `AHeistGuardAIController` | Keep + Extend | StateTree, Inspection Target |
| `FHeistGuardStateTreeTask` | Add | StateTree 이동과 상태 Handoff |
| `UHeistGuardStateComponent` | Modify | Guard State |
| `UHeistGuardNoiseReactionComponent` | Modify | Footstep, GlassBreak, Coin 반응 |
| `UHeistPatrolPathComponent` | Keep | Patrol Path |

## Guard States

```text
Disabled
Stunned
Patrol
InvestigateNoise
ChasePlayer
SearchLastKnownLocation
ReturnToPatrol
InspectExhibit
```

## Inspection Target Registration

- Painting Case와 Object Assembly Case가 검사 후보가 된다.
- Guard Inspection은 두 Case의 상세 Replica Data를 공유하지 않고 서버가 확정한 최종 0~100 Quality Score만 공통 입력으로 사용한다.
- `ReplicaPlaced`, `OriginalAvailable`, `OriginalRemoved` 상태에서만 후보 등록이 가능하다.
- 유효 State를 벗어나면 등록 해제한다.
- EndPlay 시 등록 해제한다.
- 서버에서만 후보를 결정한다.
- Guard와 Case 거리 우선
- 동일 거리에서는 DisplayCaseId
- 다음 Actor Name
- 결정론적 순서를 유지한다.
- 별도 Inspection Manager를 추가하지 않는다.
- 별도 Objective Service를 추가하지 않는다.

## Guard InspectExhibit

- Patrol은 유효 후보를 선점한다.
- Guard는 Case까지 이동한다.
- Case 방향으로 정렬한다.
- 서버 고정 Cast를 시작한다.
- Chase는 Inspect보다 우선한다.
- 중단된 Case는 재개 가능 상태를 보존한다.
- Patrol 복귀 후 보존된 Case를 우선 재개한다.
- Cast 완료 결과는 서버가 적용한다.
- Score별 검사 지연은 `TASK-W4-014` 범위다.

## Noise Reaction

현재 활성 SoundPing:

```text
Footstep
GlassBreak
CoinImpact
StunHit
```

제거된 SoundPing:

```text
NoiseTrap
```

Guard Priority Switch에서 `NoiseTrap` case를 제거한다.

---

SoundPing은 Guard Noise Reaction을 위한 서버 전용 Event Dispatch다.

Client HUD Snapshot 복제, Direction Marker, Marker Pool 및 SoundPing Widget을 사용하지 않는다.

# 12. SoundPing

## `FHeistSoundPingDataRow`

필드:

```text
SoundPingId
DisplayName
SoundPingTag
PingType
Radius
Duration
RefreshInterval
bAffectsGuards
Sound
```

## Active Rows

```text
Ping_Footstep_Walk
Ping_Footstep_Run
Ping_GlassBreak
Ping_CoinImpact
Ping_StunHit
```

`Ping_StunHit`은 현재 Gameplay 사용 여부를 점검한다.

## Removed Rows

```text
Ping_NoiseTrap
```

---

# 13. UI

## Widget Classes

| 클래스 | 상태 | 현재 책임 |
|---|---|---|
| `UHeistHUDWidget` | Modify | Crosshair, Objective, Alert Banner/Color, Lockdown Countdown, Suspense/Alarm Music Layer, Team Status |
| `UHeistInventoryWidget` | Keep | Inventory |
| `UHeistInventorySlotWidget` | Keep | Inventory Slot |
| `UHeistInventoryItemWidget` | Keep | Inventory Item |
| `UHeistQuickSlotWidget` | Modify | Coin QuickSlot |
| `UHeistInteractionPromptWidget` | Modify | Interaction, Observation, Escape Progress |
| `UHeistResultWidget` | Modify | Team Result / Contribution |
| `UHeistForgeryWidget` | Keep | Owner-only Forgery UI, Local Canvas Reset, Drawing/Lockdown Remaining Time, Alert Warning |
| `UHeistObjectAssemblyWidget` | Add | Owner-only Part / Socket / Orientation Assembly UI |
| `UHeistTitleMenuWidget` | Keep + Extend | Host Session, Join Code 입력, Create/Find/Join/Travel 상태와 오류, Cancel/Retry, Settings 표시 |
| `UHeistLobbyWidget` | Modify | Session 내부 Player Slot, 참가 코드/Invite 안내, Map 선택, Travel 상태/Retry, Ready / Start, Leave |
| `UHeistRareLootAlertWidget` | Remove 또는 Deferred Review | Rare Loot 범위 결정 전 신규 사용 금지 |

## ViewModels

| 클래스 | 상태 | 현재 책임 |
|---|---|---|
| `UHeistHUDViewModel` | Modify | Objective, Alert Banner/Color, Lockdown Countdown, Audio Layer State, Escape, Observation |
| `UHeistInventoryViewModel` | Keep | Inventory Snapshot |
| `UHeistQuickSlotViewModel` | Modify | Coin QuickSlot Snapshot |
| `UHeistResultViewModel` | Modify | Team Result / Contribution |
| `UHeistTitleMenuViewModel` | Keep + Extend | Online Session 상태, Host/Join Code 요청, Timeout/Cancel/Retry와 진입 오류, Local Settings Snapshot / Apply / Defaults 요청 |
| `UHeistLobbyViewModel` | Modify | Lobby Player/Identity Slot Snapshot, 참가 코드/Invite 안내, 복제 Map 선택, Travel Failure/Retry, Ready / Start, Leave |
| `UHeistForgeryViewModel` | Keep | Forgery UI State, Alert Warning, Lockdown Countdown |
| `UHeistObjectAssemblyViewModel` | Add | Assembly Session, Template, Part Candidate, Socket, Orientation, Remaining Time, Alert Snapshot |

## Removed UI Contract

```text
UHeistSoundPingMarkerWidget
UHeistSoundPingWidgetPool
WBP_SoundPingMarker
SoundPingMarkerLayer
SoundPingMarkerWidgetClass
UI.Indicator.SoundPing
IsTrapPlacementCastActive
GetTrapPlacementCastEndServerTime
bTrapPlacementCastActive
TrapPlacementCastEndServerTime
ACTION PLACING TRAP
PLACING TRAP
MOVE TO CANCEL
TrapPlacement ActionType
```

`BP_RefreshHUDPresentation`에는 Trap 상태 인수를 전달하지 않는다.

Interaction Action Progress 우선순위:

```text
Observation
→ Escape
→ None
```

---

# 14. Debug

| 클래스 | 상태 | 현재 책임 |
|---|---|---|
| `UHeistCheatManager` | Keep | Development Command |
| `UHeistDebugFunctionLibrary` | Keep | Debug Log, Runtime Test |
| Trap 관련 Debug Function | Remove | 삭제 기능 참조 제거 |
| Smoke 관련 Debug Function | Remove | 삭제 기능 참조 제거 |

활성 Task에서 필요한 Forgery, Case, Guard, Alert, Lockdown Debug만 추가한다.

---

# 15. Module Dependencies

현재 주요 의존성:

```text
Core
CoreUObject
Engine
InputCore
EnhancedInput
NetCore
GameplayTags
UMG
Slate
SlateCore
ModelViewViewModel
AIModule
GameplayStateTreeModule
StateTreeModule
OpenCV
OpenCVHelper
ImageCore
```

OpenCV는 Forgery 유사도 평가에 사용한다.

Stroke 수집, Palette Raster, Authority, Validation, Replication은 프로젝트 C++가 계속 소유한다.

---

# 16. Forbidden Architecture

다음을 추가하지 않는다.

```text
ForgeryManager
ReplicaManager
ArtifactFactory
ObjectiveService
InspectionManager
TrapManager
SmokeManager
신규 Camera Manager
Painting별 Actor Class
```

현재 컴포넌트와 Actor 경계 안에서 책임을 배치한다.

---

# 17. Blueprint Asset Contract

## Painting

```text
/Game/Blueprints/World/Actors/Loot/BP_DisplayCase
Parent: AHeistPaintingDisplayCaseActor
```

## Sculpture

```text
/Game/Blueprints/World/Actors/Loot/BP_SculptureDisplayCase
Parent: AHeistObjectDisplayCaseActor
```

## Ceramic

```text
/Game/Blueprints/World/Actors/Loot/BP_CeramicDisplayCase
Parent: AHeistObjectDisplayCaseActor
```

## Coin

```text
/Game/Blueprints/World/Actors/Projectile/BP_HeistCoinProjectile
Parent: AHeistCoinProjectile
```

## Title Menu / Lobby

```text
/Game/Maps/TitleMenuMap
/Game/Maps/LobbyMap
/Game/Blueprints/UI/Title/WBP_TitleMenu
Parent: UHeistTitleMenuWidget
/Game/Blueprints/UI/Lobby/WBP_Lobby
Parent: UHeistLobbyWidget
```

Title Menu와 Lobby Widget Blueprint는 Layout, Binding, Color, Animation만 소유한다.

Session Rule, Player Slot Identity, Map Selection Authority와 Travel은 C++가 소유한다.

## Removed Blueprint Assets

```text
BP_HeistSmokeProjectile
BP_HeistSmokeCloud
BP_HeistGlueTrap
BP_HeistNoiseTrap
```

삭제 전 Reference Viewer를 확인한다.

삭제 후 Fix Up Redirectors를 실행한다.

---

# 18. Data Import Contract

## Item Data

허용 Row 예시:

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

## Usable Item Data

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

## SoundPing Data

삭제 Row:

```text
Ping_NoiseTrap
```

`DataTableImports/*.json`이 Source of Truth다.

삭제 Row를 JSON에서 제거한 뒤 실제 UE DataTable을 Reimport한다.

JSON 기반 DataTable의 Row를 `.uasset`에서 직접 삭제하는 방식은 사용하지 않는다.

---

# 19. Cleanup Verification

삭제 작업 완료 조건:

1. Development Editor Build 성공
2. 삭제 타입 이름에 대한 C++ 활성 참조 0
3. Data Import JSON의 삭제 Row 0
4. 실제 UE DataTable의 삭제 Row 0
5. 삭제 Blueprint Reference Viewer 0 참조
6. Widget Blueprint Refresh All Nodes 완료
7. Widget Blueprint Compile 성공
8. Fix Up Redirectors 완료
9. Missing Parent Class 없음
10. Invalid Enum 없음
11. Failed Import 없음
12. `ValidateItemDataTables()` PASS
13. Coin QuickSlot 정상
14. Coin Throw 정상
15. Coin Guard Distraction 정상
16. Observation Progress 정상
17. Escape Progress 정상
18. Trap UI 문구 없음
19. NoiseTrap SoundPing 없음
20. Smoke 관련 Runtime Tag 없음

---

# 20. Implementation Boundary

Manifest에 `Add`로 승인된 타입이라도 활성 Task 이전에 전체 로직을 구현하지 않는다.

Legacy 또는 Deprecated 타입을 물리적으로 삭제하기 전:

1. 신규 경로 구현
2. C++ Compile
3. Blueprint Reference 교체
4. Blueprint Compile
5. PIE
6. Reference Viewer 0 참조
7. Cleanup 승인

현재 `Remove`로 확정된 Smoke와 Trap 계열은 신규 경로 교체 대상이 아니다. 참조 정리 후 물리적으로 제거한다.
