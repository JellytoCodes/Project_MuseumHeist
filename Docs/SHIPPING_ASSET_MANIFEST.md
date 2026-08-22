# Project Museum Heist - Shipping Asset Manifest

Baseline: 2026-08-18
Work item: TASK-W9-007
Scope: Rev14 v1 shipping target for Surface Forgery, Patrol/CCTV/Laser presentation, TENADA font, W7 floor-plan/audio assets, and Epic StarterContent/UE5 Mannequin content. Object Assembly art remains recorded as preserved Deferred Expansion repository content and is not part of the v1 shipping target.

This manifest records evidence that exists in the repository or the current workspace. It is not a legal opinion and does not by itself establish that an asset is cleared for every territory or distribution channel. An unlisted asset must not be treated as cleared merely because it is absent from this document.

## Rev14 Release Scope Boundary

- Public v1 supports 2~4 Player Contract Runs centered on Painting Surface Forgery.
- The required security set is existing Patrol Guard plus CCTV and high-value Painting Laser Hold. CCTV/Laser runtime assets have not yet been selected, implemented, or audited; any new Mesh, Material, Texture, Niagara, Audio, Icon, Font, or source file must be added to this manifest before release sign-off.
- Object Assembly is **Deferred Expansion**, not removed. Its C++/Enum/Struct/DataTable/Blueprint Shell, SourceArt, generator, and project-original provenance records remain in the repository.
- v1 Release Contract, Runtime Assignment, Release Map, Player-facing UI, Result, Cook and QA/Release Gate must not activate Object Assembly. A fresh Rev14 cook/reference audit is still required; this document does not claim that the current package already satisfies that boundary.
- Historical packages and W6/W7 evidence predate this release rebaseline and remain valid only as evidence of the implementation tested at that time.

## Status Legend

| Status | Meaning |
|---|---|
| `VERIFIED_LOCAL` | Source, generator, notice, or provenance statement exists locally and matches the recorded project use. This is not legal clearance. |
| `REVIEW_REQUIRED` | Provenance is identifiable, but the durable license record, package inclusion, tracking state, or distribution scope still needs confirmation. |
| `RELEASE_BLOCKER` | The current repository evidence is insufficient for release sign-off. Resolve before TASK-W9-007 or the release license gate is marked complete. |

## Release Gate Summary

| Asset group | Current use | Local evidence | Status | Required action |
|---|---|---|---|---|
| M01 Surface Forgery pool | 12 timed painting templates | 12 source URLs and artwork metadata; 12 of 12 entries have no `rights` value | `RELEASE_BLOCKER` | Record a source-specific rights statement and review note for every M01 entry. Replace any source that cannot be supported. |
| M02 Surface Forgery pool | 12 timed painting templates | 12 source URLs; every entry is labelled `Public Domain` in the local manifest | `VERIFIED_LOCAL` | Retain the manifest and source URLs. Re-check the statements during final release review. |
| M03 Surface Forgery pool | 12 timed painting templates | 12 source URLs; eight entries have `Public Domain` or `CC0 Public Domain Designation`; four entries say only `No Copyright - United States` | `RELEASE_BLOCKER` | Establish the intended global distribution basis for the four US-only entries, or replace them. |
| Surface fallback texture | Default painting material texture `T_Forgery_SunArchWave` | Runtime asset and asset metadata exist; source PNG is ignored by Git and the metadata has no origin or rights statement | `RELEASE_BLOCKER` | Add a durable project-original or third-party provenance statement and decide whether the source PNG must be tracked. |
| Patrol/CCTV/Laser presentation | Rev14 v1 security target; Patrol retained, CCTV/Laser implementation pending | No Rev14 CCTV/Laser release asset inventory or provenance record yet | `REVIEW_REQUIRED` | Select or create the assets, record source/license evidence, and verify the final cooked inventory. Do not infer approval from placeholder or Engine content. |
| Object Assembly gallery meshes | Deferred Expansion repository content; no v1 runtime/cook use intended | 14 tracked OBJ sources, deterministic generator, and project-generated provenance statement | `VERIFIED_LOCAL` | Retain the tracked source art, generator, README, and runtime source assets. Separately prove Object Assembly Release Map references and cooked packages are zero in the Rev14 RC. |
| TENADA | Korean display font in UI | TTF, upstream URL, four-page upstream notice, and SIL Open Font License 1.1 text are stored locally | `REVIEW_REQUIRED` | Include the required notice and license in the distributed package or another user-viewable location, then record package evidence. |
| W7 floor plans | M01/M02/M03 full-screen map textures | Deterministic generator, three generated PNGs, and project-original declaration | `VERIFIED_LOCAL` | Retain generator, README, and generated sources. |
| W7 alert audio | Suspense and alarm loops | Deterministic generator, two generated WAVs, and project-original declaration | `VERIFIED_LOCAL` | Retain generator, README, and generated sources. |
| Title menu generated UI | Shared Normal/Hovered/Pressed button brushes and text-free logo emblem | Four tracked RGBA PNG sources, generation prompts and local alpha validation | `VERIFIED_LOCAL` | Retain the source README and confirm the imported runtime textures are included in the final UI cook. |
| Epic StarterContent | Graybox architecture and five Loose Loot visual definitions | 264 tracked assets under the Epic StarterContent folder; live map/DataTable references verified | `REVIEW_REQUIRED` | Record the applicable Epic/Unreal content terms, review date, and permitted distribution scope. Do not treat the folder name alone as license evidence. |
| Epic UE5 Mannequins | Player full-body mesh and baseline locomotion AnimBP | 128 assets copied from the installed UE 5.8 High Mannequin template resource; exact local source pack identified | `REVIEW_REQUIRED` | Record the applicable Epic/Unreal content terms, review date, and permitted distribution scope together with StarterContent. |

