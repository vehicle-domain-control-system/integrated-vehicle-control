# ResultManager 상세 설명

## 1. 한 문장 정의

`ResultManager`는:

> **BCM/WINDOW 같은 실행 ECU에서 돌아온 결과를 Command ID로 원 Command에 연결하고, MOBILE Request 또는 자동 기능 Job까지 결과를 되돌려 주는 모듈**

입니다.

---

# 2. 전체 연결

```text
MOBILE Request
      ↓
RequestManager
      ↓
Feature / Permission
      ↓
CommandManager
      ↓
BCM / WINDOW
      ↓
ResultManager
      ↓
CommandManager
      ├─ RequestManager
      └─ Automatic Job Context
```

---

# 3. 이번 단계에서 발견한 CommandManager 보완

기존 CommandManager에는:

```text
RequestContext
= SessionId + RequestId
```

만 저장되어 있었습니다.

그런데 RequestManager의 실제 Key는:

```text
DeviceContextId
+
SessionId
+
RequestId
```

입니다.

따라서 ECU Result가 돌아온 뒤 원 Request를 찾으려면:

```text
DeviceContextId
```

도 Command provenance에 있어야 합니다.

그래서 `CommandManager v0.2`에서:

```c
DeviceContextId_t requestDeviceContextId;
```

를 추가했습니다.

이제:

```text
Command #25
      ↓
Device 1
Session 10
Request 100
```

을 정확히 복원할 수 있습니다.

---

# 4. RequestManager v0.3 보완

SysRS의 결과 감시 문맥에서는 최초 실행 응답이:

```text
ACCEPTED
또는
IN_PROGRESS
```

일 수 있습니다.

따라서 기존처럼:

```text
RECEIVED
→ ACCEPTED
→ IN_PROGRESS
```

만 허용하면 최초 `IN_PROGRESS`가 들어왔을 때 문제가 됩니다.

v0.3에서는:

```text
RECEIVED
→ IN_PROGRESS
```

도 허용했습니다.

이것은 ACCEPTED를 임의로 만들어 넣는 것보다 정확합니다.

---

# 5. Result 종류

현재 공통 결과:

```text
ACCEPTED
IN_PROGRESS
DONE
REJECTED
CANCELLED
FAILED
```

를 처리합니다.

`UNKNOWN`은 실행 ECU가 보내는 실제 결과로 두지 않습니다.

```text
결과를 확인하지 못함
→ FAILED 추정 금지
→ confirmation = UNCONFIRMED
```

으로 관리합니다.

MOBILE 표시 단계에서 이를 `UNKNOWN` 의미로 변환할 수 있습니다.

---

# 6. 늦은 중간 Result

예:

```text
Command
ACCEPTED
→ IN_PROGRESS
→ DONE
```

후 네트워크 지연으로:

```text
ACCEPTED
```

가 다시 도착할 수 있습니다.

이때:

```text
DONE → ACCEPTED
```

로 상태를 뒤로 돌리면 안 됩니다.

ResultManager는:

```text
현재 IN_PROGRESS인데 늦은 ACCEPTED
→ 정상 무시

현재 FINAL인데 늦은 ACCEPTED/IN_PROGRESS
→ 정상 무시
```

합니다.

Final 결과는 뒤늦은 중간 결과로 되돌리지 않습니다.

---

# 7. BCM Event 처리

Command Result로 처리하는 BCM Event:

```text
LOCK_COMPLETED
UNLOCK_COMPLETED
ALREADY_AT_TARGET
REQUEST_REJECTED
REQUEST_FAILED
```

반면:

```text
OVERHEAT_DETECTED
FAN_MISMATCH_DETECTED
RECOVERY_CONFIRMED
```

는 Diagnostic/Protection 정보입니다.

이 이벤트들을 Command Result라고 추정하지 않습니다.

추후:

```text
DiagnosticManager
```

가 처리합니다.

---

# 8. ALREADY_AT_TARGET가 중요한 이유

일반 Door Request에서는:

```text
UNLOCK 요청
이미 UNLOCK 상태
→ DONE + ALREADY_AT_TARGET
```

이 정상 완료일 수 있습니다.

하지만 Digital Key 자동 해제에서는 다릅니다.

예:

```text
FAR → NEAR
→ Auto Unlock Command

그런데 이미 Door가 UNLOCK 상태
→ ALREADY_AT_TARGET
```

이면:

```text
실제로 이번 접근으로 잠금 해제가 발생했다
```

고 표시하면 안 됩니다.

그래서 ResultManager는 완료 증거를 따로 보존합니다.

```text
RESULT_EVIDENCE_TARGET_TRANSITION_CONFIRMED
RESULT_EVIDENCE_ALREADY_AT_TARGET
RESULT_EVIDENCE_EXECUTION_RESULT
```

추후 `DigitalKeyManager`가:

```text
Origin = DIGITAL_KEY
+
Result = DONE
+
Evidence = TARGET_TRANSITION_CONFIRMED
```

일 때만 실제 근접 자동 해제 성공으로 판단할 수 있습니다.

---

# 9. Window Result

v0.3 Interface에서는 Window Result도:

```text
CommandId
```

기준입니다.

따라서:

```text
AutoVentilation Job #3
→ Window Command #30
→ Result #30
→ ResultManager
→ Job #3
```

로 연결됩니다.

MOBILE Request가 없어도 문제없습니다.

---

# 10. Result Quality

Result/Event의:

```text
meta.quality != OK
```

이면 그 결과를 실제 확정 결과로 반영하지 않습니다.

예:

```text
STALE DONE
```

이라고 해서:

```text
Command = DONE
```

으로 만들지 않습니다.

대신:

```text
UNCONFIRMED
```

으로 처리하고 `FAILED`를 추정하지 않습니다.

---

# 11. Request 연결

Command가 MOBILE Request에서 왔다면:

```text
CommandManager
requestDeviceContextId
requestContext
```

를 읽어:

```text
RequestManager_MarkAccepted()
RequestManager_MarkInProgress()
RequestManager_Finalize()
```

중 적절한 함수를 호출합니다.

따라서 MOBILE에 보여줄 결과는 원 Request와 계속 연결됩니다.

---

# 12. Automatic Job 연결

Command가:

```text
AUTO_VENTILATION
```

에서 만들어진 경우:

```text
hasJobContext = true
jobId = ...
```

를 반환합니다.

ResultManager가 AutoVentilation 자체를 완료시키지는 않습니다.

왜냐하면:

```text
Window Move DONE
≠
AutoVentilation Job DONE
```

이기 때문입니다.

추후 `AutoVentilationManager`가 Job 결과를 해석합니다.

---

# 13. 다음 단계

현재 Core 실행 체인:

```text
GatewaySessionManager     ✅
RequestManager            ✅
SettingsManager           ✅
PermissionManager         ✅
CommandManager            ✅
ResultManager             ✅
```

이제 다음은:

```text
Domain_Interface.c
```

입니다.

여기에서 외부 Logical Rx를 실제 Manager들로 Routing합니다.

그 다음 Feature Manager:

```text
DigitalKey
Climate
InteriorLight
AutoVentilation
Warning
VehicleEvent
```

를 올리면 됩니다.
