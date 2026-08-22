# Project_MuseumHeist Local Progress Inbox

최종 갱신: 2026-08-23 KST

이 문서는 Notion에 아직 연결되지 않았거나 반영되지 않은 실질 작업을 잃지 않기 위한 Reconciliation Queue다.

- 공식 Task 상태, 우선순위와 실행 순서의 Source of Truth는 Notion `주차별 작업보드`다.
- 이 문서의 Entry는 Notion Task 진행률 또는 완료를 의미하지 않는다.
- 새 작업은 Notion을 먼저 라이브 조회한 뒤 Active Entry를 대조한다.
- 코드, Asset, 문서, Gameplay 방향 또는 검증 증거가 실질적으로 바뀐 경우만 기록한다.
- 단순 조회, 변경 없는 진단과 반복 상태 확인은 기록하지 않는다.

## State Definitions

```text
UNLINKED
- 정확히 대응하는 Notion Task가 확인되지 않았다.

READY_TO_SYNC
- 대응 Notion Task는 확인됐지만 진행 증거가 아직 Notion에 반영되지 않았다.

RECONCILED
- Notion Relation과 필요한 진행 증거 반영을 라이브로 확인했다.

NO_TASK_REQUIRED
- 프로젝트 운영 기록이며 별도 Notion Task가 필요 없다고 확정했다.
```

---

## Active Queue

### LOCAL-20260823-01 — Title Menu Responsibility Split And Button State Art

- State: `UNLINKED`
- Created: `2026-08-23 KST`
- Notion Relation: `NONE` / 라이브 조회에서 대응 Title UI Task 없음 (`TASK-W9-007`만 진행중이며 별도 범위)
- Sync Boundary: 사용자 승인으로 C++·Widget Blueprint·UI Texture를 구현하고 로컬 검증했으며 Notion Task 생성이나 상태 변경은 하지 않았다.

#### Applied Work

- Title UI C++를 `UI/Title/ViewModels`와 `UI/Title/Widgets` 아래로 이동하고, Master `UHeistTitleMenuWidget`, Session 전용 `UHeistTitleMenuViewModel`, 별도 `UHeistSessionJoinWidget`, Settings 전용 `UHeistSettingsViewModel`과 `UHeistSettingsWidget`으로 책임을 분리했다.
- 방 만들기는 기존 서버 권한 Session 생성·Lobby Travel 경로를 유지하고, 방 참가와 설정은 Master 안에 별도 Child Widget으로 열리도록 연결했다. 게임 종료는 C++에서 로컬 Owning PlayerController를 사용해 처리한다.
- `/Game/Blueprints/UI/Title`에 `WBP_TitleMenu`, `WBP_SessionJoin`, `WBP_Settings`를 구성했다. Widget Blueprint EventGraph Gameplay Logic은 추가하지 않았다.
- 공용 Title Button용 Normal/Hovered/Pressed RGBA Texture 3개와 Text-free Logo Emblem을 생성해 `/Game/Assets/UI/Title`에 Import하고, 네 메인 버튼에 동일한 3상태 Brush Set을 적용했다.
- Source PNG, 생성 근거, 재생성·검증 스크립트와 Shipping Asset Manifest 항목을 함께 보존했다.
- 후속 UI 검토에서 Master Title의 `SessionStatusText`와 `SessionErrorText` C++ BindWidget·Presentation 갱신을 제거했다. SessionJoin의 사용자 조치용 오류 피드백은 유지했고 WBP는 사용자가 편집 중이므로 변경하지 않았다.
- SessionJoin 후속 검토에서 `SessionStatusText`와 `SessionActionHintText`의 C++ BindWidget, ViewModel FieldNotify·문구 생성과 Debug Title UX 의존을 제거하고 `SessionErrorText`만 유지했다. 현재 열린 Editor의 사용자 WBP 편집은 건드리지 않았다.
- SessionJoin의 `다시 시도`와 `요청 취소` 버튼은 존재를 항상 노출하고, 실패 시 Retry만 Enable·진행 중 Cancel만 Enable하도록 C++ Visibility 전환을 제거하고 기존 `CanRetrySessionOperation` / `CanCancelSessionOperation` 상태만 적용한다.
- 버튼 3상태 Source PNG는 승인된 RGBA 픽셀을 유지한 채 Alpha Bounds 기준 6px Padding으로 Tight Crop하고 Texture 전용 Unreal 재임포트를 수행했다.

#### Evidence

```text
Project_MuseumHeistEditor Development Build       PASS / Result Succeeded / 17 actions
WBP_TitleMenu Parent / BindWidget / Compile       PASS / 8 required variables
WBP_SessionJoin Parent / BindWidget / Compile     PASS / 8 required variables
WBP_Settings Parent / BindWidget / Compile        PASS / 12 required variables
Main Button Normal/Hovered/Pressed Brush           PASS / 4 of 4
Generated PNG RGBA Alpha Validation                PASS / 4 of 4
Tight Button Source Dimensions                     PASS / 2033x491, 2033x513, 1895x514 / ColorType 6
Texture-only Unreal Reimport                       PASS / 3 of 3
Title WBP SHA-256 Before/After                      IDENTICAL / 3 of 3 / no UMG mutation
Master Status/Error C++ Removal Build               PASS / Result Succeeded / 9 actions
SessionJoin Status/ActionHint C++ Removal            STATIC CHECK PASS / BUILD NOT RUN / Editor active
git diff --check                                   PASS / line-ending warning only
Notion Write                                       NOT DONE / corresponding task absent
Rendered Designer / PIE Visual QA                  NOT RUN / user inspection pending
```

