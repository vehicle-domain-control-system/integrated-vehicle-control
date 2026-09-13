# BCM SR → SysRS Traceability — Navigation Draft

> 목적: **SR 문장 하나가 어느 SysRS 요구사항으로 이어지는지**를 대조하여 누락을 검출한다.
> `TR-SR-xxx`는 추적 문서 내부의 탐색용 Reference이며 **SR 원문에 부여하는 공식 요구사항 ID가 아니다.**
> SR 문구는 `01_BCM_SR_FUNCTIONAL_BASELINE_DRAFT` 의 표현을 그대로 사용한다.
> SysRS ID는 `02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT` 를 가리킨다.

---

## 1. 추적 요약

| 항목 | 수 |
|---|---:|
| 추적 Reference | **57** |
| `FUNCTIONAL` | 43 |
| `NON_FUNCTIONAL` | 3 |
| `BOUNDARY` | 10 |
| `DEFERRED` | 1 |

| Trace 상태 | 수 | 의미 |
|---|---:|---|
| `COVERED` | **57** | 대응하는 SysRS 요구사항이 존재한다 |
| `PARTIAL` | 0 | 일부만 대응한다 |
| `NOT COVERED` | 0 | 대응하는 SysRS 요구사항이 없다 |

> 최초 작성 시 `NOT COVERED` 2건(`TR-SR-003`, `TR-SR-056`), `PARTIAL` 1건(`TR-SR-057`)이 검출되어
> `BCM-SYS-SEM-005`, `BCM-SYS-SEM-006` 를 신설하고 `BCM-SYS-INT-015` 를 보완하였다.

---

## 2. SR → SysRS 대조

### 2. 기능 범위

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-001 | `BOUNDARY` | 각 기능은 서로 독립적으로 동작하며, 한 기능의 오류가 다른 기능의 수행을 중단시키지 않아야 한다 | [SAF-007](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-007) · [NFR-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-nfr-001) | `COVERED` |
| TR-SR-002 | `BOUNDARY` | 측정값은 BCM에 직접 제공되지 않으며, BCM은 확정된 동작을 수행한다 | [SEM-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-sem-001) | `COVERED` |
| TR-SR-003 | `BOUNDARY` | BCM은 다른 기능 사이의 정보를 중계하지 않는다 | [SEM-005](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-sem-005) | `COVERED` |

### 3.1 명령 실행

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-004 | `FUNCTIONAL` | 허용된 도어 잠금 요청이 확인된 경우 도어를 잠가야 한다 | [DL-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-001) · [CMD-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cmd-001) | `COVERED` |
| TR-SR-005 | `FUNCTIONAL` | 허용된 도어 잠금 해제 요청이 확인된 경우 잠금을 해제해야 한다 | [DL-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-001) · [CMD-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cmd-001) | `COVERED` |
| TR-SR-006 | `FUNCTIONAL` | 허용되지 않은 요청에 의해 도어 잠금 상태가 변경되지 않도록 해야 한다 | [CMD-002](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cmd-002) · [CMD-004](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cmd-004) · [DL-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-001) | `COVERED` |
| TR-SR-007 | `FUNCTIONAL` | 동일한 목표의 요청이 반복되더라도 반복 동작이 발생하지 않도록 해야 한다 | [DL-002](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-002) · [CMD-003](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cmd-003) | `COVERED` |
| TR-SR-008 | `FUNCTIONAL` | 도어 잠금 동작이 정해진 시간을 넘겨 계속되지 않도록 해야 한다 | [SAF-002](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-002) | `COVERED` |

### 3.2 도어 상태 관리

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-009 | `FUNCTIONAL` | 도어가 잠겨 있는지와 열려 있는지를 서로 구분하여 관리해야 한다 | [DL-003](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-003) | `COVERED` |
| TR-SR-010 | `FUNCTIONAL` | 잠금·개폐 상태를 상위 차량 시스템에서 확인할 수 있도록 해야 한다 | [DL-009](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-009) · [INT-008](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-008) | `COVERED` |
| TR-SR-011 | `FUNCTIONAL` | 도어 상태를 믿을 수 없는 경우 정상 상태로 단정하여 제공하지 않아야 한다 | [DL-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-006) · [DIAG-002](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-diag-002) | `COVERED` |
| TR-SR-012 | `FUNCTIONAL` | 실제로 있을 수 없는 상태가 확인된 경우 정상으로 취급하지 않아야 한다 | [DL-005](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-005) | `COVERED` |
| TR-SR-013 | `FUNCTIONAL` | 있을 수 없는 상태를 그럴듯한 정상 상태로 바꾸어 제공하지 않아야 한다 | [DL-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-006) | `COVERED` |

