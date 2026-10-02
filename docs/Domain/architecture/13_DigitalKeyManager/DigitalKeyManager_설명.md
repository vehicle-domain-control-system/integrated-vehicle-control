# DigitalKeyManager 상세 설명

## 1. 역할

`DigitalKeyManager`는 중앙 S32K344의 Digital Key 자동 잠금 해제 정책을 담당합니다.

```text
ESP32
Registration / Connection / App Active / Proximity
                  │
                  ▼
GatewaySessionManager + VehicleStateManager
                  │
SettingsManager ──┤
                  ▼
          DigitalKeyManager
                  │
            Permission Gate
                  │
                  ▼
          CommandManager
                  │
                  ▼
              BCM UNLOCK
```

ESP32는 `NEAR/FAR/UNKNOWN`까지만 제공합니다.

```text
NEAR
→ 자동 Unlock
```

을 직접 결정하지 않습니다.

---

# 2. 접근 State Machine

```text
WAIT_FAR
   │ valid FAR
   ▼
ARMED_FAR
   │ valid NEAR
   ▼
APPROACH_ACTIVE
   │ command attempt / already unlocked / manual lock
   ▼
CONSUMED
   │ valid FAR
   └──────────────→ ARMED_FAR
```

## WAIT_FAR

자동 Unlock을 위해 먼저 유효한 `FAR`가 필요합니다.

다음 상황에서 이 상태로 돌아갑니다.

```text
새 Session
Digital Key Setting 변경
App 비활성/미확인
Setting OFF
기능 사용 불가
Proximity UNKNOWN/STALE/INVALID/NO_DATA
```

즉:

```text
새 연결 직후 이미 NEAR
```

여도 자동 Unlock하지 않습니다.

---

# 3. FAR의 의미

다음만 FAR입니다.

```text
quality == OK
AND
proximity == FAR
```

다음은 FAR가 아닙니다.

```text
Bluetooth Disconnect
UNKNOWN
STALE
INVALID
NO_DATA
```

현재 구현은 위 신뢰 불가 상태가 발생하면 이전 FAR baseline도 보수적으로 폐기하고
다시 `WAIT_FAR`로 돌아갑니다.

---

# 4. 한 접근당 최대 1회

유효한:

```text
FAR → NEAR
```

이 발생하면 하나의 접근이 만들어집니다.

Command를 생성하기로 결정한 순간:

```text
APPROACH_ACTIVE
→ CONSUMED
```

으로 바뀝니다.

따라서:

```text
BCM REJECTED
BCM FAILED
ALREADY_AT_TARGET
```

이후 같은 NEAR 상태에서 새 Command를 만들지 않습니다.

다시 실제 `FAR`가 확인되어야 새 접근이 됩니다.

---

# 5. 이미 Unlock 상태

새 접근이 들어왔지만 Door가 이미:

```text
UNLOCKED
```

라면 새 구동 명령을 만들지 않습니다.

그리고 접근을 `CONSUMED`로 처리합니다.

왜냐하면 이후 같은 접근에서 사용자가 수동으로 Lock했다고 해서:

```text
NEAR 유지
→ 자동으로 다시 Unlock
```

하면 안 되기 때문입니다.

---

# 6. 수동/외부 Lock 이후

BCM `LOCK_COMPLETED`가 현재 NEAR 접근 중 발생하면:

```text
approach = CONSUMED
```

을 유지합니다.

따라서 실제 FAR 이후 새 NEAR 전까지 자동 해제하지 않습니다.

---

# 7. 실행 조건

Command 직전 공통 Gate:

```text
Domain Operational
Current Session Valid
Feature Supported
Function Available
Required Input Quality OK
```

을 `PermissionManager`에 요청합니다.

Digital Key 고유 조건:

```text
Setting ON
App Active
Registered / Connected
valid FAR → NEAR
Door State trusted
Door LOCKED
```

은 `DigitalKeyManager`가 직접 판단합니다.

---

# 8. Command Origin

자동 해제는 MOBILE Request가 아닙니다.

따라서:

