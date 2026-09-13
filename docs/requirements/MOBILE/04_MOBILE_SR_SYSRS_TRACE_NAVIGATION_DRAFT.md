# MOBILE SR → SysRS Traceability — Navigation Draft

> 목적: **SR 문장 하나가 어느 SysRS 요구사항으로 이어지는지**를 대조하여 누락을 검출한다.
> `TR-SR-xxx`는 추적 문서 내부의 탐색용 Reference이며 **SR 원문에 부여하는 공식 요구사항 ID가 아니다.**
> SR 문구는 `01_MOBILE_SR_FUNCTIONAL_BASELINE_DRAFT` 의 표현을 그대로 사용한다.
> SysRS ID는 `02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT` 를 가리킨다.

---

## 1. 추적 요약

| 항목 | 수 |
|---|---:|
| 추적 Reference | **44** |
| `FUNCTIONAL` | 30 |
| `NON_FUNCTIONAL` | 4 |
| `BOUNDARY` | 9 |
| `DEFERRED` | 1 |

| Trace 상태 | 수 | 의미 |
|---|---:|---|
| `COVERED` | **44** | 대응하는 SysRS 요구사항이 존재한다 |
| `PARTIAL` | 0 | 일부만 대응한다 |
| `NOT COVERED` | 0 | 대응하는 SysRS 요구사항이 없다 |

> 최초 작성 시 `NOT COVERED` 1건(`TR-SR-044`), `PARTIAL` 2건(`TR-SR-027`, `TR-SR-043`)이 검출되어
> `MB-SYS-REQ-012`, `MB-SYS-SAF-006`, `MB-SYS-SAF-007` 을 신설하였다.

> 제어 대상 목록(SR 3.1절)과 표시 대상 목록(SR 4.1절)은 개별 Ref 를 부여하지 않고
> `TR-SR-007` · `TR-SR-013` 에서 묶어 추적한다. 항목 자체는 요구사항이 아니라 대상의 열거이기 때문이다.

---

## 2. SR → SysRS 대조

### 2. 기능 범위

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-001 | `BOUNDARY` | 차량 기능을 직접 수행하지 않으며, 실제 수행 여부는 차량이 결정한다 | [SAF-004](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-saf-004) · [SEM-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-003) | `COVERED` |
| TR-SR-002 | `BOUNDARY` | 요청의 허용 여부와 실제 수행은 차량이 결정하며, 그 결과를 받아 표시한다 | [SEM-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-003) · [REQ-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-003) | `COVERED` |
| TR-SR-003 | `BOUNDARY` | 차량 상태를 자체적으로 추정하거나 보정하지 않는다 | [SEM-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-001) · [DSP-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-001) | `COVERED` |

### 3. 사용자 제어 요청

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-004 | `FUNCTIONAL` | 허용된 차량 기능에 대한 제어 요청을 생성할 수 있어야 한다 | [REQ-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-001) · [INT-014](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-014) | `COVERED` |
| TR-SR-005 | `FUNCTIONAL` | 접수·진행·완료·거부·중단·실패 결과가 제공되어야 한다 | [REQ-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-006) · [SEM-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-003) · 02 문서 3절 요청 상태 | `COVERED` |
| TR-SR-006 | `FUNCTIONAL` | 거부되거나 실패한 경우 그 사유가 제공되어야 한다 | [REQ-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-006) · [SEM-004](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-004) | `COVERED` |
| TR-SR-007 | `FUNCTIONAL` | 전송 완료 상태와 차량에서의 수행 완료 상태는 구분되어 제공되어야 한다 | [REQ-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-003) · [SAF-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-saf-003) | `COVERED` |
| TR-SR-008 | `FUNCTIONAL` | 최근 제어 요청의 수행 결과를 확인할 수 있어야 한다 | [REQ-010](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-010) | `COVERED` |
| TR-SR-009 | `FUNCTIONAL` | 연결이 종료된 경우 이전 요청이 자동으로 다시 전달되어서는 안 된다 | [REQ-007](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-007) · [REQ-011](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-011) | `COVERED` |
| TR-SR-010 | `FUNCTIONAL` | 3.1절의 제어 대상(도어·목표 온도·공기 순환·선행 공조·조명)에 대해 요청을 생성한다 | [REQ-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-001) · [INT-014](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-014) · [INT-017](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-017) | `COVERED` |

### 4.1 표시 대상

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-011 | `FUNCTIONAL` | 도어 — 잠금 여부·개폐 여부·상태 이상·오류 | [DSP-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-006) · [DSP-007](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-007) · [INT-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-001) · [INT-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-002) | `COVERED` |
| TR-SR-012 | `FUNCTIONAL` | 실내 환경 — 자동 공조·현재/목표 온도·요구/실제 순환 세기·냉난방 방향·선행 공조·오류 | [DSP-008](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-008) · [INT-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-003) · [INT-004](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-004) · [INT-005](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-005) · [INT-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-006) | `COVERED` |
| TR-SR-013 | `FUNCTIONAL` | 실내 조명 — 사용 여부·현재 밝기·현재 켜져 있는 알림 종류·오류 | [INT-007](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-007) | `COVERED` |
| TR-SR-014 | `FUNCTIONAL` | 기타 차량 기능 — 창문·실내 환경 정보와 탑승자 유무·음향 오류 | [INT-018](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-018) · [INT-019](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-019) · [INT-020](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-020) | `COVERED` |

