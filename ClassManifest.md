# Project_MuseumHeist — Class Manifest
## Rev 6: W4 OpenCV / Exhibit Case Isolation Handoff

상태:

- **Keep**: 기존 책임 유지
- **Modify**: 기존 타입을 새 방향에 맞게 수정
- **Add**: 현재 Manifest에서 신규 생성 허용
- **Deprecate**: 신규 흐름에서 미사용, 즉시 삭제 금지
- **Deferred**: v1.0 범위 밖, 현재 생성 금지

Design Reference: `Museum_Heist_GDD.docx` Rev.10

---

# 1. Core Types

## `Core/HeistTypes.h` — Modify

| 타입 | 상태 | 현재 책임 |
|---|---|---|
| `EHeistMatchPhase` | Modify | Enum 호환성은 유지하고 v1.0 흐름은 Lobby/ReadyCountdown/InGame/End만 사용 |
| `EHeistInputMode` | Add | Gameplay/Inventory/Forgery 상호 배타 로컬 입력 상태 |
| `FHeistPlayerResult` | Deprecate | 기존 결과 호환용. 신규 결과는 TeamResult/Contribution 사용 |
| `FHeistRareLootEventState` | Deferred | Optional Objective 검토 전까지 비활성 |
| `EHeistItemType` | Keep | Loose Loot, Throwable, Trap, KeyItem 분류 |
| `EHeistLootGrade` | Keep | Loose Loot 등급 |
| `EHeistUseType` | Keep | Coin 사용, Smoke는 Legacy |
| `EHeistTargetType` | Keep | 사용 대상 분류 |
| `EHeistSpawnCategory` | Keep | Loose Loot Spawn |
| `EHeistSoundPingType` | Modify | Alarm, Teammate Ping 확장 가능 |
| `EHeistGuardState` | Modify | `InspectExhibit` 추가 |
| `EHeistCustomizationType` | Keep | 변경 없음 |
| `EHeistZoneId` | Keep | 변경 없음 |
| `EHeistQuickSlotType` | Keep | Coin만 v1 활성, Smoke는 Legacy, Glue는 Stretch |

## 신규 Enum — Add

```cpp
EHeistForgeryType
- None
- Drawing

EHeistDisplayCaseState
- Secured
- Observed
- ForgeryInProgress
- ReplicaReady
- ReplicaPlaced
- OriginalAvailable
- OriginalRemoved
- Inspecting
- Completed
- Suspected
- Alarmed
- Failed

EHeistAlertLevel
- Quiet
- Suspicious
- Searching
- Alarmed
- Lockdown

EHeistObjectiveState
- Inactive
- Available
- InProgress
- Completed
- Failed
```

- `ReplicaPlaced`부터 액자 표면에는 Replica만 표시하며 Original 평면은 숨긴다.
- `OriginalAvailable`은 Original이 액자에서 분리되어 회수 대기 중인 서버 상태다. 평면은 숨겨도 Display Case 상호작용으로 회수할 수 있다.
- 제출 그림용 Material이 지정된 Replica는 Score Tier에 따른 Roll/Scale 변형을 적용하지 않고 Blueprint 기준 Transform을 유지한다.

## 신규 Struct — Add

```cpp
FHeistForgeryResult
- ArtifactId
- TemplateId
- ForgeryType
- SimilarityScore
- CoverageScore
- MajorShapeScore
- ColorAccuracyScore
- PaintToReferenceRatio
- bAntiFillTriggered
- MissingShapePenalty
- ExtraStrokePenalty
- TimeoutPenalty
- CompletionTime
- bReplicaPlaced

FHeistReplicaPaintingData
- Resolution
- Palette
- PackedPaletteIndices
- ScoreRevision

FHeistTeamResult
- bMissionSuccess
- bPartialSuccess
- TargetArtifactValue
- LooseLootValue
- AverageForgeryScore
- FinalAlertLevel
- ExtractedPlayerCount
- ArrestedPlayerCount
- FinalTeamReward

FHeistPlayerContribution
- PlayerId
- LooseLootValueCarried
- ForgeriesCompleted
- BestForgeryScore
- GuardsDistracted
- TeammatesRescued
- AlarmsTriggered
- bEscaped
- bArrested
```

---

# 2. Core Framework

