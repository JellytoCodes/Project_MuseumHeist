# W2 Blueprint Shell Plan

## 1. 문서 목적

W2의 목적은 최종 에셋 주입이나 시각 폴리싱이 아니다.

현재 C++ gameplay foundation에 대응하는 Blueprint/Widget Blueprint Shell을 Unreal Editor에서 생성하고, 다음 연결 관계를 명확히 만드는 것이 목적이다.

```txt
C++ class/state/authority
-> Blueprint class defaults and component assembly
-> ViewModel / FieldNotify / C++ presentation event
-> Widget Blueprint layout
-> 4-player PIE baseline verification
```

W2 완료 시점에는 placeholder mesh, 기본 material, 단색 icon, debug text를 사용해도 된다. 다만 어떤 C++ 상태를 읽는지, 어떤 클래스 참조를 할당하는지, 어느 화면에서 무엇을 확인해야 하는지는 재현 가능해야 한다.

## W2 기록 관리 원칙

W2의 작업 체크, 진행 상태, Gate 판단, 테스트 결과, 결정 로그는 Notion에서만 관리한다.

이 문서는 W2 Blueprint Shell의 기준서이며, 진행 체크리스트가 아니다. 진행률, 상태, 완료 여부, 테스트 결과는 Notion의 다음 데이터베이스를 기준으로 한다.

- 주차별 로드맵 / 리더보드
- 주차별 작업보드
- 테스트 로그
- 결정 로그
- 버그 이슈 로그

`Docs/W2_ExecutionChecklist.md`는 생성하지 않는다. 별도 Markdown 체크리스트를 만들면 Notion과 기록이 분산되므로 W2 관리 대상에서 제외한다.

## W2 운영 방식

W2의 실제 Blueprint / Widget Blueprint 생성 작업은 사용자가 Unreal Editor에서 직접 수행한다.

Codex는 W2 작업의 주 실행자가 아니다. Codex는 현재 코드와 이 기준 문서를 바탕으로 구조를 체크하고, 사용자가 만든 자산을 리뷰하며, 로그·MVVM Warning·Accessed None·Missing Class·Missing Row·owner-only UI·replicated state 문제를 추적하는 보조 역할만 담당한다.

Codex는 `.uasset`이나 `.umap`을 직접 생성하거나 수정하지 않는다. Codex는 사용자가 Editor에서 수행해야 하는 작업을 완료했다고 주장하지 않으며, 별도 Markdown 실행 체크리스트를 생성하지 않는다.

## 2. 기준과 표기

- 코드 및 Content 검사일: 2026-06-28
- 엔진: Unreal Engine 5.8
- 기준 우선순위: `AGENTS.md` -> `ClassManifest.md` -> 관련 GDD 범위
- `[기존]`: 현재 `Content`에 `.uasset`이 존재한다.
- `[신규]`: Unreal Editor에서 수동 생성해야 한다.
- `[확인 필요]`: C++ 타입은 존재하지만 현재 노출 계약만으로 요구 기능을 연결할 수 없거나, binary `.uasset` 내부 설정을 코드 검사만으로 확정할 수 없다.
- 이 문서는 `.uasset` 생성 완료를 주장하지 않는다. Blueprint/WBP/Map 작업의 실제 수행자와 완료 판단 주체는 사용자이며, 상태 기록은 Notion에서만 관리한다.

## 3. 현재 코드/Content 진단 요약

### 3.1 확인된 Core C++ 클래스

- `AHeistGameMode`
- `AHeistGameState`
- `AHeistPlayerController`
- `AHeistHUD`
- `AHeistPlayerState`
- `UHeistGameInstance`
- `AHeistPlayerCharacter`
- `AHeistGuardCharacter`
- `AHeistGuardAIController`

`AHeistGameMode`의 C++ 기본값은 다음과 같다.

| GameMode slot | C++ 기본 클래스 |
|---|---|
| Default Pawn | `AHeistPlayerCharacter` |
| Player Controller | `AHeistPlayerController` |
| Player State | `AHeistPlayerState` |
| Game State | `AHeistGameState` |
| HUD | `AHeistHUD` |

### 3.2 확인된 기존 Blueprint/Widget 자산

- `BP_HeistGameMode`
- `BP_HeistPlayerController`
- `BP_HeistPlayerCharacter`
- `BP_HeistHUD`
- `BP_Guard`
- `AIC_Guard`
- `ST_Guard` — Blueprint가 아니라 StateTree asset
- `BP_LootRoyalCrown`
- `BP_TestLoot`
- `BP_LootSpawnPoint`
- `BP_Vent`
- `BP_HeistCoinProjectile`
- `WBP_Inventory`
- `WBP_Result`

### 3.3 확인된 Data 연결

`DA_GameBalance`에서 다음 DataTable 참조가 확인됐다.

- `DT_ItemData`
- `DT_LootData`
- `DT_UsableItemData`
- `DT_SoundPingData`
- `DT_GuardData`

`UHeistGameBalanceDataAsset`에는 이외에도 LootSpawn, Vent, Customization, UI Text DataTable 슬롯이 존재하지만 현재 `DA_GameBalance`에서 실제 자산 참조는 확인되지 않았다.

### 3.4 확인된 Widget/ViewModel 계약

| 영역 | 현재 상태 |
|---|---|
| Inventory | `UHeistInventoryWidget`, `UHeistInventoryViewModel`, `UHeistQuickSlotViewModel`과 confirmed snapshot event 존재 |
| Result | `UHeistResultWidget`, `UHeistResultViewModel`과 FieldNotify 결과 값 존재 |
| Rare Loot | `UHeistRareLootAlertWidget`, `UHeistHUDViewModel`과 Blueprint presentation event 존재 |
| Gap Tracker | `UHeistGapTrackerViewModel` FieldNotify 값 존재. 전용 Widget class/생성 경로 없음 |
| Main HUD | `UHeistHUDWidget`은 constructor-only shell. `AHeistHUD`에 Main HUD WidgetClass/instance 없음 |
| HUD 상태 | `UHeistHUDViewModel`은 현재 Rare Loot 상태만 제공 |
| Lobby | Widget/ViewModel 타입은 존재하지만 presentation state가 없는 skeleton. W3 Candidate |
| QuickSlot child widget | C++ 타입은 존재하지만 presentation API가 없는 skeleton |
| Inventory Slot/Item | C++ 타입은 존재하지만 presentation API가 없는 skeleton |
| Interaction Prompt | C++ 타입은 존재하지만 presentation API가 없는 skeleton |
| Sound Ping Marker/Pool | 타입만 존재하고 runtime presentation/pool 계약은 skeleton. W3 Candidate |
| Popup Pool | 타입만 존재하고 runtime presentation/pool 계약은 skeleton. W2는 최소 text/icon shell만 대상으로 하며 Advanced Popup은 W3 Candidate |

## 4. W2 책임 경계

### Blueprint가 담당한다

- C++ base class를 부모로 하는 자산 생성
- inherited component의 mesh/material/scale 및 placeholder 설정
- Camera/SpringArm의 child-specific editor tuning
- InputAction/InputMappingContext, WidgetClass, DataAsset, DataTable row/class reference 할당
- Widget layout, anchor, visibility, animation, color, icon
- MVVM View 추가와 FieldNotify binding
- C++ `BlueprintImplementableEvent`에서 presentation 갱신
- visual-only debug label, collision wireframe 확인, animation/SFX/VFX trigger

### Blueprint가 담당하지 않는다

- loot 획득 승인, score/weight 변경
- inventory/QuickSlot authoritative mutation
- stun, immunity, smoke, trap 판정
- projectile spawn/hit/damage 판정
- escape 승인과 result 결정
- Guard target 선택, perception 결과, state transition validation
- replicated property 직접 변경
- client 값을 신뢰한 서버 상태 변경

