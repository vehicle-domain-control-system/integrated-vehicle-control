# ECU Interface 정보 요청 가이드

## 1. 목적

본 문서는 중앙 Domain Controller(S32K344)와 각 ECU 간 Interface를 정의하기 위해,  
각 ECU 담당자가 정리해서 공유해야 할 핵심 정보를 통일된 형식으로 수집하기 위한 가이드이다.

각 ECU 담당자는 아래 **7가지 항목**을 기준으로 자신의 ECU 정보를 정리한다.

> 핵심 항목  
> **State / Request / Event / Fault / Data / Local Logic / Fail-safe**

해당 정보를 기반으로 중앙 Domain 담당자가 각 ECU의 `*_Interface.h` 및 차량 상태 관리 구조를 설계한다.

---

# 2. ECU 담당자가 작성해야 할 7가지 핵심 정보

## 2.1 State

### 의미

ECU가 **현재 어떤 상태에 있는지**를 나타내는 정보이다.

State는 순간적으로 발생하고 사라지는 값이 아니라, 일정 시간 동안 유지되는 현재 상태를 의미한다.

### 예시

- Window
  - `STOPPED`
  - `OPENING`
  - `CLOSING`

- Door
  - `LOCKED`
  - `UNLOCKED`

- Climate
  - `OFF`
  - `ON`
  - `COOLING`

### 작성 요청

자신의 ECU가 가질 수 있는 주요 상태를 모두 정리한다.

---

## 2.2 Request / Command

### 의미

Domain Controller가 해당 ECU에 **어떤 동작을 요청할 수 있는지**를 나타낸다.

Request는 실제 하드웨어 동작 자체가 아니라, 해당 기능을 수행해 달라는 요청이다.

### 예시

- Window
  - `OPEN`
  - `CLOSE`
  - `STOP`
  - `VENT`

- Door
  - `LOCK`
  - `UNLOCK`

- Climate
  - `ON`
  - `OFF`
  - `AUTO`

### 작성 요청

Domain Controller 또는 다른 상위 제어기에서 받을 수 있는 명령/요청 종류를 정리한다.

---

## 2.3 Event

### 의미

ECU 내부에서 **특정 순간에 발생한 사건**을 나타낸다.

State가 현재 상태를 나타낸다면, Event는 “방금 어떤 일이 발생했는가”를 나타낸다.

### 예시

- Window
  - `FULLY_OPENED`
  - `FULLY_CLOSED`
  - `ANTIPINCH_DETECTED`

- Door
  - `LOCK_COMPLETED`
  - `UNLOCK_COMPLETED`
  - `DOOR_OPENED`

### 작성 요청

Domain Controller가 알아야 하는 주요 이벤트를 정리한다.

---

## 2.4 Fault

### 의미

ECU에서 발생할 수 있는 **고장 또는 비정상 상태**를 나타낸다.

정상 동작 상태와 Fault는 분리해서 정의한다.

### 예시

- Window
  - `MOTOR_FAULT`
  - `POSITION_SENSOR_FAULT`

- Door
  - `LOCK_ACTUATOR_FAULT`
  - `DOOR_SENSOR_FAULT`

- Sensor
  - `TEMPERATURE_SENSOR_FAULT`
  - `ULTRASONIC_SENSOR_FAULT`

### 작성 요청

ECU가 감지할 수 있고 Domain Controller에 알려야 하는 Fault 종류를 정리한다.

---

## 2.5 Data

### 의미

State만으로 표현하기 어려운 **실제 측정값 또는 수치 데이터**를 의미한다.

### 예시

- Window Position
  - 값: `0 ~ 100`
  - 단위: `%`

- Cabin Temperature
  - 단위: `°C`

- Humidity
  - 단위: `%`

- Obstacle Distance
  - 단위: `cm`

### 작성 요청

각 Data에 대해 가능하면 아래 내용을 함께 작성한다.

- Data 이름
- 자료형 또는 예상 자료형
- 값의 범위
- 단위
- 값의 의미
- Invalid 값이 필요한 경우 해당 조건

예시:

```text
Window Position

Range : 0 ~ 100
Unit  : %
0     : Fully Open
100   : Fully Closed
```

---

## 2.6 Local Logic

### 의미

Domain Controller의 판단을 기다리지 않고,  
**해당 ECU가 자체적으로 수행해야 하는 판단 및 제어 로직**을 의미한다.

주로 하드웨어에 가까운 즉각적인 제어나 안전 처리가 해당된다.

### 예시

Window ECU:

```text
Anti-Pinch 감지
→ Motor 즉시 정지
→ 필요 시 Reverse
→ Domain에 Event 전달
```

