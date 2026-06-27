# Project_MuseumHeist — Codex Instructions

## Project Overview

Project_MuseumHeist is an Unreal Engine 5.8 C++ multiplayer top-down stealth action game.

## Design Source And Scope Priority

Use the project documents in this order:

1. `AGENTS.md` defines engineering policy and current implementation constraints.
2. `ClassManifest.md` defines the C++ types allowed in the current implementation phase.
3. `Museum_Heist_GDD.docx` is the design source for gameplay rules, balance defaults, data schemas, and weekly milestones.

When starting a weekly implementation task, read only the relevant GDD sections and apply them within the current policy and manifest scope.

Do not bulk-implement the complete GDD ahead of the requested week.
If the GDD conflicts with this file or `ClassManifest.md`, update or clarify the policy documents before changing architecture or gameplay code.

Core loop:
4 players connect -> loot artifacts -> inventory weight slows movement -> use Coin / Smoke / Glue Trap to interfere -> stun and drop loot -> escape through vents -> result screen.

v1.0 scope:

* 4-player Listen Server multiplayer, LAN-first
* Lobby -> Loadout -> Ready Countdown -> In-Game -> End match flow
* 4x5 Grid Inventory
* Loot / Weight / Score
* Coin / Smoke Grenade / Glue Trap
* Stun / Pinata Drop
* Rare Loot Event
* Gap Tracker
* Guard FSM
* HUD / Result UI
* Fixed map prototype support
* Royal Crown / Rare Artifact / Painting / Ancient Sword as data-driven v1.0 loot rows

v1.1 or later scope:

* Noise Trap implementation
* Advanced Loadout inventory editing
* Steam Voice
* Security Room interaction
* PCG map generation
* Additional maps
* Cinematic systems
* Advanced guard patterns

Do not implement v1.1 systems unless explicitly requested.

---

## Primary Goal For Current Phase

The initial class-skeleton phase is complete.

The current phase is numbered weekly implementation.

Implement only the explicitly requested `TASK-Wn-###` scope, using the GDD sections
relevant to that task. Do not bulk-implement later tasks or the complete GDD.
Previously created manifest-approved skeletons may now receive gameplay logic when
the active numbered task requires it.

---

## Hard Rules

* Use Unreal Engine 5.8.
* Use C++ for gameplay logic, replicated state, validation, ViewModels, and Widget base classes.
* Use Blueprint Widgets only as layout, visual design, and animation layer.
* Do not put gameplay logic in Blueprint graphs.
* Do not create `.uasset` or `.umap` files.
* Do not modify engine version.
* Do not modify plugins unless explicitly requested.
* Do not modify packaging settings unless explicitly requested.
* Do not introduce unnecessary Manager, Service, Factory, Processor, or Subsystem classes.
* `UHeistCheatManager` is the single manifest-approved exception because it derives from Unreal's `UCheatManager` framework type.
* Do not create one Actor class per loot item.
* Loot must be data-driven through `AHeistLootActor` and DataTable rows.
* Do not implement Noise Trap actor in v1.0.
* Do not implement Steam Voice.
* Do not implement PCG.
* Do not implement Security Room interaction.
* Do not implement cinematic systems.

---

## Naming Rules

Use the `Heist` prefix.

Correct examples:

* `AHeistGameMode`
* `AHeistGameState`
* `AHeistPlayerState`
* `AHeistPlayerCharacter`
* `UHeistInventoryComponent`
* `UHeistHUDViewModel`

Avoid unclear abbreviations:

* Do not use `MHInvComp`
* Do not use `Mgr`
* Do not use `Svc`
* Do not use vague names like `Processor`, `Handler`, `Manager`, `System` unless explicitly approved.

---

## Module And API Macro

The module name is:

`Project_MuseumHeist`

Use the module API macro:

`PROJECT_MUSEUMHEIST_API`

Every exported Unreal class must use this API macro.

---

## Architecture Rules

Server-authoritative flow:

UI input
-> `AHeistPlayerController` Server RPC
-> C++ Component validation
-> replicated state update
-> ViewModel update
-> Widget refresh

