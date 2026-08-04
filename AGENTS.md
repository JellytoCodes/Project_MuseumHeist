# Project_MuseumHeist — Codex Instructions

## Rev 11: Contract Run And Player Experience Foundation

기준일: 2026-08-01 (GDD / TDD / Agent Rulebook 분리 반영)
엔진: Unreal Engine 5.8
현재 목표: 2026-09-20 Final RC / 프로젝트 마무리

이 문서는 프로젝트 엔지니어링 정책의 최상위 Source of Truth다.

현재 프로젝트는 기존 경쟁형 Top-Down 구조에서 **1~4인 온라인 협동 1인칭 잠입·유물 위조 하이스트 게임**으로 전환됐다.

Rev 11부터 한 판은 단일 목표를 한 번 위조하고 끝나는 Vertical Slice가 아니라 다음 두 계약 조건을 만족하는 15~25분 Contract Run으로 정의한다.

```text
Required Target
- 매치가 지정한 핵심 작품을 반드시 반출한다.

Loot Value Quota
- Original과 Loose Loot의 Secured Value 합계로 계약 할당량을 달성한다.
```

한 매치에서 여러 Painting / Object 전시품을 반복적으로 관찰하고 위조할 수 있다. 플레이어가 언제 더 훔치고 언제 도망칠지 판단하는 Greed Decision이 전체 게임의 중심이다.

플레이어 이름표, Walk / Sprint, Team Status, Floor Plan Map, Guard Detection, Stun / Arrest, Carry / Extraction의 화면·오디오·월드 피드백은 Polish가 아니라 v1.0 Required Gameplay Readability로 취급한다.

Forgery Gameplay는 다음 두 축으로 구분한다.

```text
Surface Forgery
- Painting 중심 2D Reference / Stroke / Palette / OpenCV 판정

Object Assembly Forgery
- Sculpture / Ceramic 중심 3D Modular Part / Socket / Orientation 판정
```

기존 경쟁형 Top-Down 구현은 아직 참조가 남아 있는 범위에서만 Legacy로 취급한다. 현재 기획에서 명시적으로 제거된 Smoke 및 플레이어 설치형 Trap 기능은 Legacy, Deferred, Stretch 또는 회귀 기준으로 유지하지 않는다.

---

# 1. Project Overview

Project_MuseumHeist는 Unreal Engine 5.8 C++ 기반의 **1~4인 온라인 협동 1인칭 잠입·유물 위조 하이스트 게임**이다.

플레이어들은 박물관에 침입해 계약이 지정한 핵심 작품을 찾고, 여러 전시품의 Replica를 현장에서 빠르게 제작해 Original과 바꿔치기한다. Guard가 위조품을 발견하고 Lockdown을 완료하기 전에 Required Target을 반출하고 Loot Value Quota를 채운 뒤, 욕심을 더 낼지 현재 전리품을 확보하고 탈출할지 결정한다.

## Core Fantasy

```text
박물관 침입
→ 계약의 Required Target과 Loot Value Quota 확인
→ 여러 전시품 탐색
→ 20~45초 Speed Forgery 또는 Object Assembly
→ 서버 품질 판정
→ Replica와 Original 교체
→ 전리품 운반 / Secured Value 누적
→ Guard 검사 및 Alert 상승
→ 더 훔치기 또는 탈출 결정
→ Required Target과 Quota를 반출하고 팀 결과 확인
```

## 현재 방향에서 사용하지 않는 요소

- 플레이어 간 공격
- 플레이어 간 기절
- 플레이어 간 전리품 강탈
- Piñata Drop
- 개인 Score 경쟁
- Winner
- Rank
- Gap Tracker
- Leader Reveal
- 선착순 Zero-Sum Extraction
- Top-Down Gameplay Camera
- Cursor World Aim
- Smoke Grenade
- Smoke Projectile
- Smoke Cloud
- Smoke Sight Blocking
- Glue Trap
- Noise Trap
- 플레이어 설치형 Trap
- Trap Placement Cast
- Trap 전용 QuickSlot
- Player-facing SoundPing Marker
- SoundPing Direction Widget
- Guard 위치와 시야를 실시간 표시하는 Minimap / Radar
- Stamina Bar와 Sprint 소모 자원
- 고정 역할 또는 Mandatory 2-player Interaction
- Original 전용 Carry Slot, `OriginalCarryEntry`, `CarryMode`와 Inventory 우측 별도 카드

---

# 2. Design Source And Scope Priority

프로젝트 문서는 역할에 따라 다음 우선순위를 사용한다.

1. `AGENTS.md`
   - Codex/Claude 계열 에이전트가 따라야 할 Hard Rule, 범위 경계, 작업 절차와 금지 범위
2. `Museum_Heist_TDD.docx`
   - C++/Blueprint 책임, Network Authority, Runtime State, Data Contract, Validation, QA와 Definition of Done
3. `Museum_Heist_GDD.docx`
   - 제품 비전, 재미의 근거, Player Experience, Contract Run, Level/Art/Audio 방향과 Balance 의도

하위 문서가 상위 문서와 충돌하면 구현 전에 상위 문서를 먼저 수정한다. 다만 문서의 역할이 다른 경우에는 해당 역할의 Source of Truth를 따른다.

- 제품 방향과 Player-facing 경험을 변경할 때는 GDD를 먼저 갱신하고 TDD와 AGENTS의 파급 범위를 동기화한다.
- Authority, Replication, Data Schema 또는 Validation을 변경할 때는 TDD를 먼저 갱신하고 AGENTS의 실행 규칙을 동기화한다.
- Codex 작업 절차와 금지 범위는 AGENTS가 최종 권한을 가진다.

Notion Task/Test 기록은 문서 우선순위에 포함하지 않는다. 에이전트는 사용자가 지정한 작업의 범위를 이해하기 위해 관련 항목을 필요할 때 한 번 조회할 수 있지만, Task를 생성·수정·삭제·재배열하거나 상태·진행률·증적을 갱신하지 않는다. Task 운영과 실시간 기록은 사용자가 직접 관리한다.

`ClassManifest.md`와 `Docs/W2_BlueprintShellPlan.md`는 더 이상 별도 거점 문서로 운영하지 않으며 AGENTS로 통합한다.
게임플레이 규칙, 데이터 계약, Shell/Presentation 경계, 구현 우선순위 변경은 AGENTS 본문에서 직접 관리한다.

## 2A. Blueprint / Presentation Rule Integration

Blueprint Shell/Presentation 운용은 별도 문서로 분리하지 않고 아래 규칙을 AGENTS 본문 규칙으로 통합해 적용한다.

- `WBP_` 계열 UI는 Layout, Animation, Color, Icon, Binding 중심으로 운영하고, 상태/값 확정은 C++ ViewModel과 게임 규칙이 소유한다.
- Nameplate는 Remote Player에 한해 항상 표시하며, 동일 Map에 대한 상태 아이콘은 Team Status 상태값(`Active`, `Forging`, `Assembling`, `CarryingOriginal`, `Heavy`, `Stunned`, `Arrested`, `Escaped`)과 동기화한다.
- Floor Plan Map은 Owner-only Full-Screen으로 운영한다. Guard 위치, 시야 Cone, SoundPing, 미탐색 Loose Loot/숨겨진 Spawn은 기본 표시하지 않는다.
- Move/Look/Mouse Capture 전환은 Owner-only Forgery, Assembly, Inventory, Map 진입 시 각각 입력 정책이 일치해야 한다.
- 2D Painting Forgery 타이머는 기본 40초(최소 20초/최대 45초), Object Assembly는 기본 30초(최소 25초/최대 35초)로 운영한다.
- Walk / Sprint / Weight / Footstep Noise는 동일한 수치 계약으로 C++ Authority와 Blueprint UI에서 일치시킨다.

## 2B. Document Boundary And Maintenance Rules

- GDD에 RPC 이름, FastArray 상세, GameplayTag Dictionary, Runtime 구조체 필드와 DataTable Schema를 중복 기재하지 않는다.
- TDD에 맵의 정서, 재미의 근거, 플레이어 감정선과 아트 무드 설명을 중복 기재하지 않는다.
- AGENTS는 GDD/TDD 본문을 복제하는 저장소가 아니라 에이전트가 반드시 지켜야 할 실행 계약만 유지한다.
- 같은 규칙을 여러 문서에 적어야 할 때는 한 문서를 Source of Truth로 지정하고 나머지는 링크와 요약만 남긴다.
- 문서 Heading은 Word Heading 1/2/3 Style을 사용하며 수동 굵기·크기로 제목을 흉내 내지 않는다.
- GDD와 TDD의 TOC는 Word Field로 유지하고 구조 변경 후 갱신한다.
- 깨진 문자, `[확인 필요]`, 임시 Placeholder 문구를 구현 근거로 사용하지 않는다. Git 이력 또는 상위 Source에서 복구한 뒤 반영한다.

---

# 3. Core Loop

```text
Title Menu
→ Host Session 또는 Join Code 입력
→ Lobby
→ Ready Countdown
→ First-Person Infiltration
→ Required Target / Loot Value Quota 확인
→ 전시품과 Loose Loot 탐색
→ Painting Speed Forgery 또는 Object Assembly
→ 서버 Quality Score 판정 / Replica 배치 / Original 회수
→ Carry Value 증가 / Guard Inspection / Alert
→ 다른 전시품을 반복해서 노리거나 탈출 결정
→ Shared Extraction에서 전리품 Secured
→ Team Result / Player Contribution
```

---

# 4. v1.0 Required Scope

- 1~4인 Listen Server
- Steam Online Session
- 별도 Title Menu Level
- 별도 Online Lobby Level
- Full First-Person
- 고정 박물관 맵 3개
- 고정 Contract Archetype 1개
- 매치별 Required Target 1개
- Player Count 기반 Loot Value Quota
- Server-seeded Exhibit Assignment / Spawn Variation
- 한 매치에서 Surface / Object Forgery를 여러 번 반복하는 Contract Run
- 목표 플레이 시간 15~25분
- `TitleMenu → Lobby → ReadyCountdown → InGame → End`
- Painting Target Artifact
- Surface Forgery Template 36개
  - M01 / M02 / M03 각 12개
- Server-selected Surface Template Pool / Shuffle Bag
- Painting Display Case State Machine
- Observation Cast
- Owner-only Full-Screen Drawing Forgery
- 서버 권한 Forgery Score
- Object Assembly Forgery Vertical Slice
- Sculpture / Ceramic Modular Kit 최소 2종
- Object Assembly Template 최소 12개
- Owner-only Full-Screen Object Assembly
- 서버 권한 Part / Socket / Orientation Score
- Replica Placement
- Original Removal
- Guard Patrol
- Guard Investigate
- Guard Chase
- Guard Search
- Guard InspectExhibit
- Alert Level
- Lockdown
- Loose Loot
- 4×5 Grid Inventory
- Weight Penalty
- Walk / Sprint와 속도별 Footstep Noise
- Coin Guard Distraction
- Remote Player Nameplate / Team Status
- Owner-only Full-Screen Floor Plan Map
- Guard Detection / Stun / Arrest / Carry / Escape Presentation
- Contract Target / Carried Value / Secured Value / Quota HUD
- Shared Extraction
- Individual Extraction Deposit와 남은 Crew 진행
- Team Success
- Partial Success
- Failure
- Team Result
- Player Contribution
- 실제 Replica Painting / Object를 보여주는 Match Recap
- 1인 완주
- 2~4인 멀티플레이 완주
- Development Build 패키징

