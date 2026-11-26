#ifndef GATEWAY_TIME_H
#define GATEWAY_TIME_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Gateway_Time.h
 *
 * ESP32 Wireless Gateway 공통 시간 추상화 계층.
 *
 * 목적:
 * - Manager가 ESP-IDF 시간 API에 직접 의존하지 않도록 한다.
 * - Timeout / Age / Freshness 계산 규칙을 한곳에 모은다.
 * - uint32_t millisecond counter wrap-around를 안전하게 처리한다.
 *
 * 주의:
 * - 이 시간은 wall-clock(날짜/시각)이 아니라 monotonic elapsed time 용도다.
 * - UART/Bluetooth Wire Format의 timestamp 필드를 정의하지 않는다.
 */

typedef uint32_t Gateway_TimeMs_t;

/*
 * 현재 monotonic time을 millisecond 단위로 반환한다.
 *
 * ESP-IDF 환경:
 *   esp_timer_get_time() 기반으로 구현한다.
 *
 * 반환값은 uint32_t이므로 약 49.7일마다 wrap-around한다.
 * 시간 차이는 Gateway_Time_ElapsedMs()를 사용해서 계산한다.
 */
Gateway_TimeMs_t Gateway_Time_GetMs(void);

/*
 * start_ms부터 now_ms까지의 경과 시간을 반환한다.
 *
 * unsigned subtraction을 사용하여 uint32_t wrap-around 1회를 자연스럽게 처리한다.
 */
Gateway_TimeMs_t Gateway_Time_ElapsedMs(
    Gateway_TimeMs_t start_ms,
    Gateway_TimeMs_t now_ms);

/*
 * start_ms 이후 timeout_ms 이상 경과했는지 확인한다.
 *
 * timeout_ms == 0인 경우 즉시 true다.
 */
bool Gateway_Time_HasElapsed(
    Gateway_TimeMs_t start_ms,
    Gateway_TimeMs_t timeout_ms,
    Gateway_TimeMs_t now_ms);

/*
 * 현재 시각 기준 timeout 확인용 convenience 함수.
 */
bool Gateway_Time_HasElapsedNow(
    Gateway_TimeMs_t start_ms,
    Gateway_TimeMs_t timeout_ms);

#endif /* GATEWAY_TIME_H */