Widgets must not directly mutate:

* Inventory items
* Score
* Weight
* Match phase
* Stun state
* Escape state
* QuickSlot state

Widgets may only send requests to `AHeistPlayerController`.

---

## Unreal C++ / Blueprint / Data Responsibility Boundary

Project_MuseumHeist uses a hybrid Unreal workflow.

C++ must define stable gameplay rules, authority, replication, validation, and safe APIs.

Blueprint must assemble editor-facing assets, visual configuration, animation references, input asset references, and tuning-friendly defaults.

DataTable and DataAsset must hold repeated design data and balance values.

Map and level assets must own placement and scene composition.

This policy is criteria-based.

Do not decide responsibility by asking "Can this be done in C++?"

Decide responsibility by asking "Who should own this so the project remains maintainable, testable, and Unreal-friendly?"

### Core Principle

Use this division:

```txt
C++ = Rules, authority, state, validation, replication, stable API
Blueprint = Asset assignment, visual composition, animation, editor defaults, presentation
DataAsset/DataTable = Design data, balance values, item definitions, reusable configuration
Map/Level = Placement, layout, scene composition
Widget Blueprint = Layout, animation, colors, visual presentation
C++ Widget/ViewModel = UI state exposure and data flow
```

Blueprint may configure and assemble systems.

Blueprint must not own authoritative gameplay rules.

C++ may expose hooks and defaults.

C++ must not swallow all editor-facing asset configuration.

### Decision Questions

Before implementing a feature, Codex must decide responsibility using these questions.

#### Use C++ when:

* The logic changes gameplay state.
* The logic must be server-authoritative.
* The logic must replicate.
* The logic validates player requests.
* The logic prevents cheating or invalid state.
* The logic determines score, weight, inventory, stun, trap, escape, match phase, or result.
* The logic must be deterministic across clients.
* The logic is a reusable base API for Blueprint child classes.
* The logic must be unit-testable or audit-friendly.
* The logic is required even when editor assets are missing.

#### Use Blueprint when:

* The work assigns visual assets.
* The work assigns SkeletalMesh, StaticMesh, materials, AnimBP, Niagara, sound, or widget classes.
* The work configures editor-facing defaults for a specific child asset.
* The work creates a playable Blueprint child from a C++ base class.
* The work tunes values that designers may iterate in the editor.
* The work controls visual presentation without changing authoritative gameplay state.
* The work wires asset references such as InputAction, InputMappingContext, WidgetClass, AnimClass, mesh, or material references.
* The work creates visual variants of the same C++ gameplay class.

#### Use DataTable or DataAsset when:

* The data is repeated across many items, actors, abilities, UI labels, or balance entries.
* The data is design-facing and likely to change.
* The data defines loot score, weight, grade, item type, use type, interaction duration, guard tuning, vent tuning, sound ping tuning, or balance constants.
* The data should be editable without recompiling C++.
* The data should be referenced by RowId, DataTableRowHandle, PrimaryDataAsset, or project-defined data assets.

#### Use Map or Level assets when:

* The work places actors in the world.
* The work defines player starts, loot spawn locations, vent positions, guard waypoints, blockout, lighting, or scene composition.
* The work changes `.umap`.

Codex must not modify `.umap` unless explicitly requested.

### C++ Base / Blueprint Child Pattern

Visual or asset-backed gameplay actors should usually follow this pattern:

```txt
C++ base class
-> Blueprint child class
-> asset assignment and editor tuning
-> map placement
```

Examples:

```txt
AHeistPlayerCharacter
-> BP_HeistPlayerCharacter
-> SkeletalMesh / AnimBP / InputAction references / visual defaults assigned in Blueprint

AHeistLootActor
-> BP_HeistLootActor
-> StaticMesh / Material / collision preset / LootDataRow assigned in Blueprint

UHeistUserWidgetBase
-> WBP_HeistHUD / WBP_Inventory / WBP_Result
-> layout, animation, font, color assigned in Widget Blueprint
```

C++ owns the gameplay contract.

Blueprint owns the asset instance.

### Character Responsibility Boundary

