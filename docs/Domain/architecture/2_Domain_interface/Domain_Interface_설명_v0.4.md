# Domain_Interface v0.4 / Core Routing 설명

## 1. 역할

`Domain_Interface.c`는 차량 기능 정책을 구현하지 않습니다.

역할은:

```text
외부 Logical Input
   ↓
Core Manager에 반영
   ↓
필요 시 Feature/Service Observer에 전달

Domain Logical Output
   ↓
등록된 Tx Port
   ↓
향후 CAN/UART Adapter
```

입니다.

---

# 2. 왜 Rx Observer를 추가했나?

현재 아직 구현되지 않은 모듈이 있습니다.

```text
DigitalKeyManager
WarningManager
DiagnosticManager
AutoVentilationManager
MobileStateMapper
```

예를 들어:

```text
WINDOW Anti-Pinch
```

를 지금 `Domain_Interface.c`가 임의로 Warning으로 만들어버리면
나중에 `WarningManager` 책임과 중복됩니다.

그래서:

```text
DomainIf_RxWindowAntiPinch()
        ↓
Rx Observer
        ↓
향후 WarningManager / DomainTask
```

로 연결할 수 있게 했습니다.

현재 소비자가 없으면 `DOMAIN_IF_NOT_AVAILABLE`을 반환해
이벤트가 실제로 처리되지 않았다는 사실을 숨기지 않습니다.

---

# 3. 왜 Tx Port를 추가했나?

현재:

```text
CanAdapter
UartAdapter
```

가 아직 없습니다.

그런데 Tx 함수를 구현하면서 그냥:

```c
return DOMAIN_IF_OK;
```

라고 하면 실제 전송하지 않았는데 전송 성공으로 꾸미게 됩니다.

그래서 Adapter가:

```c
DomainIf_SetTxPort(&port);
```

로 실제 송신 함수를 등록하기 전에는:

```text
DOMAIN_IF_NOT_AVAILABLE
```

을 반환합니다.

향후:

```text
DomainIf_TxBcmDoorCommand()
        ↓
Tx Port
        ↓
CanAdapter queue
```

가 됩니다.

---

# 4. Command DISPATCHED 시점

`CommandManager_Create()` 직후에는:

```text
CREATED
```

입니다.

`DomainIf_TxBcmDoorCommand()`가 Tx Port에 전달했고
실제 Adapter/Queue가 이를 수용한 경우에만:

```text
DISPATCHED
```

로 바뀝니다.

즉:

```text
Command 생성
≠
전송 큐 등록 완료
```

를 구분합니다.

Tx Port가 실패하면 Command는 `CREATED`로 남습니다.

---

# 5. ESP32 Connection Routing

```text
DigitalKeyConnection
      ↓
VehicleStateManager
      ↓
GatewaySessionManager
      ↓
Session Transition
      ├─ RequestManager old session deactivate
      └─ SettingsManager Digital Key context OFF/reset
      ↓
DomainLifecycleManager
```

을 연결했습니다.

---

# 6. MOBILE Request Routing

```text
MOBILE_REQUEST
      ↓
GatewaySessionManager
      ↓ current session valid?
RequestManager
      ↓ duplicate/conflict?
Rx Observer
      ↓
향후 DomainTask / Feature
```

동일 Request 재전달이면:

```text
RequestManager는 기존 Record 유지
새 Feature 실행 Observer 호출 안 함
```

이므로 재실행을 방지합니다.

---

# 7. State Routing

현재 직접 VehicleStateManager에 들어가는 항목:

```text
BCM Door
BCM Climate
BCM Interior Light

CIS Occupant
CIS Cabin
CIS Rear

WINDOW State
VSS State

ESP32 Connection
ESP32 Proximity
```

동시에 현재 품질을 기준으로
`DomainLifecycleManager`의 Sync Group도 갱신합니다.

---

# 8. Result Routing

BCM의 Command Result 성격 이벤트와 WINDOW Result는:

```text
ResultManager
      ↓
CommandManager
      ↓
RequestManager
```

까지 Core 상태를 반영합니다.

그 뒤 원 이벤트는 Rx Observer에도 전달할 수 있습니다.

이것은 향후:

```text
DigitalKeyManager
AutoVentilationManager
```

가 기능별 의미를 추가 해석할 수 있게 하기 위함입니다.

---

# 9. 아직 NOT_AVAILABLE인 항목

Core만으로 의미 처리가 끝나지 않는 정보:

```text
State Query
Warning ACK
BCM ECU-level status
Faults
CIS Status
Anti-Pinch semantic processing
```

은 해당 Observer가 아직 등록되지 않았다면
`DOMAIN_IF_NOT_AVAILABLE`입니다.

이건 구현 누락을 성공으로 숨기는 것보다 안전합니다.

Feature/Diagnostic Manager를 만들면서 Observer를 연결합니다.

---

# 10. 현재 호출 문맥

이 함수들은 ISR 직접 호출용이 아닙니다.

권장:

```text
CAN/UART ISR
  ↓
Communication Queue
  ↓
Communication/Domain Task
  ↓
Adapter Decode
  ↓
DomainIf_RxXXX()
```

또는 최종 DomainTask 구조에 맞춰 한 Task에서 직렬화합니다.

---

# 11. 다음 단계

Core Routing까지 연결됐으므로 다음부터는 Feature Manager 구현으로 넘어갑니다.

우선순위:

```text
DigitalKeyManager
ClimateManager
InteriorLightManager
AutoVentilationManager
WarningManager
VehicleEventManager
```

다만 `VehicleUsageManager`는 실제 Producer가 아직 TBD라
Skeleton 또는 후순위로 둘 수 있습니다.
