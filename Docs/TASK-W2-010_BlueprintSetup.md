# TASK-W2-010 블루프린트 작업 지침서

## 작업 범위

표현 전용 Widget Blueprint 두 개를 생성하고 기존 메인 HUD에 배치한다.

C++가 다음 항목을 담당한다.

- 현재 상호작용 대상 탐색
- 상호작용 가능 여부
- 복제된 액션 상태
- 서버 시간
- 남은 시간과 진행률 계산

두 Widget Blueprint의 Event Graph에서 다음 로직을 구현하면 안 된다.

- 게임플레이 상태 변경
- `CanInteract` 재판정
- 캐스팅 완료 판정
- 캐스팅 취소 판정

## 1. WBP_InteractionPrompt 생성

- 에셋 경로: `/Game/Blueprints/UI/HUD/WBP_InteractionPrompt`
- 작업 유형: 신규 Widget Blueprint 생성
- 부모 클래스: `UHeistInteractionPromptWidget`

아래 구조로 위젯을 구성한다. C++ 바인딩 이름은 대소문자까지 정확히
일치해야 하며, 모든 항목에서 **Is Variable**을 활성화한다.

```text
InteractionPromptContainer (Border)
└─ HorizontalBox
   ├─ KeyText (TextBlock)
   ├─ TargetText (TextBlock)
   └─ AvailabilityText (TextBlock)
```

권장 플레이스홀더 값:

- `KeyText`: `E`
- `TargetText`: `TARGET`
- `AvailabilityText`: `AVAILABLE`
- `InteractionPromptContainer` 초기 Visibility: `Collapsed`
- Class Defaults > `Interaction Key Label`: `E`

Event Graph는 비워 둔다. 텍스트와 Visibility는 C++에서 갱신한다.

완료 후 Compile과 Save를 실행한다. 누락된 바인딩 또는 컴파일 경고가
발생하지 않아야 한다.

## 2. WBP_ActionProgress 생성

- 에셋 경로: `/Game/Blueprints/UI/HUD/WBP_ActionProgress`
- 작업 유형: 신규 Widget Blueprint 생성
- 부모 클래스: `UHeistInteractionPromptWidget`

별도의 Action Progress C++ 클래스는 생성하지 않는다. 현재 manifest에서
허용된 `UHeistInteractionPromptWidget`을 재사용하는 구조다.

아래 구조로 위젯을 구성한다. C++ 바인딩 이름은 대소문자까지 정확히
일치해야 하며, 모든 항목에서 **Is Variable**을 활성화한다.

```text
ActionProgressContainer (Border)
└─ VerticalBox
   ├─ ActionTypeText (TextBlock)
   ├─ ActionProgressBar (ProgressBar)
   ├─ ActionRemainingText (TextBlock)
   └─ CancelHintText (TextBlock)
```

권장 플레이스홀더 값:

- `ActionTypeText`: `ACTION`
- `ActionProgressBar.Percent`: `0.0`
- `ActionRemainingText`: `0.0s`
- `CancelHintText`: `MOVE TO CANCEL`
- `ActionProgressContainer` 초기 Visibility: `Collapsed`

Event Graph는 비워 둔다. C++가 액션 종류, 서버 시간 기준 진행률, 남은
시간, 취소 안내와 Visibility를 결정한다. 캐스팅이 진행되는 동안
ProgressBar가 0에서 1까지 채워진다.

완료 후 Compile과 Save를 실행한다. 누락된 바인딩 또는 컴파일 경고가
발생하지 않아야 한다.

## 3. WBP_HeistHUD에 배치

- 에셋 경로: `/Game/Blueprints/UI/HUD/WBP_HeistHUD`
- 작업 유형: 기존 Widget Blueprint 수정
- 기존 부모 클래스: `UHeistHUDWidget`

다음 순서로 작업한다.

1. `WBP_HeistHUD`의 Designer를 연다.
2. `WBP_InteractionPrompt`를 HUD 계층에 추가한다.
3. 인스턴스 이름을 정확히 `InteractionPromptWidget`으로 변경한다.
4. **Is Variable**을 활성화한다.
5. QuickSlot 위쪽 공간을 확보해 화면 하단 중앙에 배치한다.
6. `WBP_ActionProgress`를 HUD 계층에 추가한다.
7. 인스턴스 이름을 정확히 `ActionProgressWidget`으로 변경한다.
8. **Is Variable**을 활성화한다.
9. Interaction Prompt를 가리지 않도록 화면 중앙 또는 하단 중앙에 배치한다.
10. Event Graph에 Setup 호출이나 게임플레이 로직을 추가하지 않는다.
11. `WBP_HeistHUD`를 Compile하고 Save한다.