`AHeistPlayerCharacter` is a C++ gameplay base class.

C++ should own required gameplay component creation, movement and interaction APIs, replication-safe state access, camera component slots required by the gameplay baseline, safe accessors, server-safe calls, and fallback values.

Blueprint should own SkeletalMesh, AnimBP, character materials, cosmetic meshes, visual scale, animation references, child-specific camera tuning, input asset references, and final playable-pawn editor defaults.

C++ may expose Blueprint-editable properties for values that need tuning.

### Input Responsibility Boundary

C++ owns the input handling path.

Blueprint or DataAsset owns the input asset references.

C++ should own binding code, validation of required references, routing to the controller, character, or components, server RPC entry points when gameplay state is affected, and no-crash behavior when references are missing.

Blueprint or DataAsset should own InputAction assignment, InputMappingContext assignment, project-specific input references, and editor-facing input configuration.

C++ must not assume input assets are always assigned. Missing references must produce a clear warning, remain compile- and PIE-safe, and be reported as required editor setup.

Allowed pattern:

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input")
TObjectPtr<class UInputMappingContext> DefaultMappingContext;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input")
TObjectPtr<class UInputAction> MoveAction;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input")
TObjectPtr<class UInputAction> InteractAction;
```

### Loot Responsibility Boundary

`AHeistLootActor` should be generic.

C++ should own the generic loot actor base, interactable support, availability state, data row reference access, safe fallback behavior, score/weight/grade/identity getters, and later server-authoritative pickup validation.

Blueprint should own StaticMesh, Material, visual scale, asset-specific collision preset, child Blueprint defaults, and visual variants.

DataTable or DataAsset should own loot ID, display name, score, weight, grade, item type, and design-facing interaction duration.

Map should own placed loot actors, spawn point positions, and test layout.

Do not create one C++ class per loot item. Prefer `AHeistLootActor + LootDataRow + BP_HeistLootActor variants` unless a special gameplay reason is explicitly requested.

### UI Responsibility Boundary

C++ should own Widget base classes, ViewModel classes, replicated-state-to-UI data flow, safe binding/update APIs, and necessary formatting helpers.

Widget Blueprint should own layout, fonts, colors, animations, icons, images, visual states, and screen composition.

Widgets must not directly mutate authoritative gameplay state.

### Asset Reference Rule

C++ may declare asset reference properties.

Blueprint, DataAsset, or editor defaults should assign them.

Avoid hardcoded project asset paths in C++ unless the user explicitly asks for an engine-level bootstrap exception.

Avoid making C++ silently choose project-specific content assets.

If a task needs assets, Codex must report the required manual setup instead of hiding it in C++.

### Component Creation Rule

C++ may create required component slots when the component is part of the gameplay contract.

Examples include the inherited CapsuleComponent and CharacterMovementComponent, required SpringArm and Camera components, and interaction, inventory, status, action, vision, and noise-emitter components.

Blueprint should configure asset-specific or variant-specific component properties.

C++ creates the slot. Blueprint fills the content.

### Required Character Component Contract

Components created as required default subobjects of `AHeistPlayerCharacter` must be validated once in `BeginPlay()` with `checkf`.

After that validation, trusted internal gameplay code such as `AHeistPlayerController` may use those required Character components without repeating nullable fallback branches.

This rule applies only to required C++-owned component slots. It does not remove runtime validation for:

* Pawn or PlayerState lifecycle and ownership
* Client-provided RPC targets
* Distance, availability, phase, escaped, stun, or other mutable gameplay state
* Optional Blueprint-assigned assets and input references

If a required Character component is missing, fail immediately as an invalid project configuration instead of continuing with partial gameplay behavior.

### Codex Output Requirement

When a task touches a class that depends on assets or editor setup, Codex must separate the final output into:

```txt
Implemented in C++
Requires Blueprint setup
Requires DataTable/DataAsset setup
Requires Map setup
Not implemented in this task
```

Codex must not imply the task is fully usable in-game if required Blueprint/DataAsset/Map setup remains.

### Manual Setup Checklist Requirement

If a task exposes Blueprint/DataAsset/Map responsibilities, Codex must provide a checklist.

Example:

```txt
1. Create BP_HeistPlayerCharacter from AHeistPlayerCharacter.
2. Assign SkeletalMesh.
3. Assign AnimBP.
4. Assign InputMappingContext.
5. Assign Move / Interact InputAction references.
6. Set BP_HeistPlayerCharacter as the default pawn when map/project settings are allowed.
```

### Blueprint Graph Logic Boundary

Blueprint Graph gameplay logic remains restricted.

Allowed Blueprint work includes asset-reference assignment, defaults, visuals, cosmetic animations, visual-state updates, child Blueprint assets, and Widget Blueprint presentation.

Restricted Blueprint work includes authoritative score changes, inventory mutation, loot pickup confirmation, stun/trap/escape result decisions, match phase transitions, replicated-state authority, and server validation.

If Blueprint needs to trigger gameplay, it should call a C++ API or C++ RPC path.

### Responsibility Summary

Use this rule:

```txt
C++ decides what is allowed.
Blueprint decides how it looks and which assets are used.
Data decides what values are used.
Map decides where it exists.
```

---

## UI Rules

C++ owns:

* ViewModels
* Widget base classes
* input routing
* drag/drop request handling
* widget pooling
* gameplay-facing UI events

WBP owns:

* layout
* fonts
* colors
* images
* animation
* visual-only effects

Blueprint graph logic is not allowed except simple visual events such as:

* play animation
* set visibility
* update visual color
* trigger visual-only feedback

---

## Networking Rules

* Validate all inventory, item use, stun, loot, trap, and escape actions on the server.
* Do not trust client-side inventory data.
* Replicate only the state needed by clients.
* Prefer replicated authoritative state and RepNotify/FastArray updates for persistent gameplay state.
* Use Multicast RPCs only for transient cosmetic events when normal replicated state is not appropriate.
* Prefer event/delegate-driven UI updates.
* Do not use Tick for replicated UI updates unless explicitly justified.
* Keep replicated properties minimal and intentional.
* Use `DOREPLIFETIME` only for properties that are actually needed by clients.

---

## Inventory Rules

Inventory is 4 columns x 5 rows.

Use FastArray for replicated inventory state.

v1.0 inventory ownership and identity:

* `UHeistInventoryComponent` on the player Character/Pawn owns the inventory.
* `AHeistPlayerState` owns result-facing values such as `TotalLootScore`, `FinalScore`, and escaped state.
* Inventory `InstanceId` starts as a component-local monotonically increasing `int32`.
* QuickSlots reference inventory entries by `InstanceId`; they do not duplicate item state.
* Reconsider `FGuid` or a server-global ID only when save/load, persistent ownership, or cross-inventory tracking is required.

Item movement must eventually validate:

* owner
* inventory open state
* stun state
* casting state
* escaped state
* target cell bounds
* occupancy
* rotation state

Current phase:

* Implement inventory behavior only when required by the active numbered task.
* Do not implement later inventory features ahead of their task.

---

## Data Rules

Use stable IDs.

* `ItemId` is an `FName`.
* `ItemId` should match the DataTable RowName.
* GameplayTag is for category, state, and conditional checks.
* DisplayName is for UI text only.

Do not mix:

* RowName
* ItemId
* GameplayTag
* DisplayName

Canonical GDD enum values:

* `EHeistMatchPhase`: `None`, `Lobby`, `Loadout`, `ReadyCountdown`, `InGame`, `End`
* `EHeistItemType`: `None`, `Loot`, `Trap`, `Throwable`, `KeyItem`
* `EHeistLootGrade`: `OneStar`, `TwoStar`, `ThreeStar`, `FourStar`
* `EHeistUseType`: `None`, `Throw`, `PlaceTrap`, `DeployArea`, `Consume`
* `EHeistTargetType`: `None`, `Self`, `WorldLocation`, `ActorHit`, `Area`
* `EHeistSpawnCategory`: `None`, `VaultFixed`, `ExhibitionRoom`, `RareEvent`, `Dropped`
* `EHeistSoundPingType`: `None`, `Footstep`, `GlassBreak`, `CoinImpact`, `NoiseTrap`, `StunHit`
* `EHeistGuardState`: `Patrol`, `Chase`, `Stunned`, `Investigate`
* `EHeistCustomizationType`: `Hat`, `Cloth`, `SkinColor`, `HatColor`, `ClothColor`
* `EHeistZoneId`: `None`, `ZoneA`, `ZoneB`, `ZoneC`, `ZoneD`

GDD balance numbers are initial DataAsset defaults, not gameplay-class constants:

* Match duration: `300.0` seconds
* Vent unlock time: `180.0` seconds
* Rare Loot events: `90.0` and `225.0` seconds
* Base / minimum walk speed: `600.0` / `200.0` cm/s
* Loot speed penalty: `15.0` cm/s per occupied loot cell
* Stun / stun immunity: `3.0` / `2.0` seconds
* Loot / escape / trap cast time: `1.5` / `2.0` / `1.5` seconds
* Shared throwable cooldown: `5.0` seconds
* Gap Tracker threshold: `1000` points

Do not hardcode balance values inside gameplay classes when they belong in `UHeistGameBalanceDataAsset`.

---

## AI Rules

Use a simple C++ FSM first.

v1.0 guard states:

* Patrol
* Chase
* Stunned
* Investigate

Do not introduce Behavior Tree unless explicitly requested.
Do not implement advanced guard behavior in the skeleton phase.

---

## Include And Dependency Rules

* Place exported headers under `Source/Project_MuseumHeist/Public/<Feature>/`.
* Place implementation files under `Source/Project_MuseumHeist/Private/<Feature>/`.
* Use module-relative includes such as `#include "Feature/ClassName.h"`.
* Use forward declarations where possible.
* Keep headers minimal.
* Do not include heavy headers in `.h` files unless necessary.
* Prefer including engine/component headers in `.cpp`.
* Do not create circular dependencies.
* Keep components independent from UI.
* Keep UI independent from gameplay component internals.