#### Remaining Evidence / Presentation Boundary

- Commandlet 검증은 C++ Parent, BindWidget, Blueprint compile과 Brush Reference를 증명하지만 실제 화면의 비율·간격·Hover/Pressed 체감은 증명하지 않는다.
- 다음 Editor 확인에서 사용자가 Title Designer/PIE 화면을 보고 크기와 배치 조정 범위를 확정한다. Session Host/Join과 Settings 적용 E2E는 이 UI 시각 조정 뒤 별도 검증한다.

---

### LOCAL-20260822-03 — Steam Proximity Voice / Guard Speech Noise

- State: `UNLINKED`
- Created: `2026-08-22 KST`
- Notion Relation: `NONE` / 라이브 조회에서 대응 Voice Task 없음
- Sync Boundary: 사용자 승인으로 구현·로컬 검증했으며 Notion Task 생성이나 상태 변경은 하지 않았다.

#### Applied Work

- GDD → TDD → AGENTS 순서로 `V` Hold PTT, Lobby/Result 비공간 팀 보이스, InGame 거리 보이스와 Guard Hearing 계약을 동기화했다.
- Steam Online Voice 전송과 Character의 `UVOIPTalker` 재생을 연결하고, InGame은 `0~250cm` 정상 음량 / `250~1,500cm` 감쇠 / 이후 무음으로 설정했다.
- Client는 PTT 중 `IOnlineVoice::IsLocalPlayerTalking`으로 실제 발화 상태만 저빈도로 보고한다. PCM·원본 진폭·녹음·전사 데이터는 Gameplay RPC와 Telemetry에 포함하지 않는다.
- Server는 Ownership, InGame Phase, Pawn과 PTT 상태를 검증한 뒤 Character 확정 위치에서 `Event.SoundPing.Voice`를 `800cm / 0.8초`로 생성한다. Alert를 직접 올리지 않고 Guard의 `InvestigateNoise` 후보로만 사용한다.
- Guard 소음 우선순위는 `StunHit 0 / GlassBreak·ReplicaSwap 1 / CoinImpact 2 / Voice 3 / Footstep 4`로 확정했다.
- 개별 출력 Mute와 Guard Hearing을 분리하고, C++에 `IsVoicePushToTalkHeld`, `IsLocalVoiceSpeaking`, `GetLocalVoiceActivityLevel`, Remote `GetVoiceLevel`, Player Mute 조회/변경 훅만 제공했다. Widget Blueprint와 UI Layout은 변경하지 않았다.

#### Evidence

```text
GDD/TDD OOXML Structural QA        PASS / ZIP·Heading·Table·Field 보존
GDD/TDD Visual PNG QA              NOT RUN / LibreOffice·soffice 미설치
Single-thread UHT                  PASS / Editor·Game generated code refreshed
Editor Development Build          PASS / Saved/Logs/VoiceEditorBuild.log
Game Development Build            PASS / Saved/Logs/VoiceGameBuild.log
Voice Static Contract Automation  PASS / 1 of 1 / Saved/Logs/VoiceAutomation.log
git diff --check                   PASS / line-ending warning only
Widget / uasset                    NOT CHANGED
Notion Write                       NOT DONE / 대응 Task 없음
```

#### Remaining Evidence / UI Boundary

- 실제 Steam 음성 송수신, 250/1,500cm 청음, 개별 Mute와 발화→Guard 조사는 서로 다른 Steam 계정 2개의 Development Package에서 검증해야 한다. Editor Null Subsystem 자동화는 이를 대체하지 않는다.
- Lobby/HUD/Nameplate에 Speaking·Mute 아이콘을 배치하는 작업은 현재 Widget 개편 논의 후 진행한다.
- 이 UE 5.8.1 설치본의 번들 .NET 10 UHT는 병렬 헤더 파싱에서 `AccessViolation`이 재현됐다. Header regeneration이 필요한 빌드는 공식 `UnrealHeaderTool -NoGoWide` 단일 스레드 생성 후 UBT를 실행하면 통과한다.

---

### LOCAL-20260822-02 — W7 Asset-Deferred Gate Completion