### Event Graph 허용 범위

- `PreConstruct`의 design-time placeholder
- C++ presentation event를 받아 Text/Image/Visibility/Animation 갱신
- cosmetic animation, sound, Niagara trigger
- Debug build용 label/outline 표시
- `AHeistPlayerController`의 공개 request API 호출

### Event Graph 금지 범위

- authority 분기 뒤 score/inventory/status 직접 변경
- Server/Multicast RPC 신규 작성
- Tick으로 replicated gameplay state를 계속 polling
- DataTable 값으로 gameplay 결과 재계산
- Guard state를 Blueprint에서 직접 결정
- Actor spawn으로 C++ validation 우회

## 5. P0 C++ Contract 확인/보완 항목

Blueprint Shell 생성 전에 아래 공백을 먼저 결정해야 한다. 이번 문서 작업에서는 코드 변경하지 않았다.

| 항목 | 변경이 필요한 이유 | 영향 범위 | 제안 Task |
|---|---|---|---|
| Main HUD 생성 경로 | `AHeistHUD`에 `UHeistHUDWidget` class/instance와 생성 함수가 없다 | `AHeistHUD`, `UHeistHUDWidget`, `BP_HeistHUD`, `WBP_HeistHUD` | `TASK-W2-005` |
| 일반 HUD 상태 | `UHeistHUDViewModel`은 Rare Loot만 제공하며 timer/score/weight/status/action/leaderboard 상태가 없다 | HUD ViewModel과 GameState/PlayerState/Character component delegate 연결 | `TASK-W2-005` |
| Flashlight 방향 | `UHeistVisionComponent`는 constructor-only이며 flashlight component/aim direction/input contract가 없다 | Player Character/Vision/Controller의 visual direction API | `TASK-W2-001` |
| Inventory Slot/Item | child widget에 InstanceId/grid size/rotation/icon setter 또는 event가 없다 | Slot/Item Widget, Inventory Widget | `TASK-W2-008` |
| QuickSlot 표시 | child widget에 slot type/item/count/cooldown 표시 계약이 없다 | QuickSlot Widget/ViewModel | `TASK-W2-009` |
| Interaction/Action Progress | Prompt Widget은 skeleton이고 전용 Action Progress class/VM이 없다 | Interaction/Action components, HUD presentation | `TASK-W2-010` |
| Lobby | Lobby ViewModel이 상태를 제공하지 않는다 | W3 Candidate. W2 Gate blocker로 사용하지 않음 | `TASK-W2-012` |
| Gap Tracker Widget 연결 | VM은 있지만 Widget class reference/생성/주입 경로가 없다 | HUD와 GapTracker ViewModel | `TASK-W2-014` |
| Sound Ping | Marker/Pool이 skeleton이고 max 4/merge/priority presentation API가 없다 | W3 Candidate. W2 Gate blocker로 사용하지 않음 | `TASK-W2-015` |
| Popup/Status feedback | Popup pool이 skeleton이며 rejection/status presentation payload가 없다 | W2는 최소 text/icon shell만 확인. Advanced Pool/animation/sound는 W3 이후 | `TASK-W2-016` |

P0 변경은 gameplay authority를 이동시키는 작업이 아니다. C++ authoritative state를 Widget이 읽을 수 있도록 class reference, read-only snapshot, delegate, FieldNotify, `BlueprintImplementableEvent`를 추가하는 범위로 제한한다.

## 6. 공통 Editor 확인 기준

사용자는 모든 신규 Blueprint/WBP를 Unreal Editor에서 다음 기준으로 생성·확인한다. 아래 항목은 진행률 체크가 아니라 구조 리뷰 기준이다.

- 아래 명시된 C++ Parent Class로 생성한다.
- 제안 경로에 저장하고 redirector를 정리한다.
- inherited gameplay component를 삭제하거나 duplicate하지 않는다.
- placeholder mesh/material/icon을 할당한다.
- exposed class/data reference를 Class Defaults에서 할당한다.
- Event Graph에 authoritative gameplay logic이 없는지 확인한다.
- Compile 후 Save한다.
- Reference Viewer에서 예상 C++/Data/WBP 참조만 연결됐는지 확인한다.
- 4-player PIE에서 server/client별 표시를 확인한다.

## 7. 4-player PIE 공통 검증 프로필

- `PIE-Core`: Listen Server 1 + Client 3가 모두 동일 GameMode class chain으로 spawn된다.
- `PIE-Owner`: owner-only UI/Input은 해당 local controller 창에서만 보인다.
- `PIE-Rep`: 서버가 변경한 replicated state가 세 클라이언트에 동일하게 표시된다.
- `PIE-Actor`: 서버 spawn actor class와 visual component가 모든 창에서 식별된다.
- `PIE-AI`: 서버 Guard state/perception 결과가 movement와 최소 debug presentation으로 관찰된다.
- `PIE-UI`: MVVM source 누락, binding warning, Accessed None 없이 표시된다.
- `PIE-Reject`: client invalid request가 서버에서 거부되고 UI가 confirmed state로 돌아온다.

---

# A. Blueprint 자산 계획

## 8. Core Framework Blueprint

### 8.1 `[기존] BP_HeistGameMode`

- **Blueprint 이름:** `BP_HeistGameMode`
- **예상 저장 경로:** `/Game/Blueprints/Core/GameMode/BP_HeistGameMode`
- **Parent C++ Class:** `AHeistGameMode`
- **담당 영역:** framework class 선택과 balance data reference
- **생성 목적:** C++ 기본 framework를 실제 Player/HUD Blueprint child로 교체
- **필수 Components:** 없음
- **필수 Exposed Properties / Class References:** `DefaultPawnClass=BP_HeistPlayerCharacter`, `PlayerControllerClass=BP_HeistPlayerController`, `HUDClass=BP_HeistHUD`, `GameStateClass=AHeistGameState`, `PlayerStateClass=AHeistPlayerState`, `GameBalanceDataAsset=DA_GameBalance`
- **연결:** `DA_GameBalance`와 그 내부 5개 DataTable
- **Event Graph 허용:** 없음
- **Event Graph 금지:** match start/escape/rare loot/result authority 구현
- **완료 체크리스트:** 위 class slot 할당, C++ GameState/PlayerState 유지, Class Defaults compile
- **4-player PIE 검증:** `PIE-Core`; 모든 pawn/controller/HUD 실제 class name 확인
- **관련 W2 Task:** `TASK-W2-005`, `TASK-W2-018`

현재 `DefaultEngine.ini`의 Global Default GameMode는 C++ `AHeistGameMode`다. `SandBoxMap` binary에서는 `BP_HeistGameMode` 참조가 발견됐으므로 World Settings의 GameMode Override를 Editor에서 확인해야 한다. W2에서는 Map Override 또는 Global Default 중 한 경로를 명시적으로 선택한다.

### 8.2 `[기존] BP_HeistPlayerController`

- **Blueprint 이름:** `BP_HeistPlayerController`
- **예상 저장 경로:** `/Game/Blueprints/Core/Player/BP_HeistPlayerController`
- **Parent C++ Class:** `AHeistPlayerController`
- **담당 영역:** Enhanced Input asset reference
- **생성 목적:** C++ input/request 경로에 project input asset 할당
- **필수 Components:** C++ 기본 Controller만 사용
- **필수 Exposed Properties / Class References:** `MoveInputAction=IA_Move`, `InteractInputAction=IA_Interact`, `InventoryInputAction=IA_Inventory`, `GameplayInputMappingContext=IMC_Default`
- **연결:** `AHeistPlayerCharacter`, `AHeistHUD`, server RPC request path
- **Event Graph 허용:** local cursor/debug presentation
- **Event Graph 금지:** inventory/loot/escape/item-use 결과 직접 확정
- **완료 체크리스트:** 네 Input reference 할당, BeginPlay gameplay graph 없음, request API 유지
- **4-player PIE 검증:** `PIE-Owner`; 각 창에서 Move/Interact/Inventory 입력이 자기 pawn에만 작동
- **관련 W2 Task:** `TASK-W2-001`