| 파일 / 클래스 | 상태 | 책임 |
|---|---|---|
| `Core/HeistGameplayTags.*` / `FHeistGameplayTags` | Keep + Extend | Forgery/Objective/Alert 태그 등록 |
| `Core/HeistLogChannels.*` | Keep | 기존 로그 채널 유지 |
| `Core/HeistGameMode.*` / `AHeistGameMode` | Modify | Match Phase, Objective, Alert/Lockdown 전이 판정, Team Result 확정 |
| `Core/HeistGameState.*` / `AHeistGameState` | Modify | Replicated Objective, Alert, Lockdown, Team Result |
| `Core/HeistPlayerState.*` / `AHeistPlayerState` | Modify | Contribution, Escape/Arrest, Carry Value. Rank 필드는 Legacy 미사용 |
| `Core/HeistPlayerController.*` / `AHeistPlayerController` | Modify | Input Mode, Center Interaction, Forgery Request RPC |
| `Core/HeistHUD.*` / `AHeistHUD` | Keep + Extend | HUD/Forgery/Result ViewModel 생성 |
| `Core/HeistGameInstance.*` / `UHeistGameInstance` | Keep | Session/global settings placeholder |

Authority:

- GameMode: Server only
- GameState/PlayerState: Server mutation + Replication
- PlayerController: Local input + Server RPC entry
- HUD: Local presentation

---

# 3. Character

## `Character/HeistPlayerCharacter.*`
`AHeistPlayerCharacter : public ACharacter` — Modify

- First-Person Camera Component 소유
- Controller Yaw 기반 Rotation
- Head-Socket True First-Person Camera Contract
- Forgery Movement Lock 반영
- Full Body Mesh 유지
- 기존 Gameplay Component 유지
- SpringArm은 참조 감사 후 Deprecate

---

# 4. Character Components

Folder: `Character/Components/`

| 클래스 | 상태 | 책임 |
|---|---|---|
| `UHeistTagComponent` | Keep | Gameplay Tag 상태 |
| `UHeistStatusComponent` | Modify | 일반 Timed Status만 유지. Player PvP Stun/Immunity 제거 |
| `UHeistInventoryComponent` | Keep + Extend | Grid/FastArray 유지, Original Carry Entry 연결 |
| `UHeistInteractionComponent` | Modify | Center Screen Trace, Target Filter, Prompt Snapshot |
| `UHeistActionComponent` | Modify | Action Lock, Forgery Cast/Cancel, Submit 중복 방지 |
| `UHeistVisionComponent` | Modify | Camera Forward Flashlight |
| `UHeistCustomizationComponent` | Keep | 외형 |
| `UHeistNoiseEmitterComponent` | Keep | Footstep/Coin/Alarm Noise |
| `UHeistForgeryComponent` | Add | Session, Stroke, Timeout, Submit/Cancel, 서버 판정, Cleanup |

`UHeistForgeryComponent`는 Player Character에 기본 Subobject로 생성한다. 별도 Manager를 만들지 않는다.

---

# 5. Inventory And Data

기존 Inventory/FastArray 타입은 Keep한다.

- `FHeistInventoryItem`
- `FHeistInventoryFastArrayItem`
- `FHeistReplicatedInventory`
- `FHeistQuickSlotState`
- `FHeistItemDataRow`
- `FHeistLootDataRow`
- `FHeistUsableItemDataRow`
- `FHeistSoundPingDataRow`
- `FHeistGuardDataRow`
- `FHeistLootSpawnRow`
- `FHeistVentDataRow`
- `FHeistCustomizationRow`
- `FHeistUITextRow`

변경:

- `FHeistLootDataRow`는 Loose Loot 데이터로 사용한다.
- `FHeistGuardDataRow`에 Inspect/Alert 튜닝을 추가한다.
- `FHeistVentDataRow`는 Shared Extraction 설정으로 재해석한다.

## 신규 Data Row — Add

```cpp
FHeistArtifactDataRow
- ArtifactId
- DisplayName
- ArtifactValue
- Weight
- GridWidth
- GridHeight
- ForgeryType
- ForgeryTemplateId
- MinimumForgeryScore
- BaseInspectionDelay
- VisualActorClass

FHeistForgeryTemplateRow
- TemplateId
- ReferenceImage
- ReferenceMask
- BackgroundFilterMode (None / Black / White)
- BackgroundColorTolerance
- AllowedPalette (2~8 colors)
- ObservationDuration
- ForgeryDuration
- StrokeLimit
- BrushSize
- CoverageWeight
- MajorShapeWeight
- ExtraStrokePenaltyWeight
- TimeoutPenalty
- ShapeAccuracyWeight
- ColorAccuracyWeight
- MaximumPaintToReferenceRatio
- OverpaintScoreCap
```

