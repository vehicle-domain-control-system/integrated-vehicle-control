# `RequestManager` v0.2 수정 설명

## 기존 문제

기존:

```c
RequestManager_InvalidateSession()
```

은 해당 Session의 Request Record를 `memset()`으로 지웠습니다.

하지만 실제 요구는:

```text
현재 Session 실행 권한 무효화
```

와

```text
과거 Request Result History 보관
```

이 서로 다른 개념입니다.

재연결했다고 과거 Result가 즉시 사라지면 안 됩니다.

## v0.2 변경

### 실행 Context 비활성화

```c
RequestManager_DeactivateSession()
```

은 Record를 삭제하지 않고:

```c
executionContextActive = false;
```

로 바꿉니다.

따라서 과거 요청:

```text
Session 10 / Request 42 / DONE
```

은 그대로 조회할 수 있습니다.

### 전체 Session 재수립

재시작/재인증 시:

```c
RequestManager_DeactivateAllExecutionContexts()
```

를 사용합니다.

단, 실제 MCU Reset으로 RAM 자체가 소실되면 저장되지 않은 History는 복구할 수 없습니다. 이 경우 기존 Session을 무효화하고 새 인증/Session을 요구하는 정책이 필요합니다.

### 최근 5건

```c
RequestManager_GetRecentFinalHistory()
```

를 추가했습니다.

MOBILE 표시용으로 최대 5건을 반환하지만:

```text
5건 표시 제한
≠
Domain 내부 중복 실행 방지 Record 삭제 기준
```

입니다.

### GatewaySessionManager와 책임 분리

RequestManager는 새 Session이 인증된 Session인지 직접 판정하지 않습니다.

향후 흐름:

```text
ESP32
  ↓
GatewaySessionManager
  ├─ Device Registered?
  ├─ Current Connection?
  └─ Session Valid?
  ↓
RequestManager_Register()
```

따라서 무효 Session에서 새로운 Request ID가 들어오는 것은 GatewaySessionManager 단계에서 먼저 차단합니다.