### 8.3 `[기존] BP_HeistHUD`

- **Blueprint 이름:** `BP_HeistHUD`
- **예상 저장 경로:** `/Game/Blueprints/Core/UI/BP_HeistHUD`
- **Parent C++ Class:** `AHeistHUD`
- **담당 영역:** WidgetClass reference 할당
- **생성 목적:** C++ HUD가 local ViewModel과 WBP instance를 생성하도록 class asset 연결
- **필수 Components:** 없음
- **필수 Exposed Properties / Class References:** 현재 `InventoryWidgetClass=WBP_Inventory`, `RareLootAlertWidgetClass=WBP_RareLootAlert`, `ResultWidgetClass=WBP_Result`; P0 후 `MainHUDWidgetClass`, Gap/SoundPing/Popup 관련 class slot
- **연결:** 모든 UI ViewModel과 local owning controller
- **Event Graph 허용:** 없음
- **Event Graph 금지:** Widget을 authoritative state owner로 사용
- **완료 체크리스트:** 모든 class slot 비어 있지 않음, local controller에서만 Widget 생성, duplicate add 없음
- **4-player PIE 검증:** `PIE-Owner`, `PIE-UI`
- **관련 W2 Task:** `TASK-W2-005`, `TASK-W2-006`

### 8.4 W2에서 만들지 않는 Core Blueprint

- `BP_HeistGameState`: 전용 asset assignment slot이 없어 현재 필요하지 않다.
- `BP_HeistPlayerState`: presentation 값은 C++ replicated state가 소유하므로 현재 필요하지 않다.
- `BP_HeistGameInstance`: 현재 C++ class가 skeleton이고 `GameInstanceClass` config도 설정되지 않아 W2 shell 목적에는 필요하지 않다.

필요성이 생기면 먼저 실제 editor-configurable property가 무엇인지 정의한 뒤 생성한다.

## 9. Player Blueprint

### 9.1 `[기존] BP_HeistPlayerCharacter`

- **Blueprint 이름:** `BP_HeistPlayerCharacter`
- **예상 저장 경로:** `/Game/Blueprints/Player/BP_HeistPlayerCharacter`
- **Parent C++ Class:** `AHeistPlayerCharacter`
- **담당 영역:** player visual shell, camera tuning, placeholder flashlight
- **생성 목적:** C++ pawn/component 구조를 Editor에서 식별 가능한 playable pawn으로 구성
- **필수 Components:** inherited Capsule, Mesh, CharacterMovement, `CameraSpringArm`, `TopDownCamera`, Tag/Status/Inventory/Interaction/Action/Vision/Customization/NoiseEmitter components. 중복 생성 금지
- **필수 Exposed Properties / Class References:** `BaseMoveSpeed`, `WeightSpeedPenalty`, `MinimumMoveSpeed`; placeholder SkeletalMesh/AnimClass/Material
- **연결:** `BP_HeistPlayerController`, `AHeistPlayerState`, native GameplayTags, Inventory/Status/Action components
- **Event Graph 허용:** mesh/flashlight visual initialization, cosmetic animation
- **Event Graph 금지:** movement 가능 여부, stun, weight speed, inventory open 상태 결정
- **완료 체크리스트:** required C++ components 유지, camera view 확인, placeholder mesh 식별, C++ input path 유지
- **4-player PIE 검증:** `PIE-Core`, `PIE-Owner`, `PIE-Rep`; weight/stun/inventory-open 때 C++ movement restriction 유지
- **관련 W2 Task:** `TASK-W2-001`

`Flashlight` component와 mouse-direction contract는 현재 C++에서 확인되지 않았다. W2 Shell 단계에서는 placeholder `SpotLightComponent` 또는 `DecalComponent`를 visual child로 추가할 수 있지만, mouse aim을 Blueprint Tick으로 확정 구현하지 않는다. 방향 API를 P0에서 보완하기 전까지는 `[확인 필요]`다.

## 10. Guard / AI Blueprint

### 10.1 `[기존] BP_Guard`

- **Blueprint 이름:** `BP_Guard`
- **예상 저장 경로:** `/Game/Blueprints/AI/Guard/BP_Guard`
- **Parent C++ Class:** `AHeistGuardCharacter`
- **담당 영역:** guard mesh/animation/profile shell
- **생성 목적:** 서버 Guard FSM 결과를 눈으로 식별
- **필수 Components:** inherited Capsule, Mesh, CharacterMovement, GuardState, PatrolPath, NoiseReaction
- **필수 Exposed Properties / Class References:** `AIControllerClass=AIC_Guard`, `GuardProfileId=Guard_Default`, placeholder Mesh/AnimClass
- **연결:** `DT_GuardData`, `AIC_Guard`, `ST_Guard`, AI state GameplayTags
- **Event Graph 허용:** state별 material/debug text/animation trigger
- **Event Graph 금지:** target 선정, sight 판정, state transition 호출로 FSM 우회
- **완료 체크리스트:** profile resolve 로그, controller possession, state별 식별 표시
- **4-player PIE 검증:** `PIE-AI`, `PIE-Rep`
- **관련 W2 Task:** `TASK-W2-004`

### 10.2 `[기존] AIC_Guard`

- **Blueprint 이름:** `AIC_Guard`
- **예상 저장 경로:** `/Game/Blueprints/AI/Guard/AIC_Guard`
- **Parent C++ Class:** `AHeistGuardAIController`
- **담당 영역:** StateTree asset assignment
- **생성 목적:** C++ state event를 `ST_Guard` high-level flow에 전달
- **필수 Components:** inherited `GuardPerceptionComponent`, `GuardSightConfig`, `GuardStateTreeComponent`
- **필수 Exposed Properties / Class References:** StateTree component asset=`ST_Guard`, `bStartStateTreeAutomatically` 정책 확인
- **연결:** `BP_Guard`, `DT_GuardData`, native `AI.State.*` tags
- **Event Graph 허용:** debug-only visualization
- **Event Graph 금지:** perception stimulus를 gameplay 결과로 직접 확정
- **완료 체크리스트:** StateTree asset 할당, possess 시 listener warning 없음, event tag 수신 확인
- **4-player PIE 검증:** `PIE-AI`; 서버에서만 AI decision 실행
- **관련 W2 Task:** `TASK-W2-004`

`ST_Guard`는 Blueprint가 아니라 StateTree asset이다. StateTree는 `Patrol`, `InvestigateNoise`, `ChasePlayer`, `SearchLastKnownLocation`, `ReturnToPatrol`, `Stunned`, `Disabled`의 읽기 쉬운 상위 흐름만 표현하고 실제 판정은 `UHeistGuardStateComponent`와 `AHeistGuardAIController`에 유지한다.

## 11. World Actor Blueprint

### 11.1 `[기존] BP_LootRoyalCrown`

- **Blueprint 이름:** `BP_LootRoyalCrown`
- **예상 저장 경로:** `/Game/Blueprints/World/Actors/Loot/BP_LootRoyalCrown`
- **Parent C++ Class:** `AHeistLootActor`
- **담당 영역:** Royal Crown visual/data row shell
- **생성 목적:** generic loot C++ actor를 특정 row의 visual variant로 사용
- **필수 Components:** inherited InteractionCollision, VisualMeshComponent
- **필수 Exposed Properties / Class References:** `LootDataRow=(DT_LootData, Loot_RoyalCrown)`, placeholder mesh/material
- **연결:** `DT_LootData`, `DT_ItemData`, native `Item.Loot.RoyalCrown`
- **Event Graph 허용:** available/picked cosmetic 표시
- **Event Graph 금지:** score/weight 계산, pickup 승인
- **완료 체크리스트:** row handle 일치, visual collision 없음, interaction collision 유지
- **4-player PIE 검증:** `PIE-Actor`, `PIE-Rep`
- **관련 W2 Task:** `TASK-W2-002`