### 3.3 동작 결과 확인

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-014 | `FUNCTIONAL` | 실제로 잠기거나 열린 것이 확인된 경우에만 정상 완료로 처리해야 한다 | [DL-007](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-007) | `COVERED` |
| TR-SR-015 | `FUNCTIONAL` | 확인되지 않은 경우 그 결과와 이유를 제공해야 한다 | [DL-008](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-008) · [INT-010](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-010) | `COVERED` |

### 4.1 실내 공기 순환

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-016 | `FUNCTIONAL` | 확정된 세기로 실내 공기를 순환시켜야 한다 | [CL-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cl-001) | `COVERED` |
| TR-SR-017 | `FUNCTIONAL` | 실내 공기가 실제로 순환되고 있는지 확인할 수 있어야 한다 | [CL-002](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cl-002) | `COVERED` |
| TR-SR-018 | `FUNCTIONAL` | 공기 순환 여부를 확인할 수 없는 경우 정상 동작으로 처리하지 않아야 한다 | [CL-004](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cl-004) | `COVERED` |
| TR-SR-019 | `FUNCTIONAL` | 확정된 세기와 실제 순환 상태가 일치하지 않는 경우 오류 상태로 처리해야 한다 | [CL-003](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cl-003) | `COVERED` |

### 4.2 실내 온도 조절

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-020 | `FUNCTIONAL` | 확정된 방향에 따라 실내 공기를 식히거나 데워야 한다 | [CL-005](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cl-005) | `COVERED` |
| TR-SR-021 | `FUNCTIONAL` | 확정된 세기로 냉각 또는 가열을 수행해야 한다 | [CL-005](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cl-005) · [CMD-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cmd-006) | `COVERED` |
| TR-SR-022 | `FUNCTIONAL` | 발생한 열이 배출되고 있는지 확인할 수 있어야 한다 | [CL-007](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cl-007) | `COVERED` |
| TR-SR-023 | `FUNCTIONAL` | 열이 배출되지 않거나 과열이 확인된 경우 냉각 또는 가열을 중단해야 한다 | [SAF-004](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-004) · [SAF-005](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-005) · [SAF-010](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-010) | `COVERED` |
| TR-SR-024 | `FUNCTIONAL` | 냉각과 가열이 전환될 때 장치에 손상이 발생하지 않도록 해야 한다 | [CL-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cl-006) | `COVERED` |
| TR-SR-025 | `FUNCTIONAL` | 현재 냉각·가열 여부와 세기를 상위 차량 시스템에서 확인할 수 있도록 해야 한다 | [CL-008](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cl-008) · [INT-012](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-012) | `COVERED` |

### 5. 실내 조명

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-026 | `FUNCTIONAL` | 확정된 알림 종류에 따라 실내 조명을 켜야 한다 | [AL-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-al-001) · [AL-002](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-al-002) | `COVERED` |
| TR-SR-027 | `FUNCTIONAL` | 확정되지 않은 조명 요청을 정상적인 알림으로 실행하지 않아야 한다 | [AL-003](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-al-003) | `COVERED` |
| TR-SR-028 | `FUNCTIONAL` | 현재 켜져 있는 알림 종류와 밝기를 상위 차량 시스템에서 확인할 수 있도록 해야 한다 | [AL-004](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-al-004) · [INT-013](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-013) | `COVERED` |
| TR-SR-029 | `FUNCTIONAL` | 조명이 확정된 대로 켜지지 않은 경우 정상 동작으로 처리하지 않아야 한다 | [AL-005](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-al-005) | `COVERED` |
| TR-SR-030 | `FUNCTIONAL` | 조명 관련 오류를 조명 이외의 방법으로 알려야 한다 | [AL-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-al-006) | `COVERED` |