---

## C++ Code Organization And Region Standard

Project_MuseumHeist C++ code must prioritize readability, auditability, and stable class structure.

Use `#pragma region` / `#pragma endregion` only when it improves readability.

Regions must be organized by responsibility, not by access specifier.

### Core Principle

The primary grouping unit is the responsibility of the code.

Examples:

```cpp
#pragma region Construction
#pragma endregion

#pragma region Lifecycle
#pragma endregion

#pragma region Movement
#pragma endregion

#pragma region Camera
#pragma endregion

#pragma region Input
#pragma endregion

#pragma region Interaction
#pragma endregion

#pragma region Inventory
#pragma endregion

#pragma region ScoreAndWeight
#pragma endregion

#pragma region Networking
#pragma endregion

#pragma region Replication
#pragma endregion

#pragma region UI
#pragma endregion

#pragma region Debug
#pragma endregion

#pragma region InternalHelpers
#pragma endregion
```

Access specifiers belong inside the responsibility region when needed.

Correct:

```cpp
#pragma region Movement

public:
	void MoveOnGameplayPlane(const FVector2D& MovementInput);

private:
	void ConfigureMovementDefaults();

#pragma endregion
```

Incorrect:

```cpp
public:
#pragma region Movement
	void MoveOnGameplayPlane(const FVector2D& MovementInput);
#pragma endregion

private:
#pragma region Movement
	void ConfigureMovementDefaults();
#pragma endregion
```