## Surface Forgery Art

### M01 - Classical

- Runtime pool: `Content/Data/Forgery/Textures/M01/`
- Durable source manifest: [M01_SourceManifest.json](../SourceArt/Forgery/M01/M01_SourceManifest.json)
- Per-asset metadata: `SourceArt/Forgery/M01/*.asset.json`
- Recorded inventory: 12 entries, 12 source URLs, 0 per-entry rights values.
- Manifest-level art direction says the pool is based on public-domain real artworks, but that general statement does not replace a source-specific rights record.

Release blocker: add a `rights` value and, where needed, a short jurisdiction or review note to all 12 entries. This document does not infer a rights status from artist death dates, artwork age, or museum ownership.

### M02 - Moonlit

- Runtime pool: `Content/Data/Forgery/Textures/M02/`
- Durable source manifest: [M02_SourceManifest.json](../SourceArt/Forgery/M02/M02_SourceManifest.json)
- Per-asset metadata: `SourceArt/Forgery/M02/*.asset.json`
- Recorded inventory: 12 entries, 12 source URLs, 12 entries labelled `Public Domain`.

The status above means only that the local manifest is complete and internally consistent. Final release review should preserve the URLs and confirm that the recorded statements still support the intended distribution.

### M03 - Glasshouse

- Runtime pool: `Content/Data/Forgery/Textures/M03/`
- Durable source manifest: [M03_SourceManifest.json](../SourceArt/Forgery/M03/M03_SourceManifest.json)
- Per-asset metadata: `SourceArt/Forgery/M03/*.asset.json`
- Recorded inventory: 12 entries and 12 source URLs.

The following entries require a distribution-scope decision because the manifest says only `No Copyright - United States`:

| Slot | Work | Artist | Recorded year | Source |
|---|---|---|---|---|
| `M03_GeometricAbstract_02` | Simultaneous Composition | Theo van Doesburg | 1929 | [Yale University Art Gallery](https://artgallery.yale.edu/collections/objects/49581) |
| `M03_GeometricAbstract_03` | Sunrise III | Arthur Dove | 1936-1937 | [Yale University Art Gallery](https://artgallery.yale.edu/collections/objects/46733) |
| `M03_GeometricAbstract_04` | Proun 99 | El Lissitzky | ca. 1923-1925 | [Yale University Art Gallery](https://artgallery.yale.edu/collections/objects/51116) |
| `M03_GeometricAbstract_05` | Multicolored Circle | Wassily Kandinsky | 1921 | [Yale University Art Gallery](https://artgallery.yale.edu/collections/objects/43960) |

Release blocker: do not convert the US-only wording into a global public-domain assertion without supporting evidence. Record the reviewed distribution basis or replace the four sources.

### Surface Fallback Texture

- Runtime asset: `Content/Data/Forgery/Textures/T_Forgery_SunArchWave.uasset`
- Durable metadata: [T_Forgery_SunArchWave.asset.json](../SourceArt/Forgery/T_Forgery_SunArchWave.asset.json)
- Local source named by the metadata: `SourceArt/Forgery/T_Forgery_SunArchWave.png`
- Runtime consumers include the painting surface material under `Content/Assets/Art/SurfaceForgery/Materials/`.

The PNG exists in the current workspace but is excluded by the repository's `SourceArt/**/*.png` ignore rule. The metadata records palette and import behavior but no creator, external source, or rights statement. Treat this as unresolved until provenance is recorded.

## Object Assembly Art — Deferred Expansion

- Runtime meshes: `Content/Assets/Art/ObjectAssembly/Sculpture/` and `Content/Assets/Art/ObjectAssembly/Ceramic/`
- Source README: [SourceArt/ObjectAssembly/README.md](../SourceArt/ObjectAssembly/README.md)
- Deterministic generator: [rebuild_object_assembly_content.py](../SourceArt/ObjectAssembly/rebuild_object_assembly_content.py)
- Source geometry: `SourceArt/ObjectAssembly/Meshes/` - 14 OBJ files.

The local README states that the geometry is project-generated procedural source art and that no external mesh source is embedded. All 14 OBJ files, the deterministic generator, and the README are tracked by Git, so the provenance record is durable in the repository.

Rev14 does not authorize deleting these sources or their dormant implementation contracts. It changes only the v1 shipping boundary: Object Assembly must not be eligible for a v1 Contract Assignment, placed or referenced by a Release Map, constructed by Player-facing UI/Result, or pulled into the final cook. Because the existing local runtime assets still exist and the last packaged-build evidence predates Rev14, a fresh hard-reference and staged-package audit is required before this exclusion can be marked PASS.

## Rev14 Patrol, CCTV And Laser Assets

- Patrol Guard remains part of the v1 gameplay baseline. This revision does not by itself change the previously recorded Character/Mannequin or project asset provenance.
- CCTV and Laser Hold are required v1 gameplay presentation targets, but no final project-owned or third-party asset set has been selected in this manifest.
- Placeholder Engine assets, Marketplace/Fab downloads, generated files, and externally sourced sounds or textures are not release-cleared by being referenced in a Blueprint.
- When the implementation lands, record the exact source and runtime paths, creator/vendor, license or project-original declaration, required notices, and final cook evidence here.

Current Rev14 status: `DOCUMENTED / IMPLEMENTATION_AND_ASSET_AUDIT_PENDING`.

## TENADA Font

- Runtime assets: `Content/Assets/UI/Fonts/FF_TENADA.uasset` and `F_TENADA.uasset`
- Local source font: [Tenada.ttf](../SourceArt/UI/Fonts/TENADA/Tenada.ttf)
- Project record: [TENADA README](../SourceArt/UI/Fonts/TENADA/README.md)
- Preserved upstream notice and license: [reedme_tenada_font.pdf](../SourceArt/UI/Fonts/TENADA/reedme_tenada_font.pdf)
- Recorded upstream page: <https://en.tenada.co.kr/Font>
- License identified in the preserved upstream file: SIL Open Font License 1.1.

The preserved PDF contains an author note and the full SIL Open Font License 1.1 text. Its conditions include retaining the copyright notice and license with redistributed Font Software. Final package verification must show where a user can view that notice and license. This manifest does not decide whether a particular packaging method satisfies the license.

## W7 Project-Original Presentation Assets

- Provenance record: [SourceArt/W7/README.md](../SourceArt/W7/README.md)
- Deterministic generator: [GenerateW7PresentationAssets.ps1](../SourceArt/W7/GenerateW7PresentationAssets.ps1)
- Source outputs:
  - `SourceArt/W7/Generated/T_FloorPlan_M01.png`
  - `SourceArt/W7/Generated/T_FloorPlan_M02.png`
  - `SourceArt/W7/Generated/T_FloorPlan_M03.png`
  - `SourceArt/W7/Generated/SW_HeistSuspenseLoop.wav`
  - `SourceArt/W7/Generated/SW_HeistAlarmLoop.wav`
- Runtime outputs:
  - `Content/Assets/UI/Map/T_FloorPlan_M01.uasset`
  - `Content/Assets/UI/Map/T_FloorPlan_M02.uasset`
  - `Content/Assets/UI/Map/T_FloorPlan_M03.uasset`
  - `Content/Assets/Audio/W7/SW_HeistSuspenseLoop.uasset`
  - `Content/Assets/Audio/W7/SW_HeistAlarmLoop.uasset`

The README states that these are project-original schematic textures and procedurally generated PCM audio with no embedded third-party visual or audio material. Source, generator, and runtime assets are all present locally.

## Title Menu Button Textures

- Source record: [SourceArt/UI/Title/README.md](../SourceArt/UI/Title/README.md)
- Source PNGs:
  - `SourceArt/UI/Title/T_TitleButton_Normal.png`
  - `SourceArt/UI/Title/T_TitleButton_Hovered.png`
  - `SourceArt/UI/Title/T_TitleButton_Pressed.png`
  - `SourceArt/UI/Title/T_TitleLogo_Emblem.png`
- Intended runtime root: `Content/Assets/UI/Title/`

The three text-free button backgrounds and the text-free logo emblem were generated specifically for the project with the built-in OpenAI `imagegen` tool. Their source record preserves the prompts, rejected-draft note and RGBA alpha validation. Final release verification must still confirm that the imported runtime textures are the versions referenced by the Title Widget and included in the cooked UI package.

## Epic StarterContent

- Content root: `Content/Assets/StarterContent/`
- Current tracked count: 264 files.
- Confirmed shipping-facing references:
  - [DT_LootDataRow.json](../DataTableImports/DT_LootDataRow.json) uses StarterContent shapes and materials for five Loose Loot visuals.
  - `Content/Maps/M01_ClassicalPrototype.umap` uses StarterContent architecture.
- Developer-only reference:
  - `Content/Maps/SandBoxMap.umap` also uses StarterContent architecture/materials but is not listed in `MapsToCook`.

The asset origin is identifiable from the Epic StarterContent directory, but no applicable Epic license/EULA snapshot or reviewed terms record is stored in this repository. Before release sign-off, record the reviewed official terms, review date, reviewer, and the conclusion for packaged game distribution. This is a documentation requirement, not a statement that the current use is prohibited.

## Epic UE5 Mannequins

- Content root: `Content/Assets/Mannequins/`
- Current local count: 128 files.
- Installed source pack: `D:/UE_5.8/Templates/TemplateResources/High/Characters/Content/Mannequins/`
- Source pack identity: UE 5.8 `Mannequin shared resource high` (Manny/Quinn mesh, skeleton, animation, IK and Control Rig content).
- Current player presentation references:
  - Mesh: `Content/Assets/Mannequins/Meshes/SKM_Manny_Simple.uasset`
  - Locomotion AnimBP: `Content/Assets/Mannequins/Anims/Unarmed/ABP_Unarmed.uasset`

The 128 project files were matched one-for-one by filename with the installed UE 5.8 template resource. They are kept as a self-contained imported pack under `Content/Assets` so its internal animation and rig dependencies are not split across project-owned folders. As with StarterContent, final release sign-off still requires a dated review of the applicable official Epic/Unreal terms and packaged-game distribution scope.

## Release Sign-off Checklist

- [ ] All 12 M01 manifest entries have reviewed per-source rights values.
- [ ] The four M03 US-only entries have a documented global distribution basis or replacements.
- [ ] `T_Forgery_SunArchWave` has durable creator/source/rights provenance.
- [x] `SourceArt/ObjectAssembly/` is version-controlled and its project-original statement is present.
- [ ] Release Maps, v1 Contract data, Player-facing UI/Result and final cooked packages contain no active Object Assembly reference or package; Deferred source/data/Shell remain preserved.
- [ ] Final CCTV/Laser Mesh, Material, Texture, VFX, Audio and Icon inventory has source/license or project-original evidence.
- [ ] The final package exposes the TENADA notice and SIL Open Font License 1.1 text in a user-viewable form.
- [ ] The applicable Epic StarterContent terms and review date are recorded.
- [ ] The applicable Epic UE5 Mannequin terms and review date are recorded.
- [x] A 2026-08-16 strict fresh Development package audit found 57 staged StarterContent chunks totaling 40.69 MiB and no StarterContent sample maps or Blueprints.
- [ ] [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) matches this manifest and the final package contents.
- [ ] Any newly added art, audio, font, icon, texture, or model has a manifest entry before release.
