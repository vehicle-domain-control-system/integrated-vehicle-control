#include "mock_p6.h"
#include "p6_authorization_provider.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(uint32_t) == 4U, "DeviceContextID ABI requires u32");
_Static_assert(sizeof(uint64_t) == 8U, "SessionID ABI requires u64");

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);      \
            return false;                                                       \
        }                                                                       \
    } while (0)

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
    p6_bond_identity_t peer_a;
    p6_bond_identity_t peer_b;
} fixture_t;

static p6_bond_identity_t make_identity(uint8_t seed)
{
    p6_bond_identity_t identity;
    size_t i;

    memset(&identity, 0, sizeof(identity));
    identity.address_type = 2U;
    for (i = 0U; i < P6_BOND_IDENTITY_BYTES; ++i) {
        identity.bytes[i] = (uint8_t)(seed + (uint8_t)i);
    }
    return identity;
}

static p6_session_status_t fixture_init(fixture_t *fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    fixture->peer_a = make_identity(0x10U);
    fixture->peer_b = make_identity(0x40U);
    fixture->config.session_timeout_ms = 1000U;
    fixture->config.id_generation_attempts = 4U;
    mock_registration_store_init(&fixture->store);
    mock_entropy_init(&fixture->entropy);
    mock_monotonic_clock_init(&fixture->clock, 1000U);
    fixture->store_port = mock_registration_store_port(&fixture->store);
    fixture->entropy_port = mock_entropy_port(&fixture->entropy);
    fixture->clock_port = mock_monotonic_clock_port(&fixture->clock);
    return p6_session_manager_init(&fixture->manager, &fixture->config,
                                   &fixture->store_port,
                                   &fixture->entropy_port,
                                   &fixture->clock_port);
}

static bool register_peer_a(fixture_t *fixture, uint32_t device_context_id)
{
    mock_entropy_push_u32(&fixture->entropy, device_context_id);
    mock_entropy_push_u32(&fixture->entropy,
                          device_context_id ^ UINT32_C(0xA5A50000));
    CHECK(p6_session_manager_begin_registration(&fixture->manager) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&fixture->manager, &fixture->peer_a, 1U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture->manager, 1U,
                                              true, true) == P6_SESSION_OK);
    CHECK(p6_session_manager_register_confirmed_bond(&fixture->manager, 1U) ==
          P6_SESSION_OK);
    return true;
}

static bool start_peer_a_session(fixture_t *fixture,
                                 uint32_t generation,
                                 uint64_t session_id)
{
    if (!fixture->manager.connected) {
        CHECK(p6_session_manager_connect(&fixture->manager, &fixture->peer_a,
                                         generation) == P6_SESSION_OK);
        CHECK(p6_session_manager_observe_security(&fixture->manager, generation,
                                                  true, true) == P6_SESSION_OK);
    }
    mock_entropy_push_u64(&fixture->entropy, session_id);
    CHECK(p6_session_manager_start_session(&fixture->manager, generation) ==
          P6_SESSION_OK);
    return true;
}

static bool test_first_registration_and_reconnect(void)
{
    fixture_t fixture;
    p6_authorization_snapshot_t snapshot;
    uint64_t first_session;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 0x10203040U));
    CHECK(fixture.store.has_record);
    CHECK(fixture.store.record.state == P6_REGISTRATION_RECORD_ACTIVE);
    CHECK(start_peer_a_session(&fixture, 1U, 0x1111222233334444ULL));
    CHECK(p6_session_manager_get_snapshot(&fixture.manager, &snapshot) ==
          P6_SESSION_OK);
    CHECK(snapshot.registered && snapshot.connected && snapshot.bonded);
    CHECK(snapshot.encrypted && snapshot.authorization_verified);
    CHECK(snapshot.device_context_id == 0x10203040U);
    first_session = snapshot.session_id;

    CHECK(p6_session_manager_disconnect(&fixture.manager, 1U) == P6_SESSION_OK);
    CHECK(!p6_session_manager_context_is_current(&fixture.manager,
                                                 0x10203040U,
                                                 first_session, 1U));
    CHECK(start_peer_a_session(&fixture, 2U, 0x5555666677778888ULL));
    CHECK(fixture.manager.session_id != first_session);
    CHECK(fixture.manager.device_context_id == 0x10203040U);
    return true;
}