### 11.2 `[신규] BP_LootRareArtifact`

- **Blueprint 이름:** `BP_LootRareArtifact`
- **예상 저장 경로:** `/Game/Blueprints/World/Actors/Loot/BP_LootRareArtifact`
- **Parent C++ Class:** `AHeistLootActor`
- **담당 영역:** Rare Artifact placeholder variant
- **생성 목적:** Rare Loot event의 `WorldLootActorClass` target
- **필수 Components:** inherited InteractionCollision, VisualMeshComponent
- **필수 Exposed Properties / Class References:** `LootDataRow=(DT_LootData, Loot_RareArtifact)`
- **연결:** `DT_LootData.WorldLootActorClass`, `DA_GameBalance.RareLootItemId`, `Item.Loot.RareArtifact`
- **Event Graph 허용:** marker/pillar cosmetic trigger
- **Event Graph 금지:** rare event spawn time/authority
- **완료 체크리스트:** ItemId/RowName 일치, ItemData class reference 할당
- **4-player PIE 검증:** `PIE-Actor`, `PIE-Rep`; `HeistRareLootForce`로 spawn class 확인
- **관련 W2 Task:** `TASK-W2-002`, `TASK-W2-013`

### 11.3 `[신규] BP_LootPainting`

- **Blueprint 이름:** `BP_LootPainting`
- **예상 저장 경로:** `/Game/Blueprints/World/Actors/Loot/BP_LootPainting`
- **Parent C++ Class:** `AHeistLootActor`
- **담당 영역:** Painting placeholder variant
- **생성 목적:** data-driven loot shape/visual 확인
- **필수 Components:** inherited InteractionCollision, VisualMeshComponent
- **필수 Exposed Properties / Class References:** `LootDataRow=(DT_LootData, Loot_Painting)`
- **연결:** `DT_ItemData`, `DT_LootData`, `Item.Loot.Painting`
- **Event Graph 허용:** cosmetic only
- **Event Graph 금지:** loot mutation
- **완료 체크리스트:** row/class reference 일치, placeholder aspect ratio 식별 가능
- **4-player PIE 검증:** `PIE-Actor`, pickup 후 replicated disappearance/score
- **관련 W2 Task:** `TASK-W2-002`

### 11.4 `[신규] BP_LootAncientSword`

- **Blueprint 이름:** `BP_LootAncientSword`
- **예상 저장 경로:** `/Game/Blueprints/World/Actors/Loot/BP_LootAncientSword`
- **Parent C++ Class:** `AHeistLootActor`
- **담당 영역:** Ancient Sword placeholder variant
- **생성 목적:** data-driven loot variant 확인
- **필수 Components:** inherited InteractionCollision, VisualMeshComponent
- **필수 Exposed Properties / Class References:** `LootDataRow=(DT_LootData, Loot_AncientSword)`
- **연결:** `DT_ItemData`, `DT_LootData`, `Item.Loot.AncientSword`
- **Event Graph 허용:** cosmetic only
- **Event Graph 금지:** loot mutation
- **완료 체크리스트:** row/class reference 일치, placeholder shape 식별 가능
- **4-player PIE 검증:** `PIE-Actor`, pickup/drop/reacquire class 유지
- **관련 W2 Task:** `TASK-W2-002`

### 11.5 `[기존] BP_Vent`

- **Blueprint 이름:** `BP_Vent`
- **예상 저장 경로:** `/Game/Blueprints/World/Actors/Escape/BP_Vent`
- **Parent C++ Class:** `AHeistVentActor`
- **담당 영역:** vent mesh/collision/active-state presentation
- **생성 목적:** C++ escape phase와 interaction 가능 상태를 시각화
- **필수 Components:** inherited InteractionCollision, VisualMeshComponent
- **필수 Exposed Properties / Class References:** `bRequiresEscapePhase`, `bVentManuallyEnabled`
- **연결:** `AHeistGameState` escape state, Vent GameplayTags
- **Event Graph 허용:** locked/active material 또는 light 변경
- **Event Graph 금지:** escape 승인, cast 완료, escaped state 변경
- **완료 체크리스트:** active 전후 식별, collision 범위 확인
- **4-player PIE 검증:** `PIE-Rep`; phase open 전후 네 창의 active state 일치
- **관련 W2 Task:** `TASK-W2-002`, `TASK-W2-010`

별도 Extraction Actor C++ 클래스는 발견되지 않았다. 현재 escape actor는 `AHeistVentActor`다.

### 11.6 `[기존] BP_LootSpawnPoint`

- **Blueprint 이름:** `BP_LootSpawnPoint`
- **예상 저장 경로:** `/Game/Blueprints/World/Spawn/BP_LootSpawnPoint`
- **Parent C++ Class:** `AHeistLootSpawnPoint`
- **담당 영역:** spawn category/occupancy debug shell
- **생성 목적:** Map에서 rare/fixed/dropped spawn 위치를 구분
- **필수 Components:** inherited SceneRoot; Editor-only billboard/arrow는 BP 추가 가능
- **필수 Exposed Properties / Class References:** `SpawnCategory`, `bSpawnEnabled`, `OccupancyRadius`
- **연결:** `EHeistSpawnCategory`, Rare Loot GameMode flow
- **Event Graph 허용:** Editor/debug label
- **Event Graph 금지:** loot spawn authority
- **완료 체크리스트:** category별 색상/label, radius 확인
- **4-player PIE 검증:** 서버 Rare Loot spawn 위치와 category 일치
- **관련 W2 Task:** `TASK-W2-002`, `TASK-W2-018`

### 11.7 `[신규·선택] BP_DisplayCase`

- **Blueprint 이름:** `BP_DisplayCase`
- **예상 저장 경로:** `/Game/Blueprints/World/Actors/Loot/BP_DisplayCase`
- **Parent C++ Class:** `AHeistDisplayCaseActor`
- **담당 영역:** display case placeholder
- **생성 목적:** 전시대 상호작용 shell이 Map에 필요할 때만 사용
- **필수 Components:** inherited InteractionCollision, VisualMeshComponent
- **필수 Exposed Properties / Class References:** 현재 전용 property 없음
- **연결:** `IHeistInteractable`
- **Event Graph 허용:** cosmetic open/highlight
- **Event Graph 금지:** loot 지급
- **완료 체크리스트:** 실제 W2 Map 사용 여부 확인 후 생성
- **4-player PIE 검증:** interaction target으로 식별되는지만 확인
- **관련 W2 Task:** `TASK-W2-002`

## 12. Interference Item Blueprint

### 12.1 `[기존] BP_HeistCoinProjectile`

- **Blueprint 이름:** `BP_HeistCoinProjectile`
- **예상 저장 경로:** `/Game/Blueprints/World/Actors/Projectile/BP_HeistCoinProjectile`
- **Parent C++ Class:** `AHeistCoinProjectile`
- **담당 영역:** coin projectile visual shell
- **생성 목적:** server-spawned coin projectile 식별
- **필수 Components:** inherited Collision, VisualMesh, ProjectileMovement
- **필수 Exposed Properties / Class References:** placeholder mesh/material
- **연결:** `DT_UsableItemData.SpawnedActorClass`, `Item.Throwable.Coin`, CoinImpact ping
- **Event Graph 허용:** trail/impact cosmetic
- **Event Graph 금지:** hit/stun/noise 판정
- **완료 체크리스트:** row class reference가 BP class, mesh collision disabled
- **4-player PIE 검증:** `PIE-Actor`; `HeistCoinThrow` 결과를 모든 창에서 확인
- **관련 W2 Task:** `TASK-W2-003`