### 4.2 정보 신뢰성 표시 원칙

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-015 | `FUNCTIONAL` | 상태 정보가 최신이 아닌 경우 최신 상태가 아님이 표시되어야 한다 | [DSP-004](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-004) · [SEM-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-002) · [SEM-007](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-007) | `COVERED` |
| TR-SR-016 | `FUNCTIONAL` | 신뢰할 수 없는 경우 마지막 정상 값이 현재 정상 상태로 표시되어서는 안 된다 | [DSP-005](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-005) · [CON-005](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-con-005) · [SAF-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-saf-002) | `COVERED` |
| TR-SR-017 | `FUNCTIONAL` | 상태 확인 실패·정보 미전달·동작 실패 오류를 구분할 수 있도록 표시되어야 한다 | [DIAG-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-diag-001) · [SEM-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-006) · [INT-012](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-012) | `COVERED` |
| TR-SR-018 | `FUNCTIONAL` | 확인할 수 없는 경우 확인 불가임이 표시되고 임의의 값으로 대체되어서는 안 된다 | [DSP-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-003) · [NFR-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-nfr-002) | `COVERED` |
| TR-SR-019 | `FUNCTIONAL` | 설정이 안전 정책에 의해 적용되지 않는 경우 그 사유가 제공되어야 한다 | [DSP-009](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-009) | `COVERED` |

### 5. 경고 및 알림

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-020 | `FUNCTIONAL` | 안전 보호 동작 또는 위험 경고는 일반 상태 정보보다 우선하여 식별되어야 한다 | [ALT-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-alt-001) · [SAF-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-saf-001) | `COVERED` |
| TR-SR-021 | `FUNCTIONAL` | 안전 경고 및 중요 상태 변화는 일반 상태 정보와 구분되어 제공되어야 한다 | [ALT-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-alt-002) · [SEM-005](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-005) | `COVERED` |
| TR-SR-022 | `FUNCTIONAL` | 확인하지 않은 중요 경고가 있는 경우 해당 상태를 확인할 수 있어야 한다 | [ALT-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-alt-003) · [ALT-007](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-alt-007) | `COVERED` |
| TR-SR-023 | `FUNCTIONAL` | 사용자 이탈 상태에서 도어가 열린 채 유지되는 경우 해당 상태가 제공되어야 한다 | [INT-011](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-011) · [ALT-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-alt-001) | `COVERED` |
| TR-SR-024 | `FUNCTIONAL` | 엔진룸에 동물이 들어온 경우 권한이 있는 사용자에게 즉시 제공되어야 한다 | [INT-013](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-013) · [PERF-004](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-perf-004) · [ALT-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-alt-006) | `COVERED` |
| TR-SR-025 | `FUNCTIONAL` | 확인한 이후에도 상태가 유효한 동안에는 표시가 유지되어야 한다 | [ALT-004](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-alt-004) · [ALT-005](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-alt-005) · [SAF-005](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-saf-005) | `COVERED` |

### 6. 인증 및 통신 안전

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-026 | `FUNCTIONAL` | 인증 및 권한 확인이 완료된 사용자에게만 원격 제어가 제공되어야 한다 | [SEC-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sec-003) · [REQ-009](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-009) | `COVERED` |
| TR-SR-027 | `FUNCTIONAL` | 차량에서 온 정보임을 확인할 수 없는 경우 새 제어 요청이 생성되어서는 안 된다 | [REQ-012](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-012) | `COVERED` |
| TR-SR-028 | `FUNCTIONAL` | 같은 요청이 두 번 또는 늦게 전달되더라도 차량에서 반복 수행되지 않아야 한다 | [REQ-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-002) · [INT-015](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-015) | `COVERED` |
| TR-SR-029 | `FUNCTIONAL` | 인증 정보는 사용자가 직접 열람하거나 입력하지 않아야 한다 | [SEC-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sec-001) · [SEC-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sec-002) | `COVERED` |

### 7. 연결 상태

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-030 | `FUNCTIONAL` | 현재 차량과의 연결 가능 상태를 확인할 수 있어야 한다 | [CON-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-con-001) · [DSP-010](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-010) | `COVERED` |
| TR-SR-031 | `FUNCTIONAL` | 연결 불가 상태에서는 요청이 생성되지 않아야 하며 그 사유가 제공되어야 한다 | [REQ-009](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-009) | `COVERED` |
| TR-SR-032 | `FUNCTIONAL` | 연결이 복구된 경우 차량이 확정한 최신 상태로 표시가 갱신되어야 한다 | [CON-004](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-con-004) · [CON-005](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-con-005) | `COVERED` |
| TR-SR-033 | `FUNCTIONAL` | 연결 복구만을 근거로 이전 요청이 재수행되어서는 안 된다 | [REQ-008](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-008) | `COVERED` |