## Excluded

다음 기능은 현재 프로젝트 범위에 포함하지 않는다.

- 보석·문서·화석 복제 Gameplay
- 외부 AI 이미지 판정
- Smoke Grenade
- Smoke Projectile
- Smoke Cloud
- Smoke Sight Blocking
- Glue Trap
- Noise Trap
- 플레이어 설치형 Trap
- Trap Placement Cast
- Steam Voice
- PCG
- Security Room
- Cinematic
- 추가 맵
- 고급 Loadout
- Progression
- PvP
- 배신
- 경쟁 랭킹
- 전용 서버
- Skill Matchmaking
- Perspective Toggle
- Third-Person Gameplay
- 별도 First-Person Arms
- 복잡한 Hand Interaction
- Guard 위치, 시야 Cone 또는 SoundPing을 표시하는 Minimap / Radar
- Stamina Resource
- Mandatory Class / Role Lock

## Stretch

필수 기능과 멀티플레이 Gate가 모두 PASS한 뒤에만 검토한다.

- Optional Rare Artifact
- First-Person Hand Animation
- 추가 Surface Forgery Template
- 추가 Object Assembly Kit / Template
- 추가 Loose Loot

Smoke 및 Trap 계열 기능은 Stretch 목록에 포함하지 않는다.

필요성이 다시 확정될 경우 기존 삭제 코드를 복구하지 않고, 당시의 기획과 현재 아키텍처를 기준으로 설계와 구현 범위를 다시 정의한다.

---

# 5. Hard Rules

- Unreal Engine 5.8을 유지한다.
- `Heist` 접두사를 유지한다.
- Export 대상 타입에는 `PROJECT_MUSEUMHEIST_API`를 유지한다.
- Gameplay Rule은 C++가 소유한다.
- Authority는 C++가 소유한다.
- Validation은 C++가 소유한다.
- Replication은 C++가 소유한다.
- Stable Gameplay API는 C++가 소유한다.
- Blueprint는 Asset Assignment를 담당한다.
- Blueprint는 Component Assembly를 담당한다.
- Blueprint는 Mesh, Material, Animation, Audio, Visual Hook을 담당한다.
- Widget Blueprint는 Layout, Animation, Color, Icon, Binding만 담당한다.
- DataTable과 DataAsset은 반복 데이터와 밸런스 값을 담당한다.
- Map은 Actor 배치, 공간 구성, Lighting, Navigation을 담당한다.
- Map Presentation Data는 Floor Plan Texture, World Bounds, Zone Label, Exit Marker 기준만 담당한다.
- `.uasset`은 Unreal Editor 또는 명시적으로 승인된 MCP 경로로만 수정한다.
- `.umap`은 사용자가 명시적으로 요청한 경우에만 수정한다.
- 불필요한 Manager, Service, Factory, Processor, Subsystem을 추가하지 않는다.
- Painting마다 별도 Actor Class를 만들지 않는다.
- Manifest에 없는 타입을 현재 요청 범위에서 임의 생성하지 않는다.
- 현재 사용자 요청 범위를 벗어난 전체 시스템을 선행 구현하지 않는다.
- 현재 기획에서 삭제된 기능을 호환성 명목으로 다시 추가하지 않는다.
- 플레이어의 진행 판단에 영향을 주는 Runtime State는 화면, 월드, 오디오 중 최소 두 채널로 피드백한다.
- Player Name, Crew Status, Contract Progress, Alert, Stun / Arrest, Original Carrier와 Extraction 상태를 로그 전용 또는 숨은 상태로 남기지 않는다.
- Contract Value와 Forgery Quality Score를 같은 수치로 취급하지 않는다.
- Required Target / Loot Value Quota / Secured Value / Contract Outcome은 서버가 확정한다.
- 고정 역할을 만들지 않으며 모든 Player가 Forgery, Assembly, Carry, Loot, Coin, Map과 Extraction을 사용할 수 있다.

## Surface Forgery / Object Assembly Boundary

- Painting 전시품과 Surface Forgery는 `AHeistPaintingDisplayCaseActor`가 담당한다.
- Sculpture와 Ceramic을 포함한 3D 조립 전시품은 `AHeistObjectDisplayCaseActor`가 담당한다.
- `Object Assembly Forgery`는 Sculpture / Ceramic을 포괄하는 Gameplay System 명칭이다.
- Surface Forgery와 Object Assembly는 서로의 Template Row를 공유하지 않는다.
- Surface Forgery와 Object Assembly는 서로의 제출 Payload와 Replica Data를 공유하지 않는다.
- Surface Forgery와 Object Assembly는 서로의 State Machine과 상세 Result를 공유하지 않는다.
- 두 방식은 Owner-only Input Mode 원칙과 서버가 확정한 최종 0~100 Quality Score의 Guard Inspection Handoff만 공유할 수 있다.
- `AHeistDisplayCaseActor`는 기존 Asset 호환 전용 Deprecated Painting Alias다.
- 신규 Asset은 `AHeistDisplayCaseActor`를 부모로 사용하지 않는다.
- 기존 `BP_DisplayCase`는 `AHeistPaintingDisplayCaseActor`를 부모로 사용한다.
- `AHeistSculptureDisplayCaseActor`는 기존 `BP_SculptureDisplayCase` 호환 전용 Deprecated Alias로 전환한다.
- 신규 Sculpture / Ceramic Asset은 `AHeistObjectDisplayCaseActor`를 부모로 사용한다.
- `AHeistSculptureDisplayCaseActor`(Deprecated) Case는 시각 Shell만 유지하며 v1.0 Gameplay에 사용하지 않는다.

## Removed Feature Boundary

다음 기능은 프로젝트에서 제거됐다.

```text
AHeistSmokeProjectile
AHeistSmokeCloudActor
AHeistTrapActor
AHeistGlueTrapActor
AHeistNoiseTrapActor
Trap Placement Cast
Smoke QuickSlot
Glue Trap QuickSlot
Noise Trap SoundPing
```

이 기능들은 다음 상태로 남기지 않는다.

- Legacy
- Deferred
- Stretch
- Regression Baseline
- Post-v1.0
- Placeholder

---

# 6. Server Authority Flow

```text
Local Input / Widget Request
→ AHeistPlayerController Server RPC
→ C++ Request Context Validation
→ Server Component Validation
→ Server State Mutation
→ Replicated State 또는 Owner Client Response
→ ViewModel
→ C++ Widget
→ Widget Blueprint Presentation
```

Client가 직접 확정할 수 없는 항목:

- Forgery Score
- Replica Placement
- Original Removal
- Display Case State
- Objective State
- Alert Level
- Lockdown
- Guard Inspection Result
- Extraction Result
- Team Result
- Player Contribution 확정값
- Inventory Item Mutation
- QuickSlot Assignment
- Coin 사용 결과
- Contract Assignment
- Required Target
- Loot Value Quota
- Carried / Secured Value
- Contract Outcome
- Player Escape Deposit

Client Preview는 확정값으로 취급하지 않는다.

---

# 6A. Online Session And Level Architecture

- `UHeistGameInstance`가 Online Subsystem 선택과 Create / Find / Join / Travel 상태를 단독 소유한다.
- Editor PIE는 로컬 다중 인스턴스 검증을 위해 `OnlineSubsystemNull`을 사용한다.
- 비 Editor 실행과 패키지 빌드는 기본 `OnlineSubsystemSteam`을 사용한다.
- Editor `OnlineSubsystemNull` 검증은 구현 검증용이며 Steam 최종 PASS를 대체하지 않는다.
- Online Session의 로컬 이름은 PIE가 선점하는 `GameSession`과 분리된 `HeistSession`을 사용한다.
- Session은 1~4인 Listen Server, Presence, Lobby, Join In Progress를 사용한다.

## Level Flow

- 비 Editor 실행은 별도 Title Menu Level에서 시작한다.
- Title Menu는 Host Session, Join Code 입력과 Local Settings(FOV, Mouse Sensitivity, Master Volume, Resolution / Window Mode)를 소유하며 이 값들은 First-Person 적용과 함께 로컬에 저장한다.
- Session 생성 또는 참가 성공 시 별도 Lobby Level로 이동한다.
- Lobby는 참가 코드 표시, Player Slot, Map 선택, Ready / Start, Leave만 소유한다.
- Session Leave 또는 Host Quit 시 Title Menu Level로 복귀한다.
- Editor 직접 Gameplay Map PIE는 기존 Gameplay 회귀 검증을 위해 InGame 시작을 유지한다.
- PIE New Editor Window가 저장된 Resolution을 Editor 창 크기로 덮어쓰는 경우 Settings 진단은 `DisplayApply=EDITOR_OVERRIDE`로 구분한다.

## Session / Lobby Contract

- Host는 혼동 문자를 제외한 6자리 참가 코드를 생성하고 Session Setting에 게시한다.
- Join은 참가 코드, Product Id, Build Unique Id, 공개 슬롯을 검증한 뒤 서버 주소로 이동한다.
- Host는 Lobby에서 `M01`, `M02`, `M03`, `Random`을 선택할 수 있고 선택 결과는 `AHeistGameState`를 통해 모든 Client에 복제한다.
- Lobby Player Id는 현재 `PlayerArray`에서 사용하지 않는 가장 낮은 `1~4` 번호를 할당한다. 퇴장한 Slot은 `EMPTY`가 되고 다음 참가자가 해당 번호를 재사용한다.
- `UHeistLobbyViewModel`은 Player 추가·제거뿐 아니라 각 `AHeistPlayerState`의 Identity 변경에도 반응해 모든 Client의 Slot 표시를 갱신한다.
- Package Client는 로컬 PlayerController `BeginPlay`에서 Session World Ready를 통지해 성공한 `TravelJoin`의 Pending 상태와 30초 감시 타이머를 해제한다.
- Steam 최종 PASS는 서로 다른 Steam 계정 2개와 Development Package 증거가 있을 때만 처리한다.

---

# 7. First-Person Camera Rules