### 12.2 `[신규] BP_HeistSmokeProjectile`

- **Blueprint 이름:** `BP_HeistSmokeProjectile`
- **예상 저장 경로:** `/Game/Blueprints/World/Actors/Projectile/BP_HeistSmokeProjectile`
- **Parent C++ Class:** `AHeistSmokeProjectile`
- **담당 영역:** smoke projectile visual/class reference
- **생성 목적:** smoke throw와 cloud child class 연결
- **필수 Components:** inherited Collision, VisualMesh, ProjectileMovement
- **필수 Exposed Properties / Class References:** `SmokeCloudActorClass=BP_HeistSmokeCloud`, `SmokeRadius` placeholder 확인
- **연결:** `DT_UsableItemData.SpawnedActorClass`, `Item.Throwable.Smoke`
- **Event Graph 허용:** trail/impact cosmetic
- **Event Graph 금지:** cloud spawn 판정/overlap status 적용
- **완료 체크리스트:** projectile row와 cloud class 연결
- **4-player PIE 검증:** `PIE-Actor`; `HeistSmokeThrow`
- **관련 W2 Task:** `TASK-W2-003`

### 12.3 `[신규] BP_HeistSmokeCloud`

- **Blueprint 이름:** `BP_HeistSmokeCloud`
- **예상 저장 경로:** `/Game/Blueprints/World/Actors/Area/BP_HeistSmokeCloud`
- **Parent C++ Class:** `AHeistSmokeCloudActor`
- **담당 영역:** smoke radius/lifetime placeholder presentation
- **생성 목적:** replicated cloud와 InSmoke 범위를 식별
- **필수 Components:** inherited SmokeCollision, VisualMesh
- **필수 Exposed Properties / Class References:** `SmokeRadius`, `bBlocksAISight`, translucent placeholder material
- **연결:** `State.InSmoke`, Guard sight C++ check
- **Event Graph 허용:** fade in/out cosmetic
- **Event Graph 금지:** status add/remove와 sight blocking 판정
- **완료 체크리스트:** visual radius와 collision radius 대략 일치
- **4-player PIE 검증:** `PIE-Actor`, `PIE-Rep`; overlap 진입/해제 표시 확인
- **관련 W2 Task:** `TASK-W2-003`

### 12.4 `[신규] BP_HeistGlueTrap`

- **Blueprint 이름:** `BP_HeistGlueTrap`
- **예상 저장 경로:** `/Game/Blueprints/World/Actors/Trap/BP_HeistGlueTrap`
- **Parent C++ Class:** `AHeistGlueTrapActor`
- **담당 영역:** glue trap trigger visual/class reference
- **생성 목적:** server-spawned trap 위치와 범위 식별
- **필수 Components:** inherited Trigger, VisualMesh
- **필수 Exposed Properties / Class References:** placeholder mesh/decal/material
- **연결:** `DT_UsableItemData.SpawnedActorClass`, `Item.Trap.Glue`, `State.Stunned`
- **Event Graph 허용:** armed/trigger cosmetic
- **Event Graph 금지:** overlap 판정, stun 적용, source item 소비
- **완료 체크리스트:** trigger radius 표시, source row class reference
- **4-player PIE 검증:** `PIE-Actor`, `PIE-Rep`; `HeistGlueTrapPlace`
- **관련 W2 Task:** `TASK-W2-003`

별도의 Stun/Status 표시 Actor C++ 클래스는 발견되지 않았다. W2에서는 Character material/Widget feedback로 처리하고 신규 gameplay Actor를 만들지 않는다.

## 13. HUD / Widget Blueprint

### 13.1 `[신규·P0 이후] WBP_HeistHUD`

- **Blueprint 이름:** `WBP_HeistHUD`
- **예상 저장 경로:** `/Game/Blueprints/UI/HUD/WBP_HeistHUD`
- **Parent C++ Class:** `UHeistHUDWidget`
- **담당 영역:** local main HUD composition
- **생성 목적:** C++/ViewModel 상태를 한 viewport shell에 배치
- **필수 Components:** root Canvas/SafeZone, named panels for score, weight, QuickSlot, action, status, alerts
- **필수 Exposed Properties / Class References:** P0 Main HUD setup API와 HUD ViewModel source
- **연결:** `UHeistHUDViewModel`, `UHeistGapTrackerViewModel`, nested WBP classes
- **Event Graph 허용:** visual animation/visibility
- **Event Graph 금지:** gameplay state polling/mutation
- **완료 체크리스트:** BP_HeistHUD가 생성, 한 local instance, anchor baseline
- **4-player PIE 검증:** `PIE-Owner`, `PIE-UI`
- **관련 W2 Task:** `TASK-W2-005`, `TASK-W2-006`

### 13.2 `[기존] WBP_Inventory`

- **Blueprint 이름:** `WBP_Inventory`
- **예상 저장 경로:** `/Game/Blueprints/UI/Inventory/WBP_Inventory`
- **Parent C++ Class:** `UHeistInventoryWidget`
- **담당 영역:** 4x5 grid composition과 request forwarding
- **생성 목적:** confirmed inventory/QuickSlot snapshot 표시
- **필수 Components:** 4x5 GridPanel, item overlay, QuickSlot panel, close control
- **필수 Exposed Properties / Class References:** MVVM sources `UHeistInventoryViewModel`, `UHeistQuickSlotViewModel`; Slot/Item/QuickSlot widget classes는 P0에서 노출 여부 확인
- **연결:** `BP_RefreshConfirmedInventory`, `BP_RefreshConfirmedQuickSlots`, `AHeistPlayerController` request functions
- **Event Graph 허용:** C++ refresh event에서 child widget 재배치, request 호출
- **Event Graph 금지:** local array를 authoritative inventory로 취급
- **완료 체크리스트:** open/close, 20 cells, owner-only, warning 없음
- **4-player PIE 검증:** `PIE-Owner`, `PIE-UI`, `PIE-Reject`
- **관련 W2 Task:** `TASK-W2-007`

### 13.3 `[신규·P0 이후] WBP_InventorySlot`

- **Blueprint 이름:** `WBP_InventorySlot`
- **예상 저장 경로:** `/Game/Blueprints/UI/Inventory/WBP_InventorySlot`
- **Parent C++ Class:** `UHeistInventorySlotWidget`
- **담당 영역:** cell coordinate/occupancy/drop preview
- **생성 목적:** 4x5 grid cell의 visual shell
- **필수 Components:** Border/Image, optional coordinate debug Text
- **필수 Exposed Properties / Class References:** P0 grid position/occupied/valid drop setter
- **연결:** Inventory Widget confirmed snapshot
- **Event Graph 허용:** hover/drop preview color
- **Event Graph 금지:** item move 성공 판정
- **완료 체크리스트:** 20개 재사용, coordinate 식별, preview 초기화
- **4-player PIE 검증:** `PIE-UI`, `PIE-Reject`
- **관련 W2 Task:** `TASK-W2-008`

### 13.4 `[신규·P0 이후] WBP_InventoryItem`

