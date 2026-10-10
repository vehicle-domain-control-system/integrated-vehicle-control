# ProximityManager 설계 설명서 v0.1

> 대상 코드  
> - `ESP32/feature/ProximityManager.h`  
> - `ESP32/feature/ProximityManager.c`
>
> 관련 코드  
> - `Gateway_PolicyConfig.h/.c`
> - `Gateway_Types.h`
> - `DeviceRegistrationManager`
> - `LinkStateManager`
> - `Gateway_Interface`
>
> 관련 테스트  
> - `test_proximity_manager.c`
> - `test_gateway_policy_config.c`
>
> 상태  
> - ESP32 근접 정보 제공 책임: `CONFIRMED`
> - Bluetooth RSSI 사용 방법: `PROVISIONAL`
> - 실제 threshold/안정화/expiry 값: `TBD`

---

# 1. 목적

`ProximityManager`는 현재 Bluetooth 연결의 RSSI 관측을 차량에서 사용할 논리 근접 상태로 변환한다.

```text
Bluetooth RSSI
      ↓
현재 등록/연결 Peer 확인
      ↓
Validity
      ↓
Threshold + Hysteresis
      ↓
Stable Sample
      ↓
NEAR / FAR / UNKNOWN
      ↓
Quality + Age + New Update
      ↓
Domain
```

중요한 것은 실제 거리를 계산하는 것이 아니라:

```text
NEAR
FAR
UNKNOWN
```

을 제공하는 것이다.

---

# 2. SysRS 책임 경계

현재 SysRS 기준으로 ESP32는 Bluetooth RSSI를 기반으로:

```text
NEAR
FAR
UNKNOWN
품질
갱신 근거
```

를 중앙에 제공한다.

반면 중앙은:

```text
등록/연결
Auto Unlock 설정
유효한 새 접근
Door 상태
실행 허용
```

을 함께 확인해서 최종 Unlock 명령 여부를 판단한다.

따라서:

```text
ProximityManager
≠
AutoUnlockManager
```

이다.

---

# 3. 가장 중요한 안전 규칙

다음을 절대로 같은 의미로 처리하지 않는다.

```text
Disconnect
≠
FAR

미수신
≠
FAR

STALE
≠
FAR

INVALID
≠
FAR
```

이런 경우에는:

```text
state = UNKNOWN
```

을 사용한다.

---

# 4. 왜 이 구분이 중요한가?

중앙의 새 접근 판정은:

```text
유효한 비근접(FAR)
        ↓
유효한 근접(NEAR)
```

전이를 이용할 수 있다.

만약 Bluetooth Disconnect를 FAR로 바꾸면:

```text
NEAR
 ↓
Disconnect → 잘못된 FAR
 ↓
Reconnect 후 NEAR
```

가 새 접근처럼 보일 수 있다.

그래서 연결 상실은 FAR가 아니다.

---

# 5. `STALE` 공통 품질 추가

이번 단계에서 `Gateway_DataQuality_t`에:

```c
GATEWAY_DATA_QUALITY_STALE
```

를 추가했다.

이유는 SysRS가:

```text
STALE
INVALID
NO_DATA
```

를 구분하고 있기 때문이다.

---

# 6. STALE의 의미

현재 ProximityManager에서:

```text
마지막 실제 RSSI 관측 후
proximity_expiry_ms 도달
```

하면:

```text
state   = UNKNOWN
quality = STALE
```

로 만든다.

---

# 7. STALE에서 Age를 초기화하지 않음

매우 중요하다.

예:

```text
RSSI 마지막 관측 = 1000 ms
Expiry           = 500 ms
현재             = 1500 ms
```

STALE로 변경한다고:

```text
last_observed = 1500
```

으로 바꾸지 않는다.

원본 관측 시각은:

```text
1000 ms
```

로 유지한다.

따라서 Domain에 전달되는 Age도 원 관측 기준을 유지한다.

---

# 8. 재전송도 Age를 초기화하지 않음

동일한 Proximity 상태를 UART로 다시 보냈다고:

```text
새 RSSI 관측
```

이 되는 것이 아니다.

`is_new_update`는 실제 관측/평가 변화와 단순 Publish를 구분한다.

---

# 9. 현재 입력 조건

RSSI sample을 정상 Proximity 입력으로 사용하려면:

```text
Gateway Policy Config 준비
Bluetooth Link AVAILABLE
현재 Peer REGISTERED + CONNECTED
RSSI Sample의 Device Ref == 현재 Peer
```

조건이 필요하다.

---

# 10. 다른 Peer RSSI 거부

예:

```text
등록/현재 연결 = Phone A
RSSI event      = Phone B
```

이면 해당 RSSI를 A의 근접 정보로 사용하지 않는다.

현재 API는:

```c
GATEWAY_STATUS_NOT_READY
```

를 반환한다.

---

# 11. 식별과 근접 분리

RSSI가 강하다고:

```text
REGISTERED
```

를 만들지 않는다.

반대로 등록된 단말이라고:

```text
NEAR
```

를 만들지도 않는다.

구조:

```text
DeviceRegistrationManager
→ 누구인가?

ProximityManager
→ 그 현재 peer의 근접 상태는 무엇인가?
```

이다.

---

# 12. App Active와도 분리

RSSI가 수신된다고:

```text
APP_ACTIVE
```

라고 판단하지 않는다.

Bluetooth 응답과 앱 화면 활성은 별도 정보다.

현재 App Activity 획득 계약은 아직 `INPUT-TBD`다.

---

# 13. RSSI 기반 방법은 PROVISIONAL

Bluetooth RSSI는 환경 영향을 많이 받을 수 있으므로 현재 프로젝트에서도 실제 구성 검증이 필요하다.

따라서 이번 구현은:

```text
RSSI Input Adapter
+
교체 가능한 Policy
```

형태로 둔다.

향후 방법이 바뀌어도 Domain 인터페이스는:

```text
NEAR / FAR / UNKNOWN
```

을 유지할 수 있다.

---

# 14. 실제 거리(m)를 출력하지 않음

현재 Manager는:

```text
1.2 m
2.4 m
```

같은 거리값을 만들지 않는다.

SysRS의 요구는 차량에서 사용할 **근접 상태**다.

---

# 15. Threshold 분리

Policy:

```text
near_enter_rssi_dbm
far_exit_rssi_dbm
```

두 값을 사용한다.

조건:

```text
near_enter > far_exit
```

이어야 한다.

---

# 16. UNKNOWN에서의 분류

현재 상태가 UNKNOWN일 때:

```text
RSSI >= near_enter
→ NEAR candidate

RSSI <= far_exit
→ FAR candidate

중간 band
→ UNKNOWN 유지
```

이다.

---

# 17. NEAR에서의 Hysteresis

현재 NEAR일 때:

```text
RSSI <= far_exit
→ FAR candidate

그 외
→ NEAR 유지
```

한다.

즉 중간 band에서 바로 FAR가 되지 않는다.

---

# 18. FAR에서의 Hysteresis

현재 FAR일 때:

```text
RSSI >= near_enter
→ NEAR candidate

그 외
→ FAR 유지
```

한다.

---

# 19. Stable Sample

Candidate가 나왔다고 바로 상태를 바꾸지 않을 수 있다.

```text
Candidate NEAR
Candidate NEAR
...
```

가 `stable_sample_count`만큼 연속 확인되면 실제 NEAR로 전환한다.

실제 횟수는 Policy Config에서 주입한다.

---

# 20. 실제 수치를 왜 문서에 확정하지 않는가?

테스트에서는 예를 들어:

```text
NEAR = -60
FAR  = -70
stable = 2
expiry = 500 ms
```

같은 값을 사용한다.

이 값은 **Unit Test용 예시값**이다.

차량/휴대폰/안테나/환경 시험 없이 실제 프로젝트 값으로 채택하면 안 된다.

---

# 21. Invalid Sample

Bluetooth 계층이 RSSI 값 자체를 유효하지 않다고 알려주면:

```text
state   = UNKNOWN
quality = INVALID
```

로 처리한다.

---

# 22. Invalid를 FAR로 바꾸지 않는 이유

Invalid는:

```text
멀리 있음
```

이 아니라:

```text
현재 입력을 신뢰할 수 없음
```

이다.

의미가 다르다.

---

# 23. 아직 Sample이 없는 경우

등록/연결은 정상인데 RSSI를 아직 한 번도 받지 못했다면:

```text
state   = UNKNOWN
quality = NO_DATA
```

로 표현한다.

---

# 24. Bluetooth Disconnect