The incorrect version splits one responsibility across multiple access sections and makes the class harder to scan.

### Header Organization Standard

In header files, group declarations by responsibility first.

Each region may contain one or more access specifiers.

Correct example:

```cpp
UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistPlayerCharacter();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;

#pragma endregion

#pragma region Movement

public:
	void MoveOnGameplayPlane(const FVector2D& MovementInput);

private:
	void ConfigureMovementDefaults();

#pragma endregion

#pragma region Camera

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USpringArmComponent> CameraSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCameraComponent> TopDownCamera;

#pragma endregion

#pragma region GameplayComponents

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UHeistInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UHeistInteractionComponent> InteractionComponent;

#pragma endregion
};
```

### Source Organization Standard

In `.cpp` files, group method implementations by the same responsibility regions used in the header when practical.

Correct example:

```cpp
#pragma region Construction

AHeistPlayerCharacter::AHeistPlayerCharacter()
{
}

#pragma endregion

#pragma region Lifecycle

void AHeistPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

#pragma endregion

#pragma region Movement

void AHeistPlayerCharacter::MoveOnGameplayPlane(const FVector2D& MovementInput)
{
}

void AHeistPlayerCharacter::ConfigureMovementDefaults()
{
}

#pragma endregion
```

### Region Placement Rules