- **Blueprint 이름:** `WBP_InventoryItem`
- **예상 저장 경로:** `/Game/Blueprints/UI/Inventory/WBP_InventoryItem`
- **Parent C++ Class:** `UHeistInventoryItemWidget`
- **담당 영역:** InstanceId/item/grid size/rotation/icon visual
- **생성 목적:** confirmed item을 draggable visual로 표시
- **필수 Components:** SizeBox, Image, selection/drag overlay
- **필수 Exposed Properties / Class References:** P0 item presentation payload; `UHeistInventoryDragDropOperation`
- **연결:** `FHeistInventoryItem`, `DT_ItemData.Icon`
- **Event Graph 허용:** drag operation 생성, rotate/drop request 전달
- **Event Graph 금지:** item 위치/rotation local commit
- **완료 체크리스트:** size/rotation 표시, InstanceId 유지, rollback
- **4-player PIE 검증:** `PIE-Owner`, `PIE-Reject`
- **관련 W2 Task:** `TASK-W2-008`

### 13.5 `[신규·P0 이후] WBP_QuickSlot`

- **Blueprint 이름:** `WBP_QuickSlot`
- **예상 저장 경로:** `/Game/Blueprints/UI/Inventory/WBP_QuickSlot`
- **Parent C++ Class:** `UHeistQuickSlotWidget`
- **담당 영역:** Q/E/R slot visual
- **생성 목적:** `FHeistQuickSlotState` confirmed snapshot 표시
- **필수 Components:** key Text, icon Image, count Text, cooldown overlay
- **필수 Exposed Properties / Class References:** P0 slot payload; current VM에는 QuickSlots 배열만 존재
- **연결:** `UHeistQuickSlotViewModel`, `EHeistQuickSlotType`
- **Event Graph 허용:** assign/clear request 전달, visual cooldown
- **Event Graph 금지:** source item 소비/slot mutation
- **완료 체크리스트:** Coin/Smoke/Glue 세 slot, empty/assigned 상태
- **4-player PIE 검증:** `PIE-Owner`, `PIE-Reject`
- **관련 W2 Task:** `TASK-W2-009`

### 13.6 `[신규·P0 이후] WBP_InteractionPrompt`

- **Blueprint 이름:** `WBP_InteractionPrompt`
- **예상 저장 경로:** `/Game/Blueprints/UI/HUD/WBP_InteractionPrompt`
- **Parent C++ Class:** `UHeistInteractionPromptWidget`
- **담당 영역:** target/key/availability prompt
- **생성 목적:** current interaction target을 local UI로 표시
- **필수 Components:** action Text, key Text, disabled reason Text
- **필수 Exposed Properties / Class References:** P0 read-only prompt payload
- **연결:** `UHeistInteractionComponent`, `IHeistInteractable`
- **Event Graph 허용:** visibility/text/animation
- **Event Graph 금지:** CanInteract 결과 재판정
- **완료 체크리스트:** target enter/leave, unavailable reason baseline
- **4-player PIE 검증:** `PIE-Owner`, `PIE-UI`
- **관련 W2 Task:** `TASK-W2-010`

### 13.7 `[신규·확인 필요] WBP_ActionProgress`

- **Blueprint 이름:** `WBP_ActionProgress`
- **예상 저장 경로:** `/Game/Blueprints/UI/HUD/WBP_ActionProgress`
- **Parent C++ Class:** 전용 class 없음. 임시로 `UHeistUserWidgetBase` 사용 가능하나 P0 계약 확정 필요
- **담당 영역:** escape/trap cast progress presentation
- **생성 목적:** replicated cast active/end server time 표시
- **필수 Components:** ProgressBar, action Text, cancel indicator
- **필수 Exposed Properties / Class References:** Action presentation source 필요
- **연결:** `UHeistActionComponent`의 escape/trap active와 end server time
- **Event Graph 허용:** UI progress interpolation
- **Event Graph 금지:** cast 완료/취소 결정
- **완료 체크리스트:** action type, remaining time, cancel/hide
- **4-player PIE 검증:** `PIE-Owner`, `PIE-Rep`
- **관련 W2 Task:** `TASK-W2-010`

### 13.8 `[신규·확인 필요] WBP_ScoreLeaderboard`

- **Blueprint 이름:** `WBP_ScoreLeaderboard`
- **예상 저장 경로:** `/Game/Blueprints/UI/HUD/WBP_ScoreLeaderboard`
- **Parent C++ Class:** 전용 class 없음. `UHeistUserWidgetBase` 또는 Main HUD composition 여부 확인 필요
- **담당 영역:** local score/weight와 4-player ranking visual
- **생성 목적:** PlayerState/GameState replicated result-facing 값 표시
- **필수 Components:** local score/weight Text, four player rows
- **필수 Exposed Properties / Class References:** P0 HUD presentation snapshot/FieldNotify
- **연결:** `AHeistPlayerState`, `AHeistGameState`
- **Event Graph 허용:** row 정렬 결과 표시/강조
- **Event Graph 금지:** 점수 계산/순위 authority
- **완료 체크리스트:** 네 player row, local/leader highlight
- **4-player PIE 검증:** `PIE-Rep`, `PIE-UI`
- **관련 W2 Task:** `TASK-W2-005`, `TASK-W2-006`

### 13.9 `[기존] WBP_Result`

- **Blueprint 이름:** `WBP_Result`
- **예상 저장 경로:** `/Game/Blueprints/UI/Result/WBP_Result`
- **Parent C++ Class:** `UHeistResultWidget`
- **담당 영역:** final result layout
- **생성 목적:** `UHeistResultViewModel` FieldNotify 결과 표시
- **필수 Components:** winner, local rank/score/escaped, player result rows
- **필수 Exposed Properties / Class References:** MVVM source `UHeistResultViewModel`
- **연결:** `PlayerResults`, `WinnerIdText`, `MyRankText`, `MyFinalScoreText`, `EscapedVisibility`
- **Event Graph 허용:** result animation
- **Event Graph 금지:** winner/rank 재계산
- **완료 체크리스트:** existing bindings 검토, four-player rows 보완
- **4-player PIE 검증:** `PIE-Rep`, `PIE-UI`
- **관련 W2 Task:** `TASK-W2-011`

### 13.10 `[신규] WBP_RareLootAlert`

- **Blueprint 이름:** `WBP_RareLootAlert`
- **예상 저장 경로:** `/Game/Blueprints/UI/Alert/WBP_RareLootAlert`
- **Parent C++ Class:** `UHeistRareLootAlertWidget`
- **담당 영역:** incoming warning/direction marker
- **생성 목적:** existing Rare Loot presentation event 구현
- **필수 Components:** warning panel, item Text, edge arrow
- **필수 Exposed Properties / Class References:** `UHeistHUDViewModel`
- **연결:** `BP_RefreshRareLootPresentation`, `BP_UpdateRareLootDirectionMarker`
- **Event Graph 허용:** warning/arrow animation
- **Event Graph 금지:** event spawn/marker authority
- **완료 체크리스트:** BP_HeistHUD class slot 할당, warning/marker visibility
- **4-player PIE 검증:** `PIE-Rep`, `PIE-UI`; `HeistRareLootForce`
- **관련 W2 Task:** `TASK-W2-013`

### 13.11 `[신규·P0 이후] WBP_GapTracker`

- **Blueprint 이름:** `WBP_GapTracker`
- **예상 저장 경로:** `/Game/Blueprints/UI/Alert/WBP_GapTracker`
- **Parent C++ Class:** 전용 class 없음. `UHeistUserWidgetBase` 사용 여부와 setup API 확인 필요
- **담당 영역:** leader warning/non-leader direction
- **생성 목적:** existing GapTracker ViewModel FieldNotify 표시
- **필수 Components:** direction arrow, leader border
- **필수 Exposed Properties / Class References:** `UHeistGapTrackerViewModel`
- **연결:** `bShowDirectionArrow`, `bShowLeaderWarning`, `DirectionAngleDegrees`
- **Event Graph 허용:** rotation/visibility animation
- **Event Graph 금지:** leader/threshold 계산
- **완료 체크리스트:** HUD creation/injection 경로, local perspective
- **4-player PIE 검증:** `PIE-Owner`, `PIE-Rep`
- **관련 W2 Task:** `TASK-W2-014`

