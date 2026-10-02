/**
 * @file Domain_PolicyConfig.h
 * @brief Domain 기능 정책/Calibration 값과 요구 성숙도 관리
 *
 * 목적:
 * - SR/SysRS의 확정값/후보값/잠정값을 Network_Config와 분리한다.
 * - Feature Manager가 숫자 literal을 직접 하드코딩하지 않도록 한다.
 * - 후보값을 "확정값"처럼 승격시키지 않고 maturity를 함께 보존한다.
 *
 * 이 파일에 들어가지 않는 것:
 * - CAN ID / Signal ID / Byte / Bit
 * - UART Message Type / Baud
 * - Network Period / Timeout / CRC / Alive Counter
 */

#ifndef DOMAIN_POLICY_CONFIG_H
#define DOMAIN_POLICY_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DOMAIN_POLICY_MATURITY_CONFIRMED = 0,
    DOMAIN_POLICY_MATURITY_PROJECT_SELECTED,
    DOMAIN_POLICY_MATURITY_CANDIDATE,
    DOMAIN_POLICY_MATURITY_PROVISIONAL,
    DOMAIN_POLICY_MATURITY_TBD
} DomainPolicyMaturity_t;

typedef struct
{
    uint32_t value;
    DomainPolicyMaturity_t maturity;
} DomainPolicyU32_t;

typedef struct
{
    int32_t value;
    DomainPolicyMaturity_t maturity;
} DomainPolicyS32_t;

typedef struct
{
    uint8_t value;
    DomainPolicyMaturity_t maturity;
} DomainPolicyU8_t;

/**
 * @brief 현재 문서에서 Domain이 관리하는 정책/Calibration.
 *
 * 주의:
 * - 단위는 필드명에 명시한다.
 * - CANDIDATE/PROVISIONAL 값은 실기 검증 전 최종 양산값으로 해석하지 않는다.
 * - Feature Manager는 값과 함께 maturity를 진단/시험에서 확인할 수 있어야 한다.
 */
typedef struct
{
    /* Rear obstacle policy - current candidate boundaries */
    DomainPolicyU32_t rearValidMinCm;           /* 10 cm */
    DomainPolicyU32_t rearEmergencyMaxCm;       /* 40 cm inclusive */
    DomainPolicyU32_t rearCautionMaxCm;         /* 70 cm inclusive */
    DomainPolicyU32_t rearValidMaxCm;           /* 100 cm */

    /* Auto ventilation demo policy */
    DomainPolicyS32_t autoVentStartTempDegC;    /* 30 °C */
    DomainPolicyS32_t autoVentStopTempDegC;     /* 28 °C */
    DomainPolicyU32_t autoVentMaxDurationMs;    /* 5 min */
    DomainPolicyU8_t autoVentTargetClosedPct;   /* 80% closed */

    /* Request/result observation deadlines.
     * These are "result wait" deadlines, NOT actuator safety timeouts.
     */
    DomainPolicyU32_t settingResultWaitMs;      /* 2 s */
    DomainPolicyU32_t doorResultWaitMs;         /* 3 s */
    DomainPolicyU32_t climateResultWaitMs;      /* 5 s */
    DomainPolicyU32_t lightApplyResultWaitMs;   /* 1 s */
    DomainPolicyU32_t windowMoveResultWaitMs;   /* 10 s */
    DomainPolicyU32_t windowStopResultWaitMs;   /* 1 s */

    /* One-shot semantic event handling */
    DomainPolicyU32_t eventDuplicateAssistMs;   /* 150 ms auxiliary window */
    DomainPolicyU32_t oneShotEventMaxAgeMs;     /* 2 s provisional validity */

    /* MOBILE visible recent control-result count */
    DomainPolicyU8_t recentControlHistoryCount; /* project selection: 5 */
} DomainPolicyConfig_t;

typedef enum
{
    DOMAIN_POLICY_STATUS_OK = 0,
    DOMAIN_POLICY_STATUS_INVALID_ARGUMENT,
    DOMAIN_POLICY_STATUS_INVALID_CONFIGURATION,
    DOMAIN_POLICY_STATUS_NOT_INITIALIZED
} DomainPolicyStatus_t;

/**
 * @brief 현재 SysRS의 candidate/project-selected 값을 로드한다.
 *
 * 이름 그대로 "document candidates"이며 값의 maturity는 유지된다.
 */
void DomainPolicyConfig_LoadDocumentCandidates(
    DomainPolicyConfig_t *outConfig);

/**
 * @brief Config 관계 검증.
 *
 * 예:
 * min <= emergency <= caution <= max
 * ventilation stop <= start
 * target percentage <= 100
 */
bool DomainPolicyConfig_Validate(
    const DomainPolicyConfig_t *config);

/**
 * @brief Runtime active policy 초기화.
 *
 * config == NULL이면 문서 candidate 기본값을 사용한다.
 */
DomainPolicyStatus_t DomainPolicyConfig_Init(
    const DomainPolicyConfig_t *config);

bool DomainPolicyConfig_IsInitialized(void);

/**
 * @brief 현재 active policy의 읽기 전용 포인터.
 *
 * 초기화 전에는 NULL.
 */
const DomainPolicyConfig_t *DomainPolicyConfig_Get(void);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_POLICY_CONFIG_H */