static bool test_single_device_and_peer_mismatch(void)
{
    fixture_t fixture;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 7U));
    CHECK(p6_session_manager_begin_registration(&fixture.manager) ==
          P6_SESSION_ALREADY_REGISTERED);
    CHECK(p6_session_manager_register_confirmed_bond(&fixture.manager, 1U) ==
          P6_SESSION_ALREADY_REGISTERED);
    CHECK(p6_session_manager_disconnect(&fixture.manager, 1U) == P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_b, 2U) ==
          P6_SESSION_PEER_MISMATCH);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_security_gate_and_app_active_not_authorization(void)
{
    fixture_t fixture;
    p6_authorization_snapshot_t snapshot;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(p6_session_manager_observe_app_state(&fixture.manager, 1U, 9U, true) ==
          P6_SESSION_STALE_EVENT);
    CHECK(p6_session_manager_get_snapshot(&fixture.manager, &snapshot) ==
          P6_SESSION_OK);
    CHECK(!snapshot.authorization_verified);

    CHECK(p6_session_manager_begin_registration(&fixture.manager) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a, 1U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture.manager, 1U,
                                              true, false) ==
          P6_SESSION_NOT_SECURE);
    CHECK(p6_session_manager_register_confirmed_bond(&fixture.manager, 1U) ==
          P6_SESSION_NOT_SECURE);
    mock_entropy_push_u32(&fixture.entropy, 8U);
    mock_entropy_push_u32(&fixture.entropy, UINT32_C(0xA5A50008));
    CHECK(p6_session_manager_observe_security(&fixture.manager, 1U,
                                              true, true) == P6_SESSION_OK);
    CHECK(p6_session_manager_register_confirmed_bond(&fixture.manager, 1U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture.manager, 1U,
                                              true, false) ==
          P6_SESSION_NOT_SECURE);
    CHECK(p6_session_manager_start_session(&fixture.manager, 1U) ==
          P6_SESSION_NOT_SECURE);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_device_context_zero_collision_and_entropy_failure(void)
{
    fixture_t fixture;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    fixture.config.id_generation_attempts = 3U;
    fixture.manager.config.id_generation_attempts = 3U;
    CHECK(p6_session_manager_begin_registration(&fixture.manager) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a, 1U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture.manager, 1U,
                                              true, true) == P6_SESSION_OK);
    mock_entropy_push_u32(&fixture.entropy, 0U);
    mock_entropy_push_u32(&fixture.entropy, 0U);
    mock_entropy_push_u32(&fixture.entropy, 0U);
    CHECK(p6_session_manager_register_confirmed_bond(&fixture.manager, 1U) ==
          P6_SESSION_ID_COLLISION);
    CHECK(!fixture.manager.registered);

    fixture.entropy.fail_u32 = true;
    CHECK(p6_session_manager_register_confirmed_bond(&fixture.manager, 1U) ==
          P6_SESSION_ENTROPY_ERROR);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_registration_epoch_zero_collision_is_rejected(void)
{
    fixture_t fixture;
    uint8_t attempt;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(p6_session_manager_begin_registration(&fixture.manager) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a, 1U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture.manager, 1U,
                                              true, true) == P6_SESSION_OK);
    mock_entropy_push_u32(&fixture.entropy, UINT32_C(0x12345678));
    for (attempt = 0U; attempt < fixture.config.id_generation_attempts; ++attempt) {
        mock_entropy_push_u32(&fixture.entropy, 0U);
    }
    CHECK(p6_session_manager_register_confirmed_bond(&fixture.manager, 1U) ==
          P6_SESSION_ID_COLLISION);
    CHECK(!fixture.manager.registered);
    CHECK(!fixture.store.has_record);
    return true;
}

static bool test_session_zero_collision_and_generation_binding(void)
{
    fixture_t fixture;
    uint64_t first_session;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 9U));
    mock_entropy_push_u64(&fixture.entropy, 0U);
    mock_entropy_push_u64(&fixture.entropy, 0U);
    mock_entropy_push_u64(&fixture.entropy, 0U);
    mock_entropy_push_u64(&fixture.entropy, 0U);
    CHECK(p6_session_manager_start_session(&fixture.manager, 1U) ==
          P6_SESSION_ID_COLLISION);
    CHECK(!fixture.manager.authorization_verified);

    CHECK(start_peer_a_session(&fixture, 1U, 0xAA55AA55AA55AA55ULL));
    first_session = fixture.manager.session_id;
    CHECK(p6_session_manager_start_session(&fixture.manager, 1U) ==
          P6_SESSION_INVALID_STATE);
    CHECK(p6_session_manager_disconnect(&fixture.manager, 1U) == P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a, 2U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture.manager, 2U,
                                              true, true) == P6_SESSION_OK);
    mock_entropy_push_u64(&fixture.entropy, first_session);
    mock_entropy_push_u64(&fixture.entropy, 0x123456789ABCDEF0ULL);
    CHECK(p6_session_manager_start_session(&fixture.manager, 2U) == P6_SESSION_OK);
    CHECK(fixture.manager.session_id != first_session);
    CHECK(p6_session_manager_context_is_current(&fixture.manager, 9U,
                                                fixture.manager.session_id, 2U));
    CHECK(!p6_session_manager_context_is_current(&fixture.manager, 9U,
                                                 first_session, 1U));
    return true;
}