- State: `RECONCILED`
- Created / Reconciled: `2026-08-22 KST`
- Notion Relations: [`W7`](https://app.notion.com/p/39a1d26a5dfb81948e6ce351c3abb413), [`TASK-W7-004`](https://app.notion.com/p/3ad1d26a5dfb81ceaf09d805b0435905), [`TASK-W7-005`](https://app.notion.com/p/3ad1d26a5dfb812c80d5f37473fed9ab), [`TASK-W7-006`](https://app.notion.com/p/3ad1d26a5dfb81beb59afc7f124fca89), [`TEST-W7-006`](https://app.notion.com/p/3be1d26a5dfb8137ab1cf7482289c24a)
- Live Status: `TASK-W7-004~006 완료`, `W7 Gate=Pass`, `TEST-W7-006=Pass` 쓰기·재조회 확인

#### User Decision

- 현재 확보하지 않은 최종 Niagara/Sound·Remote Stun/Carry/Heavy Pose 에셋은 W7 완료 Gate에서 제외한다.
- 기존 C++/복제/HUD/Nameplate/후처리/Audio 호출/Footstep/Input/Cleanup과 상태별 Asset 슬롯을 에셋 비의존 완료 기준으로 사용한다.
- 최종 에셋은 추후 별도 결합하며 W7 Task와 Gate를 다시 열지 않는다.

#### Evidence

```text
Editor Development Build   PASS / Target up to date / Result Succeeded
W7 Regression              PASS / 11 of 11 / Success 9 / WithWarnings 2 / Failed 0 / NotRun 0
W7 Regression Report       Saved/Automation/W7-AssetDeferred-FinalRegression/index.json
W7 Regression Log          Saved/Logs/W7-AssetDeferred-FinalRegression.log
M01 2P TwoRuns             PASS / Success / WithWarnings 1 / Failed 0 / 2 Runs / Lobby Clean Reset
M01 2P Report              Saved/Automation/W7-AssetDeferred-Final2P/index.json
M01 2P Log                 Saved/Logs/W7-AssetDeferred-Final2P.log
Notion                     TASK-W7-004~006 완료 / TEST-W7-006 Pass / W7 Gate Pass / live re-fetch PASS
```

#### Warning Boundary

- 자동화 경고는 v1에서 의도적으로 비활성인 Object Assembly 접근 거부, 임시 PIE의 Recast 부재, 범위 밖 Noise 거부와 렌더 CVar 진단이다.
- 자동화 Error와 Failed Test는 0이다.
- NullRHI/D3D12 자동화는 최종 에셋의 시각 품질과 주관적 청음을 증명하지 않는다. 해당 검증은 실제 에셋 결합 시 수행한다.

---

### LOCAL-20260818-01 — Rev14 Public Release Scope Rebaseline

- State: `PARTIALLY_RECONCILED`
- Created: `2026-08-18 KST`
- Last Updated: `2026-08-22 KST`
- Reconciled: `2026-08-18 KST` (문서 재기준화 범위만 해당)
- Notion Relations: [`W8`](https://app.notion.com/p/39a1d26a5dfb81ffad91ff21b194e505), [`TASK-W8-008`](https://app.notion.com/p/3c01d26a5dfb81a68634d1d63f15e791), `TASK-W8-001~007`
- Sync Boundary: 2026-08-18 문서 재기준화에 이어 2026-08-22 CCTV·Laser 2P 자동 계약은 Notion [`TEST-W8-001`](https://app.notion.com/p/3c41d26a5dfb81ccb1fcd8e59c4dcc4f)로 기록·재조회했다. 실제 Holder Disconnect는 사용자 결정으로 `SKIPPED / NOT RUN / RISK ACCEPTED` 처리했고, `TASK-W8-001~008`은 라이브 재조회 기준 `미시작`이며 어떤 Task도 완료·PASS로 올리지 않았다.

#### User Request / Decision

- Public Contract 지원 인원을 1~4인에서 `2~4인`으로 바꾸고, 1인 실행은 Editor/자동화 개발 경로로만 남긴다.
- 2인으로 시작한 Match가 InGame 이탈로 1인이 되어도 즉시 실패·즉시 종료하지 않고, 이탈자의 Loot/Original Drop·Session/Interaction Lock 해제 후 남은 Player가 계속 진행하거나 탈출하는 soft-fail을 사용한다.
- v1 시큐리티 기믹은 기존 `Patrol Guard`를 유지하고 `CCTV`와 고가 Painting용 `Laser Hold`를 추가한다.
- Laser Hold는 한 Player가 외부 Hold Switch를 유지하고 다른 Player가 Painting 영역에 진입하는 선택적 2인 협동 기믹으로 사용한다.
- v1 위조 콘텐츠는 Painting `Surface Forgery`를 중심으로 재편한다.
- `Object Assembly`는 삭제하지 않고 `Deferred Expansion`으로 전환한다. Repository의 소스·C++·Enum·Struct·DataTable·Blueprint Shell·SourceArt는 보존하되, v1 Runtime Assignment·Release Map·Player-facing UI·Result·Cook·QA/Release Gate에서는 비활성화한다.

#### Documentation And Implementation Boundary

- `CURRENT_PROJECT_STATUS.md`와 Release Asset/Notice 문서에 Rev14 출시 범위와 Deferred 경계를 재기준화한다.
- C++ Rule/Authority/Replication 기반에는 Public 2~4 Start Gate, immutable Start Player Snapshot, InGame Join 거부, Surface-only Assignment/Result, Object Runtime Entry 차단, CCTV, Laser/Hold Button, 공통 Alert/Guard Incident 경로를 반영했다.
- InGame 원격 Client Quit은 Pawn 파괴 전 Loose Loot를 월드에 복구하고, 기존 Logout 경로에서 Original·Case Lock·Security Hold를 해제한 뒤 Contract Carried Value를 다시 계산하도록 연결했다.
- 기존 자동화의 Public Solo Start와 Object Assembly 성공 기대는 Rev14의 거부·불변 경계로 재정렬했다.
- Pending Start Count와 Local Hold 입력 초기화, GameState Delegate 해제, Loot 제거 후처리, 중복 Timer Handle 무효화와 Laser 기본 상태 복구를 같은 파일 안의 private helper/delegation으로 정리했다. 파일 분할·새 Manager·공개 Gameplay 계약 변경은 하지 않았다.
- Widget은 변경하지 않았다. M01/M02/M03 Release Map에는 맵별 Painting Case 6개, Loose Loot 5개, CCTV/Laser/Hold Button 각 1개, Guard 1개, Waypoint 4개와 NavData를 배치·연결했다. Artifact DataTable에는 맵별 Target/Optional Surface Row와 Loose Loot Row를 추가했다.
- `/Game/Blueprints/World/Actors/Security`에 `BP_SecurityCamera`, `BP_LaserBarrier`, `BP_SecurityHoldButton` 공용 Shell을 추가했다. C++ Parent와 상속 Component를 유지하고 EventGraph는 비워 둔다. 기본 Mesh 가시성과 Button 눌림 Scale은 C++ `ApplyPresentation()`이 갱신한다.
- StarterContent Mesh/Material은 배치와 동작 확인용 임시 표현이다. 최종 Security VFX/Audio 슬롯은 비워 두고 W8 Presentation 작업이 교체·할당한다.
- 사용자가 세 EventGraph를 제거한 상태를 유지하고 기본 표현을 C++로 이동했다. 현재 Revision은 Full Editor Build를 통과했고, 세 Blueprint도 Unreal Editor에서 재컴파일·저장 후 `BS_UP_TO_DATE`, Data Validation 3/3 유효를 확인했다. `SandBoxMap` Security 자동화에 이어 M01/M02/M03의 실제 배치 CCTV 탐지 1회→Alert/근처 Guard 조사 1회, 단일 Holder, 비-Holder Laser 통과, Release/Rearm과 2인 Contract 두 판을 자동 검증했다. Cook은 실행하지 않았다.
- 기존 W6/W7의 Solo/1P, Surface/Object, 1/2/3/4P 자동화·PIE·Package 증거는 당시 구현의 역사적 증거로 보존한다. Rev14 현재 계약으로 바꿔 쓰거나 삭제하지 않는다.

#### Required Follow-up Evidence

1. Notion W8과 `TASK-W8-001~008`의 Rev14 Acceptance Criteria·우선순위·실행 순서 동기화 및 재조회는 완료했다. 구현 증거와 상태 변경은 별도다.
2. Public Lobby 2~4 Start Validation과 InGame 2→1 Snapshot 불변은 C++에 반영했다. 현재 Revision Build와 Host/Client 멀티플레이 증거가 필요하다.
3. Release Contract의 Eligible Forgery Type을 Surface/Drawing으로 제한하고 Object Runtime Entry·Result 경로를 차단했다. User Widget 개편이 끝난 뒤 Player-facing Object 경로 0을 별도로 대조한다.
4. Release Map Object Case 배치/참조 0과 Final Cook의 Object Assembly 전용 Hard Reference/Package 0을 검증하되, Deferred 소스·데이터·Shell은 보존한다.
5. CCTV 서버 Detection/Alert와 Laser Hold/Release/Stun·Arrest·Disconnect 기반 및 비-Widget Blueprint Shell을 반영했다. `SandBoxMap` 및 M01/M02/M03의 `Hold.LinkedLaserBarrier → Laser`, `Laser.ProtectedPaintingCase → FourStar Painting Case`, CCTV 사건당 Alert+경비 조사 1회, 단일 Holder와 Release/Rearm은 2인 Listen Server 자동화에서 PASS했다. 실제 Holder Disconnect는 사용자 결정으로 `SKIPPED / RISK ACCEPTED`이며 PASS로 재분류하지 않는다. 3인/4인 밸런스, 독립 Egress와 이탈 Player의 Loose Loot/Original 월드 복구는 후속 Host/Client Gate 범위로 남긴다.
6. 실제 사용할 CCTV/Laser Mesh·Material·VFX·Audio·Icon을 추가하면 `Docs/SHIPPING_ASSET_MANIFEST.md`와 Notice/라이선스 근거를 같이 갱신한다.
7. 마지막 Player Disconnect의 `Logout()` Outcome 판정이 `PlayerArray` 제거보다 먼저 실행되는 순서를 별도 기능 이슈로 재현·수정한다. 이번 보일러플레이트 정리에서는 동작 변경을 피하기 위해 미수정했다.

#### Verification

```text
Local Handoff Docs              UPDATED / 2026-08-22 Sandbox and public-path results
C++ Rule/Authority/Replication  UPDATED / Full Editor Build PASS
Duplicate / Boilerplate Cleanup APPLIED / same-file / git diff --check PASS
Automation Expectations         UPDATED / Rev14 rebaseline
Security Blueprint Shell        PASS / 3 assets / EventGraph removed by user / Compile+Save+Validation 3/3
Temporary Security Visuals      ASSIGNED / StarterContent / final VFX+Audio pending
Widget                          NOT CHANGED
Release umap                    UPDATED / M01·M02·M03 instances and references
SandBoxMap Test Instances       PASS / CCTV + Hold→Laser→FourStar Painting
Release Map Instance Links      PASS / 3 maps / direct references valid
Release Map NavData             PASS / M01·M02·M03 rebuilt and saved
Full Editor Link                PASS / UnrealEditor-Project_MuseumHeist.dll
Build Evidence                  Saved/Logs/W8-ReleaseGameplay-FullBuild.log
Blueprint Evidence              Saved/Logs/W8-SecurityBlueprint-CompileSave.log
Security Policy Automation      PASS / Saved/Automation/W8-SecurityIncidentPolicy
SandBox 2P Listen Server        PASS WITH WARNINGS / RecastNavMesh absent + EOS offline / gameplay errors 0
M01·M02·M03 2P TwoRuns          PASS WITH WARNINGS / placed security and contract assertions pass
3P / 4P Release Gate            NOT RUN / user multiplayer and balance evidence required
Holder Actual Disconnect        SKIPPED / NOT RUN / user accepted residual risk
Visual / Audio PIE              NOT RUN / temporary presentation assets only
Cook                            NOT RUN
Notion Read                     PASS / TASK-W8-001~008 미시작
Notion Test Log                 PASS / TEST-W8-001 created and re-fetched
Notion Task Write               NOT DONE / status unchanged
Historical Evidence             PRESERVED / not reclassified
```

---

### LOCAL-20260809-01

- State: `UNLINKED`
- Created: `2026-08-09 KST`
- Last Updated: `2026-08-09 17:19 KST`
- Notion Relation: `NONE`
- Explicit Exclusion: [`TASK-W6-006`](https://app.notion.com/p/39a1d26a5dfb8132a835ee75d548e348) `Player Contribution Capture`와 완료 기준이 다르므로 연결하지 않는다.

#### User Request / Decision

- Forgery Quality 기반 Alert/Lockdown 패널티를 제거한다.
- 서버 최종 Quality 70 이상일 때만 Replica를 승인한다.
- Timeout은 작업을 폐기하고 Alert 변화 없이 근처 Guard 한 명의 1회 조사만 발생시킨다.
- Surface Forgery와 Object Assembly의 한글 Title, 70점 기준을 포함한 단일 예상 품질, Timer, Submit/Cancel과 통합 하단 안내 구조를 통일한다.
- 별도 `InstructionText`와 `ModeStatusText`는 사용하지 않고 작업 설명은 Tutorial과 통합 하단 안내가 담당한다.
- 작업 화면의 Alert/Lockdown 상세 Text는 제거한다. Suspicious/Searching에서는 작업을 유지하고, Alarmed/Lockdown에서만 서버가 Session을 선취소한 뒤 화면을 강제 종료해 탈출 판단을 우선한다.
- Object Assembly는 버튼식 3D Preview 대신 Original/정답을 숨긴 기억 기반 2D 조각 Drag & Drop으로 교체한다.
- 공통 UI의 제목·제출·취소에는 TENADA를 사용하고, 점수·시간·통합 하단 안내와 Brush Label은 가독성용 한글 본문 폰트를 유지한다.

#### Changed Scope

- C++ Authority / Validation / Timeout Investigation
- Surface/Object Widget C++ 및 Widget Blueprint
- 한글 Runtime Font Asset 및 WBP TextBlock 27개 적용
- TENADA FontFace/Composite Font 임포트, 제목·제출·취소 6개 적용, WBP의 `??` 정적 한글 19개 복구
- 사용자 편집본에서 `WBP_HeistForgery.ModeStatusText`와 두 WBP의 `InstructionText` 제거를 확인하고 C++ `BindWidgetOptional`/갱신 코드 정리. WBP Layout 수치는 변경하지 않음
- Object Assembly 2D Part Tray/Canvas, 숨은 Socket Anchor 양자화, Wheel 회전, 우클릭 제거, R Reset
- Surface 서버 `QualityBelowMinimum` 거절 시 동일 Drawing 재제출 차단 및 로컬 수정 후 제출 재활성화
- Artifact DataTable의 `MinimumForgeryScore=0.7`
- `AGENTS.md`, `Museum_Heist_GDD.docx`, `Museum_Heist_TDD.docx` Rev 12
- 상세 파일 목록과 변경 계약: [`CURRENT_PROJECT_STATUS.md`](CURRENT_PROJECT_STATUS.md#3-implemented-artifacts)

#### Verification

```text
C++ Editor Build        REBUILD REQUIRED / 마지막 PASS 이후 Instruction·ModeStatus C++ 정리
WBP Compile / Save      PASS / 두 WBP Compile=true, Dirty=false
Removed Widget Tree     PASS / Forgery Instruction·ModeStatus=None, Assembly Instruction=None
Korean Font Render      PASS
TENADA Import / Render  PASS / Compile+Save / QuestionMark=0 / TargetBlocks=6
DataTable Save          PASS
Document Render QA      PASS / Word PDF GDD 29p, TDD 40p, 전 페이지+변경 페이지 검수
git diff --check        PASS
Automation              PASS / ReplicaAcceptanceContract
User PIE                NOT RUN / REQUIRED
Multiplayer Runtime     NOT RUN / REQUIRED
Notion Write            NOT DONE
```

#### Remaining Evidence

- `ProjectMuseumHeist.Forgery.ReplicaAcceptanceContract` 자동화 PASS (`2026-08-09 15:42 KST`)
- 2 Player Listen Server PIE에서 Owner-only UI, 70점 Gate, Timeout 1회 조사, Alert 불변 확인
- Surface/Object 작업 중 Suspicious/Searching Session 유지, Alarmed/Lockdown 서버 선취소, Widget 강제 종료, 신규 재진입 거부, Gameplay Input 복원 확인
- Object 2D Drag & Drop, Socket 양자화, Wheel/우클릭/R 입력과 제출 후 Compact Payload 기반 3D Replica 확인

#### Next Reconciliation Action

1. User PIE 증거를 수집한다. 자동화 Gate는 PASS했다.
2. 2026-08-09 15:44 KST 라이브 검색에서는 정확히 일치하는 진행중 Task가 없고, 기존 `TASK-W5-013`은 버튼식 계약으로 이미 완료임을 유지한다.
3. 사용자가 신규 Task 생성 또는 별도 회귀 Task Relation을 결정한다.
4. 사용자가 Notion 반영을 요청하고 쓰기 성공을 재조회한 뒤 `RECONCILED`로 이동한다.

---

## Reconciled Archive

### LOCAL-20260816-02 — W7 Final Presentation Verification

- State: `RECONCILED`
- Created: `2026-08-16 KST`
- Reconciled: `2026-08-18 KST`
- Notion Relations:
  - [`TASK-W7-004`](https://app.notion.com/p/3ad1d26a5dfb81ceaf09d805b0435905)
  - [`TASK-W7-005`](https://app.notion.com/p/3ad1d26a5dfb812c80d5f37473fed9ab)
  - [`TASK-W7-006`](https://app.notion.com/p/3ad1d26a5dfb81beb59afc7f124fca89)
  - [`TEST-W7-006`](https://app.notion.com/p/3be1d26a5dfb8137ab1cf7482289c24a)
- Last Live Status: `완료` / 2026-08-22 에셋 비의존 완료 기준 재검증, TASK-W7-004~006 완료 및 W7 Gate Pass 재조회 확인

#### Applied Work

- 실제 `WBP_HeistHUD` Tree에 `StunOverlay`, `StunCountdownText`, `ArrestOverlay`, `ArrestTitleText`, `ArrestInstructionText`를 추가하고 기존 Widget Size는 변경하지 않았다.
- Stun Vignette·Low-pass, Arrest/Rescue Edge Audio, Carry/Heavy Icon·Spatial One-shot Audio와 Match/Lobby Cleanup을 구현했다.
- 2P TwoRuns Lobby Reset 실패는 지연 생성되는 `ResultWidget=null`을 정상 Lobby 상태로 허용하도록 Test Fixture를 수정해 Production 계약과 일치시켰다.
- UE5 `SKM_Manny_Simple`과 `ABP_Unarmed`를 `BP_HeistPlayerCharacter`의 임시 Full-body/Locomotion 베이스로 연결하고 Camera Socket을 `head`로 맞췄다.
- `/Game/Blueprints`의 Texture·Audio·Input·StateTree·Font·Material 29개를 `/Game/Assets`로 이동해 Blueprint/WBP 전용 경계를 확정했다.
- 상태별 Component를 증식하지 않고 `CrewStatusVFXComponent`·`CrewStatusTransitionAudioComponent` 각 1개를 재사용하며, 7개 non-Active 상태별 Niagara/Sound 슬롯을 Class Defaults에 노출했다.
- 미할당 슬롯은 정상 No-op, Active는 Cleanup, Escaped는 Character Hidden 전 World one-shot, 최초 복제 Snapshot은 전환음·Burst 억제 계약으로 고정했다.

#### Evidence

```text
HUD Actual Widget Tree       PASS / Saved/Logs/W7-FinalPresentation-HUDTreeSync2.log
2P Presentation TwoRuns      PASS / Saved/Automation/W7-FinalPresentation-2P-PostLobbyFix/index.json
2P Presentation Log          PASS / Saved/Logs/W7-FinalPresentation-2P-PostLobbyFix.log
Full Regression              PASS / 27 Success / WithWarnings 7 / Failed 0 / NotRun 0
Full Regression Report       Saved/Automation/W7-FinalPresentation-FullRegression/index.json
Full Regression Log          Saved/Logs/W7-FinalPresentation-FullRegression.log
Asset Boundary               PASS / 29 moved / Blueprints NonBlueprintAfter=0
Manny Character Setup        PASS / SKM_Manny_Simple / ABP_Unarmed / head socket
Manny Editor Build           PASS / Saved/Logs/W7-Mannequin-AssetBoundary-EditorBuild.log
W7 Manny Regression          PASS / 10 of 10 / Saved/Automation/W7-Mannequin-AssetBoundary-Regression/index.json
2P Camera Socket             PASS / Host·Client SocketResolved=true / FullBodyVisible=true
Status FX Slots              PASS / 1 of 1 / warnings 0 / Saved/Automation/W7-StatusEffectSlots/index.json
W7 Status FX Regression      PASS / 11 of 11 / Failed 0 / Saved/Automation/W7-StatusEffectSlots-FullRegression/index.json
```

#### Status Boundary

- Character/AnimBP 베이스 결정 차단은 UE5 Manny와 `ABP_Unarmed` 연결로 해소했다.
- 상태 VFX·전환 Sound 슬롯과 Lifecycle은 완료했고 미할당 슬롯은 정상 No-op이다.
- 2026-08-22 사용자 결정으로 실제 Niagara/Sound와 Remote Stun/Carry/Heavy Pose를 W7 Gate에서 제외했다.
- 현재 작업 트리의 W7 11/11과 M01 2P TwoRuns 재검증 후 `TASK-W7-004~006 완료`, `W7 Gate=Pass`를 Notion에 반영·재조회했다.

---

### LOCAL-20260816-01 — Legacy Cleanup / Post-Verify Strict Fresh Package

- State: `RECONCILED`
- Created / Reconciled: `2026-08-16 KST`
- Notion Relation: [`TASK-W9-007`](https://app.notion.com/p/39a1d26a5dfb8192940dd5cdee4c1a07)
- Notion Status: `진행중` / 검증 증거 반영 후 Live Re-fetch 확인

#### Applied Work

- 구 경쟁형 Score/Result, Rare Loot Runtime, Forgery Timeout/Inspection Alert shell, 빈 Tag/Customization Component와 obsolete Prototype/Data Row를 제거했다.
- Redirector와 옛 Serialized Path를 Canonical Asset으로 Fix Up하고 Release Map을 재저장했다.
- 패키징 스크립트에 기존 Output 정리와 구분되는 명시적 `-CleanCook` 옵션을 추가했다.
- Development Game에서 `WITH_METADATA=0`인 경우에도 Nameplate Test가 컴파일되도록 Metadata assertion만 조건부로 제한했다.
- W7 Footstep 자동화의 Sprint가 Walk 경로를 역방향으로 되짚도록 해 맵 Geometry 의존 Flake를 제거했다.

#### Evidence

```text
UE 5.8.1 Launcher Verify          PASS / 656 files / 40,274,808 bytes restored
Post-Verify Editor Build          PASS
Post-Verify Full Regression       PASS / 27/27 / Failed 0 / NotRun 0
Nameplate Post-Game Guard         PASS / 1/1
Strict Fresh Development Cook     PASS / UAT -clean / 5 maps
Package Validator                 PASS / Version 0.5.0 / Development
Packaged HeistBuildDump           PASS / Windows / Cooked / STEAM / SessionBuild 55116800
Cooked Legacy Project Paths       0
Cooked StarterContent Footprint   57 chunks / 40.69 MiB
Notion Live Re-fetch              PASS
```

#### Remaining Scope

- M01/M03 원화의 Global Distribution 권리 근거를 보완한다.
- `T_Forgery_SunArchWave` provenance를 기록한다.
- StarterContent 배포 약관 근거와 TENADA OFL/Notice를 Release Package에 포함한다.
- 위 권리·Notice Gate가 남아 있으므로 Notion Task는 완료가 아니라 `진행중`을 유지한다.

---

### LOCAL-20260815-02 — W7 Team Readability Foundation And Integration Gate

- State: `RECONCILED`
- Created / Reconciled: `2026-08-15 KST`
- Notion Relations: `TASK-W7-001`~`TASK-W7-011`
- Test Logs:
  - [`TEST-W7-001`](https://app.notion.com/p/3bc1d26a5dfb81f9a48ac7f4566fa7be)
  - [`TEST-W7-002`](https://app.notion.com/p/3bd1d26a5dfb81549291f537370bfd37)
  - [`TEST-W7-003`](https://app.notion.com/p/3bd1d26a5dfb813c8f05ee11c1ecc46c)
  - [`TEST-W7-004`](https://app.notion.com/p/3bd1d26a5dfb81e5aaabc7f01159e089)
  - [`TEST-W7-005`](https://app.notion.com/p/3bd1d26a5dfb81379889d4e36d76f8ee)
- Notion Status:
  - 완료: `TASK-W7-001`, `002`, `003`, `007`, `008`, `009`, `011`
  - 진행중: `TASK-W7-004`, `005`, `006`
  - 취소/통합: `TASK-W7-010` → `TASK-W8-007`

#### Applied Work

- PlayerState 기반 Crew Status를 Remote Nameplate와 Main HUD가 함께 소비하도록 통합했다.
- 서버 Walk/Sprint/Weight, Pace 기반 Footstep, Guard Stun→Arrest→Rescue 상태를 복제·입력 계약과 연결했다.
- Owner-only Floor Plan Map과 Map Input Mode를 추가하고 Local/Team/Exit/Zone/발견 Target만 표시하도록 제한했다.
- Surface/Object 작업 중 Suspicious/Searching은 유지하고 Alarmed/Lockdown에서 서버가 Session을 선취소한 뒤 Widget과 Gameplay Input을 정리하며, 위험 단계의 신규 재진입과 Case Lock 누수를 막도록 검증했다.
- 실제 `WBP_HeistNameplate_C`와 Team Status의 8상태 색·글리프 및 Remote 3/Local 0을 4P TwoRuns 상태 전이에서 검증하고, 거리 Fade는 별도 수식 계약 테스트로 검증했다.
- M01/M02/M03 Floor Plan Data/Texture, 명시적 Marker whitelist, 4P Map 입력 잠금·복원과 Map-open Stun 강제 종료를 검증했다.
- 렌더·오디오 활성 4P에서 Alert 0~4 한글/색, Alarmed Countdown, click-free edge로 재임포트한 Suspense/Alarm Loop 재생과 Lockdown Cleanup을 검증했다.
- GDD/TDD를 Rev 13으로 맞추고 Suspicious/Searching 유지, Alarmed/Lockdown 서버 선취소·재진입 거부 계약으로 동기화했으며 변경 페이지를 Word PDF/PNG로 검증했다.
- 실제 M01/M02/M03 Surface Template 각 12개, 고정 Seed 결정성, 24회 Cycle 중복 방지, 최근 3개 보호, Random Map Bag, Optional Exhibit 조합을 검증했다.
- Player Count별 경비 수·발각 유예·검사 시간을 실제 Runtime에 적용하고 1P/2P/4P M01 TwoRuns 및 Lobby Reset을 검증했다.
- 2P Client의 실제 `IA_Move` 입력으로 Walk/Sprint Footstep 500/1000cm, 서버 Guard Investigate와 Client 상태 복제, Alert 불변을 검증했다.

#### Evidence

```text
Project_MuseumHeistEditor Build          PASS
ProjectMuseumHeist Full Automation       23/23 / Failed 0
Solo ContractRun Two Runs                PASS
Two Player ContractRun Two Runs          PASS
Four Player ContractRun Two Runs         PASS
2P Walk/Sprint Footstep→Guard Investigate PASS
Stun / Arrest / Rescue                    PASS
Surface / Object Alert Forced Close      PASS
Variation Real Data + Determinism        PASS
Notion Test Logs                         TEST-W7-001/002 Pass
W7 Presentation Test Logs                TEST-W7-003/004/005 Pass
W7 Presentation Full Regression          10/10 / Failed 0 / NotRun 0
GDD/TDD Rev 13 Alert Contract             PASS / changed-page Word render
Notion Live Re-fetch                     PASS
```

#### Remaining Scope

- Stun/Arrest/Carry 최종 화면·오디오·Pose Presentation은 개별 `진행중` Task의 완료 기준으로 남겼다.
- `TASK-W7-004~006`은 단일 2P 렌더·오디오 Presentation Pass와 하나의 Test Log로 공동 검증한다.
- 실제 2~3분 Escape 리듬은 NullRHI 자동화 Pass로 대체하지 않고 `TASK-W8-007`의 3-Map 9판 Gate에 통합했다.
- `TASK-W11-002`는 `TASK-W11-001 External Test / RC1 Gate & Issue Triage`에 통합하고 취소 상태로 이력을 보존했다.
- 최적화 후 W7~W12 활성 잔여는 29개, 필수 잔여는 27개다.

---

### LOCAL-20260815-01 — W6 Contract Run Feature Complete

- State: `RECONCILED`
- Created / Reconciled: `2026-08-15 KST`
- Notion Relations:
  - [`TASK-W6-000`](https://app.notion.com/p/3af1d26a5dfb819199d9e3b1be2eeaf5)
  - [`TASK-W6-009`](https://app.notion.com/p/39a1d26a5dfb81b08540c8824de8dc30)
  - [`TASK-W6-010`](https://app.notion.com/p/39a1d26a5dfb810a8591d5d48ed7dd56)
  - [`TASK-W6-011`](https://app.notion.com/p/39a1d26a5dfb81b0a157d294b89edfc8)
- Test Logs:
  - [`TEST-W6-008`](https://app.notion.com/p/3bc1d26a5dfb810a9cb1f1462b3ee8ca)
  - [`TEST-W6-009`](https://app.notion.com/p/3bc1d26a5dfb816aa446e67ce0b1db5a)
  - [`TEST-W6-010`](https://app.notion.com/p/3bc1d26a5dfb81b281f0de6af59cddf2)
- Notion Status: `TASK-W6-000~011 완료` / `W6 Gate=Pass` / live re-fetch confirmed

#### Applied Work

- M01 Shared Exit의 실제 배치·Nav·Deposit을 검증하고 M02/M03 W8 배치 체크리스트를 확정했다.
- M01 Solo와 4P Contract Run을 각각 두 번 연속 수행해 Lobby Return 및 두 번째 Match clean reset을 검증했다.
- Loose Loot 5종의 정의·월드 비주얼을 구분하고 2P Pickup/Drop/Re-pickup/Deposit 전 과정을 자동화했다.
- TASK-W6-000의 독립 수동 검토를 TEST-W6-008/009/010과 TASK-W6-010 상위 E2E 증거로 대체해 완료 처리했다.
- replicated level actor 파괴로 발생하던 ActorChannelFailure를 Contract 활성 상태 RepNotify로 제거했다.
- seamless transition 중 LocalPlayer 미부착 Controller의 Widget 생성 오류를 차단했다.

#### Evidence

```text
Project_MuseumHeistEditor ForceUnity Build       PASS
TASK-W6-009 M01 Exit/Nav/Deposit                 PASS
TASK-W6-010 Solo Two Runs                        PASS / Errors=0
TASK-W6-010 Four Player Two Runs                 PASS / Errors=0
TASK-W6-011 Loot Definition                      PASS / Errors=0
TASK-W6-011 Loot LifecycleTwoPlayer              PASS / Errors=0
Notion Test Logs                                 TEST-W6-008/009/010 Pass
Notion Task Status                               TASK-W6-000~011 완료
Notion Week Gate                                 W6 Pass
```

#### Scope Boundary

- M02/M03 실제 `.umap` Shared Exit 배치와 최종 3-Map 조명·Lockdown·Original Carrier 동선은 W8 범위다.

---

### LOCAL-20260809-02 — TASK-W6-006 Progress Checkpoint

- State: `RECONCILED`
- Created / Reconciled: `2026-08-09 17:46 KST`
- Notion Relation: [`TASK-W6-006`](https://app.notion.com/p/39a1d26a5dfb8132a835ee75d548e348)
- Test Log: [`TEST-W6-005`](https://app.notion.com/p/3b71d26a5dfb81428a21f134d49cd06a) / `Integration Pass`
- Notion Status: `진행중`

#### Applied Work

- 기존 서버 누적·복제·Result Snapshot 경로를 완료 기준별로 재감사했다.
- Coin Distraction을 단순 반경 내 Guard 존재가 아닌 실제 `ReactToSoundPing` 수락 Guard 수로 누적하도록 수정했다.
- Result Row에 Surface/Assembly 최고 품질, Original 운반 시간과 `탈출/체포/미탈출` 설명을 추가했다.
- Winner/Rank 또는 정렬 기반 경쟁 결과는 추가하지 않았다.

#### Evidence

```text
Project_MuseumHeistEditor Build              PASS / 2026-08-09 17:42 KST
ProjectMuseumHeist.Result Automation         PASS / 6 of 6
git diff --check                             PASS
Notion Progress Note                         PASS / live re-fetch confirmed
2 Player Listen Server PIE                   NOT RUN / REQUIRED
```

#### Remaining Evidence

- Host/Client 각자의 기여 이벤트가 서버에서 해당 PlayerState에만 누적되는지 확인한다.
- `HeistContributionDump`를 Host와 Client에서 실행해 `DataContract=PASS`, `TerminalFlags=PASS`, `WinnerRank=None`과 동일 PlayerCount를 확인한다.
- Result 화면에서 각 Player Row의 최고 품질·운반 시간·기여 항목과 `탈출/체포/미탈출`이 일치하고 Winner/Rank가 없는지 확인한다.