### 6. 자체 안전

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-031 | `BOUNDARY` | 본 절의 판단은 직접 측정하는 정보에 근거하며, 연결이 끊긴 상태에서도 수행되어야 한다 | [SAF-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-001) | `COVERED` |
| TR-SR-032 | `FUNCTIONAL` | 도어가 닫혀 있는 것이 확인되지 않은 경우 도어를 잠그지 않아야 한다 | [SAF-003](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-003) | `COVERED` |
| TR-SR-033 | `FUNCTIONAL` | 열 배출을 확인할 수 없거나 과열이 확인된 경우 냉각 또는 가열을 중단해야 한다 | [SAF-004](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-004) · [SAF-005](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-005) | `COVERED` |
| TR-SR-034 | `FUNCTIONAL` | 정해진 시간을 넘긴 동작은 소프트웨어가 멈춘 상태에서도 멈출 수 있어야 한다 | [SAF-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-001) · [SAF-002](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-002) | `COVERED` |
| TR-SR-035 | `FUNCTIONAL` | 전원이 켜질 때 모든 동작을 멈춘 상태에서 시작해야 한다 | [SAF-008](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-008) | `COVERED` |
| TR-SR-036 | `FUNCTIONAL` | 전원이 켜질 때 이전에 하던 동작을 다시 시작하지 않아야 한다 | [SAF-009](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-009) | `COVERED` |
| TR-SR-037 | `FUNCTIONAL` | 안전을 위해 요청을 수행하지 않은 경우 그 이유를 제공해야 한다 | [INT-015](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-015) | `COVERED` |
| TR-SR-038 | `BOUNDARY` | 자체 안전 판단은 전달받은 동작을 수행하지 않을 수 있다 | [SAF-003](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-003) · [INT-015](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-015) | `COVERED` |

### 7. 상태 및 오류

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-039 | `FUNCTIONAL` | 각 기능의 동작 상태를 서로 구분하여 확인할 수 있도록 해야 한다 | [INT-016](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-016) · [NFR-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-nfr-001) | `COVERED` |
| TR-SR-040 | `FUNCTIONAL` | 상태 확인 실패·정보 미전달·동작 실패로 인한 오류를 서로 구분하여 제공해야 한다 | [DIAG-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-diag-001) · [INT-014](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-014) | `COVERED` |
| TR-SR-041 | `FUNCTIONAL` | 신뢰할 수 없는 상태 정보를 현재 정상 상태로 제공하지 않아야 한다 | [DIAG-002](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-diag-002) | `COVERED` |
| TR-SR-042 | `FUNCTIONAL` | 전달받은 동작 내용을 믿을 수 없는 경우 그 동작을 수행하지 않아야 한다 | [DIAG-003](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-diag-003) · [CMD-004](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cmd-004) | `COVERED` |
| TR-SR-043 | `FUNCTIONAL` | 믿을 수 없다는 이유만으로 이미 하고 있던 동작을 멈추지 않아야 한다 | [DIAG-004](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-diag-004) | `COVERED` |
| TR-SR-044 | `FUNCTIONAL` | 통신 복구 후 마지막으로 받아 둔 내용만 보고 새 동작을 시작하지 않아야 한다 | [DIAG-005](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-diag-005) | `COVERED` |
| TR-SR-045 | `FUNCTIONAL` | 오류의 복구 조건이 충족되기 전에는 동작을 다시 시작하지 않아야 한다 | [DIAG-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-diag-006) · [SAF-010](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-010) | `COVERED` |
| TR-SR-046 | `FUNCTIONAL` | 복구 조건이 충족된 경우 새로 전달받은 내용에 의해서만 다시 시작해야 한다 | [DIAG-007](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-diag-007) | `COVERED` |
| TR-SR-047 | `FUNCTIONAL` | 하나의 기능에서 발생한 오류를 이유로 다른 기능을 중단시키지 않아야 한다 | [SAF-007](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-007) | `COVERED` |
| TR-SR-048 | `FUNCTIONAL` | 동작 중 오류가 발생한 경우 확정되지 않은 동작이 계속되지 않도록 해야 한다 | [SAF-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-006) · [SAF-011](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-011) | `COVERED` |