static bool test_stale_generation_and_disconnect(void)
{
    fixture_t fixture;
    uint64_t session;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 10U));
    CHECK(p6_session_manager_disconnect(&fixture.manager, 1U) == P6_SESSION_OK);
    CHECK(start_peer_a_session(&fixture, 2U, 22U));
    session = fixture.manager.session_id;
    CHECK(p6_session_manager_disconnect(&fixture.manager, 1U) ==
          P6_SESSION_STALE_EVENT);
    CHECK(p6_session_manager_context_is_current(&fixture.manager, 10U,
                                                session, 2U));
    CHECK(p6_session_manager_disconnect(&fixture.manager, 2U) == P6_SESSION_OK);
    CHECK(!fixture.manager.authorization_verified);
    CHECK(!fixture.manager.session_valid);
    CHECK(fixture.manager.state == P6_SESSION_STATE_SESSION_EXPIRED);
    return true;
}

static bool test_app_instance_rotation(void)
{
    fixture_t fixture;
    uint64_t old_session;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 11U));
    CHECK(start_peer_a_session(&fixture, 1U, 100U));
    old_session = fixture.manager.session_id;
    CHECK(p6_session_manager_observe_app_state(&fixture.manager, 1U, 77U,
                                               false) == P6_SESSION_OK);
    CHECK(fixture.manager.authorization_verified);
    CHECK(!fixture.manager.app_active);
    CHECK(p6_session_manager_observe_app_state(&fixture.manager, 1U, 77U,
                                               true) == P6_SESSION_OK);
    CHECK(fixture.manager.app_active);
    CHECK(p6_session_manager_observe_app_state(&fixture.manager, 1U, 78U,
                                               true) ==
          P6_SESSION_ROTATION_REQUIRED);
    CHECK(!fixture.manager.authorization_verified);
    CHECK(!p6_session_manager_context_is_current(&fixture.manager, 11U,
                                                 old_session, 1U));
    mock_entropy_push_u64(&fixture.entropy, 100U);
    mock_entropy_push_u64(&fixture.entropy, 101U);
    CHECK(p6_session_manager_start_session(&fixture.manager, 1U) == P6_SESSION_OK);
    CHECK(fixture.manager.session_id != old_session);
    return true;
}

static bool test_domain_boot_rotation_and_link_down(void)
{
    fixture_t fixture;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 12U));
    CHECK(start_peer_a_session(&fixture, 1U, 200U));
    CHECK(p6_session_manager_observe_domain_boot(&fixture.manager, 1U, 10U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_domain_boot(&fixture.manager, 1U, 10U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_domain_boot(&fixture.manager, 1U, 11U) ==
          P6_SESSION_ROTATION_REQUIRED);
    CHECK(!fixture.manager.authorization_verified);
    mock_entropy_push_u64(&fixture.entropy, 200U);
    mock_entropy_push_u64(&fixture.entropy, 201U);
    CHECK(p6_session_manager_start_session(&fixture.manager, 1U) == P6_SESSION_OK);
    CHECK(p6_session_manager_domain_link_down(&fixture.manager, 1U) ==
          P6_SESSION_OK);
    CHECK(!fixture.manager.authorization_verified);
    CHECK(!fixture.manager.domain_boot_known);
    return true;
}

static bool test_session_timeout_and_clock_rollback(void)
{
    fixture_t fixture;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 13U));
    CHECK(start_peer_a_session(&fixture, 1U, 300U));
    fixture.clock.now_ms = 1999U;
    CHECK(p6_session_manager_tick(&fixture.manager) == P6_SESSION_OK);
    CHECK(fixture.manager.authorization_verified);
    fixture.clock.now_ms = 2000U;
    CHECK(p6_session_manager_tick(&fixture.manager) == P6_SESSION_EXPIRED);
    CHECK(!fixture.manager.authorization_verified);

    mock_entropy_push_u64(&fixture.entropy, 300U);
    mock_entropy_push_u64(&fixture.entropy, 301U);
    CHECK(p6_session_manager_start_session(&fixture.manager, 1U) == P6_SESSION_OK);
    fixture.clock.now_ms = 1999U;
    CHECK(p6_session_manager_tick(&fixture.manager) == P6_SESSION_CLOCK_ERROR);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_64bit_clock_wrap_is_fail_closed(void)
{
    fixture_t fixture;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    fixture.clock.now_ms = UINT64_MAX - 100U;
    fixture.manager.last_clock_ms = fixture.clock.now_ms;
    CHECK(register_peer_a(&fixture, 14U));
    CHECK(start_peer_a_session(&fixture, 1U, 400U));
    fixture.clock.now_ms = UINT64_MAX - 50U;
    CHECK(p6_session_manager_tick(&fixture.manager) == P6_SESSION_OK);
    fixture.clock.now_ms = 10U;
    CHECK(p6_session_manager_tick(&fixture.manager) == P6_SESSION_CLOCK_ERROR);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_reboot_restores_registration_only(void)
{
    fixture_t fixture;
    p6_session_manager_t rebooted;
    uint32_t device_context_id;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 15U));
    CHECK(start_peer_a_session(&fixture, 1U, 500U));
    device_context_id = fixture.manager.device_context_id;
    CHECK(p6_session_manager_init(&rebooted, &fixture.config,
                                  &fixture.store_port,
                                  &fixture.entropy_port,
                                  &fixture.clock_port) == P6_SESSION_OK);
    CHECK(rebooted.state == P6_SESSION_STATE_REGISTERED);
    CHECK(rebooted.registered);
    CHECK(rebooted.device_context_id == device_context_id);
    CHECK(!rebooted.connected && !rebooted.session_valid);
    CHECK(!rebooted.authorization_verified && (rebooted.session_id == 0U));
    CHECK(!rebooted.registration_bond_reconciled);
    CHECK(p6_session_manager_connect(&rebooted, &fixture.peer_a, 1U) ==
          P6_SESSION_BOND_RECONCILIATION_REQUIRED);
    CHECK(p6_session_manager_reconcile_registered_bond(
              &rebooted, false, false) ==
          P6_SESSION_BOND_RECONCILIATION_REQUIRED);
    CHECK(rebooted.state == P6_SESSION_STATE_REVOKED);
    CHECK(p6_session_manager_reconcile_registered_bond(
              &rebooted, true, false) == P6_SESSION_BOND_NOT_FOUND);
    CHECK(!rebooted.registration_bond_present);
    CHECK(p6_session_manager_reconcile_registered_bond(
              &rebooted, true, true) == P6_SESSION_OK);
    CHECK(rebooted.registration_bond_reconciled &&
          rebooted.registration_bond_present);
    CHECK(p6_session_manager_connect(&rebooted, &fixture.peer_a, 1U) ==
          P6_SESSION_OK);
    return true;
}

