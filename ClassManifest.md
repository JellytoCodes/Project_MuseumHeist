# Project_MuseumHeist — Class Manifest

## Current Goal

Generate C++ class skeletons only.

Do not implement internal gameplay logic yet.
Do not implement full inventory movement, item usage, stun, AI chase, scoring, UI binding, or networking behavior yet.
Only create compile-ready class and type structure.

---

# 1. Core

## Files

`Source/Project_MuseumHeist/Core/HeistTypes.h`

Contains:

* `EHeistMatchPhase`
* `EHeistItemType`
* `EHeistLootGrade`
* `EHeistUseType`
* `EHeistTargetType`
* `EHeistGuardState`
* `EHeistZoneId`
* `EHeistQuickSlotType`

---

`Source/Project_MuseumHeist/Core/HeistGameplayTags.h`
`Source/Project_MuseumHeist/Core/HeistGameplayTags.cpp`

Contains:

* `FHeistGameplayTags`

Purpose:

* Central native GameplayTag registration point.

---

`Source/Project_MuseumHeist/Core/HeistLogChannels.h`
`Source/Project_MuseumHeist/Core/HeistLogChannels.cpp`

Contains:

* `LogHeist`
* `LogHeistInventory`
* `LogHeistNetwork`
* `LogHeistUI`
* `LogHeistAI`

---

`Source/Project_MuseumHeist/Core/HeistGameMode.h`
`Source/Project_MuseumHeist/Core/HeistGameMode.cpp`

Class:

* `AHeistGameMode : public AGameModeBase`

Purpose:

* Server-only match flow owner.
* Placeholder methods for match phase transitions.

Do not implement full match flow yet.

---

`Source/Project_MuseumHeist/Core/HeistGameState.h`
`Source/Project_MuseumHeist/Core/HeistGameState.cpp`

Class:

* `AHeistGameState : public AGameStateBase`

Purpose:

* Replicated match phase and timer state.

Create replicated properties only if compile-safe and minimal.

---

`Source/Project_MuseumHeist/Core/HeistPlayerState.h`
`Source/Project_MuseumHeist/Core/HeistPlayerState.cpp`

Class:

* `AHeistPlayerState : public APlayerState`

Purpose:

* Player score, carried weight, escaped state, customization state placeholder.

---

`Source/Project_MuseumHeist/Core/HeistPlayerController.h`
`Source/Project_MuseumHeist/Core/HeistPlayerController.cpp`

Class:

* `AHeistPlayerController : public APlayerController`

Purpose:

* UI request routing.
* Future Server RPC entry point.

Do not implement actual Server RPC behavior yet.

---

`Source/Project_MuseumHeist/Core/HeistHUD.h`
`Source/Project_MuseumHeist/Core/HeistHUD.cpp`

Class:

* `AHeistHUD : public AHUD`

Purpose:

* Future UI / ViewModel creation hub.

---

`Source/Project_MuseumHeist/Core/HeistGameInstance.h`
`Source/Project_MuseumHeist/Core/HeistGameInstance.cpp`

Class:

* `UHeistGameInstance : public UGameInstance`

Purpose:

* Future session and global game settings placeholder.

---

# 2. Character

`Source/Project_MuseumHeist/Character/HeistPlayerCharacter.h`
`Source/Project_MuseumHeist/Character/HeistPlayerCharacter.cpp`

Class:

* `AHeistPlayerCharacter : public ACharacter`

Purpose:

* Player character root.
* Owns core gameplay components.

Create component default subobjects.
Do not implement movement, combat, item use, or interaction logic yet.

---

# 3. Character Components

Folder:
`Source/Project_MuseumHeist/Character/Components/`

Classes:

* `UHeistTagComponent : public UActorComponent`
* `UHeistStatusComponent : public UActorComponent`
* `UHeistInventoryComponent : public UActorComponent`
* `UHeistInteractionComponent : public UActorComponent`
* `UHeistActionComponent : public UActorComponent`
* `UHeistVisionComponent : public UActorComponent`
* `UHeistCustomizationComponent : public UActorComponent`
* `UHeistNoiseEmitterComponent : public UActorComponent`

Purpose:

* Split gameplay responsibility across server-authoritative components.

Do not implement internal gameplay logic yet.

---

# 4. Inventory And Data

Folder:
`Source/Project_MuseumHeist/Inventory/`

Files:

* `HeistInventoryTypes.h`
* `HeistItemDataTypes.h`

Types:

* `FHeistInventoryItem`
* `FHeistInventoryFastArrayItem`
* `FHeistReplicatedInventory`
* `FHeistQuickSlotState`
* `FHeistItemDataRow`
* `FHeistLootDataRow`
* `FHeistUsableItemDataRow`
* `FHeistSoundPingDataRow`
* `FHeistGuardDataRow`
* `FHeistLootSpawnRow`
* `FHeistVentDataRow`
* `FHeistCustomizationRow`
* `FHeistUITextRow`

Use `FFastArraySerializerItem` and `FFastArraySerializer` for inventory skeleton types.

Do not implement full add, move, remove, rotate, or occupancy algorithms yet.

---

Folder:
`Source/Project_MuseumHeist/Data/`

Files:

* `HeistGameBalanceDataAsset.h`
* `HeistGameBalanceDataAsset.cpp`

Class:

* `UHeistGameBalanceDataAsset : public UDataAsset`

Purpose:

* Central balance values and DataTable references.

---

# 5. World And Interactable

Folder:
`Source/Project_MuseumHeist/World/`

Classes:

* `UHeistInteractable : public UInterface`
* `IHeistInteractable`
* `AHeistInteractableActor : public AActor`
* `AHeistLootActor : public AHeistInteractableActor`
* `AHeistDisplayCaseActor : public AHeistInteractableActor`
* `AHeistVentActor : public AHeistInteractableActor`
* `AHeistTrapActor : public AActor`
* `AHeistGlueTrapActor : public AHeistTrapActor`
* `AHeistThrowableProjectile : public AActor`
* `AHeistCoinProjectile : public AHeistThrowableProjectile`
* `AHeistSmokeProjectile : public AHeistThrowableProjectile`
* `AHeistSmokeCloudActor : public AActor`
* `AHeistLootSpawnPoint : public AActor`
* `AHeistPlayerStart : public APlayerStart`
* `AHeistGuardWaypoint : public AActor`

Do not create:

* `AHeistNoiseTrapActor`
* `ARoyalCrownActor`
* `APaintingActor`
* `AAncientSwordActor`
* `ARareArtifactActor`

Loot must remain data-driven.

---

# 6. AI

Folder:
`Source/Project_MuseumHeist/AI/`

Classes:

* `AHeistGuardCharacter : public ACharacter`
* `AHeistGuardAIController : public AAIController`
* `UHeistGuardStateComponent : public UActorComponent`
* `UHeistPatrolPathComponent : public UActorComponent`
* `UHeistGuardNoiseReactionComponent : public UActorComponent`

Use enum:

* `EHeistGuardState`

Do not implement full AI logic yet.
Do not introduce Behavior Tree.

---

# 7. UI

Folder:
`Source/Project_MuseumHeist/UI/Widgets/`

Classes:

* `UHeistUserWidgetBase : public UUserWidget`
* `UHeistHUDWidget : public UHeistUserWidgetBase`
* `UHeistInventoryWidget : public UHeistUserWidgetBase`
* `UHeistInventorySlotWidget : public UHeistUserWidgetBase`
* `UHeistInventoryItemWidget : public UHeistUserWidgetBase`
* `UHeistQuickSlotWidget : public UHeistUserWidgetBase`
* `UHeistResultWidget : public UHeistUserWidgetBase`
* `UHeistLobbyWidget : public UHeistUserWidgetBase`
* `UHeistInteractionPromptWidget : public UHeistUserWidgetBase`
* `UHeistSoundPingMarkerWidget : public UHeistUserWidgetBase`
* `UHeistRareLootAlertWidget : public UHeistUserWidgetBase`

---

Folder:
`Source/Project_MuseumHeist/UI/ViewModels/`

Classes:

* `UHeistHUDViewModel : public UMVVMViewModelBase`
* `UHeistInventoryViewModel : public UMVVMViewModelBase`
* `UHeistQuickSlotViewModel : public UMVVMViewModelBase`
* `UHeistResultViewModel : public UMVVMViewModelBase`
* `UHeistLobbyViewModel : public UMVVMViewModelBase`
* `UHeistGapTrackerViewModel : public UMVVMViewModelBase`

---

Folder:
`Source/Project_MuseumHeist/UI/DragDrop/`

Classes:

* `UHeistInventoryDragDropOperation : public UDragDropOperation`

---

Folder:
`Source/Project_MuseumHeist/UI/Pool/`

Classes:

* `UHeistSoundPingWidgetPool : public UObject`
* `UHeistPopupWidgetPool : public UObject`

Do not create WBP assets.
Do not implement full binding logic yet.
Do not implement actual drag/drop item movement yet.

---

# 8. Debug

Folder:
`Source/Project_MuseumHeist/Debug/`

Classes:

* `UHeistCheatManager : public UCheatManager`
* `UHeistDebugFunctionLibrary : public UBlueprintFunctionLibrary`

Purpose:

* Future test commands and utility validation.

Do not implement actual cheat commands yet.
Only create placeholder functions if needed.

---

# 9. Build.cs Dependencies

Ensure required dependencies are present if classes require them:

* Core
* CoreUObject
* Engine
* InputCore
* EnhancedInput
* NetCore
* GameplayTags
* UMG
* Slate
* SlateCore
* ModelViewViewModel
* AIModule

Do not add extra dependencies unless required for compilation.

---

# 10. Explicitly Forbidden In Skeleton Phase

Do not implement:

* Full match flow
* Full inventory placement algorithm
* Full FastArray mutation logic
* Full item use logic
* Full projectile behavior
* Full stun logic
* Full Piñata Drop logic
* Full scoring logic
* Full Rare Loot event
* Full Gap Tracker logic
* Full guard AI behavior
* Full Sound Ping behavior
* Full UI binding
* Full drag/drop behavior
* Full session or Steam logic

Do not create:

* Blueprint assets
* Widget Blueprint assets
* DataTable assets
* DataAsset assets
* Maps
* Materials
* Niagara systems
* Animations
