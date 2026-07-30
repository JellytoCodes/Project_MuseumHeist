# Project_MuseumHeist — Blueprint Shell And Presentation Plan

## Rev 6: Contract Run And Player Experience Foundation

기준일: 2026-07-30

기준 문서:

- `AGENTS.md` Rev 11
- `ClassManifest.md` Rev 10
- `Museum_Heist_GDD.docx` Rev 14

이 문서는 W2에서 시작된 Blueprint Shell 계획을 현재 게임 방향에 맞게 통합한 Presentation Contract다.

Gameplay Rule, Authority, Validation과 Replication은 C++가 소유한다.

Blueprint와 Widget Blueprint는 다음을 담당한다.

- Mesh / Material / Texture / Icon Assignment
- Component Assembly
- First-Person Camera Socket Offset
- Animation / Pose
- Audio / VFX / Post Process
- Widget Layout / Color / Binding / Transition
- Map별 Actor Placement / Floor Plan Texture / World Bounds

---

# 1. Current Game Flow

```text
Title Menu
→ Online Lobby
→ Required Target / Loot Value Quota 확인
→ First-Person Infiltration
→ Walk / Sprint / Coin / Map으로 탐색
→ 여러 Painting / Object Exhibit 반복 위조
→ Original / Loose Loot 운반
→ Guard Inspection / Alert / Lockdown
→ 더 훔치기 또는 탈출 결정
→ Shared Extraction Deposit
→ Contract Outcome / Replica Recap
```

한 매치는 15~25분을 목표로 한다.

Surface Forgery는 20~45초, 기본 40초의 Speed Painting이다.

Object Assembly는 25~35초, 기본 30초의 빠른 조립이다.

---

# 2. Presentation Is Required Gameplay

다음 항목은 W8 Polish가 아니라 v1.0 Required Gameplay다.

- Walk / Sprint Animation과 Footstep 차이
- Remote Player Display Name
- Player Color / Crew Status / Original Carrier 표시
- Team Status HUD
- Owner-only Full-Screen Floor Plan Map
- Required Target / Quota / Carried / Secured Value HUD
- Guard Detection Warning
- Stun / Arrest / Heavy / Carry / Escape Feedback
- Forgery / Assembly 중 Remote Player Pose 또는 Icon
- Shared Extraction과 Deposit Feedback
- 실제 Replica를 보여주는 Result Recap

중요 상태는 다음 중 최소 두 채널로 표현한다.

```text
Local HUD / Screen Effect
World Animation / Nameplate / Material
Audio
```

로그만 출력되는 Player State는 완료로 취급하지 않는다.

---

# 3. Ownership Boundary

## C++가 소유

- Contract Assignment
- Required Target
- Loot Value Quota
- Carried / Secured Value
- Exhibit Assignment
- Walk / Sprint 요청 검증
- Status / Arrest / Escape State
- Forgery / Assembly Score
- Alert / Lockdown
- Extraction Deposit
- Team Result / Contribution
- Widget 생성과 ViewModel Source

## Blueprint가 소유

- Skeleton / Animation Blueprint / Blend Space
- Nameplate Widget Component 배치
- Floor Plan Texture와 Marker Icon
- Detection / Stun / Arrest / Carry VFX
- Post Process Material
- Audio Cue / MetaSound / Sound Class
- Widget Layout, Binding, Color, Icon과 Animation

## Blueprint Graph에서 금지

- Contract Target 또는 Quota 확정
- Secured Value 증가
- Player Escape / Arrest 확정
- Forgery Quality Score 계산
- Guard Detection 또는 Alert 확정
- Map에서 Guard / Loot 위치 탐색
- 신규 Server RPC
- Replicated State 직접 Mutation

---

# 4. Player Character Blueprint

권장 Asset:

```text
/Game/Blueprints/Character/BP_HeistPlayerCharacter
Parent: AHeistPlayerCharacter
```

Required Components:

- Capsule
- Full Body Skeletal Mesh
- FirstPersonCamera
- Character Movement
- Tag / Status / Inventory / Interaction / Action
- Forgery / Object Assembly
- Vision / Customization / Noise Emitter
- Remote Player Nameplate Widget Component

## First-Person

- Camera는 `FirstPersonCameraSocket`에 부착한다.
- Owning Player와 Remote Player 모두 Full Body Mesh를 유지한다.
- 얼굴 Clipping은 Socket Offset으로 해결한다.
- Local Head를 자동으로 숨기지 않는다.
- Head Bob, Camera Roll과 Sprint FOV Kick을 사용하지 않는다.

## Walk / Sprint

- Walk Base 목표: `300 cm/s`
- Sprint Base 목표: `600 cm/s`
- Sprint 기본 입력: `Left Shift` Hold
- Stamina Bar 없음
- Walk / Sprint Animation Blend를 Velocity로 표현한다.
- Weight가 증가할수록 이동 속도, 호흡과 Footstep이 무거워진다.
- Sprint 중 Footstep은 Walk보다 크고 자주 발생한다.
- Inventory / Map / Forgery / Stun / Arrest / Escape 완료 중 Sprint Pose로 남지 않는다.

## Required Remote Poses

- Idle / Walk / Sprint
- Carrying Original
- Heavy Carry
- Forging
- Assembling
- Stunned
- Arrested
- Escaped 또는 Hidden

Pose는 Gameplay State를 결정하지 않고 복제된 상태를 표현한다.

---

# 5. Player Nameplate

권장 Asset:

```text
/Game/Blueprints/UI/HUD/WBP_HeistPlayerNameplate
Parent: UHeistPlayerNameplateWidget
```

Required Presentation:

- Player Display Name
- Player Color
- 거리
- Crew Status Icon
- Required Target Carrier Icon

Fallback Display Name:

```text
PLAYER {HeistPlayerId}
```

상태 후보:

```text
FORGING
ASSEMBLING
CARRYING
HEAVY
STUNNED
ARRESTED
ESCAPED
```

규칙:

- Local Owning Player의 Nameplate는 표시하지 않는다.
- Remote Player만 표시한다.
- 기본 표시 범위는 `2~2,500 cm`다.
- 원거리에서 Fade한다.
- Guard, SoundPing, Loot에 같은 Nameplate 체계를 재사용하지 않는다.
- 이름표가 벽을 통해 Guard 또는 Loot 위치를 노출하지 않도록 한다.

---

# 6. Main HUD

권장 Asset:

```text
/Game/Blueprints/UI/HUD/WBP_HeistHUD
Parent: UHeistHUDWidget
```

## P0 Layout

```text
Top Left
- Required Target
- Secured Value / Required Quota
- Team Carried Value

Top Center
- Security Level
- Lockdown Countdown

Left or Right Team Rail
- Player Name / Color / Crew Status
- Original Carrier
- Escaped / Arrested

Center
- Crosshair
- Interaction Prompt
- Detection Warning

Bottom
- Weight
- Coin QuickSlot
- Context Action / Temporary Feedback
```

## Contract Copy

Raw Row Name 또는 Enum을 노출하지 않는다.

예시:

```text
STEAL THE REQUIRED PAINTING.
SECURE $8,500 / $12,000.
THE CREW IS CARRYING $3,200.
THE REQUIRED TARGET HAS BEEN SECURED.
```

## Team Status

Forgery / Assembly Full-Screen 중에도 최소 다음은 유지한다.

- Player Name
- Arrested / Escaped
- Required Target Carrier
- Security Level

---

# 7. Floor Plan Map

권장 Asset:

```text
/Game/Blueprints/UI/Map/WBP_HeistMap
Parent: UHeistMapWidget
```

기본 입력:

```text
M
```

Map Mode에서 Move, Look, Interaction, Throw와 다른 UI 진입을 차단한다.