static bool test_reset_preconditions_reject_without_active_registration(void)
{
    fixture_t fixture;
    p6_session_manager_t reset_required;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(p6_session_manager_request_reset(&fixture.manager) ==
          P6_SESSION_INVALID_STATE);
    CHECK(fixture.store.save_calls == 0U);
    CHECK(!fixture.store.has_record);

    CHECK(register_peer_a(&fixture, 30U));
    CHECK(p6_session_manager_request_reset(&fixture.manager) ==
          P6_SESSION_RESET_REQUIRED);
    CHECK(fixture.store.record.state == P6_REGISTRATION_RECORD_RESET_PENDING);
    CHECK(p6_session_manager_init(&reset_required, &fixture.config,
                                  &fixture.store_port,
                                  &fixture.entropy_port,
                                  &fixture.clock_port) ==
          P6_SESSION_RESET_REQUIRED);
    CHECK(p6_session_manager_request_reset(&reset_required) ==
          P6_SESSION_INVALID_STATE);
    CHECK(fixture.store.save_calls == 2U);
    return true;
}

static bool test_bond_reset_success_and_device_id_nonreuse(void)
{
    fixture_t fixture;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 16U));
    CHECK(start_peer_a_session(&fixture, 1U, 600U));
    CHECK(p6_session_manager_reset_registration(&fixture.manager) ==
          P6_SESSION_OK);
    CHECK(!fixture.store.has_record);
    CHECK(fixture.manager.state == P6_SESSION_STATE_UNREGISTERED);
    CHECK(fixture.manager.reset_state == P6_RESET_STATE_COMPLETE);
    CHECK(!fixture.manager.authorization_verified);

    CHECK(p6_session_manager_begin_registration(&fixture.manager) ==
          P6_SESSION_OK);
    CHECK(fixture.manager.reset_state == P6_RESET_STATE_NONE);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_b, 2U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture.manager, 2U,
                                              true, true) == P6_SESSION_OK);
    mock_entropy_push_u32(&fixture.entropy, 16U);
    mock_entropy_push_u32(&fixture.entropy, 17U);
    mock_entropy_push_u32(&fixture.entropy, 18U);
    CHECK(p6_session_manager_register_confirmed_bond(&fixture.manager, 2U) ==
          P6_SESSION_OK);
    CHECK(fixture.manager.device_context_id == 17U);
    return true;
}

static bool test_bond_reset_erase_failure_and_restart(void)
{
    fixture_t fixture;
    p6_session_manager_t rebooted;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 18U));
    CHECK(start_peer_a_session(&fixture, 1U, 700U));
    fixture.store.erase_result = P6_STORE_NOT_COMMITTED;
    CHECK(p6_session_manager_reset_registration(&fixture.manager) ==
          P6_SESSION_RESET_REQUIRED);
    CHECK(fixture.store.record.state == P6_REGISTRATION_RECORD_RESET_PENDING);
    CHECK(fixture.manager.state == P6_SESSION_STATE_RESET_REQUIRED);
    CHECK(fixture.manager.reset_state == P6_RESET_STATE_REQUIRED);
    CHECK(!fixture.manager.authorization_verified);

    CHECK(p6_session_manager_init(&rebooted, &fixture.config,
                                  &fixture.store_port,
                                  &fixture.entropy_port,
                                  &fixture.clock_port) ==
          P6_SESSION_RESET_REQUIRED);
    CHECK(rebooted.state == P6_SESSION_STATE_RESET_REQUIRED);
    CHECK(rebooted.reset_state == P6_RESET_STATE_REQUIRED);
    CHECK(!rebooted.registered && !rebooted.authorization_verified);
    fixture.store.erase_result = P6_STORE_COMMITTED;
    CHECK(p6_session_manager_reset_registration(&rebooted) == P6_SESSION_OK);
    CHECK(!fixture.store.has_record);
    CHECK(rebooted.reset_state == P6_RESET_STATE_COMPLETE);
    return true;
}

