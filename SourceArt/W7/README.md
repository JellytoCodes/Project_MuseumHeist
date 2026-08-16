# W7 Presentation Source Assets

`GenerateW7PresentationAssets.ps1` deterministically creates the project-original source textures and PCM audio used by the W7 presentation tasks. Running the generator again with the same script produces byte-identical files.

## Generated textures

Floor plans are 1024 x 640 schematic PNGs aligned to the shared prototype coordinate contract:

- `T_FloorPlan_M01.png`
- `T_FloorPlan_M02.png`
- `T_FloorPlan_M03.png`

Status icons are 256 x 256 transparent PNGs. They use simple geometric silhouettes so they remain legible at nameplate and HUD sizes:

- `T_HeistStatus_Stunned.png`
- `T_HeistStatus_Arrested.png`
- `T_HeistStatus_CarryingOriginal.png`
- `T_HeistStatus_Heavy.png`

The Editor import script places floor plans in `/Game/Blueprints/UI/Map` and status icons in `/Game/Blueprints/UI/Status`.

## Generated audio

All audio is 48 kHz, stereo, 16-bit PCM and is imported into `/Game/Blueprints/Audio/W7`.

The two 12-second alert beds use a 20 ms raised-cosine edge envelope and are imported with `Looping = true`:

- `SW_HeistSuspenseLoop.wav`
- `SW_HeistAlarmLoop.wav`

The four event cues contain no embedded loop metadata and are imported with `Looping = false`:

- `SW_HeistArrested.wav` (1.10 seconds)
- `SW_HeistRescue.wav` (0.85 seconds)
- `SW_HeistCarryFootstep.wav` (0.32 seconds)
- `SW_HeistHeavyFootstep.wav` (0.46 seconds)

The Editor script also creates `/Game/Blueprints/Audio/W7/SMX_HeistStunLowPass`. Its single Sound Class Adjuster targets `/Engine/EngineSounds/Master.Master`, applies to children, keeps volume and pitch at 1.0, and sets the low-pass filter to 1200 Hz. The mix uses a 0.08-second fade-in, indefinite duration, and a 0.20-second fade-out so gameplay code can push and pop it with the stun state.

No third-party visual, font, or audio material is embedded in these generated files. Generated files are imported through Unreal Editor; the resulting `.uasset` files remain the runtime assets.