### 8. 동작 품질 및 비기능

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-034 | `NON_FUNCTIONAL` | 서로 다른 의미를 가진 상태와 경고는 사용자가 구분할 수 있어야 한다 | [ALT-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-alt-002) · [DSP-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-002) | `COVERED` |
| TR-SR-035 | `NON_FUNCTIONAL` | 제어 요청 결과는 조작 실패로 오인하지 않을 시간 안에 제공되어야 한다 | [PERF-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-perf-002) · [PERF-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-perf-003) | `COVERED` |
| TR-SR-036 | `NON_FUNCTIONAL` | 동일한 요청은 정상 동작 상태에서 일관된 결과를 제공해야 한다 | [NFR-004](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-nfr-004) · [NFR-005](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-nfr-005) | `COVERED` |
| TR-SR-037 | `NON_FUNCTIONAL` | 현재 차량 상태와 과거 정보를 혼동하지 않도록 제공되어야 한다 | [DSP-004](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-004) · [DSP-010](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-dsp-010) · [SEM-008](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-008) | `COVERED` |
| TR-SR-038 | `DEFERRED` | 실제 표시 형식과 평가 조건은 적용 단말 및 시험 환경이 확정된 후 정의되어야 한다 | 02 문서 13절 · 14.3절 | `COVERED` |

### 9. 상위 시스템 연계 원칙

| Ref | 분류 | SR 원문 | 대응 SysRS | 상태 |
|---|---|---|---|---|
| TR-SR-039 | `BOUNDARY` | 각 기능이 확정한 상태가 유효성과 함께 제공되어야 한다 | [SEM-001](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-001) · [INT-008](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-008) | `COVERED` |
| TR-SR-040 | `BOUNDARY` | 처리 결과와 거부 또는 실패 사유가 제공되어야 한다 | [SEM-004](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-004) · [INT-010](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-010) | `COVERED` |
| TR-SR-041 | `BOUNDARY` | 안전 경고가 일반 상태와 구분 가능한 형태로 제공되어야 한다 | [SEM-005](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-005) · [INT-011](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-011) | `COVERED` |
| TR-SR-042 | `BOUNDARY` | 상태 정보의 최신 여부를 판단할 수 있는 근거가 함께 제공되어야 한다 | [SEM-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-002) · [INT-009](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-009) | `COVERED` |
| TR-SR-043 | `BOUNDARY` | 모바일 인터페이스의 요청은 차량 기능의 허용 조건을 우회하지 않아야 한다 | [SAF-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-saf-006) | `COVERED` |
| TR-SR-044 | `BOUNDARY` | 사용자 인증의 판단은 차량 측에서 수행되어야 한다 | [SEC-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sec-006) | `COVERED` |

---

## 3. 역방향 확인 — SR에서 직접 파생되지 않은 SysRS

| SysRS | 신설 이유 |
|---|---|
| [SEM-007](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-007) · [SEM-008](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sem-008) | 무선 구간 신선도 평가 — 차량이 `OK` 로 보낸 값도 무선 지연 시 최신이 아니므로 필요 |
| [CON-002](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-con-002) · [CON-003](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-con-003) | 재연결 간격과 지수 백오프 — TR-SR-032 를 배터리 소모 없이 성립시키기 위해 필요 |
| [CON-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-con-006) · [ALT-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-alt-006) | 백그라운드 연결 및 경고 인지 — TR-SR-024 의 "즉시" 를 앱이 화면에 없을 때도 성립시키기 위해 필요 |
| [SEC-005](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sec-005) | 차량 등록 해제 시 인증 정보 삭제 — 단말 분실·양도 대응 |
| [REQ-011](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-011) | `UNKNOWN` 요청의 자동 재전송 금지 — 6.3절 산문에만 있던 규칙을 요구사항으로 승격 |
| [REQ-012](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-req-012) | 신뢰할 수 없는 정보 수신 시 요청 생성 금지 — TR-SR-027 이 SysRS 에 없어 신설 |
| [SAF-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-saf-006) · [SEC-006](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-sec-006) | 허용 판정 대체 금지 · 인증 판정 대체 금지 — TR-SR-043 · TR-SR-044 가 SysRS 에 없어 신설 |
| [INT-018](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-018) ~ [INT-020](./02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md#mb-sys-int-020) | 창문·실내 환경·음향 정보 수신 — TR-SR-014 가 SysRS 에 없어 신설 |

---

## 4. 유지 규칙

- SR 문장을 추가·수정·삭제하면 본 문서의 해당 Ref 를 함께 갱신한다.
- `NOT COVERED` 또는 `PARTIAL` 이 남은 상태로 SR 을 확정하지 않는다.
- SysRS ID 를 변경하면 본 문서의 링크를 함께 갱신한다.
- 대응 SysRS 가 없는 SR 은 **요구사항 누락**이거나 **SR 이 과도하게 추상적**인 것이므로 둘 중 하나를 고친다.