static bool test_store_failures_and_corruption(void)
{
    fixture_t fixture;
    p6_session_manager_t manager;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    fixture.store.save_result = P6_STORE_NOT_COMMITTED;
    CHECK(p6_session_manager_begin_registration(&fixture.manager) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a, 1U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture.manager, 1U,
                                              true, true) == P6_SESSION_OK);
    mock_entropy_push_u32(&fixture.entropy, 19U);
    mock_entropy_push_u32(&fixture.entropy, UINT32_C(0xA5A50019));
    CHECK(p6_session_manager_register_confirmed_bond(&fixture.manager, 1U) ==
          P6_SESSION_STORE_NOT_COMMITTED);
    CHECK(!fixture.manager.registered && !fixture.manager.authorization_verified);

    fixture.store.save_result = P6_STORE_COMMITTED;
    fixture.store.corrupt = true;
    CHECK(p6_session_manager_init(&manager, &fixture.config, &fixture.store_port,
                                  &fixture.entropy_port,
                                  &fixture.clock_port) ==
          P6_SESSION_STORE_CORRUPT);
    CHECK(manager.state == P6_SESSION_STATE_REVOKED);
    CHECK(!manager.authorization_verified);
    fixture.store.corrupt = false;
    CHECK(p6_session_manager_reset_registration(&manager) ==
          P6_SESSION_INVALID_STATE);
    CHECK(manager.state == P6_SESSION_STATE_REVOKED);
    CHECK(fixture.store.erase_calls == 0U);
    CHECK(!fixture.store.has_record);

    fixture.store.fail_load = true;
    CHECK(p6_session_manager_init(&manager, &fixture.config, &fixture.store_port,
                                  &fixture.entropy_port,
                                  &fixture.clock_port) ==
          P6_SESSION_STORE_ERROR);
    CHECK(manager.state == P6_SESSION_STATE_REVOKED);
    return true;
}

static bool test_provider_is_read_only_and_not_production_bound(void)
{
    fixture_t fixture;
    p6_authorization_provider_t provider;
    p6_authorization_snapshot_t snapshot;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 20U));
    CHECK(start_peer_a_session(&fixture, 1U, 800U));
    p6_authorization_provider_init(&provider, &fixture.manager);
    CHECK(p6_authorization_provider_read(&provider, &snapshot) == P6_SESSION_OK);
    CHECK(snapshot.authorization_verified && snapshot.session_valid);
    CHECK(p6_authorization_provider_matches(&provider, 20U, 800U, 1U));
    CHECK(!p6_authorization_provider_matches(&provider, 20U, 801U, 1U));
    CHECK(!p6_authorization_provider_is_production_bound());
    CHECK(P6_PRODUCTION_BINDING_ENABLED == 0);
    return true;
}

static bool test_failed_transition_never_elevates_authority(void)
{
    fixture_t fixture;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 21U));
    fixture.entropy.fail_u64 = true;
    CHECK(p6_session_manager_start_session(&fixture.manager, 1U) ==
          P6_SESSION_ENTROPY_ERROR);
    CHECK(!fixture.manager.authorization_verified);
    CHECK(!fixture.manager.session_valid);
    CHECK(fixture.manager.state == P6_SESSION_STATE_SESSION_PENDING);
    return true;
}

static bool test_stale_generation_cannot_reauthorize(void)
{
    fixture_t fixture;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 22U));
    CHECK(start_peer_a_session(&fixture, 1U, 900U));
    CHECK(p6_session_manager_disconnect(&fixture.manager, 1U) == P6_SESSION_OK);
    mock_entropy_push_u64(&fixture.entropy, 901U);
    CHECK(p6_session_manager_start_session(&fixture.manager, 1U) ==
          P6_SESSION_STALE_EVENT);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a, 1U) ==
          P6_SESSION_STALE_EVENT);
    CHECK(!fixture.manager.authorization_verified);

    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a, 2U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture.manager, 1U,
                                              true, true) ==
          P6_SESSION_STALE_EVENT);
    CHECK(!fixture.manager.authorization_verified);
    CHECK(p6_session_manager_observe_security(&fixture.manager, 2U,
                                              true, true) == P6_SESSION_OK);
    mock_entropy_push_u64(&fixture.entropy, 902U);
    CHECK(p6_session_manager_start_session(&fixture.manager, 2U) == P6_SESSION_OK);
    CHECK(p6_session_manager_disconnect(&fixture.manager, 2U) == P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a, 2U) ==
          P6_SESSION_STALE_EVENT);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a, 1U) ==
          P6_SESSION_STALE_EVENT);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a, 3U) ==
          P6_SESSION_OK);
    return true;
}