- Camera는 머리 높이에 배치한다.
- Controller Yaw와 Pitch가 시점을 제어한다.
- Character Yaw는 Controller Yaw를 따른다.
- Interaction은 Player Capsule과 각 Actor의 Interaction Collision 사이 `BeginOverlap / EndOverlap` 후보 관리로 처리한다.
- Interaction Target은 현재 Overlap 중이고 `CanInteract`를 만족하는 Actor 가운데 가장 가까운 대상으로 선택한다.
- Interaction Target 탐색을 위한 실시간 Line Trace 또는 주기적 Trace Scan을 사용하지 않는다.
- Flashlight Direction은 Camera Forward를 기준으로 한다.
- Coin Throw Direction은 Camera Forward 또는 검증된 Camera Target을 기준으로 한다.
- Top-Down Gameplay Camera를 사용하지 않는다.
- SpringArm Gameplay Camera를 사용하지 않는다.
- Cursor World Aim을 사용하지 않는다.
- First-Person Camera는 Full Body Mesh의 Head Bone 또는 Socket에 부착한다.
- Camera 위치 Offset은 Blueprint에서 Character Mesh에 맞게 조정한다.
- Owning Player와 다른 플레이어 모두 Full Body Mesh를 유지한다.
- 자연스러운 Character Shadow를 유지한다.
- Local Head를 자동으로 숨기지 않는다.
- 얼굴 Clipping은 Camera Socket Offset으로 해결한다.
- 기본 FOV는 90이다.
- Head Bob은 v1.0에서 사용하지 않는다.
- Camera Roll은 v1.0에서 사용하지 않는다.
- Sprint Camera Effect는 v1.0에서 사용하지 않는다.
- Inventory와 Forgery 진입 시 Cursor와 UI Input Mode를 활성화한다.
- UI 종료 시 Mouse Capture, Look, Movement, Interaction Context를 복원한다.

---

# 7A. Player Locomotion Rules

- 기본 Gameplay 이동은 Walk와 Sprint 두 Pace를 지원한다.
- Walk는 잠입과 낮은 Footstep Noise를 위한 기본 Pace다.
- Sprint는 `Left Shift` Hold 입력을 기본으로 하며 빠른 이동과 큰 Footstep Noise를 발생시킨다.
- Sprint는 Stamina를 소비하지 않는다. v1.0의 이동 선택 비용은 소음과 Loot Weight다.
- 기본 목표값은 Walk `300 cm/s`, Sprint `600 cm/s`다.
- Weight Penalty는 Walk와 Sprint에 각각 적용한다.
- 기본 목표 공식은 다음과 같다.

```text
ResolvedWalkSpeed = Clamp(300 - TotalCarryWeight × 7.5, 150, 300)
ResolvedSprintSpeed = Clamp(600 - TotalCarryWeight × 15, 250, 600)
```

- 정확한 수치는 Balance Data가 소유하며 플레이 테스트에서 조정한다.
- Walk Footstep 기본 반경은 `500 cm`, Sprint Footstep 기본 반경은 `1,000 cm`다.
- Inventory, Map, Surface Forgery, Object Assembly, Stun, Arrest, Escape 완료와 World Restriction 중에는 Sprint를 허용하지 않는다.
- Sprint 요청과 서버 확정 이동 속도는 Weight, Match Phase와 Player State를 검증한다.
- Head Bob, Camera Roll, Sprint FOV Kick과 Stamina UI는 v1.0에서 사용하지 않는다.

---

# 7B. Player Identity And Team Readability Rules

- `AHeistPlayerState`가 Platform Display Name, Heist Player Id, Player Color와 Crew Status의 Source of Truth다.
- Display Name을 사용할 수 없으면 `PLAYER {HeistPlayerId}`를 사용한다.
- Remote Player는 머리 위 Nameplate를 표시한다.
- Nameplate는 Player Name, Player Color, 거리와 현재 핵심 상태 Icon을 표시할 수 있다.
- 핵심 상태는 `Active`, `Forging`, `Assembling`, `CarryingOriginal`, `Heavy`, `Stunned`, `Arrested`, `Escaped`를 구분한다.
- 이름표는 Local Owning Player 자신에게 표시하지 않는다.
- 이름표는 일반적으로 `2~2,500 cm` 범위에서 표시하고 원거리에서 Fade한다.
- 벽을 통과하는 Guard, Loot 또는 SoundPing Marker는 추가하지 않는다.
- Main HUD Team Status는 연결된 모든 Player의 Name, Color, Crew Status, Original Carrier와 Escape / Arrest 상태를 항상 요약한다.
- Forgery 또는 Assembly Full-Screen 중에도 최소 Team Status와 Alert Warning을 유지한다.

---

# 7C. Floor Plan Map Rules

- v1.0은 Minimap 대신 Owner-only Full-Screen Floor Plan Map을 사용한다.
- 기본 입력은 `M` Hold 또는 Toggle이며 별도 `Map` Input Mode로 관리한다.
- Map은 Local Player, Teammate, 출구, Zone Label, Contract Target Gallery, 발견된 Required Target, Dropped Original과 Extracted / Arrested Teammate를 표시할 수 있다.
- Contract가 정확한 Case 위치를 제공하지 않는 경우 Target Gallery 또는 Zone만 표시한다.
- Map은 Guard 위치, Guard 시야 Cone, SoundPing, 미발견 Loose Loot과 비공개 Spawn을 표시하지 않는다.
- Fixed Map별 Floor Plan Texture와 World Bounds / UV Projection Data를 사용한다.
- Map 표시 중 Move, Look, Interaction, Throw와 다른 UI 진입을 차단하고 종료 시 Gameplay Input Mode를 복원한다.
- Map Widget은 Gameplay State를 변경하지 않는다.

---

# 7D. Status And Feedback Rules

- Gameplay 판단에 중요한 상태는 최소 두 개의 Feedback Channel을 사용한다.

```text
Local Screen / HUD
World Presentation / Animation / Nameplate
Audio
```

- Guard Detection은 Detection Build-up, 방향을 강제하지 않는 화면 Warning과 Notice Audio를 제공한다.
- Player Stun은 Guard 또는 승인된 Environment Source만 적용할 수 있으며 PvP 공격에서 발생하지 않는다.
- Stun 중에는 Movement, Look 또는 Action Lock 범위를 서버 상태와 동일하게 적용하고 남은 시간을 HUD에 표시한다.
- Stun Presentation은 Vignette, 낮은 Desaturation, 짧은 Audio Low-pass 또는 Ring, Remote Pose / Nameplate Icon을 사용한다.
- 강한 Blur, 지속 Camera Shake와 색상 하나에만 의존하는 경고는 사용하지 않는다.
- Arrest는 Stun과 구분된 Cuffed / Disabled Presentation, Team Status, Rescue Prompt 또는 Final State를 가진다.
- Original Carry와 Heavy 상태는 HUD, Nameplate Icon, Movement / Footstep Audio와 Remote Carry Pose로 식별할 수 있어야 한다.
- Escape 완료 Player는 Team Status에서 `ESCAPED`로 유지하고 남은 Crew 상태를 관찰한다.
- 상태 해제, Arrest 해제, Match End와 Lobby Return에서 Post Process, Audio Filter, Input Lock과 Widget을 정리한다.

---

# 8. Gameplay Item Rules

## Item Types

현재 지원하는 `EHeistItemType`:

```text
None
Loot
Throwable
KeyItem
```

현재 지원하지 않는 Item Type:

```text
Trap
```

## Use Types

현재 `EHeistUseType`은 다음 값을 유지한다.

```text
None
Throw
DeployArea
Consume
```

단, 현재 v1.0 활성 Gameplay Item은 Coin Throw뿐이다.

`DeployArea`와 `Consume`은 범용 Enum 값으로 남을 수 있지만, 승인된 Item Row와 Gameplay 실행 경로가 없으면 사용하지 않는다.

삭제된 Use Type:

```text
PlaceTrap
```

## QuickSlot

현재 QuickSlot은 Coin 하나만 지원한다.

```text
EHeistQuickSlotType::None
EHeistQuickSlotType::Coin
```

현재 QuickSlot Item:

```text
Slot: Coin
Input: Q
ItemId: Throwable_Coin
UseType: Throw
```

다음 QuickSlot은 존재하지 않는다.

- Smoke Grenade
- Glue Trap
- Noise Trap
- E Key Smoke Slot
- R Key Trap Slot

## DataTable Rules

다음 Row는 Item 및 UsableItem DataTable에 존재해서는 안 된다.

```text
Trap_Glue
Trap_Noise
Throwable_Smoke
```

삭제된 C++ 또는 Blueprint Class를 참조하는 Soft Class Reference를 남기지 않는다.

`AHeistGameMode::ValidateItemDataTables()`는 다음 조건을 만족해야 한다.

```text
Result=PASS
InvalidRows=0
OrphanExtensions=0
```

## Guard Alert Profile Data Rule

- Guard Alert Profile은 `DT_GuardData`의 `Guard_Alert_Low / Medium / High` Row를 사용한다.
- 위치명 기반 `Guard_Default / Guard_Vault / Guard_SecurityRoom` Row는 사용하지 않는다.
- 신규 Guard의 기본 `GuardProfileId`는 `Guard_Alert_Medium`이다.
- 실제 맵의 Guard별 등급 배정, Patrol 영역, 공간 압박과 최종 수치 조정은 Release Level Design / Map Balance에서 수행한다.

---

# 9. Action Component Rules

`UHeistActionComponent`가 현재 관리하는 Gameplay Cast:

- Observation Cast
- Escape Cast

제거된 Gameplay Cast:

- Trap Placement Cast

## Mutual Exclusion

Gameplay Cast는 상호 배타적이어야 한다.

```text
Observation 활성 중 Escape 시작 금지
Escape 활성 중 Observation 시작 금지
Forgery 활성 중 Observation / Escape 시작 금지
Inventory 활성 중 허용되지 않은 Cast 시작 금지
```

`IsGameplayCastActive()`는 현재 활성 Cast 전체를 나타내야 한다.

Cast 종료 시 Component Tick은 다른 활성 Cast가 없는 경우에만 비활성화한다.

## Cancellation

Observation 취소 조건:

- Input Release
- Movement
- Damage
- Arrest
- Session Invalid
- Display Case Invalid
- Match Phase 변경
- Owner EndPlay
- Disconnect

Escape 취소 조건:

- Movement
- Damage
- Arrest
- Vent Invalid
- Escape Phase 종료
- Match Phase 변경
- Owner EndPlay
- Disconnect

---

# 10. Surface Forgery Rules

## Match Exhibit / Template Assignment

- 서버는 현재 Map의 Eligible Exhibit Case와 Contract Definition으로 매치별 Exhibit Assignment를 확정한다.
- Required Target Case는 반드시 하나 지정한다.
- Optional Painting / Object Case는 Player Count와 Loot Value Quota가 요구하는 수량만 활성화한다.
- Surface Template 선택은 Map Pool별 Shuffle Bag을 사용하며 한 Match Assignment 안에서 같은 Template을 중복 사용하지 않는다.
- Shuffle Bag 재충전 시 직전 Cycle의 최근 3개 Template을 첫 선택 후보에서 제외한다.
- Object Assembly Template은 별도 Family Pool과 Shuffle Bag을 사용한다.
- Assignment는 `CaseId`, `ArtifactId`, `ForgeryType`, `TemplateId`, `ArtifactValue`, `bRequiredTarget`을 포함한다.
- Assignment Snapshot과 Contract Snapshot은 모든 Client에 복제한다.
- 선택된 Reference Image는 해당 Assignment를 받은 Painting Case의 Original World Visual에만 적용한다.
- 비활성 Case 또는 다른 Case의 Original World Visual을 현재 Assignment로 덮어쓰지 않는다.
- Lobby 복귀 또는 Contract Clear 시 Original World Visual은 Blueprint가 지정한 기준 Material로 복원한다.