컴파일 순서:

1. `WBP_InteractionPrompt`
2. `WBP_ActionProgress`
3. `WBP_HeistHUD`

예상 결과:

- 세 에셋이 모두 오류 없이 컴파일된다.
- `Accessed None`, 클래스 누락, 바인딩 타입 불일치가 발생하지 않는다.
- 유효한 대상이 없으면 Interaction Prompt가 숨겨진다.
- 진행 중인 캐스팅이 없으면 Action Progress가 숨겨진다.

## 4. 사용자 PIE 검증

- PIE 모드: Listen Server
- 플레이어 수: 4명

### 상호작용 프롬프트 검증

1. Client 1에서 배치된 사용 가능한 Loot Actor 또는 활성 Vent에 접근한다.
2. 프롬프트가 Client 1 화면에만 나타나는지 확인한다.
3. `E`, 대상 이름, `AVAILABLE`이 표시되는지 확인한다.
4. 상호작용 범위 밖으로 이동해 프롬프트가 사라지는지 확인한다.

예상 로그:

```text
LogHeistUI: [<Client1Character>] Interaction prompt target changed: Previous=None Target=<Actor> Available=true Key=E
LogHeistUI: [<Client1Character>] Interaction prompt target changed: Previous=<Actor> Target=None Available=false Key=E
```

### 함정 설치 진행률 검증

Client 1 콘솔에서 다음 명령을 실행한다.

```text
HeistGlueTrapPlace 100
```

예상 화면:

- Client 1에만 `PLACING TRAP`이 표시된다.
- ProgressBar가 채워지며 남은 시간이 감소한다.
- 설치 완료 후 Action Progress가 숨겨진다.

예상 핵심 로그:

```text
Trap placement cast started: ... Duration=1.50 EndServerTime=<value>
Trap placement cast state replicated: ... IsActive=true EndServerTime=<value>
Trap placed: ...
Trap placement cast state replicated: ... IsActive=false EndServerTime=0.00
```

### 탈출 진행률 검증

1. 탈출 가능 시간이 열릴 때까지 기다린다.
2. 빠른 로컬 확인이 필요하면 `slomo 20`을 입력하고, 탈출 가능 시간이
   열린 후 `slomo 1`로 복구한다.
3. Client 1에서 활성 `BP_Vent`에 접근한다.
4. 설정된 Interact 입력을 한 번 누른다.

예상 화면:

- Client 1에만 `ESCAPING`이 표시된다.
- ProgressBar가 채워지며 남은 시간이 감소한다.
- 완료 또는 취소 시 Action Progress가 숨겨진다.

예상 핵심 로그:

```text
Escape cast started: ... Duration=<value> EndServerTime=<value>
Escape cast state replicated: ... IsActive=true EndServerTime=<value>
Escape cast completed: ...
```

취소 검증:

1. 탈출 캐스팅을 다시 시작한다.
2. 설정된 이동 허용 범위보다 멀리 움직인다.
3. Action Progress가 즉시 숨겨지는지 확인한다.

예상 로그:

```text
Escape cast cancelled: ... Reason=Movement
```

## PASS 기준

- 세 Widget Blueprint가 모두 컴파일된다.
- 프롬프트 진입·이탈 화면과 대상 변경 로그가 일치한다.
- 함정 및 탈출 진행률이 해당 로컬 플레이어에게만 나타난다.
- 액션 이름, 진행률, 남은 시간이 표시된다.
- 완료 또는 취소 후 Action Progress가 숨겨진다.
- `Accessed None`이나 바인딩 타입 불일치가 없다.
- Blueprint에 권한 판정 또는 게임플레이 상태 변경 로직이 없다.

## FAIL 기준

- C++의 캐스팅 시작 로그가 있는데 Action Progress가 나타나지 않는다.
- 소유하지 않은 다른 Client에도 Action Progress가 나타난다.
- ProgressBar가 복제된 서버 종료 시간이 아닌 로컬 임의 시간으로 동작한다.
- 액션 상태가 비활성화된 후에도 위젯이 남아 있다.
- Blueprint가 상호작용 가능 여부, 완료 또는 취소를 판정한다.

이 검증은 개별 `TASK-W2-010` 확인이다. 이 결과만으로 W2 멀티플레이
Gate 또는 Formal Test Log를 통과 처리하지 않는다.