### Forgery UI Runtime Contract

- Reference Image / Drawing Canvas display area: 400×400 square
- Palette: 2~8 DataTable colors, mouse button selection and number keys 1~8
- Preview Score: throttled owner-local estimate using the shared C++ evaluator
- Final Score: server-authoritative result only

### OpenCV Forgery Score Contract

- UE 5.8 Runtime OpenCV 4.5.5의 `core`, `imgproc`, `quality` 모듈을 사용한다.
- Stroke 수집과 Palette Raster 생성은 기존 C++ 경로를 유지하고 최종 유사도 평가만 OpenCV가 담당한다.
- Shape는 3×3 Morphology Close 후 Reference→Submitted, Submitted→Reference 양방향 Distance Transform으로 평가한다.
- Color는 고정 Canvas의 비교 ROI를 Lab으로 변환한 SSIM과 Palette 분포 Histogram 유사도를 조합한다.
- 완전 일치는 그대로 유지하고 중간 품질만 보수적으로 환산하는 Shape 1.15 / Color 1.10 응답 곡선을 사용한다.
- SSIM 평균은 Foreground Union으로 제한하며, 제출/Reference 면적 비율에 0.65 지수 완성도 계수를 적용해 점·짧은 선의 기본 점수를 차단한다.
- Coverage, Missing, Extra는 정확한 픽셀 일치 개수가 아니라 Distance Similarity의 Recall/Precision에서 계산한다.
- Anti-Fill은 실제 제출/Reference Foreground 면적 비율로 별도 적용한다.
- Local Preview와 Server Final은 동일한 OpenCV Evaluator를 사용하며 서버 결과만 확정값이다.

## `Data/HeistGameBalanceDataAsset.*`
`UHeistGameBalanceDataAsset` — Modify

- Alert/Lockdown 기본값
- Player Count Scaling
- Legacy Gap/PvP 값은 미사용 처리
- Painting별 Observation/Forgery 값은 Template Row가 우선

---

# 6. World And Interactable

| 클래스 | 상태 | 현재 책임 |
|---|---|---|
| `IHeistInteractable` | Keep | 공통 인터랙션 |
| `AHeistInteractableActor` | Keep | 공통 기반 |
| `AHeistLootActor` | Keep | Loose Loot |
| `AHeistPaintingDisplayCaseActor` | Modify | Painting Target, Session Lock, Palette Texture Replica/Original, Inspection State |
| `AHeistDisplayCaseActor` | Deprecate | Legacy serialized/C++ reference 호환용 Painting Alias. `BP_DisplayCase`는 새 부모로 이전 완료 |
| `AHeistSculptureDisplayCaseActor` | Add | Sculpture 전용 배치/시각 Shell, Gameplay Interaction은 Stretch Gate 전까지 차단 |
| `AHeistVentActor` | Modify | Shared Extraction |
| `AHeistCoinProjectile` | Modify | Guard Distraction |
| `AHeistSmokeProjectile` / `AHeistSmokeCloudActor` | Legacy | 신규 PvE 호출 차단, 회귀 기준으로만 보존 |
| `AHeistGlueTrapActor` | Deferred | Guard 전용 Stretch |
| `AHeistNoiseTrapActor` | Deferred | Post-v1.0 |
| `AHeistLootSpawnPoint` | Keep | Loose Loot Spawn |
| `AHeistPlayerStart` | Keep | Spawn |
| `AHeistGuardWaypoint` | Keep | Patrol |

### Replica World Visual Contract

- `AHeistPaintingDisplayCaseActor`는 서버에서 확정·복제된 `FHeistForgeryResult`를 4단계 Score Tier로 변환한다.
- Tier 선택과 적용 상태는 C++가 소유하며 Blueprint는 Tier Material 지정 또는 `BP_ApplyReplicaWorldVisual` 시각 연출만 담당한다.
- 별도 Tier Material이 없으면 Replica Mesh의 상대 회전·크기 변형을 대체 비주얼로 사용한다.
- Score, Coverage, Color Accuracy, Tier는 Replica Mesh Custom Primitive Data 0~3에 기록한다.
- 최종 제출 Stroke는 서버 Score와 동일한 `128×128` Palette Raster로 변환하고 Background를 0, Palette 색을 1~8로 매핑한 4-bit Index Data로 패킹한다.
- `AHeistPaintingDisplayCaseActor`는 확정된 Palette와 Packed Index Data를 제출 시 한 번만 복제한다.
- 각 Client는 RepNotify에서 동일한 Transient `UTexture2D`를 재구성하고 `ReplicaVisualComponent`의 Dynamic Material `PaintingTexture` Parameter에 적용한다.
- 늦게 참가하거나 Actor Relevancy가 복구된 Client도 복제된 확정 Data로 동일한 그림을 재구성한다.
- Blueprint는 `AHeistPaintingDisplayCaseActor` 기반의 재사용 가능한 Painting Frame Shell, Frame Mesh, UV가 정규화된 Original/Replica Plane, `PaintingTexture` Parameter Material을 담당한다.
- Render Target 또는 전체 Stroke Payload를 World Visual 목적으로 추가 복제하지 않는다.

