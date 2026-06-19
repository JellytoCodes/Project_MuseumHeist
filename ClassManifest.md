# Project_MuseumHeist — Codex Instructions

## Project Overview

Project_MuseumHeist is an Unreal Engine 5.8 C++ multiplayer top-down stealth action game.

Core loop:
4 players connect -> loot artifacts -> inventory weight slows movement -> use Coin / Smoke / Glue Trap to interfere -> stun and drop loot -> escape through vents -> result screen.

v1.0 scope:

* Listen Server multiplayer
* 4x5 Grid Inventory
* Loot / Weight / Score
* Coin / Smoke Grenade / Glue Trap
* Stun / Pinata Drop
* Rare Loot Event
* Gap Tracker
* Guard FSM
* HUD / Result UI
* Fixed map prototype support

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
* Prefer event/delegate-driven UI updates.
* Do not use Tick for replicated UI updates unless explicitly justified.
* Keep replicated properties minimal and intentional.
* Use `DOREPLIFETIME` only for properties that are actually needed by clients.

---

## Inventory Rules

Inventory is 4 columns x 5 rows.

Use FastArray for replicated inventory state.

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