static bool test_invalid_bond_identities(void)
{
    fixture_t fixture;
    p6_bond_identity_t invalid;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(p6_session_manager_begin_registration(&fixture.manager) ==
          P6_SESSION_OK);
    memset(&invalid, 0, sizeof(invalid));
    CHECK(!p6_bond_identity_is_valid(&invalid));
    CHECK(p6_session_manager_connect(&fixture.manager, &invalid, 1U) ==
          P6_SESSION_INVALID_ARGUMENT);
    memset(&invalid, 0xFF, sizeof(invalid));
    invalid.address_type = 1U;
    CHECK(!p6_bond_identity_is_valid(&invalid));
    invalid = fixture.peer_a;
    invalid.address_type = 4U;
    CHECK(!p6_bond_identity_is_valid(&invalid));
    return true;
}

static bool test_retired_session_reuse_is_rejected(void)
{
    fixture_t fixture;
    uint64_t session_a = UINT64_C(0xA1);
    uint64_t session_b = UINT64_C(0xB2);

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 23U));
    CHECK(start_peer_a_session(&fixture, 1U, session_a));
    CHECK(p6_session_manager_disconnect(&fixture.manager, 1U) == P6_SESSION_OK);
    CHECK(start_peer_a_session(&fixture, 2U, session_b));
    CHECK(p6_session_manager_disconnect(&fixture.manager, 2U) == P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a, 3U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture.manager, 3U,
                                              true, true) == P6_SESSION_OK);
    fixture.manager.config.id_generation_attempts = 3U;
    mock_entropy_push_u64(&fixture.entropy, session_a);
    mock_entropy_push_u64(&fixture.entropy, session_a);
    mock_entropy_push_u64(&fixture.entropy, session_a);
    CHECK(p6_session_manager_start_session(&fixture.manager, 3U) ==
          P6_SESSION_ID_COLLISION);
    CHECK(!fixture.manager.authorization_verified);
    mock_entropy_push_u64(&fixture.entropy, UINT64_C(0xC3));
    CHECK(p6_session_manager_start_session(&fixture.manager, 3U) == P6_SESSION_OK);
    CHECK(fixture.manager.session_id == UINT64_C(0xC3));
    return true;
}

static bool test_retired_session_cache_full_fails_closed(void)
{
    fixture_t fixture;
    uint32_t generation;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 24U));
    for (generation = 1U;
         generation <= P6_RETIRED_SESSION_CAPACITY;
         ++generation) {
        if (generation > 1U) {
            CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a,
                                             generation) == P6_SESSION_OK);
            CHECK(p6_session_manager_observe_security(&fixture.manager,
                                                      generation, true, true) ==
                  P6_SESSION_OK);
        }
        mock_entropy_push_u64(&fixture.entropy,
                              UINT64_C(0x1000) + generation);
        CHECK(p6_session_manager_start_session(&fixture.manager, generation) ==
              P6_SESSION_OK);
        CHECK(p6_session_manager_disconnect(&fixture.manager, generation) ==
              P6_SESSION_OK);
    }
    CHECK(fixture.manager.retired_session_cache_full);
    generation = P6_RETIRED_SESSION_CAPACITY + 1U;
    CHECK(p6_session_manager_connect(&fixture.manager, &fixture.peer_a,
                                     generation) == P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&fixture.manager, generation,
                                              true, true) == P6_SESSION_OK);
    mock_entropy_push_u64(&fixture.entropy, UINT64_C(0x2000));
    CHECK(p6_session_manager_start_session(&fixture.manager, generation) ==
          P6_SESSION_HISTORY_FULL);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_reset_commit_results_and_reboot_limit(void)
{
    fixture_t fixture;
    fixture_t unknown_fixture;
    p6_session_manager_t rebooted;
    p6_session_manager_t unknown_rebooted;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 25U));
    fixture.store.save_result = P6_STORE_NOT_COMMITTED;
    CHECK(p6_session_manager_reset_registration(&fixture.manager) ==
          P6_SESSION_STORE_NOT_COMMITTED);
    CHECK(fixture.manager.state == P6_SESSION_STATE_RESET_REJECTED);
    CHECK(fixture.manager.reset_state == P6_RESET_STATE_REJECTED);
    CHECK(!fixture.manager.authorization_verified && !fixture.manager.registered);
    CHECK(p6_session_manager_init(&rebooted, &fixture.config,
                                  &fixture.store_port, &fixture.entropy_port,
                                  &fixture.clock_port) == P6_SESSION_OK);
    CHECK(rebooted.registered);
    CHECK(!p6_authorization_provider_is_production_bound());

    CHECK(fixture_init(&unknown_fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&unknown_fixture, 28U));
    unknown_fixture.store.save_result = P6_STORE_COMMIT_UNKNOWN;
    unknown_fixture.store.apply_unknown_save = false;
    CHECK(p6_session_manager_reset_registration(&unknown_fixture.manager) ==
          P6_SESSION_STORE_COMMIT_UNKNOWN);
    CHECK(unknown_fixture.manager.reset_state == P6_RESET_STATE_REJECTED);
    CHECK(p6_session_manager_init(&unknown_rebooted, &unknown_fixture.config,
                                  &unknown_fixture.store_port,
                                  &unknown_fixture.entropy_port,
                                  &unknown_fixture.clock_port) == P6_SESSION_OK);
    CHECK(unknown_rebooted.registered);
    CHECK(!p6_authorization_provider_is_production_bound());

    fixture.store.save_result = P6_STORE_COMMIT_UNKNOWN;
    fixture.store.apply_unknown_save = true;
    CHECK(p6_session_manager_reset_registration(&rebooted) ==
          P6_SESSION_STORE_COMMIT_UNKNOWN);
    CHECK(rebooted.reset_state == P6_RESET_STATE_REJECTED);
    CHECK(p6_session_manager_init(&rebooted, &fixture.config,
                                  &fixture.store_port, &fixture.entropy_port,
                                  &fixture.clock_port) ==
          P6_SESSION_RESET_REQUIRED);
    CHECK(rebooted.reset_state == P6_RESET_STATE_REQUIRED);
    CHECK(!rebooted.registered && !rebooted.authorization_verified);
    return true;
}