Do not use regions as decoration.

Use regions only when a file has multiple meaningful responsibility groups.

Do not create one region per function unless the class is exceptionally large and the grouping is justified.

Do not duplicate the same region name multiple times inside one class declaration unless there is a strong reason.

Prefer one contiguous region per responsibility.

### Unreal Reflection Safety Rules

Never place `#pragma region` between an Unreal reflection macro and the declaration it belongs to.

Do not place `#pragma region` between:

* `UCLASS()` and the class declaration.
* `USTRUCT()` and the struct declaration.
* `UENUM()` and the enum declaration.
* `UFUNCTION()` and its function declaration.
* `UPROPERTY()` and its property declaration.
* `UDELEGATE()` and its delegate declaration.
* `#include "FileName.generated.h"` and the declarations that depend on it.

Correct:

```cpp
#pragma region Camera

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCameraComponent> TopDownCamera;

#pragma endregion
```

Incorrect:

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Camera", meta = (AllowPrivateAccess = "true"))
#pragma region Camera
TObjectPtr<class UCameraComponent> TopDownCamera;
#pragma endregion
```

### Behavior Preservation Rule

Region organization must never change behavior.

When applying or updating regions:

* Do not rename classes.
* Do not rename functions.
* Do not rename variables.
* Do not move files.
* Do not change architecture.
* Do not change access specifiers unless explicitly required by the requested task.
* Do not change RPC declarations.
* Do not change replication behavior.
* Do not change UPROPERTY metadata.
* Do not change UFUNCTION metadata.
* Do not change constructor initialization behavior.
* Do not change gameplay logic.
* Do not change generated header include order.

### Expected Codex Output For Region-Only Tasks

For region-only organization tasks, Codex must report:

```txt
Modified files
Region groups added or corrected
Confirmation that regions are responsibility-first
Confirmation that behavior was not changed
Confirmation that UPROPERTY/UFUNCTION/GENERATED_BODY placement was preserved
Compile result
Formal Test Log decision
```

Formal Test Log:
Not required.

Reason:
Region-only organization does not independently verify multiplayer authority, replication, ownership, Gate readiness, or bug-fix validity.

---

## Current Phase Output Rules

When creating class skeletons:

* Do not implement internal gameplay logic.
* Do not add complex algorithms.
* Do not add temporary fake gameplay.
* Do not create Blueprint assets.
* Do not create DataTable assets.
* Do not create Maps.
* Do not create Materials.
* Do not create Widgets as `.uasset`.

Allowed:

* Create `.h` and `.cpp`.
* Create enums.
* Create structs.
* Create UCLASS/USTRUCT/UENUM declarations.
* Create constructors.
* Create placeholder methods.
* Add minimal replication setup.
* Add TODO comments.
* Add minimal log categories.
* Add minimal Build.cs dependencies if required.

---

## Required Response Format

After making changes, report:

1. Files created
2. Files modified
3. Classes added
4. Whether gameplay logic was intentionally left empty
5. Whether manual Unreal Editor setup is required
6. Whether the project should compile
7. Any Build.cs changes

Do not claim packaging success unless packaging was actually run.
Do not claim editor validation success unless the editor was actually opened.

---

## Notion ID And Test Report Standard

All Project_MuseumHeist Notion database IDs must use the same structure:

```txt
<TYPE>-W<WeekNumber>-<SequenceNumber>
```

Examples:

```txt
TASK-W1-001
TEST-W1-001
BUG-W1-001
DEC-W1-001
```

### ID Prefix Rules

Use the following prefixes:

| Database          | Prefix | Example       |
| ----------------- | ------ | ------------- |
| Weekly Task Board | `TASK` | `TASK-W1-001` |
| Test Log          | `TEST` | `TEST-W1-001` |
| Bug Issue Log     | `BUG`  | `BUG-W1-001`  |
| Decision Log      | `DEC`  | `DEC-W1-001`  |

### Invalid ID Formats

Do not use mixed ID styles.

Invalid:

```txt
W1-001
W1-T001
W2-LOG-01
DEC-W1-001 mixed with W1-001
```

Correct:

```txt
TASK-W1-001
TEST-W1-001
BUG-W1-001
DEC-W1-001
```

### Notion Title Rules

Notion database title properties must use IDs.

Weekly Task Board:

* Title / `작업 ID` = `TASK-Wn-###`
* Description / `작업 항목` = human-readable task name

