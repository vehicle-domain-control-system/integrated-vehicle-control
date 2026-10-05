#include "Gateway_Time.h"

#ifdef ESP_PLATFORM

#include "esp_timer.h"

Gateway_TimeMs_t Gateway_Time_GetMs(void)
{
    const int64_t now_us = esp_timer_get_time();

    if (now_us <= 0)
    {
        return 0U;
    }

    return (Gateway_TimeMs_t)((uint64_t)now_us / 1000ULL);
}

#elif defined(GATEWAY_TIME_HOST_TEST)

/*
 * Host unit-test 전용 fallback.
 *
 * 실제 ESP32 firmware에서 사용하면 안 된다.
 * Host 단위 테스트가 Gateway_Time_ElapsedMs() / HasElapsed() 같은
 * 순수 시간 계산 함수를 검증할 수 있도록만 제공한다.
 */
Gateway_TimeMs_t Gateway_Time_GetMs(void)
{
    return 0U;
}

#else

#error "Gateway_Time.c requires ESP-IDF (ESP_PLATFORM) or GATEWAY_TIME_HOST_TEST."

#endif

Gateway_TimeMs_t Gateway_Time_ElapsedMs(
    Gateway_TimeMs_t start_ms,
    Gateway_TimeMs_t now_ms)
{
    return (Gateway_TimeMs_t)(now_ms - start_ms);
}

bool Gateway_Time_HasElapsed(
    Gateway_TimeMs_t start_ms,
    Gateway_TimeMs_t timeout_ms,
    Gateway_TimeMs_t now_ms)
{
    return Gateway_Time_ElapsedMs(start_ms, now_ms) >= timeout_ms;
}

bool Gateway_Time_HasElapsedNow(
    Gateway_TimeMs_t start_ms,
    Gateway_TimeMs_t timeout_ms)
{
    return Gateway_Time_HasElapsed(
        start_ms,
        timeout_ms,
        Gateway_Time_GetMs());
}