static bool test_registration_commit_unknown_reboot_limit(void)
{
    fixture_t unapplied;
    fixture_t applied;
    p6_session_manager_t rebooted;

    CHECK(fixture_init(&unapplied) == P6_SESSION_OK);
    mock_entropy_push_u32(&unapplied.entropy, 30U);
    mock_entropy_push_u32(&unapplied.entropy, UINT32_C(0xA5A50030));
    CHECK(p6_session_manager_begin_registration(&unapplied.manager) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&unapplied.manager, &unapplied.peer_a,
                                     1U) == P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&unapplied.manager, 1U,
                                              true, true) == P6_SESSION_OK);
    unapplied.store.save_result = P6_STORE_COMMIT_UNKNOWN;
    unapplied.store.apply_unknown_save = false;
    CHECK(p6_session_manager_register_confirmed_bond(&unapplied.manager, 1U) ==
          P6_SESSION_STORE_COMMIT_UNKNOWN);
    CHECK(!unapplied.manager.registered);
    CHECK(!unapplied.manager.authorization_verified);
    CHECK(unapplied.manager.storage_fault_latched);
    CHECK(p6_session_manager_init(&rebooted, &unapplied.config,
                                  &unapplied.store_port,
                                  &unapplied.entropy_port,
                                  &unapplied.clock_port) == P6_SESSION_OK);
    CHECK(!rebooted.registered && !rebooted.authorization_verified);

    CHECK(fixture_init(&applied) == P6_SESSION_OK);
    mock_entropy_push_u32(&applied.entropy, 31U);
    mock_entropy_push_u32(&applied.entropy, UINT32_C(0xA5A50031));
    CHECK(p6_session_manager_begin_registration(&applied.manager) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&applied.manager, &applied.peer_a, 1U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&applied.manager, 1U,
                                              true, true) == P6_SESSION_OK);
    applied.store.save_result = P6_STORE_COMMIT_UNKNOWN;
    applied.store.apply_unknown_save = true;
    CHECK(p6_session_manager_register_confirmed_bond(&applied.manager, 1U) ==
          P6_SESSION_STORE_COMMIT_UNKNOWN);
    CHECK(!applied.manager.registered);
    CHECK(!applied.manager.authorization_verified);
    CHECK(applied.manager.storage_fault_latched);
    CHECK(p6_session_manager_init(&rebooted, &applied.config,
                                  &applied.store_port,
                                  &applied.entropy_port,
                                  &applied.clock_port) == P6_SESSION_OK);
    CHECK(rebooted.registered);
    CHECK(!rebooted.authorization_verified);
    CHECK(!rebooted.registration_bond_reconciled);
    CHECK(!p6_authorization_provider_is_production_bound());
    return true;
}

static bool test_restored_old_active_record_is_not_detectable(void)
{
    fixture_t fixture;
    p6_session_manager_t rebooted;
    p6_registration_record_t old_record;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 26U));
    old_record = fixture.store.record;
    CHECK(p6_session_manager_reset_registration(&fixture.manager) ==
          P6_SESSION_OK);
    fixture.store.record = old_record;
    fixture.store.has_record = true;
    CHECK(p6_session_manager_init(&rebooted, &fixture.config,
                                  &fixture.store_port, &fixture.entropy_port,
                                  &fixture.clock_port) == P6_SESSION_OK);
    CHECK(rebooted.registered);
    CHECK(!p6_authorization_provider_is_production_bound());
    return true;
}

