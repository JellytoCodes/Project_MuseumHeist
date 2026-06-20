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

The current phase is class skeleton generation only.

Create headers and source files.
Create Unreal reflection declarations.
Create minimal constructors.
Create placeholder methods only when needed for compile and later extension.
Do not implement gameplay logic yet.

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

* Create the types and placeholder methods only.
* Do not implement full movement logic yet.

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

### Weekly Gate Rule

A weekly Gate can be marked `Pass` only when:

1. Required tasks are complete.
2. Required tests are recorded in the test log.
3. Critical failures are either resolved or explicitly deferred.
4. The weekly decision / summary log is recorded when required.
