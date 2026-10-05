/**
 * @file Domain_Time.c
 */

#include "Domain_Time.h"

static DomainTimeProvider_t g_timeProvider = (DomainTimeProvider_t)0;

DomainTimeStatus_t DomainTime_Init(
    DomainTimeProvider_t provider)
{
    if (provider == (DomainTimeProvider_t)0)
    {
        return DOMAIN_TIME_STATUS_INVALID_ARGUMENT;
    }

    g_timeProvider = provider;
    return DOMAIN_TIME_STATUS_OK;
}

bool DomainTime_IsInitialized(void)
{
    return (g_timeProvider != (DomainTimeProvider_t)0);
}

DomainTimeStatus_t DomainTime_GetNowMs(
    uint32_t *outNowMs)
{
    if (outNowMs == (uint32_t *)0)
    {
        return DOMAIN_TIME_STATUS_INVALID_ARGUMENT;
    }

    if (!DomainTime_IsInitialized())
    {
        return DOMAIN_TIME_STATUS_NOT_INITIALIZED;
    }

    *outNowMs = g_timeProvider();
    return DOMAIN_TIME_STATUS_OK;
}

uint32_t DomainTime_ElapsedMs(
    uint32_t nowMs,
    uint32_t sinceMs)
{
    /*
     * unsigned modulo subtraction은 uint32 wrap-around를 자연스럽게 처리한다.
     * 단, 단일 측정 구간이 uint32 범위의 절반 이상인 장시간 타이머에는
     * 별도 시간 모델을 사용해야 한다.
     */
    return (uint32_t)(nowMs - sinceMs);
}

bool DomainTime_HasElapsed(
    uint32_t nowMs,
    uint32_t sinceMs,
    uint32_t durationMs)
{
    return (DomainTime_ElapsedMs(nowMs, sinceMs) >= durationMs);
}