```text
CommandOrigin = DIGITAL_KEY
RequestContext 없음
JobContext 없음
```

으로 생성합니다.

```text
MOBILE
→ "자동 해제 사용 ON"

ESP32
→ FAR / NEAR

Domain
→ 실제 UNLOCK Command 생성
```

구조입니다.

---

# 9. 전송 실패

Command를 생성했지만:

```text
Tx Port 없음
Adapter Queue 실패
```

등으로 실제 DISPATCH가 되지 못한 경우:

```text
CommandManager_AbortBeforeDispatch()
```

로 Command를 종료합니다.

같은 접근에서 새 Command를 다시 만들지는 않습니다.

이번 단계에서 이를 위해 `CommandManager v0.3`에
`AbortBeforeDispatch()`를 추가했습니다.

---

# 10. Result Timeout

`Domain_PolicyConfig.doorResultWaitMs`까지 최종 결과를 확인하지 못하면:

```text
FAILED로 추정 X
```

입니다.

```text
ResultManager_MarkCommandUnconfirmed()
```

을 사용해:

```text
confirmation = UNCONFIRMED
```

으로 둡니다.

그리고 같은 접근에서 재요청하지 않습니다.

---

# 11. 실제 자동 해제 성공

Digital Key에서 가장 중요한 구분입니다.

### 실제 BCM Unlock transition

```text
BCM_EVENT_UNLOCK_COMPLETED
+
DONE
```

이면:

```text
actualUnlockTransition = true
lastUnlockOrigin = PROXIMITY_AUTO
```

입니다.

### 이미 Unlock 상태

```text
BCM_EVENT_ALREADY_AT_TARGET
+
DONE
```

이면:

```text
result = DONE
actualUnlockTransition = false
```

입니다.

즉 DONE이라고 해서 모두:

```text
"근접 자동 해제 성공"
```

으로 표시하지 않습니다.

---

# 12. Domain Interface v0.5 변경

기존 `DomainIf_DigitalKeyResult_t`에는 실제 해제 전이 여부를 명시할 필드가 부족했습니다.

v0.5:

```c
bool hasVehicleResult;
RequestResult_t result;
ResultReason_t reason;
ResultConfirmation_t confirmation;

UnlockOrigin_t origin;
DomainCommandId_t relatedDoorCommandId;

bool actualUnlockTransition;
```

을 사용합니다.

따라서:

```text
결과 미확인
→ hasVehicleResult = false
  confirmation = UNCONFIRMED

ALREADY_AT_TARGET DONE
→ hasVehicleResult = true
  result = DONE
  actualUnlockTransition = false

실제 UNLOCK_COMPLETED
→ actualUnlockTransition = true
```

로 구분할 수 있습니다.

---

# 13. 현재 구조의 단일 Current Proximity 모델

현재 `VehicleStateManager`의 `DigitalKeyInput_t`는 차량 전체의 현재 ESP32 Connection/Proximity 하나를 보관합니다.

따라서 현재 DigitalKeyManager도:

```text
동시에 여러 MOBILE이 각각 독립 RSSI 접근을 수행
```

하는 구조보다:

```text
현재 활성 MOBILE Connection Context
```

하나를 중심으로 동작합니다.

`GatewaySessionManager/SettingsManager`는 여러 DeviceContext를 저장할 수 있지만,
실제 동시 다중 Digital Key를 지원하려면 향후:

```text
VehicleStateManager
DigitalKeyInput
Proximity
```

도 Device별로 확장해야 합니다.

현재 프로젝트 요구에는 그 확장을 임의로 추가하지 않습니다.

---

# 14. 다음 단계

Digital Key Feature가 완성되면 다음은 `ClimateManager`입니다.

```text
SettingsManager
Target Temperature / Auto Mode / Manual Fan
        +
VehicleStateManager
Cabin Temperature / BCM State
        ↓
ClimateManager
        ↓
Final Fan / Thermal Target
        ↓
CommandManager
        ↓
BCM
```

이후:

```text
InteriorLightManager
AutoVentilationManager
WarningManager
VehicleEventManager
```

순으로 진행합니다.