## Session Ownership

- 한 Painting Display Case는 동시에 한 명만 위조할 수 있다.
- 서버가 Session Owner를 확정한다.
- 서버가 거리, Match Phase, Player State, Case State를 검증한다.
- Disconnect 시 Session Lock을 해제한다.
- Arrest 시 Session Lock을 해제한다.
- Cancel 시 Session Lock을 해제한다.
- Timeout 시 Session Lock을 해제한다.
- Match End 시 Session Lock을 해제한다.
- Owner 또는 Display Case EndPlay 시 Session을 정리한다.

## Owner-only Full-Screen Mode

- Forgery Widget은 Owning Player에게만 표시한다.
- Forgery 중 World View는 완전히 가린다.
- World Audio와 팀 통신은 유지한다.
- Move를 차단한다.
- Look을 차단한다.
- Jump를 차단한다.
- Sprint를 차단한다.
- Throw를 차단한다.
- QuickSlot을 차단한다.
- Inventory를 차단한다.
- Loot를 차단한다.
- 다른 Interaction을 차단한다.
- Draw를 허용한다.
- Erase를 허용한다.
- Drawing 단계의 `R`은 현재 Client의 Local Stroke와 Preview만 초기화하며 Server RPC를 전송하지 않는다.
- Submit 성공 즉시 Forgery Widget을 닫고 Gameplay Input, 이동과 시점을 복원한다. 제출 결과를 Full-Screen Result 단계로 전환하지 않는다.
- Case는 제출자의 Owner Lock과 Replica Texture 후보를 `ReplicaReady`로 유지한다. 플레이어는 EndOverlap 후 경비를 피해 이탈했다가 나중에 돌아올 수 있다.
- `ReplicaReady` Case에 다시 BeginOverlap한 Owner의 `E`는 서버에 Replica 교체와 Original 회수를 함께 요청한다.
- `ReplicaReady` Case에 다시 BeginOverlap한 Owner의 `R`은 서버에 현재 Preview 폐기와 새 Drawing Timer로 Full-Screen Forgery 재진입을 요청한다.
- `ReplicaReady`의 `E`와 `R`은 요청 순간 Player Capsule과 해당 Case의 Interaction Collision이 현재 Overlap 중인지 서버가 검증한다. Submit 이후 연속 Overlap은 요구하지 않는다.
- World UX는 Owner에게 `E 회수 | R 다시 그리기` 안내를 표시하고, 회수 준비 상태는 Green, 미작업 상태는 Red를 기본 Outline 의미로 사용한다. 색상만으로 상태를 전달하지 않고 Text Prompt를 함께 제공한다.
- 남은 Drawing Time은 Owner에게 복제된 `SessionEndServerTime`과 Server World Time의 차이로 표시한다.
- Drawing Time은 독립된 `DrawingTimeRemainingText`에 표시하며 키 가이드 또는 제목 Text와 결합하지 않는다.
- Submit을 허용한다.
- Cancel을 허용한다.
- Push-To-Talk를 허용한다.
- Pause를 허용한다.

## Speed Painting Pacing

- Surface Forgery의 목표 시간은 `20~45초`, 기본값은 `40초`다.
- 플레이어는 남은 시간과 관계없이 언제든 현재 Drawing을 Submit할 수 있다.
- Timeout은 유효 Stroke가 하나 이상 있으면 현재 Drawing을 자동 Submit한다.
- 유효 Stroke가 없으면 Timeout Cancel로 처리한다.
- Submit은 Score와 Replica Texture Preview를 확정하고 Case를 `ReplicaReady`로 전환할 뿐, Original 회수나 최종 Replica 배치를 즉시 확정하지 않는다. 동시에 Full-Screen UI Session은 종료하고 Gameplay로 복귀한다.
- `ReplicaReady`에서는 Owner Lock을 유지하되 Submit 당시의 Overlap 후보는 유지할 필요가 없다. Owner가 같은 Case에 재접근했을 때 `E` 교체·회수 또는 `R` Full-Screen 다시 그리기를 선택한다.
- `E` 확정은 Replica 배치, Original Grid 추가, Case 상태 변경, Objective/Carrier 갱신과 교체 소음을 하나의 서버 작업으로 처리한다.
- Inventory Grid 또는 Weight 검증이 실패하면 위 작업 전체를 거부하고 `ReplicaReady`를 유지한다.
- 낮은 Score는 즉시 Match Failure가 아니라 짧은 Guard Inspection Delay와 Alert Escalation으로 이어진다.
- Reference Image는 약 15초 Drawing으로도 핵심 실루엣을 알아볼 수 있도록 단순화한다.
- Template Palette는 2~8색을 허용하되 일반적인 Release Template은 3~5색을 목표로 한다.
- 실제 작품 기반 Reference는 Public Domain 또는 사용 권한이 확인된 Source만 사용하며 직접 단순화한 파생 이미지를 제작한다.
- Forgery Quality Score는 Loot Value Quota에 직접 더하지 않는다.

## Stroke Transport

- Client는 정규화된 Stroke Point를 수집한다.
- Client는 Stroke별 Point Count를 수집한다.
- Client는 Stroke별 Palette Index를 수집한다.
- Client는 Stroke마다 승인된 고정 `BrushPresetIndex`를 기록하고 제출한다.
- 일반 Surface Forgery Brush Preset은 Template 비율값을 사용하지 않고 정규화 지름 `소 0.020 / 중 0.040 / 대 0.080`의 세 단계로 고정한다. 기준 `800×800` Drawing Surface에서는 각각 `16 / 32 / 64 px`에 해당하며 서버가 허용 집합을 재검증한다.
- Brush Size 선택 변경은 변경 이후 새로 시작하는 Stroke에만 적용한다. 이미 그린 Stroke의 굵기, Local Preview, 서버 Score와 Replica 굵기는 변경하지 않으며 Eraser 반경에도 영향을 주지 않는다.
- Surface Forgery의 WBP Drawing Surface 크기와 내부 Painter 해상도는 분리한다. Drawing Surface는 정사각형 Responsive Layout을 사용하며 `400×400`, `800×800` 같은 특정 Slate Unit 크기를 C++ 계약으로 고정하지 않는다.
- Surface Forgery의 화면 Drawing은 현재 Brush로 전체 Stroke를 다시 그리는 Vector Line 방식이 아니라 `1024×1024` Local Palette Raster에 Pointer Segment를 순서대로 누적하는 Painter 방식으로 표시한다.
- Pointer 입력은 실제 Drawing Surface Geometry에서 정규화하고, Local Palette Raster Texture는 같은 `DrawingSurface`의 UMG `Image` 또는 `Border` Brush에 직접 연결한다. 부모 Widget의 `NativePaint`에서 별도 DrawElement 좌표를 재구성하지 않으며 DPI Scale, PIE Window 크기와 WBP Layout 크기가 바뀌어도 입력과 표시 좌표가 일치해야 한다.
- Drawing Pointer의 Mouse Capture는 Button Down에서 한 번 획득하고 Button Up, Surface 이탈, UI 종료 또는 실제 Capture Lost까지 유지한다. Pointer Move마다 Capture를 다시 요청해 하나의 Drag를 여러 Stroke로 분할하지 않는다.
- Local Painter는 모든 Pointer Segment를 연속 Capsule로 누적하고, 서버 전송용 Polyline은 입력 중 고정 간격으로 별도 샘플링한다. 로컬 Stroke와 화면 Raster는 전송 Point Budget과 무관하게 계속 유지하며, 제출 시에만 로컬 데이터를 변경하지 않는 전송용 복사본을 단순화한다. 전송 Point Budget에 도달했다는 이유로 화면 붓칠이 중단되거나 이미 그린 결과와 예상 점수가 감소해서는 안 된다.
- Local Palette Raster는 나중에 칠한 색이 이전 픽셀을 덮어쓴다. 따라서 소/중/대 Brush는 이미 칠한 영역의 크기를 다시 해석하지 않으며, 뒤에 사용한 작은 Brush도 앞서 사용한 큰 Brush 위에 정상 합성돼야 한다.
- Local Palette Raster와 최종 서버 Palette Raster는 모두 Canvas 경계에서 Brush Stamp를 Clamp한다. Brush 중심이 가장자리에 있어도 색 픽셀이 Drawing Surface 밖으로 표시되거나 판정 데이터 밖으로 기록되어서는 안 된다.
- Surface Forgery UI는 현재 Palette, Brush Size, 남은 시간과 `예상 점수`를 표시한다. Point Budget, Payload Byte와 Score Raster Resolution은 플레이어용 정보가 아니므로 일반 UI에 표시하지 않고 Debug Dump에서만 확인한다.
- Surface Forgery의 서버 Score와 Replica Palette Raster는 `256×256`을 사용한다. 더 큰 Reference Image는 이 판정 해상도로 정규화한다.
- Reference Image는 직접 제작한 단순한 이미지를 사용한다.
- Template별 Palette는 2~8색으로 제한한다.
- Template은 `None / Black / White` 배경 필터를 사용한다.
- `Black / White`는 Reference Image에서 해당 배경색을 제거한다.
- `None`은 별도 Reference Mask를 사용한다.
- 플레이어는 Template Palette에서 색을 직접 선택한다.
- 위치에 맞는 정답 색을 자동 선택하지 않는다.
- Stroke는 임의 RGB가 아니라 `PaletteIndex`를 전송한다.
- 정규화 Stroke Point는 X/Y를 각각 16-bit Unsigned Integer로 양자화하고 하나의 `uint32`에 Packing해 RPC로 전송한다. 서버는 이를 `FVector2D`로 복원한 뒤 동일한 판정 Raster를 구성한다.
- 서버는 Packed Coordinate, Stroke Count, Palette Index, Brush Preset과 Session Revision을 포함한 Payload 크기가 `48 KiB`를 초과하지 않는지 검증한다.
- 서버는 좌표 범위를 검증한다.
- 서버는 Stroke Count를 검증한다.
- 서버는 Point Count를 검증한다. Template Transport Point Budget은 Easy `4096`, Medium `5120`, Hard `6144`를 기본으로 사용하며 Local Drawing의 입력 한계로 사용하지 않는다.
- 서버는 Palette Index를 검증한다.
- 서버는 Stroke 수와 `BrushPresetIndex` 수의 일치 여부 및 각 Preset 허용 범위를 검증한다.
- 서버는 Session Revision을 검증한다.
- Client는 최종 Score를 전송하지 않는다.

## Submitted Texture Replication