Sensor ECU:

```text
Raw Sensor 값 수집
→ 유효성 확인
→ 물리 단위로 변환
→ Domain에 전달
```

### 작성 요청

해당 ECU 내부에서 자체적으로 처리해야 하는 판단과 제어 동작을 정리한다.

---

## 2.7 Fail-safe

### 의미

통신 오류, 센서 고장, 하드웨어 Fault 등의 비정상 상황이 발생했을 때  
ECU가 **어떤 안전한 상태로 동작할지**를 의미한다.

### 예시

Window ECU:

```text
Domain 통신 Timeout
→ Motor Stop
→ 현재 위치 유지
```

Door ECU:

```text
통신 두절
→ 현재 Lock 상태 유지
→ 임의 Unlock 금지
```

Sensor ECU:

```text
Sensor 측정 실패
→ 이전 값을 정상값처럼 사용하지 않음
→ Invalid/Fault 상태 전달
```

### 작성 요청

다음과 같은 상황을 기준으로 Fail-safe 동작을 정리한다.

- Domain 통신 두절
- Sensor Fault
- Actuator Fault
- 잘못된 Request 수신
- ECU 내부 오류

---

# 3. 작성 형식

각 ECU 담당자는 아래 양식을 복사하여 작성한다.

```text
# [ECU 이름]

## 1. 주요 기능

- 
- 


## 2. State

- 
- 


## 3. Request / Command

- 
- 


## 4. Event

- 
- 


## 5. Fault

- 
- 


## 6. Data

### Data 1

Name :
Type :
Range :
Unit :
Description :

### Data 2

Name :
Type :
Range :
Unit :
Description :


## 7. Local Logic

- 
- 


## 8. Fail-safe

- 
- 
```

---

# 4. 작성 예시 - Window ECU

```text
# Window ECU

## 주요 기능

- Window Open / Close
- Window Stop
- Ventilation Position
- Anti-Pinch


## State

- STOPPED
- OPENING
- CLOSING


## Request / Command

- OPEN
- CLOSE
- STOP
- VENT


## Event

- FULLY_OPENED
- FULLY_CLOSED
- ANTIPINCH_DETECTED


## Fault

- MOTOR_FAULT
- POSITION_SENSOR_FAULT


## Data

Window Position

Range : 0 ~ 100
Unit  : %
0     : Fully Open
100   : Fully Closed


## Local Logic

- OPEN / CLOSE Request에 따라 Motor 제어
- Limit 도달 시 Motor Stop
- Anti-Pinch 감지 시 즉시 Motor Stop
- 필요 시 Reverse 수행
- Anti-Pinch Event를 Domain에 전달


## Fail-safe

- Domain 통신 Timeout 시 Motor Stop
- Position Sensor Fault 시 위치 기반 자동제어 중지
- Motor Fault 발생 시 움직임 관련 Request 거부
```

---

# 5. 역할 구분

## 각 ECU 담당자

각 ECU 담당자는 자신의 ECU에 대해 다음 내용을 정의한다.

- 실제 구현 기능
- 가능한 State
- 받을 수 있는 Request / Command
- 발생 가능한 Event
- 발생 가능한 Fault
- 제공 가능한 Data
- ECU 내부 Local Logic
- Fault 발생 시 Fail-safe

## Domain 담당자

Domain 담당자는 각 ECU 담당자가 전달한 정보를 기반으로 다음 작업을 수행한다.

- ECU 간 명칭 및 표현 규칙 통일
- Request / State / Event / Fault 구조 통일
- 자료형 및 Interface 구조 정리
- `*_Interface.h` 작성
- 차량 전체 상태 관리 구조 설계
- 여러 ECU 간 요청 충돌 및 우선순위 정책 설계

## 통신 담당자

통신 담당자는 확정된 Interface를 실제 CAN/UART 통신 규격으로 매핑한다.

예:

- CAN ID
- Start Bit
- Length
- DLC
- Byte Order
- Cycle Time
- UART Packet 구조
- CRC

---

# 6. 핵심 원칙

```text
State      = 현재 어떤 상태인가
Request    = 무엇을 해달라는가
Event      = 방금 어떤 일이 발생했는가
Fault      = 어떤 문제가 발생했는가
Data       = 실제 값이 얼마인가
Local Logic = ECU가 스스로 무엇을 판단하는가
Fail-safe  = 문제가 생겼을 때 어떻게 안전하게 동작하는가
```

각 ECU 담당자는 위 정보를 먼저 정리하고,  
Domain 담당자는 이를 기반으로 전체 차량 Interface를 통일한다.
