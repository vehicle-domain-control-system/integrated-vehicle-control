# 모바일 앱 기반 차량 HMI 기능 구성안

## 1. 목적

기존 차량 내 HMI를 제거하고, 스마트폰 모바일 앱을 차량의 주요 사용자 인터페이스로 사용한다.

모바일 앱의 역할은 크게 다음 4가지로 구성한다.

1. **Digital Key**
2. **차량 상태 확인**
3. **차량 기능 제어**
4. **경고 및 Fault 확인**

전체 구조는 다음과 같다.

```text
Smartphone App
      │
   BLE / Wi-Fi
      │
    ESP32
      │
     UART
      │
   S32K344
Domain Controller
      │
     CAN
      │
 ┌────┼───────────────┐
 │    │               │
Window Function      Sensing
ECU    ECU            ECU
```

모바일 앱은 각 ECU를 직접 제어하지 않고, 차량 제어에 대한 **Request**를 전달하며 실제 제어 가능 여부 판단은 S32K344 또는 해당 ECU가 담당하도록 구성한다.

---

# 2. 모바일 앱에 표시할 차량 정보

## 2.1 주요 차량 상태

앱 메인 화면에는 사용자가 차량 상태를 빠르게 파악할 수 있는 정보를 표시한다.

| 구분 | 표시 정보 | 중요도 |
|---|---|---|
| 차량 상태 | 차량 전원 ON/OFF | 매우 높음 |
| 도어 | 잠금/해제 상태 | 매우 높음 |
| 도어 | 도어 열림 여부 | 매우 높음 |
| 파워윈도우 | 창문 위치 및 열림 상태 | 매우 높음 |
| 실내환경 | 실내 온도 | 매우 높음 |
| 실내환경 | 실내 습도 | 높음 |
| Smart Access | Digital Key 연결 상태 | 매우 높음 |
| Smart Access | 사용자 근접 상태 | 매우 높음 |
| 공조 | 현재 공조 동작 상태 | 높음 |
| 조명 | 실내등/전조등 상태 | 높음 |
| 센서 | 탑승자 감지 여부 | 높음 |
| 시스템 | ECU 정상/통신 오류 | 매우 높음 |
| 시스템 | Fault / Warning | 매우 높음 |

### 메인 화면 예시

```text
┌──────────────────────────┐
│       MY VEHICLE         │
│                          │
│ 🔑 Digital Key Connected │
│                          │
│ 🔒 Vehicle Locked        │
│                          │
│ 🚪 Doors      CLOSED     │
│ 🪟 Windows    CLOSED     │
│ 🌡 Temperature  24°C     │
│ 💧 Humidity     48%      │
│                          │
│ Vehicle Status : NORMAL  │
└──────────────────────────┘
```

---

# 3. Digital Key 기능

Digital Key는 모바일 앱의 핵심 기능으로 구성하는 것을 권장한다.

## 주요 기능

- BLE 연결 상태 확인
- 사용자 인증 상태 확인
- 스마트폰 근접 여부 판단
- 자동 Unlock
- 자동 Lock
- 수동 Lock
- 수동 Unlock

## 동작 구조

```text
Smartphone 접근
      ↓
BLE 연결 / RSSI
      ↓
ESP32
      ↓
인증 / 근접 정보 전달
      ↓
S32K344
      ↓
차량 조건 판단
      ↓
Door ECU
      ↓
Door Unlock / Lock
```

앱에서는 다음 정보를 표시한다.

```text
Digital Key

Smartphone Connected
Authentication : Valid
Proximity      : Near

[ LOCK ]
[ UNLOCK ]
```

---

# 4. 도어 제어

모바일 앱에서 도어 잠금 및 해제 기능을 제공한다.

## 기능

- Door Lock
- Door Unlock
- Door 상태 표시
- Door Open 상태 표시

## 데이터 흐름

```text
App
 ↓
ESP32
 ↓
DoorUnlockRequest
 ↓
S32K344
 ↓
조건 확인
 ↓
Function Controller
 ↓
Door Lock Actuator
```