- Replica Preview와 최종 위조 그림은 동일한 서버 Score용 Palette Raster에서 생성한다.
- 고정 해상도 Palette Index Data는 Submit 시 Preview로 한 번 복제하고, `E` 확정 시 같은 Data를 Committed Replica로 승격한다.
- `FHeistReplicaPaintingData`는 전용 `NetSerialize`로 Palette와 4-bit Packed Index Buffer를 연속 바이트 직렬화한다. 일반 반영형 배열 요소 직렬화로 64KB Actor Bunch 제한을 초과하지 않는다.
- 각 Client는 복제된 Index Data로 Transient Texture를 재구성한다.
- Render Target을 World Visual 목적으로 복제하지 않는다.
- 전체 Stroke Payload를 World Visual 목적으로 추가 복제하지 않는다.

## OpenCV Score

서버와 Local Preview는 동일한 C++ Evaluator를 사용한다.

Local Preview의 전체 `256×256` OpenCV 평가는 Pointer Drawing / Erase 입력 중 최대 `1.25초` 간격으로만 실행하고, Pointer Release 뒤에는 `0.12초` 이내의 다음 Tick에서 갱신한다. 입력 Event마다 갱신 타이머를 다시 시작해 장시간 점수가 고정되게 하지 않는다. UI 문구는 서버 확정값과 구분하기 위해 `예상 점수`를 사용한다.

Local Preview 평가와 진단 로그 억제는 클라이언트 반응성 정책일 뿐이며, Submit 시 서버 권한 최종 평가는 생략하거나 저해상도로 대체하지 않는다.

최종 확정값은 서버 결과뿐이다.

OpenCV 평가 구성:

```text
Reference 전처리
→ Player Stroke Palette Raster
→ Binary Foreground Mask
→ 3×3 Morphology Close
→ 양방향 Distance Transform
→ Mask Precision / Recall / Dice / IoU
→ Lab SSIM
→ Palette Histogram Similarity
→ Shape / Color 결합
→ Completeness
→ Anti-Fill
→ Final Score
```

Shape Score:

- Reference → Submitted 거리 기반 Coverage
- Submitted → Reference 거리 기반 Precision
- 양방향 Harmonic Mean
- Dice Similarity
- Response Curve 1.15

Color Score:

- Foreground Union ROI
- Lab SSIM
- Palette Histogram Similarity
- Response Curve 1.10

Completeness:

```text
min(SubmittedForeground / ReferenceForeground, 1)^0.65
```

Anti-Fill:

- Reference 대비 과도한 면적을 칠하면 Score Cap을 적용한다.
- 빈 배경 일치로 점수를 얻을 수 없도록 한다.

## Score Data Contract

Template Weight는 유효한 합계를 가져야 한다.

```text
CoverageWeight + MajorShapeWeight > 0
ShapeAccuracyWeight + ColorAccuracyWeight > 0
```

0으로 나누거나 NaN Score를 생성할 수 있는 Template Row를 허용하지 않는다.

Penalty 또는 Diagnostic Field가 Final Score에 직접 적용되지 않는 경우 문서와 필드 이름에서 그 사실을 명확히 한다.

## Alert Presentation

- `UHeistHUDViewModel`과 `UHeistForgeryViewModel`은 `AHeistGameState`의 복제 Alert Snapshot만 읽는다.
- Main HUD는 Quiet, Suspicious, Searching, Alarmed, Lockdown을 단계별 Text와 Color로 표시한다.
- 플레이어 표시는 `SECURITY LEVEL 0/4~4/4`와 4칸 별 Indicator를 사용한다.
- Guard의 확정 발각은 서버에서 최소 Suspicious를 요청하고, Painting 검사 결과는 Score Mapping 결과를 요청한다.
- Main HUD가 가려지는 Owner-only Forgery 화면에서도 동일한 Security Level Indicator를 표시한다.
- Alarmed의 Lockdown Countdown은 복제된 `AlertNextTransitionServerTime`과 Server World Time의 차이로 표시한다.
- HUD Lockdown Countdown은 독립된 `LockdownCountdownText`에 표시한다.
- Forgery 화면은 Quiet 이외 Alert에서 독립된 `ForgeryAlertWarningText`를 표시한다.
- Forgery 화면의 Lockdown Countdown은 `ForgeryLockdownCountdownText`에 별도로 표시한다.
- Suspicious/Searching은 Suspense Music Layer, Alarmed/Lockdown은 Alarm Music Layer를 사용한다.
- Forgery Full-Screen UI 중에도 Alert Music Layer는 유지한다.
- 실제 Music Asset 지정은 Widget Blueprint Class Default가 담당한다.

## Cleanup

Forgery 종료 시 반드시 복원한다.

- Painting Display Case Lock
- Forgery Owner
- Movement
- Look
- Interaction
- Cursor
- Mouse Capture
- Input Mapping Context
- HUD 접근
- QuickSlot 접근
- Inventory 접근
- Forgery Widget Instance

중간 Drawing 진행도는 v1.0에서 저장하지 않는다.

---

# 10A. Object Assembly Forgery Rules

## Supported Artifact Families

현재 Object Assembly Forgery의 활성 Family:

```text
Sculpture
Ceramic
```

Jewelry, Fossil 및 기타 Family는 명시적인 설계·구현 승인 전 활성화하지 않는다.

## Modular Kit

- 하나의 Object를 Voxel 또는 파편 단위로 분해하지 않는다.
- 하나의 Template은 고정 Core와 조작 가능한 Part 3~5개를 기본으로 한다.
- 한 번의 조립에서 사용하는 전체 Static Mesh Component는 일반적으로 4~6개다.
- Part는 Family 안에서 여러 Template이 재사용할 수 있다.
- Core Mesh는 승인된 Socket을 소유한다.
- Part Pivot은 연결 지점을 기준으로 제작한다.
- Runtime Mesh Cutting, Geometry Collection, 자유 물리 조립을 사용하지 않는다.
- v1.0 조립은 Socket Snap과 승인된 회전 단계만 사용한다.
- Scale 자유 조절은 v1.0에서 사용하지 않는다.

## Owner-only Assembly Mode

- Object Assembly Widget은 Owning Player에게만 표시한다.
- Assembly 중 World View는 완전히 가린다.
- Move, Look, Jump, Sprint, Throw, QuickSlot, Inventory와 다른 Interaction을 차단한다.
- Part 선택, Socket 선택, 승인된 회전, Submit, Cancel, Push-To-Talk와 Pause를 허용한다.
- 조립 화면은 로컬 Preview Component를 사용하며 World Actor를 직접 변경하지 않는다.
- Session 종료 시 Gameplay Input Mode와 HUD 접근을 Surface Forgery와 동일한 원칙으로 복원한다.
- Object Assembly 목표 시간은 `25~35초`, 기본값은 `30초`다.
- 플레이어는 언제든 현재 Assembly를 Submit할 수 있다.
- Timeout은 현재 유효 Entry가 하나 이상 있으면 현재 Payload를 자동 Submit하고, 유효 Entry가 없으면 Cancel 처리한다.

## Server Authority And Payload

- 서버가 Object Assembly Session Owner, Revision, End Server Time과 선택 Template을 확정한다.
- Client는 Part Mesh 또는 임의 Transform을 전송하지 않는다.
- Client 최종 Payload는 승인된 `PartId`, `SocketId`, Quantized Orientation과 Material Id만 포함한다.
- 서버는 Session Revision, Part Count, 중복 Part, Part Family, Socket 호환, Orientation 범위, Material 범위와 Payload 크기를 검증한다.
- 중간 Drag 또는 Preview Transform은 복제하지 않는다.
- Submit 시 검증된 최종 Assembly Payload를 한 번만 확정한다.

## Deterministic Score

Object Assembly는 OpenCV를 사용하지 않는다.

```text
Required Part Match
→ Socket / Topology Match
→ Orientation Accuracy
→ Material Match
→ Completeness
→ Extra Part Score Cap
→ Final Quality Score
```

기본 Weight:

```text
Required Part Match: 35%
Socket / Topology Match: 30%
Orientation Accuracy: 25%
Material Match: 10%
```

- 누락 Part는 Completeness를 낮춘다.
- 불필요한 Part를 과도하게 추가하면 Final Score Cap을 적용한다.
- Client Preview는 확정값이 아니다.
- 최종 Quality Score는 서버만 확정한다.
- Guard Inspection은 서버가 확정한 0~100 Quality Score만 공통 입력으로 사용한다.

## Replica Data

- Object Assembly Replica는 Surface Forgery의 Palette Raster를 사용하지 않는다.
- Object Assembly Replica는 승인된 Part Id, Socket Id, Quantized Orientation, Material Id와 Revision만 복제한다.
- Client는 로컬 Template Data에서 Static Mesh와 Material을 해석해 Replica Component를 재구성한다.
- 전체 Mesh Data, Render Target, Physics State 또는 Preview Actor를 복제하지 않는다.
- 늦게 참가한 Client도 동일한 Assembly Replica를 재구성해야 한다.

## Cleanup

Session 종료, Cancel, Timeout, Arrest, Disconnect, Match End, Owner EndPlay 또는 Display Case EndPlay에서 다음을 정리한다.

- Object Display Case Lock
- Assembly Owner
- Assembly Session Revision / Timer
- Local Preview Components
- Assembly Widget
- Movement / Look / Interaction Block
- Cursor / Mouse Capture / Input Mapping Context
- QuickSlot / Inventory / HUD 접근

---

# 10B. Contract Run Rules

## Contract Definition

- v1.0은 하나의 Contract Archetype을 사용한다.
- 각 Match는 `Required Target 1개 + Loot Value Quota 1개`를 확정한다.
- Required Target은 Painting 또는 Object Assembly Exhibit가 될 수 있다.
- Required Target의 Artifact Value는 Loot Value Quota에 포함된다.
- Quota는 Required Target만 훔쳐서는 일반적으로 달성할 수 없도록 Data Validation한다.
- Player Count가 증가하면 Quota와 활성 Optional Exhibit 수를 Data로 조정한다.
- Forgery Time 자체는 Player Count에 따라 크게 늘리지 않는다.

## Grid Inventory And Original Acquisition

- Painting / Object Original과 Loose Loot는 동일한 Owner-only `FHeistReplicatedInventory` FastArray와 4×5 GridSlot을 사용한다.
- Painting의 Original은 `ReplicaReady`에서 `E`로 교체·회수를 확정할 때 Artifact Data의 `GridWidth`, `GridHeight`, `Weight`, `ArtifactValue`를 복제 Item Instance에 복사하고 빈 GridSlot에 즉시 자동 배치한다.
- Painting의 Replica 교체와 Original Grid 추가는 분리된 두 번의 상호작용이 아니며, 어느 한쪽만 성공한 상태를 남기지 않는다.
- 빈 GridSlot이 없거나 Weight 제한을 넘으면 획득을 거부하며, Original은 전시 케이스 또는 기존 World Drop 상태를 유지한다.
- Grid와 Weight가 허용하는 한 한 Player가 여러 Original을 동시에 보유할 수 있다.
- `CarryingOriginal`과 `Heavy`는 Grid Item 목록과 총 Weight에서 파생되는 상태이며 별도 원본 소유 컨테이너가 아니다.
- 원본 전용 `OriginalCarryEntry`, `CarryMode`, Inventory 우측 별도 카드와 아이템별 반복 출구 전달 흐름을 만들지 않는다.
- Individual Player가 Shared Extraction을 완료하면 그 시점에 Grid가 보유한 모든 유효 Original과 Loose Loot를 한 번의 서버 Deposit으로 처리한다.