### 13.12 `[W3 Candidate] WBP_SoundPingMarker`

- **Blueprint 이름:** `WBP_SoundPingMarker`
- **예상 저장 경로:** `/Game/Blueprints/UI/Alert/WBP_SoundPingMarker`
- **Parent C++ Class:** `UHeistSoundPingMarkerWidget`
- **담당 영역:** directional sound ping marker
- **생성 목적:** GameState sound ping event 표시. W2 Gate 필수 아님
- **필수 Components:** edge arrow/wave, type icon, lifetime indicator
- **필수 Exposed Properties / Class References:** P0 marker payload와 pool setup
- **연결:** `DT_SoundPingData`, `FHeistSoundPingEvent`, `UI.Indicator.SoundPing`
- **Event Graph 허용:** marker transform/fade
- **Event Graph 금지:** guard reaction/ping authority
- **완료 체크리스트:** W3에서 max 4, priority/merge/pool reuse 확인
- **4-player PIE 검증:** `PIE-Rep`, `PIE-UI`
- **관련 W2 Task:** `TASK-W2-015`

### 13.13 `[W2 최소 Shell / Advanced는 W3 Candidate] WBP_PopupFeedback`

- **Blueprint 이름:** `WBP_PopupFeedback`
- **예상 저장 경로:** `/Game/Blueprints/UI/Feedback/WBP_PopupFeedback`
- **Parent C++ Class:** 전용 Widget class 없음. `UHeistUserWidgetBase`와 `UHeistPopupWidgetPool` 계약 보완 필요
- **담당 영역:** request failure/bag full/info popup
- **생성 목적:** W2에서는 confirmed rejection reason을 최소 text/icon으로 표시. Advanced Pool/animation/sound는 W3 이후
- **필수 Components:** message Text, optional icon, fade animation
- **필수 Exposed Properties / Class References:** P0 popup payload/pool class
- **연결:** `UI.Warning.InventoryFull` 등 native tag 또는 typed reason
- **Event Graph 허용:** message/animation
- **Event Graph 금지:** 실패를 성공으로 local 처리
- **완료 체크리스트:** W2는 최소 message reset 확인. pooled reuse와 advanced UX는 W3 Candidate
- **4-player PIE 검증:** `PIE-Owner`, `PIE-Reject`
- **관련 W2 Task:** `TASK-W2-016`

### 13.14 `[신규·확인 필요] WBP_StatusFeedback`

- **Blueprint 이름:** `WBP_StatusFeedback`
- **예상 저장 경로:** `/Game/Blueprints/UI/Feedback/WBP_StatusFeedback`
- **Parent C++ Class:** 전용 class 없음. Main HUD composition 또는 `UHeistUserWidgetBase` 여부 확인 필요
- **담당 영역:** Stunned/StunImmune/InSmoke status visual
- **생성 목적:** local replicated status를 vignette/icon으로 표시
- **필수 Components:** vignette Border/Image, status icons
- **필수 Exposed Properties / Class References:** P0 status presentation snapshot/delegate
- **연결:** `UHeistStatusComponent`, `State.Stunned`, `State.StunImmune`, `State.InSmoke`
- **Event Graph 허용:** visual state animation
- **Event Graph 금지:** status duration/clear 결정
- **완료 체크리스트:** local status별 visible state, clear 시 reset
- **4-player PIE 검증:** `PIE-Owner`, `PIE-Rep`
- **관련 W2 Task:** `TASK-W2-016`

### 13.15 `[W3 Candidate] WBP_Lobby`

- **Blueprint 이름:** `WBP_Lobby`
- **예상 저장 경로:** `/Game/Blueprints/UI/Lobby/WBP_Lobby`
- **Parent C++ Class:** `UHeistLobbyWidget`
- **담당 영역:** four-player lobby/ready/loadout baseline
- **생성 목적:** Lobby -> ReadyCountdown presentation shell. W2 Gate 필수 아님
- **필수 Components:** four player slots, ready state, countdown, default loadout preview
- **필수 Exposed Properties / Class References:** `UHeistLobbyViewModel` presentation state가 P0에서 필요
- **연결:** match phase/ready state C++ source `[확인 필요]`
- **Event Graph 허용:** ready request forwarding과 visual animation
- **Event Graph 금지:** match phase 전환/ready authority
- **완료 체크리스트:** W3에서 four slots, local owner 표시, countdown source 확인
- **4-player PIE 검증:** `PIE-Core`, `PIE-Rep`, `PIE-UI`
- **관련 W2 Task:** `TASK-W2-012`

## 14. Test / Debug Blueprint

### 14.1 `[기존] BP_TestLoot`

- **Blueprint 이름:** `BP_TestLoot`
- **예상 저장 경로:** `/Game/Blueprints/Debug/BP_TestLoot`
- **Parent C++ Class:** binary asset 기준 `AHeistLootActor` 여부를 Editor에서 재확인
- **담당 영역:** placeholder loot interaction test
- **생성 목적:** final mesh 없이 loot/inventory/UI pipeline 검증
- **필수 Components:** inherited collision/visual mesh
- **필수 Exposed Properties / Class References:** 테스트용 valid `LootDataRow`
- **연결:** 실제 존재하는 DT row
- **Event Graph 허용:** debug label
- **Event Graph 금지:** pickup/score 우회
- **완료 체크리스트:** `/Debug` 폴더 분리, Shipping map에서 미참조
- **4-player PIE 검증:** `PIE-Actor`, `PIE-Reject`
- **관련 W2 Task:** `TASK-W2-018`, `TASK-W2-019`

### 14.2 Map 배치용 기존 C++ 클래스

다음 타입은 W2에서 별도 Blueprint가 필수는 아니다.

- `AHeistPlayerStart`
- `AHeistGuardWaypoint`

필요하면 Editor 식별용 child Blueprint를 만들 수 있으나 C++ runtime 기능이 skeleton 수준인 부분이 있다. 특히 `UHeistPatrolPathComponent`와 `AHeistGuardWaypoint` 사이의 patrol path 계약은 현재 코드에서 완성된 형태로 확인되지 않았으므로 Patrol 배치 완료를 과장하지 않는다.

### 14.3 Debug-only 분리 규칙

- Debug asset은 `/Game/Blueprints/Debug/` 아래에 둔다.
- Shipping용 GameMode, Map, DataTable에서 debug class를 참조하지 않는다.
- `UHeistCheatManager`와 `UHeistDebugFunctionLibrary` command를 우선 사용한다.
- 새 Debug Actor/Widget은 현재 필요하지 않다. 필요해질 경우 `UE_BUILD_SHIPPING` 정책과 asset reference 제거 방법을 먼저 결정한다.

---

# B. Blueprint 카테고리별 기준 목록

## 15. P1 Actor Shell

- `BP_HeistPlayerCharacter`
- `BP_Guard`
- `AIC_Guard` + `ST_Guard`
- `BP_LootRoyalCrown`
- `BP_LootRareArtifact`
- `BP_LootPainting`
- `BP_LootAncientSword`
- `BP_Vent`
- `BP_LootSpawnPoint`
- `BP_HeistCoinProjectile`
- `BP_HeistSmokeProjectile`
- `BP_HeistSmokeCloud`
- `BP_HeistGlueTrap`

## 16. P2 Widget Shell

- `WBP_HeistHUD`
- `WBP_Inventory`
- `WBP_InventorySlot`
- `WBP_InventoryItem`
- `WBP_QuickSlot`
- `WBP_InteractionPrompt`
- `WBP_ActionProgress`
- `WBP_ScoreLeaderboard`
- `WBP_Result`
- `WBP_RareLootAlert`
- `WBP_GapTracker`
- `WBP_StatusFeedback` 최소 shell