Required Areas:

- Map Title / Zone
- Floor Plan Image
- Local Player Marker
- Teammate Marker와 Name / Color
- Entrance / Extraction
- Contract Target Gallery 또는 발견된 Required Target
- Dropped Required Target
- Arrested / Escaped Teammate State
- Close Guide

표시하지 않는 정보:

- Guard 위치
- Guard 시야 Cone
- SoundPing
- 미발견 Loose Loot
- 비공개 Spawn 후보

Map별 Data:

```text
FloorPlanTexture
WorldMin
WorldMax
ZoneLabels
DefaultExitMarkers
```

Map은 Navigation 도구이며 Gameplay Sensor가 아니다.

---

# 8. Status And Screen Feedback

## Guard Detection

- Crosshair 주변 또는 화면 가장자리의 Detection Build-up
- Guard Notice Audio
- Confirmed Detection 시 짧은 Alert Transition
- 강한 Camera Shake 금지

## Stun

- 낮은 Desaturation
- 짧은 Vignette
- Audio Low-pass 또는 Ring
- 남은 시간 또는 상태 Label
- Remote Stunned Pose / Nameplate Icon
- 상태 종료 후 Post Process와 Audio Filter 정리

Player Stun은 Guard 또는 승인된 Environment Source만 사용한다.

## Arrest

- Stun과 구분되는 Cuffed / Disabled 화면
- `YOU ARE ARRESTED` 또는 Rescue 가능 상태
- Team HUD와 Nameplate에 `ARRESTED`
- Forgery / Inventory / Map / Movement Context 정리

## Carry / Heavy

- Original Carrier Icon
- Weight Gauge
- 무거운 호흡 / Footstep
- Remote Carry Pose
- Drop / Pickup Audio와 Popup

## Extraction

- Cast Progress
- Deposit Value 증가
- Player `ESCAPED` 전환
- 남은 Crew Team Rail 유지
- Match End 전까지 Result 화면을 강제하지 않는다.

색상 하나에만 의존하지 않고 Text / Icon / Audio를 함께 사용한다.

---

# 9. Input Modes

```text
Gameplay
Inventory
Map
Forgery
```

## Gameplay

- Cursor 숨김
- Mouse Capture
- Move / Look / Walk / Sprint
- Interaction / Coin / Flashlight

## Inventory

- Cursor 표시
- Move / Look / Sprint 차단
- Inventory Input Context만 활성

## Map

- Cursor 또는 Map Navigation 입력 활성
- Move / Look / Sprint 차단
- Map Context만 활성

## Forgery

- Cursor 표시
- Move / Look / Sprint / Interaction / Coin 차단
- Surface 또는 Object Forgery Context만 활성

Mode 종료 시 다음을 복원한다.

- Cursor
- Mouse Capture
- Movement / Look
- Gameplay Mapping Context
- HUD / QuickSlot 접근

---

# 10. Surface Forgery Presentation

권장 Asset:

```text
/Game/Blueprints/UI/Forgery/WBP_HeistForgery
Parent: UHeistForgeryWidget
```

Required:

- Reference
- Drawing Canvas
- Palette
- Brush / Erase / Reset
- Remaining Time
- Submit / Cancel
- Security Level / Team Status
- Validation Pending
- Local Preview

Pacing:

- 기본 40초
- 20~45초 Data 범위
- 언제든 조기 Submit
- 유효 Stroke가 있으면 Timeout Auto Submit
- 유효 Stroke가 없으면 Timeout Cancel

Reference Image:

- Public Domain 또는 권리 확인 Source
- 실제 작품을 15초에도 핵심 형태가 보이도록 직접 단순화
- 일반적으로 3~5색
- 못 그린 결과도 World Replica에 그대로 표시

Quality Score는 Contract Value가 아니라 Guard Inspection Delay에 영향을 준다.

---

# 11. Object Assembly Presentation

권장 Asset:

```text
/Game/Blueprints/UI/Forgery/WBP_HeistObjectAssembly
Parent: UHeistObjectAssemblyWidget
```

Required:

- Core Preview
- Part Tray
- Socket Target
- Orientation Step
- Material 선택
- Remaining Time
- Submit / Cancel
- Security Level / Team Status

Pacing:

- 기본 30초
- 25~35초 Data 범위
- 유효 Entry가 있으면 Timeout Auto Submit
- World Actor를 Preview로 직접 변경하지 않음

---

# 12. Exhibit And Loot World Readability

Painting / Object Case:

- Required Target
- Optional Active Exhibit
- In Use
- Replica Placed
- Original Removed
- Inspected / Suspected

상태 차이는 Material, Light, Icon, Animation 또는 Audio Hook으로 표현한다.

Required Target은 모든 Case를 동일한 HUD Marker로 덮지 않는다.

Contract가 제공하는 정보 수준에 따라 Gallery / Zone 또는 발견 후 Exact Case만 표시한다.

Loose Loot:

- Interaction 가능 여부가 Grade보다 먼저 읽혀야 한다.
- Value / Weight는 Focus Prompt 또는 Pickup Feedback에서 표시한다.
- Random Spawn은 Map의 승인된 Spawn Point만 사용한다.

---

# 13. Guard Presentation

권장 Asset:

```text
/Game/Blueprints/AI/BP_HeistGuardCharacter
Parent: AHeistGuardCharacter
```

Required State Presentation:

- Patrol
- Investigate
- Notice / Detection
- Chase
- Search
- Inspect Exhibit
- Stunned
- Arrest

Guard는 코미디 캐릭터로 연기하지 않는다.

낮은 품질 Replica에 대한 진지한 반응이 Player가 만든 결과와 대비되어 자연스러운 코미디를 만든다.

Player-facing Guard Radar 또는 SoundPing Marker는 만들지 않는다.

---

# 14. Shared Extraction

권장 Asset:

```text
/Game/Blueprints/World/Actors/BP_HeistVent
Parent: AHeistVentActor
```

Required Components:

- Interaction Volume
- Cast / Deposit Visual
- Exit Light
- Audio
- Optional Map Marker Hook

Required Presentation:

- 사용 가능 / 사용 중 / Lockdown 제한
- Player Deposit Value
- Required Target Deposit
- Individual Player Escaped
- 남은 Crew

Blueprint는 Contract Outcome 또는 Secured Value를 직접 확정하지 않는다.

---

# 15. Result / Match Story

권장 Asset:

```text
/Game/Blueprints/UI/Result/WBP_HeistResult
Parent: UHeistResultWidget
```

Required:

- Contract Success / Partial Haul / Contract Failed
- Required Target Secured 여부
- Secured Value / Quota
- Extra Value
- Escaped / Arrested Crew
- Alert / Lockdown
- Player Contribution
- 실제 Surface Replica Texture
- 실제 Object Assembly Replica

Funny Recap Label 예시:

```text
MASTERPIECE
CONVINCING ENOUGH
QUESTIONABLE
AN INCIDENT
```

Winner, Rank와 개인 Score 경쟁을 만들지 않는다.

Result는 실제 Match에서 생긴 사건과 Player가 만든 Replica를 보여준다.

---

# 16. Audio / VFX / Animation Minimum

## Player

- Walk / Sprint Footstep
- Heavy Footstep / Breathing
- Pickup / Drop / Original Carry
- Detection / Stun / Arrest / Escape

## Forgery

- Observe
- Brush / Erase / Submit / Timeout
- Assembly Part / Socket / Rotation
- Score Reaction

## Guard

- Notice
- Investigate
- Chase
- Inspect
- Arrest

## Contract

- Target Assigned
- Quota Progress
- Required Target Secured
- Quota Met
- Deposit
- Success / Partial / Failure

## Map / UI

