# MOBILE Logical Interface Needs — Pre-Network Draft

> 목적: 통신 설계를 선행하지 않고, MOBILE SysRS에서 필요한 **정보 교환 항목만 보존**한다.
> 이 문서는 Protocol Specification 또는 API 명세가 아니다.
> `Source Owner` 열은 **해당 정보를 확정하여 제공하는 노드**를 가리킨다.

---

## 1. Logical Interface Overview

```
사용자 ──▶ MOBILE ──[ 제어 요청 ]──▶ 게이트웨이 ──▶ 중앙 판단 기능 ──▶ 각 노드
                  ◀─[ 상태 · 결과 · 경고 ]──                        ◀── 각 노드
```

현재 단계에서는 정보의 **의미와 방향, 그리고 출처 노드**만 정의한다.
무선 방식, 데이터 형식, 메시지 구조 및 주기는 전체 기능 취합 후 결정한다.

게이트웨이의 구성과 담당은 본 문서의 범위가 아니며, MOBILE은 게이트웨이 너머의
개별 노드와 직접 통신하는 구조를 전제하지 않는다.

---

## 2. MOBILE → External

| Information Need | Purpose | Consumer | Protocol |
|---|---|---|---|
| 제어 요청의 대상 기능과 목표 값 | 차량 기능 실행 요청 | 중앙 판단 기능 | Deferred |
| 제어 요청 식별자 | 중복·순서 판별 | 중앙 판단 기능 | Deferred |
| 인증 절차 응답 | 사용자 인증 | 중앙 판단 기능 | Deferred |
| 탑승 예정 시각 | 선행 공조 기동 시점 산출 | 중앙 판단 기능 | Deferred |

MOBILE은 요청을 생성할 뿐이며 **허용 여부와 실제 수행은 차량이 결정한다.**
MOBILE이 자체 판정으로 차량의 판정을 대체하지 않는다.

---

## 3. External → MOBILE

### 3.1 실행 상태

| Information Need | Purpose | Source Owner | Protocol |
|---|---|---|---|
| 도어 잠금 상태 · 물리 개폐 상태 | 상태 표시 | BCM | Deferred |
| 도어 종합 상태 비정상 여부 | 이상 상태 표시 | BCM | Deferred |
| Fan 요구 수준 · 실제 동작 수준 | 지시 대비 실제 표시 | BCM | Deferred |
| 온도 장치 동작 방향 · 동작 상태 | 냉방·난방 표시 | BCM | Deferred |
| 현재 조명 알림 종류 · 밝기 수준 | 조명 상태 표시 | BCM | Deferred |
| 조명 사용 설정 상태 | 설정 표시 | BCM | Deferred |

### 3.2 센싱 및 환경

| Information Need | Purpose | Source Owner | Protocol |
|---|---|---|---|
| 현재 실내 온도 | 현재/목표 온도 표시 | CIS | Deferred |
| 차량이 확정한 실내 환경 정보 | 실내 상태 표시 | CIS | Deferred |
| 탑승자 유무 | 실내 상태 표시 | CIS | Deferred |

### 3.3 타 기능 상태

| Information Need | Purpose | Source Owner | Protocol |
|---|---|---|---|
| 창문 동작 상태 · 요청 처리 결과 | 상태 표시 | **TBD — 담당 노드 미정** | Deferred |
| 음향 기능의 오류 | 오류 표시 | VSS | Deferred |

### 3.4 판단 결과 및 공통 정보

| Information Need | Purpose | Source Owner | Protocol |
|---|---|---|---|
| 설정된 목표 온도 · 자동 공조 사용 여부 | 설정 표시 | 중앙 판단 기능 | Deferred |
| 선행 공조 수행 상태 · 설정된 탑승 예정 시각 | 선행 공조 표시 | 중앙 판단 기능 | Deferred |
| 제어 요청 처리 결과 · 거부·실패 사유 | 결과 표시 | 중앙 판단 기능 | Deferred |
| 안전 관련 경고 (등급 구분) | 경고 표시 | 중앙 판단 기능 | Deferred |
| 오류 분류 (센서 · 통신 · 기능) | 오류 구분 표시 | 각 노드 | Deferred |
| 각 상태 값의 신뢰성 정보 | 신뢰성 구분 표시 | 각 노드 | Deferred |
| 각 상태 값의 최신 여부 판단 근거 | `STALE` 판정 | 각 노드 | Deferred |

> `Source Owner` 가 **TBD** 인 항목은 담당 노드가 정해지지 않아 형태를 정의할 수 없다.
> 해당 항목의 논리 계약은 `02_MOBILE_SYSRS_FUNCTIONAL_BASELINE_DRAFT` 8.3절에 정의한다.

---

## 4. 인터페이스 설계 원칙

- 상태 값과 신뢰성 정보를 분리하지 않고 함께 전달한다. 분리하면 화면에서 신뢰성 표시가 누락된다.
- 전송 접수와 수행 완료를 구분 가능한 형태로 전달한다. 하나로 합치면 결과를 확인하지 못한 요청을 성공으로 표시하게 된다.
- 센서 원시 데이터가 아니라 차량이 확정한 상태를 전달한다.
- 안전 관련 경고는 주기 전송을 기다리지 않고 발생 시점에 전달한다.
- 같은 의미의 상태를 여러 경로로 중복 정의하지 않는다.
- 상태 최신 여부를 판단할 근거를 함께 전달한다. 근거가 없으면 무선 구간 지연을 검출할 수 없다.
- 실제 주기 및 Timeout 은 사용자 인지 한계와 단말 전력 소모를 함께 보고 결정한다.

---

## 5. 아직 만들지 않는 것

| Item | Current Status |
|---|---|
| BLE / Wi-Fi 등 무선 방식 | NOT DEFINED |
| 서비스 및 캐릭터리스틱 구조 | NOT DEFINED |
| JSON / 바이너리 등 데이터 형식 | NOT DEFINED |
| 필드 이름 및 자료형 | NOT DEFINED |
| MTU 및 분할 전송 방식 | NOT DEFINED |
| 상태 송신 주기 및 전송 트리거 | NOT DEFINED |
| Timeout 및 Freshness 표현 | NOT DEFINED |
| 인증 프로토콜 및 키 관리 방식 | NOT DEFINED |
| 알림 전달 방식 | NOT DEFINED |
| 게이트웨이 구성 및 담당 | NOT DEFINED |
| 창문 기능 담당 노드 | NOT DEFINED |
| 화면 레이아웃 및 시각 디자인 | NOT DEFINED |

---

## 6. 확인이 필요한 항목

| 항목 | 확인 대상 | 사유 |
|---|---|---|
| 창문 상태의 출처 | 팀 | 담당 노드가 정해지지 않아 `Source Owner` 미정 |
| 상태 송신 주기 | 차량 및 게이트웨이 | `STALE` 판정 시간이 이 값에 종속된다 (02 문서 13절) |
| 경고 등급 정의 | 중앙 판단 기능 | 등급 구분 없이는 우선 표시 규칙을 적용할 수 없다 |
| 오류 분류 열거값 | 전 노드 | 센서·통신·기능 구분 표시가 노드마다 달라지면 안 된다 |