## 17. W3 Candidate

- `WBP_Lobby`
- `WBP_SoundPingMarker`
- Advanced Popup Pool
- final VFX/SFX
- final UI art/icon/font
- final map/environment polish

---

# C. W2 작업 순서

## 18. 의존성 흐름

```txt
P0 C++ read-only presentation contract
├─ GameMode/HUD class references
├─ Main HUD creation
├─ Widget payload/setup APIs
└─ ViewModel/FieldNotify/delegate gaps
   ↓
P1 Actor Blueprint Shell
├─ Player + Input + Camera
├─ Guard + AIC + StateTree
├─ Loot/Vent/SpawnPoint
└─ Projectile/Smoke/Trap
   ↓
P2 Widget Blueprint Shell
├─ Main HUD
├─ Inventory/Item/Slot/QuickSlot
├─ Interaction/Action
├─ Result/Lobby
└─ Rare/Gap/Minimum Status
   ↓
P3 SandBoxMap 4-player integration
   ↓
P4 TEST-W2 and close
```

## 19. Task 순서

### P0 — Contract와 생성 경로

1. `TASK-W2-005`: Main HUD 생성 경로와 일반 HUD presentation contract
2. `TASK-W2-001` 선행 확인: Flashlight visual direction contract
3. `TASK-W2-008`/`009`/`010`: child Widget setup payload
4. `TASK-W2-014`/`016`: Gap Tracker와 최소 Status feedback setup 경로

### P1 — Actor Blueprint Shell

1. `TASK-W2-001`: Player shell
2. `TASK-W2-004`: Guard/AIC/StateTree shell
3. `TASK-W2-002`: Loot/Vent/SpawnPoint shell
4. `TASK-W2-003`: Coin/Smoke/Glue shell

### P2 — Widget Blueprint Shell

1. `TASK-W2-006`: Main HUD shell
2. `TASK-W2-007`: Inventory root
3. `TASK-W2-008`: Inventory Slot/Item
4. `TASK-W2-009`: QuickSlot
5. `TASK-W2-010`: Interaction/Action
6. `TASK-W2-011`: Result
7. `TASK-W2-013`: Rare Loot
8. `TASK-W2-014`: Gap Tracker
9. `TASK-W2-016`: 최소 Popup/Status text/icon shell

`TASK-W2-017`은 최종 Material/VFX/SFX 제작이 아니라 Shell 식별을 위한 placeholder/icon/debug material 준비로 축소한다.

`TASK-W2-012` Lobby와 `TASK-W2-015` Sound Ping Marker는 W2 Gate 필수에서 제외하고 W3 Candidate로 관리한다.

### P3 — Map/PIE

1. `TASK-W2-018`: `SandBoxMap`에 PlayerStart/Loot/Vent/Guard/SpawnPoint test layout 배치
2. `TASK-W2-019`: Listen Server 1 + Client 3 통합 시나리오 실행

### P4 — 기록과 Close

1. `TASK-W2-020`: multiplayer/ownership/replication/UI integration 결과를 `TEST-W2-###`로 기록
2. `TASK-W2-021`: Blueprint/C++ 책임 경계와 미완료 contract를 `DEC-W2-###`로 기록

---

# D. 이번 주 하지 않을 것

- 최종 character/guard/loot mesh 선정
- 최종 AnimBP locomotion polish
- 최종 Material, Niagara, post-process, SFX mix
- 최종 icon/font/UI art 완성
- final map layout, lighting, environment art
- PCG, Security Room, cinematic, Steam Voice
- Noise Trap gameplay 구현 (`AHeistNoiseTrapActor` C++/Blueprint shell은 허용)
- advanced Loadout inventory editing
- Blueprint에서 gameplay authority 구현
- 신규 item별 C++ Actor 클래스 생성
- Blueprint Tick 기반 replicated state polling
- C++ hardcoded project asset path 추가
- `.uasset`, `.umap`을 CLI에서 생성했다고 주장

---

# E. 코드 변경 전 검토 기준

P0 코드 변경을 실제 진행할 때는 각 변경 전에 다음을 기록한다.

1. 어떤 WBP가 현재 어떤 C++ 상태를 읽지 못하는가.
2. 기존 delegate/RepNotify/FieldNotify로 해결 가능한가.
3. 새 property는 `EditDefaultsOnly`, `BlueprintReadOnly`, `Transient` 중 무엇이어야 하는가.
4. Widget이 mutation API를 얻지 않는가.
5. local HUD에만 생성되는가.
6. Tick 없이 event-driven update가 가능한가.
7. 새 타입 없이 기존 manifest class를 확장할 수 있는가.

이번 문서 작성 작업에서는 C++/Build.cs/Config를 수정하지 않았다.

## 20. 변경 후 빌드/컴파일 체크 방법

### C++ 변경이 있는 Task

1. Unreal Editor를 닫거나 Live Coding 상태를 정리한다.
2. Development game target을 빌드한다.

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' Project_MuseumHeist Win64 Development '-Project=D:\Dev\UE5.8\Project_MuseumHeist\Project_MuseumHeist.uproject' -WaitMutex -NoHotReloadFromIDE -NoGoWide
```

3. Editor target이 필요하면 Editor가 DLL을 점유하지 않는 상태에서 실행한다.

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' Project_MuseumHeistEditor Win64 Development '-Project=D:\Dev\UE5.8\Project_MuseumHeist\Project_MuseumHeist.uproject' -WaitMutex -NoHotReloadFromIDE -NoGoWide
```

4. Blueprint child를 모두 Compile/Save한다.
5. Output Log에서 MVVM binding warning, missing class, missing row, Accessed None을 확인한다.

### Blueprint/문서만 변경한 Task

- C++ compile은 필수 증거가 아니다.
- 모든 변경 Blueprint의 Compile/Save 성공을 확인한다.
- `Reference Viewer`, `Asset Audit`, `Map Check`를 실행한다.
- 4-player PIE와 예상 로그를 확인한다.
- `.uasset` compile 성공만으로 multiplayer 완료 처리하지 않는다.

---

# F. W2 Gate Pass 조건

아래 조건의 실제 판정과 상태 기록은 Notion에서만 수행한다.

- P0로 분류된 Main HUD/Widget setup contract가 구현되었거나, 미구현 항목이 `[확인 필요]` 상태로 명확히 차단 기록됐다.
- `BP_HeistGameMode`의 Pawn/Controller/HUD/GameState/PlayerState/Balance class chain이 Editor에서 확인됐다.
- Player, Guard, Loot, Vent, Coin, Smoke, Glue Trap Blueprint Shell이 placeholder 상태로 식별 가능하다.
- `DT_LootData.WorldLootActorClass`와 `DT_UsableItemData.SpawnedActorClass`가 필요한 Blueprint child를 가리킨다.
- Main HUD, Inventory, QuickSlot, Result, Rare Loot Alert의 local Widget 생성 및 C++ state read baseline이 동작한다.
- Gap Tracker와 최소 Status text/icon shell이 존재하거나 명시적인 P0 blocker 상태로 기록됐다.
- Lobby, Sound Ping Marker, Advanced Popup Pool, final VFX/SFX/UI art는 W2 Gate 판정에서 제외됐다.
- Blueprint Event Graph에 authoritative gameplay mutation이나 신규 RPC가 없다.
- `SandBoxMap` Listen Server 1 + Client 3에서 owner-only UI와 replicated actor/state 표시를 구분해 확인했다.
- MVVM source 누락, invalid binding, missing class/row, Accessed None을 숨기지 않고 기록했다.
- project-level multiplayer/ownership/replication 결과가 `TEST-W2-###`에 기록되고 책임 경계가 `DEC-W2-###`에 반영됐다.