### 8. 동작 품질 및 비기능

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-049 | `NON_FUNCTIONAL` | 사용자 요청에 대한 동작은 조작 실패로 오인하지 않을 시간 안에 시작되어야 한다 | [PERF-002](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-perf-002) | `COVERED` |
| TR-SR-050 | `NON_FUNCTIONAL` | 조명 알림의 전환은 지연으로 인지하지 않을 시간 안에 수행되어야 한다 | [PERF-004](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-perf-004) | `COVERED` |
| TR-SR-051 | `NON_FUNCTIONAL` | 동일한 요청은 정상 동작 상태에서 일관된 결과를 제공해야 한다 | [NFR-004](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-nfr-004) · [NFR-005](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-nfr-005) | `COVERED` |
| TR-SR-052 | `DEFERRED` | 실제 동작 값과 평가 조건은 적용 장치 및 시험 환경이 확정된 후 정의되어야 한다 | 02 문서 13절 · 14.3절 | `COVERED` |

### 9. 상위 시스템 연계 원칙

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-053 | `BOUNDARY` | 이미 결정된 동작이 제공되어야 하며, 그 결정에 사용된 정보가 함께 제공될 필요는 없다 | [SEM-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-sem-001) | `COVERED` |
| TR-SR-054 | `BOUNDARY` | 동작 수준과 방향은 BCM이 해석 가능한 형태로 제공되어야 한다 | [INT-001](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-001) · [INT-002](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-002) · [INT-003](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-003) · [INT-004](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-004) | `COVERED` |
| TR-SR-055 | `BOUNDARY` | BCM이 제공하는 상태·동작 결과·오류는 상위 차량 시스템에서 활용 가능해야 한다 | [INT-008](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-008) ~ [INT-017](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-017) | `COVERED` |
| TR-SR-056 | `BOUNDARY` | BCM의 세부 동작 수행 방법은 상위 차량 기능이 직접 제어하지 않아야 한다 | [SEM-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-sem-006) · [NFR-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-nfr-006) | `COVERED` |
| TR-SR-057 | `BOUNDARY` | 자체 안전 판단에 의한 명령 거부는 정상 응답으로 처리되어야 한다 | [INT-015](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-int-015) | `COVERED` |

---

## 3. 역방향 확인 — SR에서 파생되지 않은 SysRS

SR에 직접 대응하지 않고 SR을 만족시키기 위해 신설한 요구사항이다.
02 문서에서 `*(파생)*` 로 표기한다.

| SysRS | 신설 이유 |
|---|---|
| [CMD-003](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cmd-003) | 중복·역순 명령의 재실행 방지 — TR-SR-007 을 통신 환경에서 성립시키기 위해 필요 |
| [CMD-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-cmd-006) | 수준·방향을 구동량으로 변환 — 실행 노드의 기본 동작 |
| [DL-004](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-dl-004) | 접점 바운스 제거 — TR-SR-009 의 상태 확정에 필요 |
| [AL-003](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-al-003) | 미정의 알림 종류 수신 시 직전 출력 유지 — TR-SR-027 의 구체화 |
| [SEM-002](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-sem-002) | 중복·순서 식별값 — CMD-003 의 전제 |
| [SEM-005](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-sem-005) | 중계 금지 — TR-SR-003 이 SysRS 에 없어 신설 |
| [SEM-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-sem-006) | 내부 구동 파라미터 직접 지정 금지 — TR-SR-056 이 SysRS 에 없어 신설 |
| [PERF-006](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-perf-006) | 연속 미수신의 `COMM` 오류 판정 기준 — 오류 분류의 발생 조건이 없어 신설 |
| [SAF-010](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-010) | 과열 복귀 임계값 적용 — 재개 조건이 요구사항으로 없어 신설 |
| [SAF-011](./02_BCM_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#bcm-sys-saf-011) | Fan 오류 시 온도 장치 정지 — 방열 상실이 과열로 이어지는 경로가 비어 있어 신설 |

---

## 4. 유지 규칙

- SR 문장을 추가·수정·삭제하면 본 문서의 해당 Ref 를 함께 갱신한다.
- `NOT COVERED` 또는 `PARTIAL` 이 남은 상태로 SR 을 확정하지 않는다.
- SysRS ID 를 변경하면 본 문서의 링크를 함께 갱신한다.
- 대응 SysRS 가 없는 SR 은 **요구사항 누락**이거나 **SR 이 과도하게 추상적**인 것이므로 둘 중 하나를 고친다.
