# Project Museum Heist - Third-Party Notices Index

Baseline: 2026-08-18
Related manifest: [SHIPPING_ASSET_MANIFEST.md](SHIPPING_ASSET_MANIFEST.md)

This file is a durable index to source and license evidence for project-added content. It does not replace official license texts, Unreal Engine generated notices, or legal review. Status descriptions report what is present locally; they do not assert worldwide copyright or license validity.

## Rev14 Shipping Scope Note

- Public v1 is now a 2~4 Player, Surface Forgery-centered release target with Patrol Guard, CCTV, and high-value Painting Laser Hold.
- Object Assembly is preserved as Deferred Expansion repository content. Its SourceArt, generator, C++/data contracts, Blueprint Shell and provenance record are not deleted, but Object Assembly must remain inactive in the v1 Runtime, Release Maps, Player-facing UI, Result, Cook and release Gate.
- This scope decision is not proof that the current package already excludes every Object Assembly reference. A fresh Rev14 cook/reference audit remains `PENDING`.
- No final third-party CCTV or Laser asset has been identified in this index. If a Marketplace/Fab, web, audio-library, font, icon, texture, model, Niagara, or other external asset is selected, its exact source, applicable terms and required notice must be recorded before release.
- Historical W6/W7 and prior package evidence predates Rev14 and is not reclassified as proof of the new shipping boundary.

## TENADA Font

- Component: TENADA Korean display font
- Project use: UI headings and primary display typography
- Runtime assets: `Content/Assets/UI/Fonts/FF_TENADA.uasset`, `Content/Assets/UI/Fonts/F_TENADA.uasset`
- Source font: [Tenada.ttf](../SourceArt/UI/Fonts/TENADA/Tenada.ttf)
- Upstream page recorded by the project: <https://en.tenada.co.kr/Font>
- Project source record: [SourceArt/UI/Fonts/TENADA/README.md](../SourceArt/UI/Fonts/TENADA/README.md)
- Preserved upstream notice and license: [reedme_tenada_font.pdf](../SourceArt/UI/Fonts/TENADA/reedme_tenada_font.pdf)
- License named in the preserved upstream file: SIL Open Font License, Version 1.1.

The preserved four-page PDF contains the author note and full OFL 1.1 text. It states that redistributed Font Software copies must contain the copyright notice and license in a user-viewable stand-alone, header, or metadata form. Package sign-off must identify the actual shipped location of this notice and license.

Package evidence: `PENDING`.

## Surface Forgery Artwork Sources

The runtime references are original gameplay reinterpretations derived from the compositions recorded in these manifests. The manifests, not this summary, are the durable source-by-source records.

| Pool | Source record | Local rights record | Notice status |
|---|---|---|---|
| M01 | [M01_SourceManifest.json](../SourceArt/Forgery/M01/M01_SourceManifest.json) | 12 source URLs; all 12 per-entry `rights` values missing | `RELEASE_BLOCKER` |
| M02 | [M02_SourceManifest.json](../SourceArt/Forgery/M02/M02_SourceManifest.json) | 12 source URLs; all 12 labelled `Public Domain` | `RECORDED_LOCAL` |
| M03 | [M03_SourceManifest.json](../SourceArt/Forgery/M03/M03_SourceManifest.json) | 12 source URLs; four entries say only `No Copyright - United States` | `RELEASE_BLOCKER` |

M01 must receive source-specific rights records. For M03, the four affected slots are `M03_GeometricAbstract_02`, `M03_GeometricAbstract_03`, `M03_GeometricAbstract_04`, and `M03_GeometricAbstract_05`. Do not describe those four as cleared for global distribution until supporting evidence or replacements are recorded.

The fallback texture `T_Forgery_SunArchWave` has durable import metadata at [T_Forgery_SunArchWave.asset.json](../SourceArt/Forgery/T_Forgery_SunArchWave.asset.json), but its local PNG source is ignored by Git and its origin/rights statement is missing. Its notice status is `RELEASE_BLOCKER`.

## Epic StarterContent

- Local content root: `Content/Assets/StarterContent/`
- Confirmed project use: M01 graybox architecture and five Loose Loot visual definitions in [DT_LootDataRow.json](../DataTableImports/DT_LootDataRow.json)
- Local official license text or reviewed terms snapshot: not present
- Applicable terms review: `PENDING`

Before release, add a dated reference to the official Epic/Unreal terms reviewed for packaged game distribution and record who performed the review. This file intentionally does not invent a license name or URL from the folder name alone.

## Epic UE5 Mannequins

- Local content root: `Content/Assets/Mannequins/`
- Installed source pack: `D:/UE_5.8/Templates/TemplateResources/High/Characters/Content/Mannequins/`
- Identified pack: UE 5.8 `Mannequin shared resource high` (Manny/Quinn)
- Confirmed project use: `SKM_Manny_Simple` full-body player mesh and `ABP_Unarmed` baseline locomotion
- Local official license text or reviewed terms snapshot: not present
- Applicable terms review: `PENDING`

The local 128-file pack matches the installed UE 5.8 template resource by filename. Review and record the applicable Epic/Unreal terms together with StarterContent before release sign-off; this index does not infer a separate license from the pack name.

## Project-Original Assets Recorded Outside This Notice

The following assets are currently documented as project-original rather than third-party. Their source records are linked here so the exclusion is auditable.

- W7 floor-plan textures and alert audio: [SourceArt/W7/README.md](../SourceArt/W7/README.md) and [GenerateW7PresentationAssets.ps1](../SourceArt/W7/GenerateW7PresentationAssets.ps1)
- Object Assembly Sculpture/Ceramic meshes: [SourceArt/ObjectAssembly/README.md](../SourceArt/ObjectAssembly/README.md) and [rebuild_object_assembly_content.py](../SourceArt/ObjectAssembly/rebuild_object_assembly_content.py)

The Object Assembly README, deterministic generator, and 14 source OBJ files are version-controlled and document project-generated procedural source art. Rev14 preserves that provenance record even though the feature is Deferred. The separate v1 requirement is to prove that Release Maps and the final staged cook do not pull Object Assembly runtime assets into the shipping build.

CCTV and Laser Hold do not yet have a final asset inventory. They therefore have no third-party notice entry beyond this `PENDING` declaration; placeholder use must not be interpreted as release clearance.

## Engine and Plugin Notices

Unreal Engine, bundled runtime, Online Subsystem, and plugin notices are outside this project-added asset index. Preserve the notices generated by the selected Unreal packaging configuration and verify them against the final staged build. A local `Build/Windows/NOTICES.txt` from an older build is not durable evidence because `Build/Windows` is ignored and regenerated.

## Final Package Record

Complete these fields during the release candidate audit:

- Package build identifier: `PENDING`
- Package date: `PENDING`
- TENADA notice/license shipped location: `PENDING`
- Epic StarterContent terms reference and review date: `PENDING`
- Epic UE5 Mannequin terms reference and review date: `PENDING`
- Rev14 Object Assembly Release Map/cook exclusion: `PENDING`
- Rev14 CCTV/Laser asset provenance and notice inventory: `PENDING`
- M01 rights review result: `BLOCKED`
- M03 global-scope review result: `BLOCKED`
- Surface fallback texture provenance: `BLOCKED`
- Final reviewer: `PENDING`
