# Project_MuseumHeist Local Progress Inbox

최종 갱신: 2026-08-09 13:17 KST

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

### LOCAL-20260809-01

- State: `UNLINKED`
- Created: `2026-08-09 KST`
- Last Updated: `2026-08-09 14:00 KST`
- Notion Relation: `NONE`
- Explicit Exclusion: [`TASK-W6-006`](https://app.notion.com/p/39a1d26a5dfb8132a835ee75d548e348) `Player Contribution Capture`와 완료 기준이 다르므로 연결하지 않는다.

#### User Request / Decision

- Forgery Quality 기반 Alert/Lockdown 패널티를 제거한다.
- 서버 최종 Quality 70 이상일 때만 Replica를 승인한다.
- Timeout은 작업을 폐기하고 Alert 변화 없이 근처 Guard 한 명의 1회 조사만 발생시킨다.
- Surface Forgery와 Object Assembly의 설명, 70점 기준, 예상 점수, Timer, Submit/Cancel UI 구조를 통일한다.

#### Changed Scope

- C++ Authority / Validation / Timeout Investigation
- Surface/Object Widget C++ 및 Widget Blueprint
- Surface 서버 `QualityBelowMinimum` 거절 시 동일 Drawing 재제출 차단 및 로컬 수정 후 제출 재활성화
- Artifact DataTable의 `MinimumForgeryScore=0.7`
- `AGENTS.md`, `Museum_Heist_GDD.docx`, `Museum_Heist_TDD.docx` Rev 12
- 상세 파일 목록과 변경 계약: [`CURRENT_PROJECT_STATUS.md`](CURRENT_PROJECT_STATUS.md#3-implemented-artifacts)

#### Verification

```text
C++ Editor Build        PASS
WBP Compile / Save      PASS
DataTable Save          PASS
Document Render QA      PASS
git diff --check        PASS
Automation              NOT RUN
User PIE                NOT RUN / REQUIRED
Multiplayer Runtime     NOT RUN / REQUIRED
Notion Write            NOT DONE
```

#### Remaining Evidence

- `ProjectMuseumHeist.Forgery.ReplicaAcceptanceContract` 자동화 실행
- 2 Player Listen Server PIE에서 Owner-only UI, 70점 Gate, Timeout 1회 조사, Alert 불변 확인

#### Next Reconciliation Action

1. 자동화와 User PIE 증거를 수집한다.
2. 이 Work Package와 일치하는 Notion Task가 있는지 라이브 검색한다.
3. 일치하는 Task가 없으면 사용자가 신규 Task 생성 또는 기존 Task Relation을 결정한다.
4. 사용자가 Notion 반영을 요청하고 쓰기 성공을 재조회한 뒤 `RECONCILED`로 이동한다.

---

## Reconciled Archive

현재 없음.