### Exhibit Case Isolation Contract

- Painting과 Sculpture Case는 형제 기능으로 취급하며 한 Actor의 enum/switch 분기로 관리하지 않는다.
- Painting Case만 Drawing Forgery, Palette Raster, Submitted Texture, Frame Plane, Original Carry 흐름을 소유한다.
- Sculpture Case는 별도 파일과 별도 Actor 타입을 사용하며 Painting의 `FHeistReplicaPaintingData` 및 Display Case State를 상속하지 않는다.
- `AHeistDisplayCaseActor` 호환 Alias는 기존 Blueprint 로드를 위한 임시 경계이며 신규 C++ Gameplay API는 `AHeistPaintingDisplayCaseActor`만 받는다.
- Sculpture Assembly, 부품 검증, Replica Mesh 교체, 전용 State/Replication은 Stretch 승인 이후 Sculpture 전용 Task에서만 추가한다.

### Blueprint Asset Contract

- `/Game/Blueprints/World/Actors/Loot/BP_DisplayCase`는 `AHeistPaintingDisplayCaseActor`를 부모로 사용한다.
- `/Game/Blueprints/World/Actors/Loot/BP_SculptureDisplayCase`는 `AHeistSculptureDisplayCaseActor`를 부모로 사용한다.
- Painting Blueprint는 Frame, Original/Replica Plane, `PaintingTexture` Material 표현을 담당한다.
- Sculpture Blueprint는 현재 `InteractionCollision`과 `VisualMeshComponent` 기반 시각 Shell만 담당하며 Painting Graph/State/Data를 참조하지 않는다.

### Forgery Recovery Contract

- Cancel/Submit 및 Timeout/Submit 경합은 서버 RPC 처리 순서에서 하나의 종료 결과만 확정한다.
- Arrest, Disconnect, Match Phase 변경, Owner/Case EndPlay는 활성 Forgery Session과 Display Case Lock을 함께 정리한다.
- Disconnect는 `UHeistForgeryComponent`를 먼저 정리하고 Display Case 전체 Sweep을 안전망으로 수행한다.
- 복구 뒤 Forgery Widget은 숨겨지고 단일 인스턴스만 유지하며 입력 모드는 Forgery가 아닌 유효한 단일 Context로 복원한다.
- `HeistForgeryRecoveryDump`는 Local Session/UI/Input과 서버의 Orphan Case Lock/Session을 함께 검증한다.
- `HeistForgeryRecoveryRace <CancelSubmit|SubmitCancel>`는 Owning Client에서 두 요청을 연속 전송해 서버 직렬화 결과를 검증한다.
- `HeistForgeryTransportTest NearTimeout`은 서버 만료 직전 유효 Payload가 확정되고, `Timeout`은 만료 이후 Payload가 거부·정리되는 경계를 검증한다.

금지:

- Painting별 `AActor` 파생 클래스
- `ForgeryManager`
- `ReplicaManager`
- `ArtifactFactory`
- `ObjectiveService`
- 신규 Camera Manager

---

# 7. AI

| 클래스 | 상태 | 책임 |
|---|---|---|
| `AHeistGuardCharacter` | Keep + Extend | First-Person 감지 튜닝 |
| `AHeistGuardAIController` | Keep + Extend | Inspect Target 선택 |
| `FHeistGuardStateTreeTask` | Add | StateTree-owned guard movement, wait, and authoritative state handoff |
| `UHeistGuardStateComponent` | Modify | InspectExhibit, Alert 반응 |
| `UHeistGuardNoiseReactionComponent` | Keep | Coin/Footstep/Alarm |
| `UHeistPatrolPathComponent` | Keep | Patrol |

### Inspection Target Registration Contract

