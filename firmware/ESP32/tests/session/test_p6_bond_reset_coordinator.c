#include "p6_bond_reset_coordinator.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mock_p6.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

typedef struct
{
    p6_bond_delete_result_t result;
    unsigned calls;
    p6_bond_identity_t last_identity;
} delete_mock_t;

typedef struct
{
    p6_session_manager_t manager;
    mock_registration_store_t store;
    mock_entropy_t entropy;
    mock_monotonic_clock_t clock;
    p6_registration_store_t store_port;
    p6_entropy_source_t entropy_port;
    p6_monotonic_clock_t clock_port;
    p6_session_config_t config;
    p6_bond_identity_t identity;
} fixture_t;

static p6_bond_delete_result_t delete_bond(
    void *context,
    const p6_bond_identity_t *identity)
{
    delete_mock_t *mock = context;

    mock->calls++;
    mock->last_identity = *identity;
    return mock->result;
}

static bool fixture_registered(fixture_t *fixture)
{
    size_t index;

    memset(fixture, 0, sizeof(*fixture));
    fixture->config.session_timeout_ms = 1000U;
    fixture->config.id_generation_attempts = 4U;
    fixture->identity.address_type = 1U;
    for (index = 0U; index < P6_BOND_IDENTITY_BYTES; ++index) {
        fixture->identity.bytes[index] = (uint8_t)(0x40U + index);
    }
    mock_registration_store_init(&fixture->store);
    mock_entropy_init(&fixture->entropy);
    mock_monotonic_clock_init(&fixture->clock, 1000U);
    fixture->store_port = mock_registration_store_port(&fixture->store);
    fixture->entropy_port = mock_entropy_port(&fixture->entropy);
    fixture->clock_port = mock_monotonic_clock_port(&fixture->clock);
    CHECK(p6_session_manager_init(&fixture->manager, &fixture->config,
                                  &fixture->store_port,
                                  &fixture->entropy_port,
                                  &fixture->clock_port) == P6_SESSION_OK);
    mock_entropy_push_u32(&fixture->entropy, UINT32_C(0x11223344));
    mock_entropy_push_u32(&fixture->entropy, UINT32_C(0x55667788));
    CHECK(p6_session_manager_begin_registration(&fixture->manager) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&fixture->manager, &fixture->identity,
                                     1U) == P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture->manager, 1U,
                                              true, true) == P6_SESSION_OK);
    CHECK(p6_session_manager_register_confirmed_bond(&fixture->manager, 1U) ==
          P6_SESSION_OK);
    return true;
}

static bool test_complete_orders_marker_before_bond_delete(void)
{
    fixture_t fixture;
    delete_mock_t mock = {.result = P6_BOND_DELETE_OK};
    p6_bond_reset_port_t port = {.delete_bond = delete_bond,
                                 .context = &mock};

    CHECK(fixture_registered(&fixture));
    CHECK(p6_bond_reset_begin(&fixture.manager, &port) ==
          P6_BOND_RESET_COMPLETE);
    CHECK(mock.calls == 1U);
    CHECK(p6_bond_identity_equal(&mock.last_identity, &fixture.identity));
    CHECK(fixture.store.save_calls == 2U); /* ACTIVE then RESET_PENDING. */
    CHECK(fixture.store.erase_calls == 1U);
    CHECK(!fixture.store.has_record);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_delete_failure_retains_marker_for_reboot_retry(void)
{
    fixture_t fixture;
    p6_session_manager_t rebooted;
    delete_mock_t mock = {.result = P6_BOND_DELETE_FAILED};
    p6_bond_reset_port_t port = {.delete_bond = delete_bond,
                                 .context = &mock};

    CHECK(fixture_registered(&fixture));
    CHECK(p6_bond_reset_begin(&fixture.manager, &port) ==
          P6_BOND_RESET_REQUIRED);
    CHECK(fixture.store.has_record);
    CHECK(fixture.store.record.state == P6_REGISTRATION_RECORD_RESET_PENDING);
    CHECK(fixture.store.erase_calls == 0U);
    CHECK(p6_session_manager_init(&rebooted, &fixture.config,
                                  &fixture.store_port,
                                  &fixture.entropy_port,
                                  &fixture.clock_port) ==
          P6_SESSION_RESET_REQUIRED);
    mock.result = P6_BOND_DELETE_OK;
    CHECK(p6_bond_reset_resume(&rebooted, &port) == P6_BOND_RESET_COMPLETE);
    CHECK(mock.calls == 2U);
    CHECK(!fixture.store.has_record);
    return true;
}

static bool test_unknown_delete_never_erases_registration_marker(void)
{
    fixture_t fixture;
    delete_mock_t mock = {.result = P6_BOND_DELETE_UNKNOWN};
    p6_bond_reset_port_t port = {.delete_bond = delete_bond,
                                 .context = &mock};

    CHECK(fixture_registered(&fixture));
    CHECK(p6_bond_reset_begin(&fixture.manager, &port) ==
          P6_BOND_RESET_REQUIRED);
    CHECK(fixture.store.record.state == P6_REGISTRATION_RECORD_RESET_PENDING);
    CHECK(fixture.store.erase_calls == 0U);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_marker_commit_rejection_skips_bond_delete(void)
{
    fixture_t fixture;
    delete_mock_t mock = {.result = P6_BOND_DELETE_OK};
    p6_bond_reset_port_t port = {.delete_bond = delete_bond,
                                 .context = &mock};

    CHECK(fixture_registered(&fixture));
    fixture.store.save_result = P6_STORE_COMMIT_UNKNOWN;
    fixture.store.apply_unknown_save = false;
    CHECK(p6_bond_reset_begin(&fixture.manager, &port) ==
          P6_BOND_RESET_REJECTED);
    CHECK(mock.calls == 0U);
    CHECK(fixture.store.erase_calls == 0U);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

int main(void)
{
    static const struct {
        const char *name;
        bool (*run)(void);
    } tests[] = {
        {"ordered_complete", test_complete_orders_marker_before_bond_delete},
        {"delete_failure_reboot_retry", test_delete_failure_retains_marker_for_reboot_retry},
        {"delete_unknown", test_unknown_delete_never_erases_registration_marker},
        {"marker_rejection", test_marker_commit_rejection_skips_bond_delete},
    };
    size_t index;

    for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (!tests[index].run()) {
            printf("FAILED: %s\n", tests[index].name);
            return 1;
        }
    }
    printf("PASS: %zu P6 bond reset coordinator tests\n",
           sizeof(tests) / sizeof(tests[0]));
    return 0;
}