Test Log:

* Title / `테스트ID` = `TEST-Wn-###`
* Description / `테스트 항목` = human-readable test name

Bug Issue Log:

* Title / `이슈ID` = `BUG-Wn-###`
* Description / `제목` = human-readable bug title

Decision Log:

* Title / `결정ID` = `DEC-Wn-###`
* Description / `결정 사항` = human-readable decision title

This rule exists so relation search works by ID and all databases use the same naming structure.

### Test Report Body Format

Every test report must use this structure:

````md
# <Test Report Title>

## 테스트 개요

- 테스트ID:
- 프로젝트: Project_MuseumHeist
- 엔진: Unreal Engine 5.8
- 테스트 일자:
- 테스트 항목:
- 테스트 환경:
- 빌드:
- 최종 결과:

## 테스트 목적

<What this test verifies.>

## 기대 결과

<Expected behavior before running the test.>

## 검증 항목

| 검증 항목 | 실제 결과 | 판정 |
|---|---|---|
| <Item> | <Observed result> | PASS/FAIL/BLOCKED |

## 주요 로그

```txt
<Paste only the important logs here.>
```

## 참고사항

<Known caveats, interpretation notes, PIE-specific behavior, warnings, or non-blocking issues.>

## 최종 결론

<State the final judgment clearly.>

따라서 <related TASK-Wn-### tasks>는 완료 처리한다.
````

### Test Result Rules

- Keep reports factual.
- Do not exaggerate results.
- Do not claim gameplay completion if only baseline behavior was verified.
- Distinguish between:
  - compile success
  - framework hookup success
  - multiplayer connection success
  - gameplay feature completion
- Mention non-blocking warnings as `Known Warning`.
- If the test passed with caveats, mark result as `Pass` and explain caveats in `참고사항`.
- If the result blocks the next task, mark result as `Blocked`.
- If the test was not executed, mark result as `Not Tested`.
- Do not hide failures.

## Test Log Recording Decision Standard

Not every implementation task requires a formal Notion Test Log entry.

Formal Test Logs are reserved for verification results that prove multiplayer reliability, server authority, replication correctness, integration stability, weekly Gate readiness, or bug fix validity.

Small implementation checks should remain as compile checks, PIE smoke checks, or Codex task output notes.

### Core Principle

Create a formal Test Log only when the result answers a project-level verification question.

Do not create a formal Test Log just because a task was implemented.

A formal Test Log must prove at least one of the following:

* Multiplayer behavior is correct.
* Server authority is correct.
* Replicated state is correct.
* Player ownership is correct.
* Multiple systems work together correctly.
* A weekly Gate can be passed.
* A Critical or Major bug was verified.
* A gameplay result remains consistent across clients.

### Formal Test Log Required

Create a formal Notion Test Log when the task verifies one or more of these conditions:

1. Multiplayer connection or session stability

   * Example criteria:

     * Multiple players can join.
     * Players remain connected.
     * Logout or disconnect behavior is handled correctly.

2. Server-authoritative gameplay

   * Example criteria:

     * The server is the only authority deciding loot pickup.
     * The server is the only authority deciding stun, escape, score, match state, or trap result.