앱은 실제 도어 상태를 직접 변경하는 것이 아니라 다음과 같은 **요청(Request)** 을 전달한다.

```text
DoorUnlockRequest = REQUESTED
```

실제 실행 여부는 S32K344 또는 Function Controller가 차량 상태를 확인한 뒤 결정한다.

---

# 5. 파워윈도우 제어

파워윈도우 ECU와 모바일 앱을 연동한다.

## 주요 기능

- Window Open
- Window Close
- Window Stop
- 환기 위치(Ventilation)
- 현재 Window 위치 표시
- Window Fault 표시

## 화면 예시

```text
WINDOW

Driver      80%
Passenger   CLOSED

[ OPEN ]
[ CLOSE ]
[ STOP ]
[ VENT ]
```

초기 구현에서는 복잡한 위치 지정 제어보다 다음 기능 위주로 구현하는 것을 권장한다.

```text
OPEN
CLOSE
STOP
VENT
```

---

# 6. Predictive Climate / 공조

온도·습도 및 탑승자 정보를 기반으로 차량 공조 및 자동환기 기능을 제공한다.

## 앱 표시 정보

- 실내 온도
- 실내 습도
- 공조 동작 상태
- Predictive Climate 동작 상태
- 자동환기 상태

## 앱 제어 기능

- Climate ON/OFF
- Predictive Climate ON/OFF
- Auto Ventilation ON/OFF

## 동작 예시

```text
실내 온도 높음
      +
탑승자 감지
      +
차량 상태 정상
      ↓
S32K344 판단
      ↓
공조 / 환기 Request
      ↓
Function ECU / Window ECU
```

앱은 자동 제어 알고리즘 자체를 수행하기보다 자동 기능의 활성화 여부를 설정하는 역할로 제한하는 것이 좋다.

---

# 7. Ambient Lighting

모바일 앱에서 차량 조명 기능을 제어한다.

## 주요 기능

- Ambient Lighting ON/OFF
- 밝기 조절
- 조명 Mode 선택
- 전조등 상태 표시
- 실내등 상태 표시

## 추천 Mode

- Normal
- Night
- Relax

초기 프로젝트에서는 RGB Color Picker보다 3~4개의 조명 프리셋을 사용하는 것이 구현이 단순하다.

---

# 8. 차량 경고 및 Fault 표시

S32K344가 각 ECU의 상태를 수집하여 모바일 앱에 전달한다.

## 표시 대상

- Central Controller
- Window Controller
- Function Controller
- Sensor Controller
- VSS
- ESP32 통신 상태

## 정상 상태 예시

```text
VEHICLE STATUS

Central Controller     NORMAL
Window Controller      NORMAL
Function Controller    NORMAL
Sensor Controller      NORMAL
VSS                    NORMAL
```

## 통신 오류 예시

```text
⚠ Window Controller

Communication Lost
Last Update : 2.3 sec ago
```

Heartbeat를 이용하면 각 ECU의 통신 상태를 확인할 수 있다.

```text
ECU
 ↓
Heartbeat
 ↓
S32K344
 ↓
ECU Health State
 ↓
ESP32
 ↓
Mobile App
```

---

# 9. 센서 정보 화면

센서 원시 정보는 메인 화면보다 별도의 Sensor 화면에 표시하는 것이 좋다.

## 표시 정보

- 실내 온도
- 습도
- 조도
- 탑승자 감지
- 장애물 거리
- 각 센서 정상/Fault 상태

예시:

```text
SENSOR STATUS

Cabin Temperature   24.3°C
Humidity            51%
Ambient Light       320 lux
Occupant            DETECTED
Obstacle Distance   82 cm
```

ADC Raw Value, CAN ID, RSSI Raw Value 등의 개발용 정보는 일반 사용자 화면이 아닌 Diagnostic 화면에 배치한다.

---

# 10. 모바일 앱 메뉴 구성

