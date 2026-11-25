#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gateway_request_admission.h"

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, \
                    #condition);                                                \
            return false;                                                       \
        }                                                                       \
    } while (0)

static gateway_request_candidate_t candidate_at(uint64_t received_at_ms)
{
    gateway_request_candidate_t candidate;

    memset(&candidate, 0, sizeof(candidate));
    candidate.request.context.device_context_id = UINT32_C(0x11223344);
    candidate.request.context.session_id = UINT64_C(0x1020304050607080);
    candidate.request.request_id = UINT32_C(0x55667788);
    candidate.request.kind = UART_M_REQUEST_DOOR;
    candidate.request.argument.door = UART_M_DOOR_OPERATION_LOCK;
    candidate.received_at_known = true;
    candidate.received_at_ms = received_at_ms;
    candidate.connection_handle = 7U;
    candidate.connection_generation = 3U;
    candidate.connection_epoch = 5U;
    candidate.domain_boot_id = UINT32_C(0xAABBCCDD);
    return candidate;
}

static gateway_request_admission_evidence_t evidence_at(uint64_t now_ms)
{
    gateway_request_admission_evidence_t evidence;

    memset(&evidence, 0, sizeof(evidence));
    evidence.now_known = true;
    evidence.now_ms = now_ms;
    evidence.domain_link_up = true;
    evidence.domain_boot_id = UINT32_C(0xAABBCCDD);
    evidence.connection.available = true;
    evidence.connection.connected = true;
    evidence.connection.connection_handle = 7U;
    evidence.connection.connection_generation = 3U;
    evidence.connection.connection_epoch = 5U;
    evidence.authorization.state = P6_SESSION_STATE_AUTHORIZED;
    evidence.authorization.reset_state = P6_RESET_STATE_NONE;
    evidence.authorization.registered = true;
    evidence.authorization.connected = true;
    evidence.authorization.bonded = true;
    evidence.authorization.encrypted = true;
    evidence.authorization.authorization_verified = true;
    evidence.authorization.session_valid = true;
    evidence.authorization.domain_boot_known = true;
    evidence.authorization.registration_bond_reconciled = true;
    evidence.authorization.registration_bond_present = true;
    evidence.authorization.device_context_id = UINT32_C(0x11223344);
    evidence.authorization.session_id = UINT64_C(0x1020304050607080);
    evidence.authorization.connection_generation = 3U;
    evidence.authorization.domain_boot_id = UINT32_C(0xAABBCCDD);
    return evidence;
}

static void init_with_ttl(gateway_request_admission_t *admission,
                          uint32_t ttl_ms)
{
    gateway_request_admission_config_t config = {.ttl_ms = ttl_ms};
    gateway_request_admission_init(admission, &config);
}

static bool test_policy_ttl_and_deep_copy(void)
{
    gateway_request_admission_t admission;
    gateway_request_candidate_t candidate = candidate_at(1000U);
    gateway_request_admission_evidence_t evidence = evidence_at(1099U);
    uart_m_request_t output;

    init_with_ttl(&admission, 0U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_POLICY_UNCONFIGURED);

    init_with_ttl(&admission, 100U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    candidate.request.request_id = 1U;
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    CHECK(output.request_id == UINT32_C(0x55667788));

    candidate = candidate_at(1000U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    evidence.now_ms = 1100U;
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_TTL_EXPIRED);
    CHECK(gateway_request_admission_pending(&admission) == 0U);
    return true;
}

static bool test_session_connection_and_domain_fail_closed(void)
{
    gateway_request_admission_t admission;
    gateway_request_candidate_t candidate = candidate_at(1000U);
    gateway_request_admission_evidence_t evidence = evidence_at(1001U);
    uart_m_request_t output;

    init_with_ttl(&admission, 100U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    evidence.authorization.session_id++;
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_SESSION_CHANGED);

    evidence = evidence_at(1001U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    evidence.connection.connection_generation++;
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_CONNECTION_CHANGED);

    evidence = evidence_at(1001U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    evidence.connection.connection_epoch++;
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_CONNECTION_CHANGED);

    evidence = evidence_at(1001U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    evidence.connection.connection_handle++;
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_CONNECTION_CHANGED);

    evidence = evidence_at(1001U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    evidence.connection.connected = false;
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_CONNECTION_CHANGED);

    evidence = evidence_at(1001U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    evidence.domain_link_up = false;
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_DOMAIN_CHANGED);

    evidence = evidence_at(1001U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    evidence.domain_boot_id++;
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_DOMAIN_CHANGED);

    evidence = evidence_at(1001U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    evidence.authorization.authorization_verified = false;
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_NOT_AUTHORIZED);

    evidence = evidence_at(1001U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    evidence.authorization.device_context_id++;
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_SESSION_CHANGED);
    return true;
}

static bool test_timestamp_duplicate_and_invalidation(void)
{
    gateway_request_admission_t admission;
    gateway_request_candidate_t candidate = candidate_at(1000U);
    gateway_request_admission_evidence_t evidence = evidence_at(999U);
    uart_m_request_t output;

    init_with_ttl(&admission, 100U);
    candidate.received_at_known = false;
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_TIMESTAMP_UNKNOWN);

    candidate = candidate_at(1000U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_DUPLICATE);
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_CLOCK_INVALID);

    candidate = candidate_at(1000U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    gateway_request_admission_invalidate(&admission);
    CHECK(gateway_request_admission_pending(&admission) == 0U);
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_NO_CANDIDATE);
    return true;
}

static bool test_uint32_rollover_is_not_silently_accepted(void)
{
    gateway_request_admission_t admission;
    gateway_request_candidate_t candidate =
        candidate_at((uint64_t)UINT32_MAX - UINT64_C(5));
    gateway_request_admission_evidence_t evidence =
        evidence_at((uint64_t)UINT32_MAX + UINT64_C(3));
    uart_m_request_t output;

    init_with_ttl(&admission, 100U);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_OK);

    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    evidence.now_ms = 3U; /* A truncated/wrapped 32-bit source is rejected. */
    CHECK(gateway_request_admission_prepare(&admission, &evidence, &output) ==
          GATEWAY_REQUEST_ADMISSION_CLOCK_INVALID);
    return true;
}

static bool test_bounded_queue_rejects_overflow(void)
{
    gateway_request_admission_t admission;
    gateway_request_candidate_t candidate;
    uint32_t index;

    init_with_ttl(&admission, 100U);
    for (index = 0U; index < GATEWAY_REQUEST_ADMISSION_CAPACITY; ++index) {
        candidate = candidate_at(1000U + index);
        candidate.request.request_id += index;
        CHECK(gateway_request_admission_capture(&admission, &candidate) ==
              GATEWAY_REQUEST_ADMISSION_OK);
    }
    candidate = candidate_at(2000U);
    candidate.request.request_id = UINT32_C(0xDEADBEEF);
    CHECK(gateway_request_admission_capture(&admission, &candidate) ==
          GATEWAY_REQUEST_ADMISSION_QUEUE_FULL);
    CHECK(gateway_request_admission_pending(&admission) ==
          GATEWAY_REQUEST_ADMISSION_CAPACITY);
    return true;
}

int main(void)
{
    CHECK(test_policy_ttl_and_deep_copy());
    CHECK(test_session_connection_and_domain_fail_closed());
    CHECK(test_timestamp_duplicate_and_invalidation());
    CHECK(test_uint32_rollover_is_not_silently_accepted());
    CHECK(test_bounded_queue_rejects_overflow());
    puts("gateway request admission tests passed");
    return 0;
}
