#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gateway_context_source_metadata.h"

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                    #condition);                                                \
            return false;                                                       \
        }                                                                       \
    } while (0)

static p6_authorization_snapshot_t authorized_snapshot(void)
{
    p6_authorization_snapshot_t snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.state = P6_SESSION_STATE_AUTHORIZED;
    snapshot.reset_state = P6_RESET_STATE_NONE;
    snapshot.registered = true;
    snapshot.registration_bond_reconciled = true;
    snapshot.registration_bond_present = true;
    snapshot.connected = true;
    snapshot.bonded = true;
    snapshot.encrypted = true;
    snapshot.authorization_verified = true;
    snapshot.session_valid = true;
    snapshot.device_context_id = UINT32_C(0x10203040);
    snapshot.session_id = UINT64_C(0x1122334455667788);
    snapshot.connection_generation = 7U;
    return snapshot;
}

static bool test_policy_disabled_is_no_data(void)
{
    gateway_context_source_metadata_t metadata;
    gateway_context_source_policy_t policy = {0};
    gateway_context_sources_t sources;
    p6_authorization_snapshot_t snapshot = authorized_snapshot();

    gateway_context_source_metadata_init(&metadata, &policy);
    CHECK(gateway_context_source_observe_registration_connection(
        &metadata, &snapshot, true, 10U));
    gateway_context_source_build(&metadata, &snapshot, true, 10U, &sources);
    CHECK(!sources.registration_connection.available);
    CHECK(!sources.app.available);
    return true;
}

static bool test_observation_age_expiry_and_rollback(void)
{
    gateway_context_source_metadata_t metadata;
    gateway_context_source_policy_t policy = {
        .registration_connection_ttl_ms = 100U,
        .app_ttl_ms = 50U,
    };
    gateway_context_sources_t sources;
    p6_authorization_snapshot_t snapshot = authorized_snapshot();

    snapshot.app_instance_known = true;
    snapshot.app_instance_id = 9U;
    snapshot.app_active = true;
    gateway_context_source_metadata_init(&metadata, &policy);
    CHECK(gateway_context_source_observe_registration_connection(
        &metadata, &snapshot, true, 1000U));
    CHECK(gateway_context_source_observe_app(
        &metadata, &snapshot, true, 1010U));

    gateway_context_source_build(&metadata, &snapshot, true, 1020U, &sources);
    CHECK(sources.registration_connection.available);
    CHECK(sources.registration_connection.quality == UART_M_QUALITY_OK);
    CHECK(sources.registration_connection.update_sequence == 1U);
    CHECK(sources.registration_connection.age_ms == 20U);
    CHECK(sources.app.available);
    CHECK(sources.app.quality == UART_M_QUALITY_OK);
    CHECK(sources.app.age_ms == 10U);

    gateway_context_source_build(&metadata, &snapshot, true, 1060U, &sources);
    CHECK(sources.app.available);
    CHECK(sources.app.quality == UART_M_QUALITY_STALE);
    CHECK(sources.app.age_ms == 50U);

    gateway_context_source_build(&metadata, &snapshot, true, 999U, &sources);
    CHECK(!sources.registration_connection.available);
    CHECK(!sources.app.available);
    gateway_context_source_build(&metadata, &snapshot, false, 2000U, &sources);
    CHECK(!sources.registration_connection.available);
    return true;
}