## Value States

```text
Carried Value
- 현재 Active Player가 운반 중인 Original과 Loose Loot의 합계

Secured Value
- Escape Deposit가 완료되어 Match Result에 보존된 가치

Required Quota
- Contract Success에 필요한 Secured Value
```

- Carried Value는 체포, Drop, Disconnect 또는 전원 실패 전에 Secured Value로 간주하지 않는다.
- Individual Player가 Shared Extraction을 완료하면 그 Player의 유효 전리품을 Secured Value에 Deposit하고 Player를 `Escaped`로 전환한다.
- 먼저 탈출한 Player는 다시 Match에 복귀하지 않는다.
- 남은 Crew는 계속 다른 전리품을 확보하거나 탈출할 수 있다.
- Required Target Original이 Secured되어야 Contract Success가 가능하다.

## Match End And Outcome

- Match는 Active Player가 없거나, Lockdown / Match Timer의 종료 조건이 충족되거나, 서버가 승인한 Team End 조건이 충족될 때 끝난다.
- Outcome과 Crew Survival은 분리해 표시한다.

```text
Contract Success
- Required Target Secured
- Secured Value >= Required Quota

Partial Haul
- Required Target Secured
- Secured Value < Required Quota

Contract Failed
- Required Target 미반출
- 전원 체포 또는 다른 Terminal Failure
```

- 모든 Crew 탈출, 일부 Arrest, Alert Level, 최고 / 최악 Replica와 Extra Value는 별도 Recap으로 표시한다.
- 낮은 Forgery Score는 Contract Value를 직접 삭제하지 않고 Guard Pressure를 높인다.
- Result는 경쟁 Rank를 만들지 않으며 팀이 만든 Replica와 발생한 사건을 보여주는 Match Story로 사용한다.

## Failure-forward

- 서툰 Replica와 불완전한 Assembly도 서버 Validation을 통과하면 World에 배치한다.
- 실수는 가능한 한 즉시 Match Failure가 아니라 Guard Investigation, Alert, Drop, Rescue 또는 급한 탈출 상황을 만든다.
- Guard와 Museum Presentation은 진지하게 유지하고, 코미디는 Player 행동과 실제 Replica 결과에서 발생하게 한다.
- 고정 Painter, Lookout, Carrier 역할을 강제하지 않는다.

---

# 11. SoundPing Rules

현재 SoundPing 시스템은 Guard가 서버에서 소음에 반응하기 위한 Gameplay Event로 사용한다.

- Footstep
- Replica Swap
- Glass Break
- Coin Impact
- 현재 기획에 포함된 환경 소음

`Replica Swap`은 Painting Case의 `E` 교체·회수 확정 시 프레임이 덜그럭거리는 World Audio와 함께 서버에서 발생한다. 주변 Guard는 이 Event를 `InvestigateNoise` 후보로 처리하고, Player-facing SoundPing Marker는 생성하지 않는다.

현재 제거된 SoundPing:

- Noise Trap
- Trap Trigger
- Smoke 관련 SoundPing

다음 값은 사용하지 않는다.

```text
EHeistSoundPingType::NoiseTrap
Event.SoundPing.NoiseTrap
AI.Stimulus.Trap
Ping_NoiseTrap
```

Guard Noise Reaction은 현재 활성 SoundPing Type만 처리한다.

Player-facing SoundPing Marker, Direction Widget 및 HUD Layer는 사용하지 않는다.

플레이어는 1인칭 공간 음향과 실제 Sound Cue에 의존해 소리 방향을 판단한다.

SoundPing Event는 Client HUD 표시를 위해 복제하지 않는다.

StunHit은 Guard 또는 승인된 Environment Source가 Player / Guard에 Stun을 확정했을 때의 서버 전용 Noise Event로만 사용할 수 있다. PvP 공격 Source와 Player-facing Direction Marker에는 사용하지 않는다.

---

# 12. GameplayTag Rules

현재 GameplayTag는 실제 Runtime Rule, DataTable, UI 또는 AI에서 사용하는 값만 등록한다.

제거 대상 Tag:

```text
State.InSmoke
Action.PlacingTrap
Event.Trap.Placed
Event.Trap.Triggered
Event.SoundPing.NoiseTrap
AI.Stimulus.Trap
Item.Trap
Item.Trap.Glue
Item.Trap.Noise
Item.Throwable.Smoke
```

삭제 기능의 Tag를 향후 호환성 용도로 남기지 않는다.

GameplayTag를 삭제하기 전에 관련 DataTable, Blueprint, Config 참조를 확인한다.

---

# 13. Input Mode Rules

입력 모드는 상호 배타적으로 관리한다.

```text
Gameplay
Inventory
Map
Forgery
```

Context 전환 시:

1. 기존 Context를 명시적으로 제거한다.
2. 새 Context를 추가한다.
3. 중복 Widget을 생성하지 않는다.
4. Input Context를 누적하지 않는다.
5. Cursor와 Mouse Capture를 현재 Mode에 맞게 설정한다.
6. 종료 시 이전 Gameplay Context를 복원한다.

`Map` Mode는 Inventory와 Forgery와 동시에 활성화하지 않는다.

---

# 14. C++ / Blueprint / Data / Map Responsibility

| 영역 | 책임 |
|---|---|
| C++ | Rule, State, Authority, Validation, Replication, Contract Assignment, Stable API |
| Blueprint | Mesh, Material, Camera Position, Component Assembly, Animation, Audio, VFX, Visual Hook |
| Widget Blueprint | Layout, Binding, Animation, Presentation |
| ViewModel / C++ Widget | HUD, Nameplate, Map, Status, Result State Exposure와 Request Routing |
| DataTable / DataAsset | Contract, Artifact, Template, Guard, Balance, Map Presentation, Scaling Data |
| Map | Painting/Object Case, Guard Route, Loot Spawn, Exit, Zone, Lighting, Navigation |

## UI Copy Rules

- v1.0의 Source UI Copy와 기본 표시 언어는 한국어를 사용한다.
- Widget Blueprint의 기본 TextBlock 문구와 C++ `FText` 원문은 한국어를 기준으로 맞춘다.
- 프로젝트 기본 Culture와 Package Stage Culture는 `ko`를 사용한다.
- 플레이어에게 상태를 전달하는 문구는 대상, 현재 상태, 결과 또는 필요한 행동을 알 수 있는 문장형으로 작성한다.
- `봉쇄`처럼 의미가 모호할 수 있는 단독 상태명 대신 `박물관 봉쇄까지 {0} 남았습니다.`처럼 게임 내 대상과 상태를 명시한다.
- Forgery 제출 제한 시간과 Museum Lockdown 제한 시간은 서로 다른 문장으로 구분한다.
- Raw Enum, Data Row ID, Blueprint Class Name을 그대로 플레이어에게 노출하지 않는다.
- 화면 제목, 버튼 동사, 키 라벨, 수량처럼 문맥이 이미 분명한 짧은 UI Label은 간결하게 유지할 수 있다.
- `NSLOCTEXT` / `LOCTEXT` Namespace와 Key는 안정적으로 유지하고 원문만 한국어로 작성한다.
- 영어를 포함한 추가 언어는 기존 Namespace와 Key를 사용하는 Localization Target 번역 리소스로 추가하며, 이를 위해 한국어 원문을 다시 영어로 되돌리지 않는다.

## Blueprint Graph 금지 항목

- Forgery Score 계산
- Original 확정
- Replica 확정
- Alert 변경
- Lockdown 변경
- Extraction 성공 판정
- Team Result 확정
- Contract Assignment / Quota / Secured Value 확정
- Map에서 Guard / Loot Sensor 정보를 생성
- Player Status의 서버 확정 State를 Widget Graph에서 변경
- 신규 Server RPC
- Replicated Gameplay State 직접 변경
- Inventory 확정 Mutation
- QuickSlot 확정 Mutation

---

# 15. Blueprint And Asset Cleanup Rules

C++ 타입을 삭제한 경우 다음 순서로 Asset 정리를 완료한다.

1. 관련 C++ 참조 제거
2. Development Editor Build
3. 관련 Blueprint 열기
4. `Refresh All Nodes`
5. 삭제된 Enum Pin과 Class Pin 제거
6. Blueprint Compile
7. Blueprint Save
8. JSON 기반 DataTable은 Import JSON 수정 후 재Import
9. 삭제 Class Reference Viewer 확인
10. Fix Up Redirectors
11. PIE 실행
12. Missing Class Log 확인
13. Invalid Enum Log 확인
14. Failed Import Log 확인

Smoke 및 Trap 관련 Blueprint와 C++ Class는 신규 Asset의 부모 또는 DataTable Class Reference로 사용하지 않는다.

`DataTableImports/*.json`을 Import Source로 사용하는 DataTable은 JSON을 Source of Truth로 취급한다.

이 DataTable의 Row를 정리할 때는 `.uasset`에서 직접 삭제하지 않고 JSON을 수정한 뒤 Unreal Editor에서 Reimport한다.

---

# 16. Work Boundary And Handoff

아래 절차는 채팅이 새로 생성되거나 이전 대화 Context가 없어도 동일하게 적용한다. 사용자가 작업 규칙을 다시 설명하게 만들지 않는다.

## Task Management Non-interference

- Notion의 Task 목록은 사용자가 지정한 작업 범위를 이해하기 위한 읽기 전용 Context다.
- 에이전트는 Task ID, 실행 순서, 상태, 진행률, 담당자, 증적 또는 완료 판정을 직접 변경하지 않는다.
- AGENTS, GDD 또는 TDD에 개별 Task 목록, 실시간 진행 상태나 Roadmap 사본을 유지하지 않는다.
- Task 기록과 설계·구현 문서가 충돌하면 사용자에게 차이만 보고하며 임의로 어느 쪽도 동기화하지 않는다.

## Work Bootstrap

- 작업 시작 시 `AGENTS.md`와 관련 GDD/TDD 범위, Manifest 상태를 먼저 확인한다.
- Git 작업 상태와 기존 Asset 경로를 읽기 전용으로 확인하고, 사용자의 기존 변경을 새 작업 결과로 오인하지 않는다.
- Editor 작업이 필요하면 사용자용 Blueprint/Data/Map 절차를 현재 대화에서 제공한다.
- 사용자가 명시적으로 요청하지 않는 한 별도 작업용 `.md` 파일을 추가하지 않는다.

## 구현 전 Deliverable Audit

소스 수정 전에 현재 요청에 필요한 산출물을 다음 범주로 분해한다.