- Map Open / Close
- Invalid Action
- State Transition

Audio Asset 지정은 Blueprint 또는 Data가 담당한다.

---

# 17. Map Placement Contract

Map은 다음을 담당한다.

- Eligible Painting / Object Exhibit Case
- Loot Spawn Point
- Player Start
- Guard Spawn / Waypoint / Patrol
- Shared Extraction
- Zone Label 기준
- Floor Plan Texture와 World Bounds
- Signage / Lighting / Navigation
- Audio Volume

Map별 최소 검증:

- Floor Plan Marker와 World 위치가 일치
- Required Target Gallery가 오해를 만들지 않음
- Walk / Sprint 동선과 Guard Noise 반응이 구분됨
- Original Carrier가 Exit까지 이동 가능
- 1~4 Player Spawn Collision 없음
- Guard Nav와 Exhibit Inspection 접근 가능

삭제된 Smoke / Trap Actor를 배치하지 않는다.

---

# 18. Active Asset Shell List

```text
BP_HeistPlayerCharacter
BP_DisplayCase
BP_SculptureDisplayCase
BP_CeramicDisplayCase
BP_HeistLootActor
BP_HeistCoinProjectile
BP_HeistVent
BP_HeistGuardCharacter
BP_HeistGuardWaypoint

WBP_TitleMenu
WBP_Lobby
WBP_HeistHUD
WBP_HeistPlayerNameplate
WBP_HeistMap
WBP_HeistInteractionPrompt
WBP_HeistInventory
WBP_HeistInventorySlot
WBP_HeistInventoryItem
WBP_HeistQuickSlot
WBP_HeistForgery
WBP_HeistObjectAssembly
WBP_HeistPopupFeedback
WBP_HeistResult
```

Removed:

```text
WBP_SoundPingMarker
SoundPingMarkerLayer
BP_HeistSmokeProjectile
BP_HeistSmokeCloud
BP_HeistGlueTrap
BP_HeistNoiseTrap
```

---

# 19. Editor Build And Compile Order

1. Development Editor Build
2. `BP_HeistPlayerCharacter` Refresh / Compile / Save
3. Painting / Object Case Blueprint Refresh / Compile / Save
4. Guard / Vent / Loot Blueprint Compile / Save
5. `WBP_HeistPlayerNameplate` Compile / Save
6. `WBP_HeistMap` Compile / Save
7. `WBP_HeistHUD` Compile / Save
8. Forgery / Assembly / Inventory / Result Widget Compile / Save
9. DataTable Reimport
10. M01 / M02 / M03 Map Save
11. Fix Up Redirectors
12. 1 Player PIE
13. 2 Player Listen Server PIE
14. 4 Player Weekly Gate

---

# 20. Presentation PASS Criteria

- Walk와 Sprint 속도 / Footstep / Animation이 구분된다.
- Weight가 Walk와 Sprint에 반영된다.
- Remote Player Name과 Crew Status를 식별할 수 있다.
- Team HUD와 Nameplate 상태가 일치한다.
- Map이 세 맵에서 Player / Team / Exit / Contract 위치를 올바르게 표시한다.
- Map이 Guard / SoundPing / 미발견 Loot를 노출하지 않는다.
- Detection / Stun / Arrest / Carry / Heavy / Escape가 화면과 Audio 또는 World에서 명확하다.
- Surface Forgery가 20~45초 Speed Painting으로 동작한다.
- 서로 다른 Case에서 여러 Player가 별도 Session을 진행할 수 있다.
- Required Target / Carried / Secured / Quota HUD가 서버 Snapshot과 일치한다.
- Individual Extraction 후 남은 Crew가 계속 플레이할 수 있다.
- Result가 실제 Replica와 Contract Outcome을 표시한다.
- Inventory / Map / Forgery / Arrest 종료 후 Input이 복원된다.
- Crash, Softlock, Orphan Session Lock, Duplicate Original / Replica가 없다.
