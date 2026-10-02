# Domain_Time 설명

## 목적

각 Manager가 직접 FreeRTOS API를 부르면 Domain Logic이 RTOS에 강하게 묶입니다.

잘못된 방향:

```c
DigitalKeyManager.c
xTaskGetTickCount();

WarningManager.c
xTaskGetTickCount();

RequestManager.c
xTaskGetTickCount();
```

권장:

```text
Manager
  ↓
Domain_Time
  ↓
FreeRTOS Tick / HW Timer
```

## 실제 사용

보드에서는 추후:

```c
uint32_t MyDomainTimeProvider(void)
{
    return ...; /* FreeRTOS tick → ms */
}

DomainTime_Init(MyDomainTimeProvider);
```

처럼 연결합니다.

Unit Test에서는 Fake Time을 넣을 수 있습니다.

따라서:

```text
Request Age
Freshness
Result deadline
Auto Ventilation 5분
Warning Hold
Event Age
```

를 실제 기다림 없이 테스트할 수 있습니다.

## 왜 monotonic time인가?

차량 제어의 Timeout은 시계 날짜가 아니라:

```text
얼마나 시간이 지났는가
```

가 중요합니다.

따라서 wall clock보다 단조 증가 시간 기준이 맞습니다.