```text
C++ Rule / Authority / Validation / Replication
Blueprint Actor / Component Assembly / Asset Assignment
Widget Blueprint / UI Layout / Animation
DataTable / DataAsset / Import JSON
Map Actor Placement / Collision / Navigation / Lighting
Player-facing Visual / Audio / Interaction Feedback
Development Editor Build
PIE Server / Client Runtime Test
Output Log
```

- 각 범주를 `Codex 담당`, `사용자 Editor 담당`, `현재 요청 불필요`, `현재 요청 범위 밖` 중 하나로 판정한다.
- 필요한 Editor 작업은 C++ 구현을 마친 뒤 발견하는 것이 아니라 작업 시작 시 식별한다.
- Editor 작업이 없으면 `Editor 작업 없음`이라고 명시한다. 있으면 Build를 요청하기 전에 Editor Checklist와 실행 순서를 미리 알린다.
- `.uasset` 파일이 존재한다는 사실은 Blueprint가 올바르게 구성됐거나 화면에 보이거나 Map에 배치됐다는 증거가 아니다.
- C++ Actor Class가 존재한다는 사실은 Player가 식별하고 상호작용할 수 있는 Presentation Actor가 존재한다는 증거가 아니다.
- Actor, UI, Data 또는 Map이 완료 조건에 포함되면 관련 Asset의 Parent Class, 필수 Component, Asset Assignment와 실제 Map 배치 여부를 별도로 확인한다.
- 테스트용 임시 배치와 Release용 영구 배치를 구분한다.
- 필수 Editor 선행 작업을 안내하고 사용자가 완료하기 전에는 해당 기능의 Runtime Test 단계로 넘어가지 않는다.

## 구현과 검증 경계

- C++ 수정과 정적 검사가 끝났다는 사실은 Build 성공이나 기능 완료를 뜻하지 않는다.
- Build 성공은 사용자가 해당 코드 Revision을 Build한 뒤 성공을 전달한 경우에만 확인된 것으로 취급한다.
- Build 성공 뒤 C++를 다시 수정했다면 이전 Build 증거는 현재 코드에 유효하지 않으므로 다시 Build를 요청한다.
- 필요한 Blueprint/Data/Map 작업은 사용자가 Compile/Save 또는 임시 테스트 배치까지 확인한다.
- Build 성공만으로 Editor 구성이나 Runtime 동작을 확인한 것으로 취급하지 않는다.
- 멀티플레이, Ownership, Replication 동작은 사용자가 실행한 Server/Client 절차와 Output Log를 근거로 판정한다.
- 뒤늦게 Editor 선행 작업 누락을 발견하면 즉시 기존 완료 표현을 철회하고, 누락된 작업·영향받는 테스트·재Build 필요 여부를 명확히 정정한다.

## Editor Handoff Gate

사용자에게 Build 또는 Editor 작업을 넘길 때 다음을 빠짐없이 제공한다.

- 현재 구현·검증 상황
- 열어야 할 정확한 Asset 또는 Map 경로
- 확인하거나 선택할 Actor / Parent Class / Component
- 변경할 Property와 값
- 임시 테스트 배치인지 Release 영구 배치인지
- Blueprint Compile / Save와 Map Save 여부
- C++ 재Build 필요 여부
- 완료 후 다음 단계가 Editor 작업인지 PIE인지

Editor 작업이 필요한 경우 위 Checklist 없이 바로 PIE 명령부터 제공하지 않는다.

## Debug Fixture와 Presentation 규칙

- Player가 찾아서 상호작용해야 하는 Actor의 Debug Spawn은 기존 Presentation Blueprint를 우선 생성한다.
- Static Mesh, Widget, Decal 또는 명확한 Debug Drawing이 없는 순수 C++ 부모 Actor를 시각 상호작용 테스트용으로 생성하지 않는다.
- Presentation Blueprint Load에 실패해 C++ 부모로 Fallback한 경우 로그를 `Result=FAIL` 또는 `BLOCKED`로 남기고, 보이지 않는 Actor를 사용해 테스트하라고 안내하지 않는다.
- Debug Spawn 성공은 Release Map 배치 완료를 의미하지 않는다.
- 기존 Blueprint를 재사용할 때도 Parent Class, Visual Component, Collision과 상호작용 가능 여부를 Editor Checklist에 포함한다.

## Runtime Test 안내 형식

- 모든 명령은 실행 창을 `SERVER` 또는 `CLIENT`로 명확히 표시한다.
- 복사할 명령은 한 명령당 하나의 `text` Code Block으로 분리한다.
- 준비 명령, Player 조작, 판정용 Dump를 섞지 않고 실행 순서대로 안내한다.
- 모든 중간 명령에 장문의 기대값을 반복하지 않고, 각 Test의 마지막 판정용 Dump와 핵심 관찰 기준만 제공한다.
- Build와 필수 Editor 선행 작업이 현재 코드 Revision에 대해 확인된 뒤에만 Runtime Test 절차를 제공한다.
- 테스트 결과를 받으면 관찰 범위의 PASS / FAIL / BLOCKED와 다음 조치를 판정해 사용자에게 보고한다.

## Codex 담당

- C++ Gameplay Rule 구현
- C++ Authority 구현
- C++ Validation 구현
- C++ Replication 구현
- Repository 코드 분석
- Config 분석
- Data Import JSON 분석
- 승인된 범위의 코드 수정
- 사용자가 제출한 Build 오류 수정
- 검증 판정용 최소 Debug Log 추가
- Debug 또는 Cheat Command 구현
- 사용자가 제출한 PIE Log 기반 동작 판정

## 사용자 Unreal Editor 담당

- Blueprint 구성
- Widget Blueprint 구성
- DataTable 편집
- DataAsset 편집
- Map 배치
- Scale
- Collision
- Lighting
- Navigation
- Asset Assignment
- Component Assembly
- Development Editor Build
- Build Log 제출
- Blueprint Compile
- Save
- PIE 실행
- Debug Command 실행
- Output Log 제출

Codex는 Unreal C++ Build를 직접 실행하지 않는다.

사용자가 Build를 실행하고 오류가 발생하면 전체 오류 위치와 메시지를 전달한다. Codex는 해당 로그를 근거로 코드를 수정한다.

Codex는 사용자가 명시적으로 요청하지 않는 한 다음을 수행하지 않는다.

- Unreal Editor 실행
- Unreal Editor 종료
- Unreal Editor UI 직접 조작
- Unreal MCP 연결
- Unreal MCP 재연결
- Unreal MCP 복구
- `.uasset` 직접 수정
- `.umap` 직접 수정

## Editor 작업 절차 형식

Editor 작업 안내에는 다음만 포함한다.

- 열 Asset 또는 Map
- 선택할 Actor 또는 Component
- 변경할 Property와 값
- Compile / Save 순서
- PIE Mode
- Player 수
- 실행할 Debug Command
- 제출할 Output Log

---

# 17. Runtime Test And Log Handoff

- Runtime 검증은 기존 `UHeistDebugFunctionLibrary`와 `UHeistCheatManager`를 우선 사용한다.
- 동작 판정용 로그가 부족하면 현재 사용자 요청 범위 안에서 최소 Debug Log를 추가한다.
- 사용자는 Unreal Editor PIE에서 Debug Command를 실행한다.
- PIE의 Disconnect, Session Cleanup, Owner EndPlay 또는 재접속 연속성 검증에서 Client 콘솔 `disconnect`를 사용하지 않는다.
- PIE Client를 종료하기 위해 `ESC`를 사용하지 않는다.
- 위 검증에서 원격 Client 연결 종료가 필요하면 Listen Server가 서버 권한 `KickPlayer` Debug Command로 대상 Player를 제거한다.
- 현재 공용 Kick 경로는 `HeistObjectAssemblyKickPlayer <PlayerId>`이며, 이름과 관계없이 `AGameSession::KickPlayer()`를 호출하는 서버 권한 진단 명령으로 사용한다.
- 테스트 안내에서 `disconnect`가 필요한 것처럼 보이는 경우에도 항상 위 Listen Server Kick 절차로 대체한다.

## Session Debug Commands

- Development 검증 명령은 `HeistSessionHost`, `HeistSessionJoin <Code>`, `HeistSessionLeave`, `HeistSessionMap <M01|M02|M03|Random>`, `HeistSessionStart`, `HeistSessionComplete`, `HeistSessionReturn`, `HeistSessionDump`를 사용한다.
- `HeistSessionComplete`는 Listen Server의 활성 Steam Session과 2명 이상의 Player를 요구하며 현재 Player를 Escaped로 확정하고 Result를 재구성한 뒤 Match Phase를 `End`로 전환한다.
- `HeistSessionDump`는 Player / Identity / Slot / Roster / UI Snapshot과 Map Selection이 일치하고 Gameplay Phase가 `InGame` 또는 `End`일 때 `Result=PASS`를 출력한다.
- Shipping은 Debug / Cheat Command가 제거되므로 위 명령 기반 Formal Test에 사용하지 않는다.
- 사용자는 관련 Output Log를 제출한다.
- 화면 동작이 완료 조건이면 관찰 결과도 제출한다.
- 제출된 로그와 관찰 결과를 해당 기능의 검증 조건에 대조한다.
- 결과는 `PASS`, `FAIL`, `BLOCKED`로 판정한다.
- Build 성공만으로 Runtime 동작을 PASS 처리하지 않는다.

## Debug Logging Policy

- Gameplay Class와 Component는 진단 및 테스트 목적으로 `UE_LOG`를 직접 호출하지 않는다.
- 진단 로그는 사건 이름이 드러나는 `UHeistDebugFunctionLibrary::Debug...` 함수로 기록한다.
- 로그 Category, Severity, 문장, 필드 순서와 `Result` Schema는 `UHeistDebugFunctionLibrary`가 소유한다.
- 호출부에 Format String을 남기는 범용 Logging Macro를 중앙화의 대체 수단으로 사용하지 않는다.
- 호출부는 Actor, Component, `FName`, 수치와 Boolean 같은 원시 Context만 전달한다.
- 문자열 조립, Soft Object Path 변환, 배열 요약과 Enum 문자열 변환은 Debug 함수의 Shipping Guard 내부에서 수행한다.
- 현재 Target의 기본 Shipping 설정에서는 `Fatal`이 아닌 `UE_LOG`가 자동 제거되므로, 단순 출력 차단만을 위한 호출부 `#if !UE_BUILD_SHIPPING`은 추가하지 않는다.
- Actor 탐색, 배열 순회, 화면 출력 또는 기타 Debug 전용 연산이 있는 함수는 `#if UE_BUILD_SHIPPING` 조기 반환 또는 동등한 Compile Guard를 유지한다.
- Runtime State를 변경하는 Debug/Cheat 함수는 로그 설정과 무관하게 Shipping에서 컴파일 경로가 활성화되지 않도록 명시적으로 Guard한다.
- 실제 Shipping 운영 로그가 필요해질 경우 Debug Log와 섞지 않고 별도 정책을 먼저 정의한다.

---

# 17A. Packaging Pipeline