Link 또는 현재 authenticated peer가 없어지면:

```text
state   = UNKNOWN
quality = NO_DATA
```

다.

---

# 25. Peer Change

Phone A의 마지막 상태가:

```text
NEAR
```

였다고 하자.

그 뒤 현재 peer가 Phone B로 바뀌면 A의 NEAR를 B에 이어 쓰면 안 된다.

현재 구현:

```text
peer mismatch
→ old observation invalidated
→ UNKNOWN + NO_DATA
→ 새 B RSSI 필요
```

이다.

---

# 26. 새 Peer 연결 자체는 NEAR가 아님

새 Bluetooth connection이 생겼다고:

```text
NEAR
```

를 자동 생성하지 않는다.

새 실제 Proximity observation이 필요하다.

---

# 27. Snapshot

`ProximityManager_Snapshot_t`는 다음을 제공한다.

```text
state
quality
has_observation
last RSSI
last observed time
pending transition candidate
candidate count
bound device reference
observation revision
last published revision
```

Debug/Test용이다.

---

# 28. Observation Revision

실제 RSSI 관측이 들어올 때:

```text
observation_revision++
```

한다.

값이 동일한 NEAR라도 실제 새 RSSI sample이면 새 관측이다.

---

# 29. Publish Revision

Domain Publish 성공 시:

```text
last_published_revision =
observation_revision
```

으로 기록한다.

---

# 30. `is_new_update`

다음 비교로 결정한다.

```text
observation_revision
!=
last_published_revision
```

이면 true다.

---

# 31. 단순 재Publish

새 RSSI 없이 동일 정보를 다시 Publish하면:

```text
is_new_update = false
```

이다.

---

# 32. Domain Update

현재 출력 타입:

```c
Gateway_ProximityUpdate_t
```

이다.

포함:

```text
Device Context
NEAR/FAR/UNKNOWN
Quality
Age
New Update
```

이다.

---

# 33. Device Context

Proximity 정보가 어떤 peer의 관측인지 알 수 있도록
Manager가 RSSI와 함께 묶인 Device Reference를 보관한다.

Domain Update의 `device_context_id`는 해당 reference를 사용한다.

---

# 34. base context

Caller가 전달한:

```text
vehicle_id
session_id
request_id
```

는 유지한다.

`device_context_id`는 ProximityManager가 자신의 현재 관측 peer reference로 채운다.

---

# 35. 실제 UART 표현은 아직 미정

다음은 여기서 결정하지 않는다.

```text
UART Message ID
Byte Offset
RSSI raw 전송 여부
CRC
Sequence
Transmission Period
Timeout
```

`NETWORK-TBD`다.

---

# 36. Raw RSSI를 Domain에 꼭 보낼 필요는 없음

현재 SysRS상 핵심 제공 정보는:

```text
NEAR/FAR/UNKNOWN
Quality
Update basis
```

다.

Raw RSSI를 추가 진단 정보로 보낼지는 후속 Interface 설계에서 결정할 수 있다.

현재 logical DTO에는 raw RSSI를 넣지 않는다.

---

# 37. `Gateway_PolicyConfig` 의존

ProximityManager는 설정이 준비되지 않으면:

```text
NOT_READY
```

다.

임의 default threshold를 사용하지 않는다.

---

# 38. `LinkStateManager` 의존

Bluetooth Link가 AVAILABLE인지 확인한다.

하지만 LinkStateManager만으로 등록 peer 여부는 알 수 없으므로
DeviceRegistrationManager도 함께 확인한다.

---

# 39. `DeviceRegistrationManager` 의존

필수 조건:

```text
REGISTERED + CONNECTED
```

이다.

이는 Proximity를 등록된 현재 peer와 묶기 위한 조건이지,
차량 Auto Unlock 허용 판정이 아니다.

---

# 40. Domain 책임

Domain은 받은 Proximity를 가지고 다음을 판단한다.

```text
유효한 FAR 확인?
그 뒤 유효한 NEAR 전환?
새 접근인가?
Auto Unlock 사용 설정 ON?
Door 잠금 상태 유효?
실행 허용 조건 충족?
```

ESP32는 새 접근을 생성하지 않는다.

---

# 41. 수동 잠금 이후 정책

같은 접근에서 수동 잠금 후 재자동해제 금지 같은 규칙은 Domain 책임이다.