static bool test_generation_session_and_app_consistency(void)
{
    gateway_context_source_metadata_t metadata;
    gateway_context_source_policy_t policy = {
        .registration_connection_ttl_ms = 1000U,
        .app_ttl_ms = 1000U,
    };
    gateway_context_sources_t sources;
    p6_authorization_snapshot_t snapshot = authorized_snapshot();

    snapshot.app_instance_known = true;
    snapshot.app_instance_id = 10U;
    gateway_context_source_metadata_init(&metadata, &policy);
    CHECK(gateway_context_source_observe_registration_connection(
        &metadata, &snapshot, true, 1U));
    CHECK(gateway_context_source_observe_app(
        &metadata, &snapshot, true, 2U));

    snapshot.connection_generation++;
    gateway_context_source_reconcile(&metadata, &snapshot);
    gateway_context_source_build(&metadata, &snapshot, true, 3U, &sources);
    CHECK(!sources.registration_connection.available);
    CHECK(!sources.app.available);

    snapshot = authorized_snapshot();
    snapshot.app_instance_known = true;
    snapshot.app_instance_id = 10U;
    CHECK(gateway_context_source_observe_registration_connection(
        &metadata, &snapshot, true, 4U));
    CHECK(gateway_context_source_observe_app(
        &metadata, &snapshot, true, 5U));
    snapshot.session_id++;
    gateway_context_source_reconcile(&metadata, &snapshot);
    gateway_context_source_build(&metadata, &snapshot, true, 6U, &sources);
    CHECK(!sources.registration_connection.available);
    CHECK(!sources.app.available);

    snapshot = authorized_snapshot();
    snapshot.app_instance_known = true;
    snapshot.app_instance_id = 10U;
    CHECK(gateway_context_source_observe_app(
        &metadata, &snapshot, true, 7U));
    snapshot.app_active = true;
    gateway_context_source_reconcile(&metadata, &snapshot);
    gateway_context_source_build(&metadata, &snapshot, true, 8U, &sources);
    CHECK(!sources.app.available);
    return true;
}

static bool test_fail_closed_snapshot_and_sequence_wrap(void)
{
    gateway_context_source_metadata_t metadata;
    gateway_context_source_policy_t policy = {
        .registration_connection_ttl_ms = 1000U,
        .app_ttl_ms = 1000U,
        .registration_connection_sequence_seed = UINT32_MAX - 1U,
        .app_sequence_seed = UINT32_MAX,
    };
    gateway_context_sources_t sources;
    p6_authorization_snapshot_t snapshot = authorized_snapshot();
    p6_authorization_snapshot_t current;

    snapshot.app_instance_known = true;
    snapshot.app_instance_id = 11U;
    gateway_context_source_metadata_init(&metadata, &policy);
    CHECK(gateway_context_source_observe_registration_connection(
        &metadata, &snapshot, true, 1U));
    CHECK(metadata.registration_connection.sequence == UINT32_MAX);
    CHECK(gateway_context_source_observe_registration_connection(
        &metadata, &snapshot, true, 2U));
    CHECK(metadata.registration_connection.sequence == 1U);
    CHECK(gateway_context_source_observe_app(
        &metadata, &snapshot, true, 2U));
    CHECK(metadata.app.sequence == 1U);

    snapshot.authorization_verified = false;
    gateway_context_source_reconcile(&metadata, &snapshot);
    gateway_context_source_build(&metadata, &snapshot, true, 3U, &sources);
    CHECK(!sources.registration_connection.available);
    CHECK(!sources.app.available);
    CHECK(!gateway_context_source_observe_registration_connection(
        &metadata, &snapshot, true, 4U));
    CHECK(!gateway_context_source_observe_app(
        &metadata, &snapshot, true, 4U));
    current = authorized_snapshot();
    CHECK(!gateway_context_source_observe_registration_connection(
        &metadata, &current, false, 4U));
    return true;
}

int main(void)
{
    unsigned passed = 0U;
    const struct { const char *name; bool (*run)(void); } tests[] = {
        {"policy_disabled_no_data", test_policy_disabled_is_no_data},
        {"age_expiry_rollback", test_observation_age_expiry_and_rollback},
        {"generation_session_app_consistency", test_generation_session_and_app_consistency},
        {"fail_closed_wrap", test_fail_closed_snapshot_and_sequence_wrap},
    };
    size_t i;

    for (i = 0U; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (!tests[i].run()) {
            fprintf(stderr, "FAIL: %s\n", tests[i].name);
            return 1;
        }
        ++passed;
    }
    printf("PASS: %u gateway context source metadata tests\n", passed);
    return 0;
}