- Project Version Source of Truth는 `Config/DefaultGame.ini`의 `ProjectVersion`이다.
- Win64 Development / Shipping Package는 `Tools/Packaging/PackageProject.ps1`로 생성한다.
- Package 출력은 `Build/Packages`, Steam Depot 후보는 `Build/SteamCandidate` 아래에 생성한다.
- 프로젝트에서 사용하지 않는 기본 `ChaosCloth` Plugin은 비활성화하며, 그 의존성인 `Buoyancy`, `Water`, `Landmass`의 Editor Content를 Release Cook에 포함하지 않는다.
- `HeistBuildDump`는 Development Package에서 Version, Configuration, Platform, Cooked Runtime, Online Subsystem과 Session Build Id를 검증한다.
- Editor Archive Directory를 Development와 Shipping에 재사용해 이전 Runtime Binary 또는 Log가 섞인 폴더는 Steam Depot 후보로 사용하지 않는다.
- `ValidatePackage.ps1`는 Development / Shipping Runtime Binary 혼합을 실패 처리하고 UE 5.8 Prerequisite의 `UEPrereqSetup_x64.exe` 또는 `vc_redist.x64.exe`를 허용한다.
- Steam Depot VDF는 `preview=1` 후보만 생성하며 Upload와 SetLive는 자동 수행하지 않는다.

---

# 18. Verification Standard

각 검증 결과는 다음을 구분한다.

- `Implementation Complete`
- `Blueprint/Data/Map Pending`
- `User PIE Pending`
- `PASS`
- `FAIL`
- `BLOCKED`

PIE가 필요한 검증은 다음을 명시한다.

- PIE Mode
- Player 수
- 실행할 Window
- 입력
- Debug Command
- 기대 화면 동작
- 기대 Log
- PASS 신호
- FAIL 신호

Known Warning은 숨기지 않는다.

## Known Non-blocking Warnings

- `aqProf.dll` / VTune 선택적 Profiler 경고와 Title / Lobby 전환 중의 일시적 AI Perception / Recast 경고는 현재 확인된 비차단 Known Warning이다.
- Crash, Travel 실패 또는 Gameplay Map 회귀가 동반되면 다시 분류한다.

다음 문제는 Release 검증을 차단한다.

- Critical Replication
- Ownership 위반
- Duplicate Artifact
- Input Restore 실패
- Orphan Session Lock
- Missing Parent Class
- Invalid DataTable Enum
- Removed Class Soft Reference
- Forgery Score NaN
- Client-side Authoritative Mutation

---

# 19. Legacy Preservation

다음 구현은 아직 참조 감사가 끝나지 않은 경우에만 Legacy로 보존할 수 있다.

- Top-Down Camera 관련 Blueprint
- Cursor Aim 관련 입력

새 흐름에서는 호출하지 않는다.

Reference Viewer와 회귀 확인 후 별도 정리 작업에서 제거한다.

기존 검증 기록은 재사용 가능한 Regression Baseline으로 보존할 수 있다.

다음 기능은 Legacy Preservation 대상이 아니다.

- Smoke Grenade
- Smoke Projectile
- Smoke Cloud
- Glue Trap
- Noise Trap
- Trap Placement Cast
- Smoke / Trap QuickSlot
- Trap 전용 GameplayTag
- NoiseTrap SoundPing

---

# 19A. Greed Decision Framework

이 규칙은 게임이 플레이어 대신 `더 훔치기 / 탈출`을 자동 선택하도록 만드는 AI가 아니다. Contract Run의 UI, Telemetry, Playtest와 코딩 에이전트가 동일한 판단 근거를 사용하도록 하는 설계·검증 계약이다.

한 번의 판단에서는 단일 결과만 허용한다. P0~P6 충돌 시 가장 높은 우선순위의 결과가 다른 분기를 독점하며 중복 실행하지 않는다.

## Decision Trigger

- 상태 전환 직후: 탐색↔작업, Alert 단계 변경, Session Owner 교체
- 작업 진행 중 10초 간격
- Lockdown, Arrest, Timeout, Kick, Disconnect 직후
- 매 Match에서 최소 1회 판단 근거를 Telemetry 또는 Debug Snapshot으로 재현할 수 있어야 한다.

## Decision Input Contract

| 입력 | 정의 | 범위 |
|---|---|---|
| `PotentialGain` | 현재 Target에서 기대되는 수익 | 0~1 정규화 |
| `SurvivalMargin` | 거리, Guard 반응, 이동 안정성 | 0~1 정규화 |
| `ExtractionProximity` | 거리와 Exit 상태를 반영한 탈출 성공성 | 0~1 정규화 |
| `AlertPressure` | Quiet~Lockdown 단계 | 0~4 원본, 식에 넣기 전 `AlertStage / 4`로 정규화 |
| `RecoveryRisk` | 실책 복구 난이도 | 0~1 정규화 |
| `CarryWeight` | 현재 운반 무게 | P1 우선순위 조건에 사용 |
| `MatchUrgency` | 경과 시간과 목표 거리 압박 | P0/P4 조건과 Telemetry에 사용 |

```text
DecisionScore =
    0.40 × PotentialGain
  + 0.25 × SurvivalMargin
  + 0.20 × ExtractionProximity
  - 0.10 × NormalizedAlertPressure
  - 0.05 × RecoveryRisk
```

- `DecisionScore >= 0.55`이면 확장 후보, 미만이면 퇴각 후보로 분류한다.
- 원문 초안의 `1.0` Threshold는 양수 항의 이론상 최대가 `0.85`라 도달할 수 없으므로 사용하지 않는다.
- Score는 P0~P6보다 낮은 보조 판단이다. Priority Rule이 발동하면 Score 결과를 무시한다.

## Priority Branch P0~P6

| 순위 | 조건 | 단일 판정 | 실행 결과 |
|---|---|---|---|
| P0 | Alert가 Searching 이상 또는 Lockdown 시작 60초 이내 | 퇴각 | 작업 중단 후 탈출 준비 |
| P1 | CarryWeight가 2.5 이상 또는 현재 속도 210cm/s 미만 | 퇴각 | 저중량 Route로 회귀, Loot Drop 또는 포기 |
| P2 | 동일 Exhibit 20초 이상 체류, Timeout 1회 누적 또는 Guard 추적 | 퇴각 | 현재 작업 Cancel 또는 즉시 Submit 후 퇴각 |
| P3 | 최근 1분 Major Incident 0회이고 Quota 미달 | 저위험 확장 | 추가 시도 1회 허용, Sprint 제한 |
| P4 | Reward Remaining이 평균 Target 기여치의 1.5배 이상, Alert가 Alarmed 이하, MatchTime 80% 이하 | 확장 | 고가 또는 중가 Target 1회 추가 시도 |
| P5 | QuotaMargin 0.15 이하, Alert가 Suspicious 이하, CarryWeight 2.0 이하, Team 경보 공유 완료 | 퇴각 | 즉시 Extraction Mode 전환, 탐색 1회만 예외 허용 |
| P6 | Team Alive 2인 이상이고 Current Target 미확인 | 분산 | 탐색과 감시 역할로 분리 |
| 기본 | 위 규칙 미충족 | 보수적 퇴각 | 안전 Route와 Deposit 우선 |

## State Transition And Feedback

| 현재 상태 | 조건 | 다음 상태 |
|---|---|---|
| 탐색 | 확장 후보이며 Target 확인 | 작업: Forgery / Assembly |
| 작업 | P0/P1/P2 발동 | 탈출 준비 |
| 작업 | Submit 성공 | 휴대 / 교체 완료 |
| 탈출 준비 | Exit 승인 | 탈출 실행 |
| 탈출 실행 | Deposit 성공 | Player 정산 또는 Team Result 대기 |
| 탈출 실행 | Route 차단 또는 재추적 | 재판단 |

상태 전이는 HUD, Team Status, Map, Audio 중 최소 두 채널에 동기화한다. 판단 근거를 Player-facing 숫자 공식으로 노출하지 않으며, 화면에는 원인과 권장 행동을 자연어·Icon·Audio로 표현한다.

## Verification

- 10초 판단과 상태 전환 판단 Trigger가 중복 실행되지 않는지 확인한다.
- P0~P6 충돌 시 하나의 Branch만 선택되는지 확인한다.
- `DecisionScore` 입력이 모두 정의된 정규화 범위를 지키고 NaN/Inf가 없는지 확인한다.
- Match End에서 선택된 Priority, Score Input과 최종 Branch를 Debug Snapshot으로 재현할 수 있어야 한다.

---

# 20. Product Direction

현재 제품 방향은 Rev 11 — Contract Run And Player Experience Foundation이다.

`Contract 확인 → Forgery/Assembly → Original/Loose Loot Carry → Alert/Chase → Player별 Deposit → Outcome/Result → Lobby Return`의 단일 완주 흐름을 먼저 닫는다. 이후 협동 가독성과 탐욕/퇴각 리듬, 세 맵의 Release Shape, QA와 Release Gate 순서로 완성도를 높인다.

## Product Completion Gate

기능은 다음 네 조건을 모두 만족해야 완료된 것으로 설명할 수 있다.

1. **Functional**: C++ Authority, Validation, Replication과 Data Contract가 구현된다.
2. **Integrated Loop**: Debug Command 없이 실제 게임 흐름에서 진입, 실행, 종료와 상태 원복이 가능하다.
3. **Player Experience**: 플레이어가 목표, 위험, 실패 원인과 다음 행동을 HUD, World Presentation, Audio 중 최소 두 채널로 이해한다.
4. **Replay**: 동일 세션 또는 Lobby Return 이후 두 번째 판을 Softlock과 잔여 상태 없이 시작한다.

Build 성공이나 단일 함수 호출 성공만으로 기능 완료를 주장하지 않는다. Asset/Map 배치가 필요한 기능은 C++ 구현과 실제 Gameplay 검증을 구분해 설명한다.

## Rev 11 Execution Priority

- Player가 이해할 수 없는 숨은 상태를 먼저 제거한다.
- Contract Run의 Required Target / Quota / Secured Value를 Extraction / Result보다 먼저 확정한다.
- Walk / Sprint, Nameplate, Team Status, Map과 Status Feedback은 Polish로 미루지 않는다.
- Gameplay Rule, Authority, Validation과 Replication은 계속 C++가 소유한다.
- Animation, Audio, VFX와 Layout은 승인된 C++ State Hook을 표현한다.
- 한 판의 완주 가능성을 먼저 닫고, 이후 협동 가독성과 탐욕/퇴각 리듬을 검증한다.
- 세 맵을 Release Shape로 만든 뒤 신규 Gameplay Feature를 잠근다.
- 그다음 QA, RC, 외부 테스트와 Final Release Gate를 진행한다.
- Public Release 목표일 `2026-09-20`을 유지하되 RC Gate가 실패하면 날짜 때문에 통과시키지 않는다.

제품 경험과 밸런스 의도는 `Museum_Heist_GDD.docx`, 구현 계약은 `Museum_Heist_TDD.docx`를 확인한다.