ProximityManager는 계속 현재 근접 상태만 제공한다.

---

# 42. Auto Lock은 추가하지 않음

멀어졌다고:

```text
자동 잠금
```

명령을 만들지 않는다.

현재 요구 범위는 자동 해제이며,
실제 FAR는 중앙의 새 접근 상태 관리 근거일 수 있을 뿐이다.

---

# 43. Unit Test — 초기 상태

확인:

```text
Init
→ UNKNOWN + UNKNOWN quality

등록/연결은 있지만 RSSI 없음
→ UNKNOWN + NO_DATA
```

이다.

---

# 44. Unit Test — Stable NEAR

설정된 sample count만큼 강한 RSSI가 연속 들어와야 NEAR로 전환되는지 확인한다.

---

# 45. Unit Test — Stable FAR

동일하게 FAR 후보도 안정화 조건을 만족해야 실제 FAR가 된다.

---

# 46. Unit Test — Hysteresis

NEAR 상태에서 RSSI가 두 threshold 사이로 이동해도
즉시 FAR로 바뀌지 않는지 확인한다.

---

# 47. Unit Test — Invalid

Invalid sample:

```text
UNKNOWN + INVALID
```

이며 FAR가 아닌지 검증한다.

---

# 48. Unit Test — Disconnect

NEAR 상태에서 Bluetooth가 끊겨도:

```text
UNKNOWN + NO_DATA
```

이며 FAR가 아닌지 확인한다.

---

# 49. Unit Test — Expiry

마지막 sample age가 expiry에 도달하면:

```text
UNKNOWN + STALE
```

이 되는지 확인한다.

---

# 50. Unit Test — Original Age

STALE 판정 시:

```text
last_observed_ms
```

를 현재 시각으로 바꾸지 않는지 확인한다.

즉 오래된 값을 평가했다는 이유로 freshness를 되살리지 않는다.

---

# 51. Unit Test — Other Peer

현재 peer A인데 B의 RSSI가 들어오면 거부하는지 확인한다.

---

# 52. Unit Test — Peer Change

A의 NEAR 상태가 B로 교체된 뒤 그대로 이어지지 않고:

```text
UNKNOWN + NO_DATA
```

가 되는지 확인한다.

---

# 53. Unit Test — Publish

첫 실제 관측 후 Publish:

```text
is_new_update = true
```

재Publish:

```text
false
```

새 RSSI:

```text
다시 true
```

인지 확인한다.

---

# 54. 현재 전체 Feature 구조

```text
feature/
├─ DeviceRegistrationManager   ✅
└─ ProximityManager            ✅
```

---

# 55. 다음 단계

Architecture 순서상 다음은 App Activity Context다.

다만 현재 SysRS에서도:

```text
Bluetooth 응답이 있다고 App Active로 판단하지 않음
App Active 확인 방법은 실제 구성에서 결정
```

상태다.

따라서 지금 `AppActivityTracker` 같은 Manager를 임의 구현하면 안 된다.

---

# 56. App Activity 다음 작업

먼저 결정해야 할 것은:

```text
App이 ACTIVE/INACTIVE를 직접 보내는가?
Bluetooth Profile이 제공하는 근거가 있는가?
Foreground lifecycle event인가?
Heartbeat인가?
갱신 주기는?
Timeout은?
UNKNOWN 전환 조건은?
```

이다.

이 계약이 정해진 후 `Gateway_Interface_PublishAppActivity()`에 연결하는 것이 맞다.

---

# 57. Phase 2 완료 기준

현재 Phase 2 구현 관점:

```text
DeviceRegistrationManager   ✅
ProximityManager            ✅
App Activity input contract ⏳ INPUT-TBD
```

이다.

---

# 58. 핵심 정리

`ProximityManager`를 한 문장으로 표현하면:

> **현재 등록·연결된 Bluetooth peer의 RSSI 관측을 안정화된 NEAR/FAR/UNKNOWN 상태와 품질·갱신 근거로 변환해 Domain에 제공하는 Feature Manager다.**

가장 중요한 규칙:

```text
RSSI
≠
등록 확인

CONNECTED
≠
NEAR

Disconnect
≠
FAR

STALE
≠
FAR

INVALID
≠
FAR

Peer A의 NEAR
≠
Peer B의 NEAR

NEAR
≠
새 접근

NEAR
≠
Auto Unlock 명령
```