앱은 복잡하게 만들기보다 다음과 같이 구성하는 것을 권장한다.

```text
                 MOBILE APP
                     │
      ┌──────────────┼──────────────┐
      │              │              │
     HOME          CONTROL        SYSTEM
      │              │              │
 Vehicle State     Door           ECU Status
 Digital Key       Window         Fault
 Temperature       Climate        Heartbeat
 Warning           Lighting       Communication
                     │
                   SENSOR
                     │
               Temperature
               Humidity
               Light
               Occupant
               Distance
```

추천 Navigation 구성:

```text
HOME | CONTROL | SENSOR | SYSTEM
```

---

# 11. 기능 우선순위

## 1순위 - 필수 구현

- Digital Key 연결 상태
- BLE 기반 근접 감지
- 자동 Lock / Unlock
- 수동 Lock / Unlock
- Door 상태
- Window 상태
- Window Open / Close
- 실내 온습도 표시
- ECU 통신 상태
- Fault / Warning 표시

## 2순위 - 프로젝트 완성도 향상

- Predictive Climate ON/OFF
- 자동 환기
- Ambient Lighting
- 탑승자 감지
- 초음파 장애물 경고
- Find My Car
- VSS 경고음 연동

## 3순위 - 시간이 남을 경우

- 사용자 계정
- 여러 Digital Key 등록
- 차량 제어 History
- Push Notification
- 사용자 설정 저장
- 차량 제어 로그
- 센서 데이터 그래프

---

# 12. 우선 제외 권장 기능

현재 프로젝트 규모에서는 다음 기능은 우선순위를 낮추는 것을 권장한다.

- MP3 / 음악 스트리밍
- 내비게이션
- 영상 재생
- 음성인식
- 복잡한 AI 기능
- RGB 자유 선택 UI
- 클라우드 기반 원격제어

특히 음악 스트리밍은 다음과 같은 별도 실시간 데이터 경로가 필요하다.

```text
Phone
 ↓
ESP32
 ↓ UART
S32K344
 ↓
Audio Codec
 ↓
Speaker
```

차량 관제, Digital Key, 중앙 도메인 제어라는 프로젝트 핵심과 비교하면 개발 난이도 대비 우선순위가 낮다.

---

# 13. 최종 권장 기능 구성

모바일 앱은 다음 6개 핵심 기능을 중심으로 구성한다.

## 1. Digital Key

- 스마트폰 연결
- 사용자 인증
- 근접 상태
- Auto Lock
- Auto Unlock

## 2. Door

- Lock
- Unlock
- Door 상태 확인

## 3. Window

- Open
- Close
- Stop
- Vent
- Window 위치 확인

## 4. Climate

- 실내 온도/습도
- 공조 상태
- Predictive Climate
- Auto Ventilation

## 5. Lighting

- Ambient Lighting
- 실내등
- 전조등
- 조명 Mode

## 6. Vehicle Health

- ECU 상태
- Heartbeat
- Fault
- Warning
- Communication Status

---

# 14. 권장 전체 데이터 흐름

모바일 앱에서 가장 중요한 구조는 다음과 같다.

```text
사용자 입력
   ↓
Mobile App
   ↓
ESP32
   ↓
Request
   ↓
S32K344 Domain Controller
   ↓
조건 및 차량 상태 판단
   ↓
각 ECU
   ↓
실제 기능 실행
   ↓
State / Result
   ↓
S32K344
   ↓
ESP32
   ↓
Mobile App
```

핵심 원칙은 다음과 같다.

> **Mobile App = 사용자 Request 입력 + 차량 State 표시**

> **S32K344 = 차량 상태 판단 + 중앙 제어 + Request 분배**

> **하위 ECU = 실제 기능 실행 + 상태/Fault 반환**

이 구조를 사용하면 모바일 앱이 기존 HMI를 단순히 대체하는 수준을 넘어, **Digital Key와 차량 원격 관제를 담당하는 스마트 차량 인터페이스** 역할을 수행할 수 있다.
