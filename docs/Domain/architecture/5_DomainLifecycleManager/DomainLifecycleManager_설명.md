# DomainLifecycleManager 설명

## 한 문장 정의

`DomainLifecycleManager`는:

> **S32K344 Domain 전체가 현재 차량 기능을 평가할 준비가 되었는지 관리하는 모듈**

입니다.

## 왜 필요한가?

S32K344 Reset 직후:

```text
MCU는 실행됨
BUT
BCM 최신 상태 아직 없음
CIS 최신 상태 아직 없음
ESP32 현재 Session 아직 모름
```

일 수 있습니다.

이 순간 과거 초기값으로:

```text
Digital Key Auto Unlock
Auto Ventilation
Warning CLEAR
```

등을 판단하면 안 됩니다.

그래서:

```text
STARTUP
   ↓
SYNCING
   ↓
READY
```

단계를 둡니다.

일부 필수 입력이 현재 사용할 수 없다고 확인되면:

```text
DEGRADED
```

가 가능합니다.

## 각 상태

### STARTUP

```text
RTD / Task / 기본 통신 인프라 준비 전
```

새 차량 기능 판단을 시작하지 않습니다.

### SYNCING

```text
현재 ECU State
현재 ESP32 Connection/Session
현재 품질
```

등의 **새 현재값**을 모으는 상태입니다.

과거 값을 자동 복원하지 않습니다.

### READY

설정한 `requiredInitialSyncMask`의 현재 상태가 모두 정상 확인됐습니다.

### DEGRADED

두 경우가 있습니다.

```text
필수 Group이 명시적으로 UNAVAILABLE
또는
설정한 Sync Timeout까지 일부 현재 상태를 확인하지 못함
```

중요:

```text
DEGRADED
≠
모든 기능 실행 금지
```

입니다.

`PermissionManager + DiagnosticManager`가 실제 Feature별 Availability를 추가 판단합니다.

## requiredInitialSyncMask를 코드가 정하지 않는 이유

현재 프로젝트에서:

```text
어느 ECU가 Domain 전체 READY의 절대 필수인가?
```

를 SysRS가 하나의 고정 Mask로 정의하지는 않았습니다.

따라서 `DomainLifecycle_Init(NULL)` 같은 임의 Default를 만들지 않았습니다.

통합 단계에서 명시적으로:

```c
cfg.requiredInitialSyncMask =
    DLM_SYNC_BCM_DOOR |
    DLM_SYNC_CIS_REAR |
    ...;
```

처럼 선택합니다.

Vehicle Usage는 Architecture상 필요하지만 Producer가 아직 TBD라
지금 당장 Required Mask에 넣을 필요는 없습니다.

## 재동기화

예:

```text
ESP32 재연결
```

이면:

```c
DomainLifecycle_BeginResync(
    DLM_SYNC_ESP32_CONNECTION |
    DLM_SYNC_ESP32_PROXIMITY,
    nowMs);
```

를 사용합니다.

그러면 이전 Connection/Proximity 근거를 새 현재값으로 취급하지 않습니다.

## Timeout의 의미

Sync Timeout은:

```text
ECU FAILED
```

를 만드는 시간이 아닙니다.

그 의미는:

```text
Domain이 무한정 SYNCING에 머무르지 않고
현재 일부 정보 미확인 상태로 DEGRADED 운영
```

입니다.

실제 통신 Fault는 `CommunicationMonitor / DiagnosticManager`가 판단합니다.
