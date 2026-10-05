/**
 * @file Domain_Time.h
 * @brief Domain Logic이 OS/FreeRTOS에 직접 종속되지 않도록 하는 monotonic time Port
 *
 * Manager 내부에서 xTaskGetTickCount() 같은 RTOS API를 직접 호출하지 않고
 * 이 모듈을 통해 현재 ms 시간을 얻는다.
 */

#ifndef DOMAIN_TIME_H
#define DOMAIN_TIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*DomainTimeProvider_t)(void);

typedef enum
{
    DOMAIN_TIME_STATUS_OK = 0,
    DOMAIN_TIME_STATUS_INVALID_ARGUMENT,
    DOMAIN_TIME_STATUS_NOT_INITIALIZED
} DomainTimeStatus_t;

/**
 * @brief monotonic millisecond provider 등록.
 *
 * 실제 S32K344에서는 FreeRTOS tick 또는 검증된 monotonic timer wrapper를 연결한다.
 * Unit Test에서는 fake provider를 연결할 수 있다.
 */
DomainTimeStatus_t DomainTime_Init(
    DomainTimeProvider_t provider);

bool DomainTime_IsInitialized(void);

/**
 * @brief 현재 monotonic time(ms) 획득.
 */
DomainTimeStatus_t DomainTime_GetNowMs(
    uint32_t *outNowMs);

/**
 * @brief uint32 wrap-around를 고려한 경과 시간 계산.
 */
uint32_t DomainTime_ElapsedMs(
    uint32_t nowMs,
    uint32_t sinceMs);

/**
 * @brief duration이 경과했는지 확인.
 */
bool DomainTime_HasElapsed(
    uint32_t nowMs,
    uint32_t sinceMs,
    uint32_t durationMs);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_TIME_H */
