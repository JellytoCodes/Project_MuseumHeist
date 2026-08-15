# W7 Presentation Source Assets

`GenerateW7PresentationAssets.ps1` deterministically creates the three fixed floor-plan source textures and two alert audio loops used by W7-007/W7-009.

- The floor plans are project-original schematic artwork aligned to the shared prototype coordinate contract.
- The suspense/alarm loops are project-original procedural PCM audio (48 kHz, stereo, 16-bit, 12 seconds) with a 20 ms raised-cosine edge envelope for click-free looping.
- No third-party visual or audio material is embedded in these generated files.

Generated files are imported into Unreal through the Editor and the resulting `.uasset` files remain the runtime assets.