3. Replication correctness

   * Example criteria:

     * PlayerState values replicate correctly.
     * Inventory, score, weight, match phase, stun state, or escape state is visible consistently across clients.

4. Ownership correctness

   * Example criteria:

     * Each client controls only its own Pawn.
     * PlayerController, PlayerState, Pawn, components, and UI state are connected to the correct owner.

5. Concurrency or conflict resolution

   * Example criteria:

     * Two players interact with the same loot at the same time.
     * Multiple players trigger the same trap.
     * Multiple players attempt escape.
     * Disconnect occurs during an interaction.
     * Only one deterministic result is accepted.

6. Cross-system integration

   * Example criteria:

     * Character movement, interaction, loot, inventory, score, weight, and UI work together.
     * A gameplay loop works across multiple components or actors.

7. Weekly Gate validation

   * Example criteria:

     * The weekly objective is complete.
     * Required systems for the week are integrated.
     * Gate-critical checks passed.

8. Critical or Major bug verification

   * Example criteria:

     * A blocking bug was reproduced and fixed.
     * A regression-prone bug was verified after a fix.
     * A multiplayer desync, crash, or data loss issue was fixed.

### Formal Test Log Not Required

Do not create a formal Notion Test Log for small local checks.

A formal Test Log is not required for:

1. Simple compile success after a small code change.
2. Basic local movement confirmation.
3. Camera value confirmation.
4. Mouse cursor visibility by itself.
5. A single input binding check.
6. A single component constructor check.
7. A simple C++ refactor with no multiplayer behavior change.
8. Pure UI layout or visual polish with no gameplay state mutation.
9. Comment, README, or documentation wording changes.
10. Log formatting changes.
11. Local editor-only checks.
12. Non-authoritative single-player-only smoke checks.

These results should be reported only in the Codex task output.

### Smoke Check Standard

A smoke check is a lightweight local verification.

Smoke checks may include:

* Project compiles.
* PIE opens.
* Existing 4-player spawn baseline is not obviously broken.
* Local player can possess the Pawn.
* Local input responds.
* Camera follows the local Pawn.
* No obvious runtime error appears in the Output Log.

Smoke checks are not formal Test Logs.

### Task Output Standard

For normal implementation tasks that do not require a formal Test Log, Codex must report:

* Modified files
* Implementation summary
* Compile result
* PIE smoke-check steps
* Known warnings or caveats
* Formal Test Log decision

Use this wording when a formal Test Log is not required:

Formal Test Log: Not required.
Reason: This task is covered by compile/PIE smoke check and does not independently verify multiplayer authority, replication, ownership, Gate readiness, or bug-fix validity.

Use this wording when a formal Test Log is required:

Formal Test Log: Required.
Reason: This task verifies multiplayer integration, server authority, replication correctness, ownership correctness, Gate readiness, or bug-fix validity.

### Decision Flow

Before creating or recommending a formal Test Log, ask:

1. Does this verify behavior with multiple players?
2. Does this verify that the server is the authority?
3. Does this verify replicated state across clients?
4. Does this verify correct ownership?
5. Does this verify simultaneous player actions or conflict resolution?
6. Does this verify multiple systems working together?
7. Does this determine whether a weekly Gate passes?
8. Does this verify a Critical or Major bug fix?

If all answers are no, do not create a formal Test Log.

### Aggregation Rule

Small implementation tasks should be aggregated into a later formal integration test when they are part of the same feature chain.

Example:

* Movement setup alone does not require a formal Test Log.
* Camera setup alone does not require a formal Test Log.
* Cursor setup alone does not require a formal Test Log.
* Interaction routing alone does not require a formal Test Log.

But when those pieces are verified together in a multiplayer environment as a character control baseline, a formal Test Log may be required.

The Test Log should describe the integrated multiplayer behavior, not each small implementation task.

### Weekly Gate Rule

A weekly Gate can be marked `Pass` only when:

1. Required tasks are complete.
2. Required tests are recorded in the test log.
3. Critical failures are either resolved or explicitly deferred.
4. The weekly decision / summary log is recorded when required.