- `AHeistPaintingDisplayCaseActor`는 서버에서 Replica가 확정되고 Case가 `ReplicaPlaced`, `OriginalAvailable`, `OriginalRemoved` 중 하나일 때만 Inspection 후보로 등록한다.
- Case가 후보 State를 벗어나거나 EndPlay되면 등록을 해제한다.
- `AHeistGuardAIController`는 서버에서만 등록된 유효 Painting Case를 선택한다.
- 선택은 Guard와 Case 사이 거리 우선이며 동일 거리에서는 `DisplayCaseId`, Actor Name 순서로 결정론적으로 고정한다.
- 등록 상태는 서버 AI 의사결정용이며 Client가 검사 대상을 확정하지 않는다.
- Guard 이동, 정렬, Inspect Cast, 검사 결과 적용은 `TASK-W4-013` 범위다.
- 별도 Inspection Manager, Service, Subsystem은 추가하지 않는다.

### Guard InspectExhibit Contract

- `EHeistGuardState::InspectExhibit`와 `AI.State.InspectExhibit` StateTree Event를 사용한다.
- Patrol은 등록된 Painting Case를 선점하면 `InspectExhibit`에 양보하고, Noise Investigate는 검사 중 상태를 선점하지 못한다.
- Guard는 Case까지 이동한 뒤 Case 방향으로 정렬하고 서버 고정 Cast를 시작한다.
- Chase는 검사보다 우선하며, 중단된 Case는 검사 전 State와 등록 상태를 복원한다.
- Chase 이후 Patrol로 돌아오면 보존된 Case를 우선 재개한다.
- Cast 완료 결과는 서버에서 Case `Suspected`로 적용한다. Score별 검사 지연은 `TASK-W4-014` 범위다.

StateTree Asset는 Editor 작업이며 사용자가 소유한다.

---

# 8. UI

## Widget Classes

| 클래스 | 상태 | 책임 |
|---|---|---|
| `UHeistHUDWidget` | Modify | Crosshair, Objective, Alert, Team Status |
| `UHeistInventoryWidget` | Keep | Inventory |
| `UHeistInventorySlotWidget` | Keep | Slot |
| `UHeistInventoryItemWidget` | Keep | Item |
| `UHeistQuickSlotWidget` | Keep | QuickSlot |
| `UHeistInteractionPromptWidget` | Modify | Center Screen Prompt |
| `UHeistResultWidget` | Modify | Team Result / Contribution |
| `UHeistRareLootAlertWidget` | Remove | Rare Loot 제거 후 Blueprint 참조 정리 |
| `UHeistForgeryWidget` | Add | Owner-only Full-Screen Forgery Presentation |

## ViewModels

| 클래스 | 상태 | 책임 |
|---|---|---|
| `UHeistHUDViewModel` | Modify | Objective, Alert, Lockdown, Carrier, Team Status |
| `UHeistInventoryViewModel` | Keep | Inventory Snapshot |
| `UHeistQuickSlotViewModel` | Keep | QuickSlot |
| `UHeistResultViewModel` | Modify | Team Result / Contribution |
| `UHeistLobbyViewModel` | Keep | 1~4인 Lobby |
| `UHeistForgeryViewModel` | Add | Forgery UI 상태만 노출, 판정 없음 |

---

# 9. Debug

- `UHeistCheatManager` — Keep
- `UHeistDebugFunctionLibrary` — Keep

활성 Task에서 필요한 Forgery/Case/Alert Debug Command만 추가한다.

---

# 10. Module Dependencies

현재 의존성:

- Core
- CoreUObject
- Engine
- InputCore
- EnhancedInput
- NetCore
- GameplayTags
- UMG
- Slate
- SlateCore
- ModelViewViewModel
- AIModule
- GameplayStateTreeModule
- StateTreeModule
- OpenCV
- OpenCVHelper
- ImageCore

OpenCV는 최종 Forgery 유사도 평가에만 사용한다. Stroke 수집, Palette Raster, Authority, Replication 계약은 프로젝트 C++ 경로가 계속 소유한다.

---

# 11. Implementation Boundary

Manifest에 Add로 승인된 타입이라도 활성 Task 이전에 전체 로직을 구현하지 않는다.

Legacy 타입이나 필드를 물리적으로 삭제하기 전에:

1. 신규 경로 구현
2. C++ Compile
3. Blueprint Reference 교체
4. Blueprint Compile
5. PIE
6. Reference Viewer 0 참조
7. Cleanup Task 승인

을 거친다.