static bool test_owner_tick_expires_provider_snapshot(void)
{
    fixture_t fixture;
    p6_authorization_provider_t provider;
    p6_authorization_snapshot_t snapshot;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 27U));
    CHECK(start_peer_a_session(&fixture, 1U, 2700U));
    p6_authorization_provider_init(&provider, &fixture.manager);
    fixture.clock.now_ms = 2000U;
    CHECK(p6_session_manager_tick(&fixture.manager) == P6_SESSION_EXPIRED);
    CHECK(p6_authorization_provider_read(&provider, &snapshot) == P6_SESSION_OK);
    CHECK(!snapshot.authorization_verified && !snapshot.session_valid);
    return true;
}

static bool test_reboot_generation_and_session_history_are_open(void)
{
    fixture_t fixture;
    p6_session_manager_t rebooted;

    CHECK(fixture_init(&fixture) == P6_SESSION_OK);
    CHECK(register_peer_a(&fixture, 29U));
    CHECK(start_peer_a_session(&fixture, 1U, 2900U));
    CHECK(p6_session_manager_disconnect(&fixture.manager, 1U) == P6_SESSION_OK);
    CHECK(p6_session_manager_init(&rebooted, &fixture.config,
                                  &fixture.store_port, &fixture.entropy_port,
                                  &fixture.clock_port) == P6_SESSION_OK);
    CHECK(p6_session_manager_reconcile_registered_bond(
              &rebooted, true, true) == P6_SESSION_OK);
    CHECK(p6_session_manager_connect(&rebooted, &fixture.peer_a, 1U) ==
          P6_SESSION_OK);
    CHECK(p6_session_manager_observe_security(&rebooted, 1U, true, true) ==
          P6_SESSION_OK);
    mock_entropy_push_u64(&fixture.entropy, 2900U);
    CHECK(p6_session_manager_start_session(&rebooted, 1U) == P6_SESSION_OK);
    CHECK(rebooted.session_id == 2900U);
    CHECK(!p6_authorization_provider_is_production_bound());
    return true;
}

typedef bool (*test_fn_t)(void);

typedef struct
{
    const char *name;
    test_fn_t function;
} test_case_t;

int main(void)
{
    static const test_case_t tests[] = {
        {"first_registration_and_reconnect", test_first_registration_and_reconnect},
        {"single_device_and_peer_mismatch", test_single_device_and_peer_mismatch},
        {"security_gate_and_app_active", test_security_gate_and_app_active_not_authorization},
        {"device_context_id_validation", test_device_context_zero_collision_and_entropy_failure},
        {"registration_epoch_validation", test_registration_epoch_zero_collision_is_rejected},
        {"session_id_and_generation", test_session_zero_collision_and_generation_binding},
        {"stale_generation_disconnect", test_stale_generation_and_disconnect},
        {"app_instance_rotation", test_app_instance_rotation},
        {"domain_boot_and_link_down", test_domain_boot_rotation_and_link_down},
        {"session_timeout_clock_rollback", test_session_timeout_and_clock_rollback},
        {"clock_wrap_fail_closed", test_64bit_clock_wrap_is_fail_closed},
        {"reboot_registration_only", test_reboot_restores_registration_only},
        {"reset_preconditions", test_reset_preconditions_reject_without_active_registration},
        {"bond_reset_and_id_nonreuse", test_bond_reset_success_and_device_id_nonreuse},
        {"bond_reset_failure_restart", test_bond_reset_erase_failure_and_restart},
        {"store_failures_corruption", test_store_failures_and_corruption},
        {"provider_mock_only", test_provider_is_read_only_and_not_production_bound},
        {"failed_transition_no_elevation", test_failed_transition_never_elevates_authority},
        {"stale_generation_no_reauthorization", test_stale_generation_cannot_reauthorize},
        {"invalid_bond_identities", test_invalid_bond_identities},
        {"retired_session_no_reuse", test_retired_session_reuse_is_rejected},
        {"retired_session_cache_full", test_retired_session_cache_full_fails_closed},
        {"reset_commit_reboot_limit", test_reset_commit_results_and_reboot_limit},
        {"registration_unknown_reboot_limit", test_registration_commit_unknown_reboot_limit},
        {"restored_active_not_detectable", test_restored_old_active_record_is_not_detectable},
        {"owner_tick_provider_expiry", test_owner_tick_expires_provider_snapshot},
        {"reboot_history_boundary_open", test_reboot_generation_and_session_history_are_open},
    };
    size_t passed = 0U;
    size_t i;

    for (i = 0U; i < (sizeof(tests) / sizeof(tests[0])); ++i) {
        if (!tests[i].function()) {
            printf("FAILED: %s\n", tests[i].name);
            return 1;
        }
        passed++;
    }
    printf("PASS: %zu/%zu P6 session tests\n", passed,
           sizeof(tests) / sizeof(tests[0]));
    return 0;
}
