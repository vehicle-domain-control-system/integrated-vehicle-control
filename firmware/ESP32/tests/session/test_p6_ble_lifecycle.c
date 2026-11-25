#include "p6_ble_lifecycle.h"

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
    mock_registration_store_t store;
    mock_entropy_t entropy;
    mock_monotonic_clock_t clock;
    p6_registration_store_t store_port;
    p6_entropy_source_t entropy_port;
    p6_monotonic_clock_t clock_port;
    p6_session_manager_t manager;
    p6_ble_lifecycle_t lifecycle;
    p6_bond_identity_t peer_a;
    p6_bond_identity_t peer_b;
} fixture_t;

static p6_bond_identity_t identity(uint8_t seed)
{
    p6_bond_identity_t value;
    size_t index;

    memset(&value, 0, sizeof(value));
    value.address_type = 1U;
    for (index = 0U; index < sizeof(value.bytes); ++index) {
        value.bytes[index] = (uint8_t)(seed + index);
    }
    return value;
}

static p6_ble_peer_observation_t observation(
    const p6_bond_identity_t *peer,
    bool bond_present,
    bool ota_rpa)
{
    p6_ble_peer_observation_t value;

    memset(&value, 0, sizeof(value));
    value.identity_resolved = true;
    value.ota_address_is_rpa = ota_rpa;
    value.bond_store_present = bond_present;
    value.identity = *peer;
    return value;
}

static bool fixture_init(fixture_t *fixture, bool registered)
{
    p6_session_config_t config = {
        .session_timeout_ms = 30000U,
        .id_generation_attempts = P6_SESSION_MAX_ID_ATTEMPTS,
    };

    memset(fixture, 0, sizeof(*fixture));
    fixture->peer_a = identity(0x10U);
    fixture->peer_b = identity(0x30U);
    mock_registration_store_init(&fixture->store);
    if (registered) {
        fixture->store.has_record = true;
        fixture->store.record.schema_version = P6_REGISTRATION_SCHEMA_VERSION;
        fixture->store.record.state = P6_REGISTRATION_RECORD_ACTIVE;
        fixture->store.record.bond_identity = fixture->peer_a;
        fixture->store.record.device_context_id = 0x12345678U;
        fixture->store.record.registration_epoch = 0x01020304U;
    }
    mock_entropy_init(&fixture->entropy);
    mock_monotonic_clock_init(&fixture->clock, 1000U);
    fixture->store_port = mock_registration_store_port(&fixture->store);
    fixture->entropy_port = mock_entropy_port(&fixture->entropy);
    fixture->clock_port = mock_monotonic_clock_port(&fixture->clock);
    CHECK(p6_session_manager_init(&fixture->manager, &config,
                                  &fixture->store_port,
                                  &fixture->entropy_port,
                                  &fixture->clock_port) == P6_SESSION_OK);
    if (registered) {
        CHECK(p6_session_manager_reconcile_registered_bond(
                  &fixture->manager, true, true) == P6_SESSION_OK);
    }
    CHECK(p6_ble_lifecycle_init(&fixture->lifecycle, &fixture->manager) ==
          P6_BLE_LIFECYCLE_OK);
    return true;
}

static bool authorize_peer_a(fixture_t *fixture, uint32_t generation,
                             bool ota_rpa, uint64_t session_id)
{
    p6_ble_peer_observation_t peer = observation(&fixture->peer_a, true,
                                                 ota_rpa);

    mock_entropy_push_u64(&fixture->entropy, session_id);
    CHECK(p6_ble_lifecycle_on_connect(&fixture->lifecycle, generation,
                                      100U + generation,
                                      &peer) == P6_BLE_LIFECYCLE_OK);
    CHECK(p6_ble_lifecycle_on_security(&fixture->lifecycle, generation,
                                       &peer, true, true, false) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(fixture->manager.authorization_verified);
    return true;
}

static bool test_first_connection_without_registration(void)
{
    fixture_t fixture;
    p6_ble_peer_observation_t peer;

    CHECK(fixture_init(&fixture, false));
    peer = observation(&fixture.peer_a, true, false);
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 1U, 101U, &peer) ==
          P6_BLE_LIFECYCLE_REGISTRATION_UNAVAILABLE);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_REGISTRATION_UNAVAILABLE);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_registered_bond_reconnect(void)
{
    fixture_t fixture;
    p6_ble_peer_observation_t peer;

    CHECK(fixture_init(&fixture, true));
    peer = observation(&fixture.peer_a, true, false);
    mock_entropy_push_u64(&fixture.entropy, 1001U);
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 1U, 101U, &peer) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(fixture.manager.authorization_verified);
    CHECK(p6_ble_lifecycle_on_disconnect(&fixture.lifecycle, 1U) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_encryption_gate(void)
{
    fixture_t fixture;
    p6_ble_peer_observation_t peer;

    CHECK(fixture_init(&fixture, true));
    peer = observation(&fixture.peer_a, true, false);
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 1U, 101U, &peer) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer,
                                       false, true, false) ==
          P6_BLE_LIFECYCLE_NOT_SECURE);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_bond_missing_and_lookup_failure(void)
{
    fixture_t missing;
    fixture_t failed;
    p6_ble_peer_observation_t peer;
    p6_ble_peer_observation_t later_bonded;

    CHECK(fixture_init(&missing, true));
    peer = observation(&missing.peer_a, false, false);
    later_bonded = observation(&missing.peer_a, true, false);
    CHECK(p6_ble_lifecycle_on_connect(&missing.lifecycle, 1U, 101U, &peer) ==
          P6_BLE_LIFECYCLE_BOND_NOT_FOUND);
    CHECK(p6_ble_lifecycle_on_security(&missing.lifecycle, 1U, &later_bonded,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_STALE_EVENT);
    CHECK(!missing.manager.authorization_verified);

    CHECK(fixture_init(&failed, true));
    peer = observation(&failed.peer_a, true, false);
    peer.lookup_error = true;
    CHECK(p6_ble_lifecycle_on_connect(&failed.lifecycle, 1U, 101U, &peer) ==
          P6_BLE_LIFECYCLE_IDENTITY_LOOKUP_ERROR);
    peer.lookup_error = false;
    CHECK(p6_ble_lifecycle_on_security(&failed.lifecycle, 1U, &peer,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_STALE_EVENT);
    CHECK(!failed.manager.authorization_verified);
    return true;
}

static bool test_identity_mismatch_blocks_generation(void)
{
    fixture_t fixture;
    p6_ble_peer_observation_t peer_a;
    p6_ble_peer_observation_t peer_b;

    CHECK(fixture_init(&fixture, true));
    peer_a = observation(&fixture.peer_a, true, true);
    peer_b = observation(&fixture.peer_b, true, true);
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 1U, 101U, &peer_a) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer_b,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_PEER_MISMATCH);
    CHECK(!fixture.manager.authorization_verified);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer_a,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_STALE_EVENT);
    return true;
}

static bool test_late_resolved_mismatch_stays_blocked(void)
{
    fixture_t fixture;
    p6_ble_peer_observation_t unresolved;
    p6_ble_peer_observation_t peer_a;
    p6_ble_peer_observation_t peer_b;

    CHECK(fixture_init(&fixture, true));
    memset(&unresolved, 0, sizeof(unresolved));
    unresolved.ota_address_is_rpa = true;
    peer_a = observation(&fixture.peer_a, true, true);
    peer_b = observation(&fixture.peer_b, true, true);
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 1U, 101U, &unresolved) ==
          P6_BLE_LIFECYCLE_IDENTITY_UNRESOLVED);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer_b,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_PEER_MISMATCH);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer_a,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_STALE_EVENT);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_rpa_changes_keep_stable_identity(void)
{
    fixture_t fixture;

    CHECK(fixture_init(&fixture, true));
    CHECK(authorize_peer_a(&fixture, 1U, true, 1001U));
    CHECK(p6_ble_lifecycle_on_disconnect(&fixture.lifecycle, 1U) ==
          P6_BLE_LIFECYCLE_OK);
    fixture.clock.now_ms = 2000U;
    CHECK(authorize_peer_a(&fixture, 2U, true, 1002U));
    CHECK(fixture.manager.session_id == 1002U);
    return true;
}

static bool test_stale_and_same_generation_events(void)
{
    fixture_t fixture;
    p6_ble_peer_observation_t peer;

    CHECK(fixture_init(&fixture, true));
    peer = observation(&fixture.peer_a, true, false);
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 2U, 102U, &peer) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_STALE_EVENT);
    CHECK(p6_ble_lifecycle_on_disconnect(&fixture.lifecycle, 1U) ==
          P6_BLE_LIFECYCLE_STALE_EVENT);
    CHECK(p6_ble_lifecycle_on_disconnect(&fixture.lifecycle, 2U) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 2U, 102U, &peer) ==
          P6_BLE_LIFECYCLE_STALE_EVENT);
    return true;
}

static bool test_late_security_after_disconnect(void)
{
    fixture_t fixture;
    p6_ble_peer_observation_t peer;

    CHECK(fixture_init(&fixture, true));
    peer = observation(&fixture.peer_a, true, false);
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 1U, 101U, &peer) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(p6_ble_lifecycle_on_disconnect(&fixture.lifecycle, 1U) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_STALE_EVENT);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_identity_resolves_at_security(void)
{
    fixture_t fixture;
    p6_ble_peer_observation_t unresolved;
    p6_ble_peer_observation_t resolved;

    CHECK(fixture_init(&fixture, true));
    memset(&unresolved, 0, sizeof(unresolved));
    unresolved.ota_address_is_rpa = true;
    resolved = observation(&fixture.peer_a, true, true);
    mock_entropy_push_u64(&fixture.entropy, 1001U);
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 1U, 101U, &unresolved) ==
          P6_BLE_LIFECYCLE_IDENTITY_UNRESOLVED);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &resolved,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(fixture.manager.authorization_verified);
    return true;
}

static bool test_app_active_does_not_create_authorization(void)
{
    fixture_t fixture;
    p6_ble_peer_observation_t peer;

    CHECK(fixture_init(&fixture, true));
    peer = observation(&fixture.peer_a, true, false);
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 1U, 101U, &peer) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(p6_ble_lifecycle_on_app_state(&fixture.lifecycle, 1U, 10U, true) ==
          P6_BLE_LIFECYCLE_SESSION_NOT_AUTHORIZED);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

static bool test_mitm_is_separate_fact(void)
{
    fixture_t fixture;
    p6_ble_peer_observation_t peer;
    p6_ble_lifecycle_snapshot_t snapshot;

    CHECK(fixture_init(&fixture, false));
    peer = observation(&fixture.peer_a, true, true);
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 1U, 101U, &peer) ==
          P6_BLE_LIFECYCLE_REGISTRATION_UNAVAILABLE);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer,
                                       true, true, true) ==
          P6_BLE_LIFECYCLE_REGISTRATION_UNAVAILABLE);
    p6_ble_lifecycle_get_snapshot(&fixture.lifecycle, &snapshot);
    CHECK(snapshot.mitm_authenticated);
    CHECK(!snapshot.authorization_verified);
    CHECK(P6_PRODUCTION_BINDING_ENABLED == 0);
    return true;
}

static bool test_duplicate_security_is_idempotent(void)
{
    fixture_t fixture;
    p6_ble_peer_observation_t peer;
    p6_ble_lifecycle_snapshot_t snapshot;
    uint64_t session_id;

    CHECK(fixture_init(&fixture, true));
    peer = observation(&fixture.peer_a, true, true);
    mock_entropy_push_u64(&fixture.entropy, UINT64_C(9001));
    CHECK(p6_ble_lifecycle_on_connect(&fixture.lifecycle, 1U, 101U,
                                      &peer) == P6_BLE_LIFECYCLE_OK);
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_OK);
    session_id = fixture.manager.session_id;
    CHECK(session_id == UINT64_C(9001));
    CHECK(p6_ble_lifecycle_on_security(&fixture.lifecycle, 1U, &peer,
                                       true, true, false) ==
          P6_BLE_LIFECYCLE_OK);
    p6_ble_lifecycle_get_snapshot(&fixture.lifecycle, &snapshot);
    CHECK(fixture.manager.session_id == session_id);
    CHECK(snapshot.authorization_verified);
    CHECK(snapshot.session_start_attempts == 1U);
    CHECK(snapshot.session_authorized == 1U);
    return true;
}

static bool test_authorization_epoch_gate_fails_before_disconnect_delivery(void)
{
    fixture_t fixture;

    CHECK(fixture_init(&fixture, true));
    CHECK(authorize_peer_a(&fixture, 1U, true, UINT64_C(9002)));
    CHECK(p6_ble_lifecycle_authorization_is_current(
        &fixture.lifecycle, true, true, 1U, 101U));
    CHECK(!p6_ble_lifecycle_authorization_is_current(
        &fixture.lifecycle, true, false, 1U, 102U));
    CHECK(!p6_ble_lifecycle_authorization_is_current(
        &fixture.lifecycle, true, true, 2U, 103U));
    CHECK(fixture.manager.authorization_verified);
    CHECK(p6_ble_lifecycle_on_disconnect(&fixture.lifecycle, 1U) ==
          P6_BLE_LIFECYCLE_OK);
    CHECK(!fixture.manager.authorization_verified);
    return true;
}

int main(void)
{
    static const struct {
        const char *name;
        bool (*run)(void);
    } tests[] = {
        {"first_connection_no_registration", test_first_connection_without_registration},
        {"registered_bond_reconnect", test_registered_bond_reconnect},
        {"encryption_gate", test_encryption_gate},
        {"bond_missing_lookup_failure", test_bond_missing_and_lookup_failure},
        {"identity_mismatch", test_identity_mismatch_blocks_generation},
        {"late_resolved_mismatch", test_late_resolved_mismatch_stays_blocked},
        {"rpa_stable_identity", test_rpa_changes_keep_stable_identity},
        {"stale_same_generation", test_stale_and_same_generation_events},
        {"late_security", test_late_security_after_disconnect},
        {"resolve_at_security", test_identity_resolves_at_security},
        {"app_active_no_authority", test_app_active_does_not_create_authorization},
        {"mitm_separate", test_mitm_is_separate_fact},
        {"duplicate_security_idempotent", test_duplicate_security_is_idempotent},
        {"authorization_epoch_gate", test_authorization_epoch_gate_fails_before_disconnect_delivery},
    };
    size_t index;

    for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (!tests[index].run()) {
            printf("FAILED: %s\n", tests[index].name);
            return 1;
        }
    }
    printf("PASS: %zu P6 BLE lifecycle tests\n",
           sizeof(tests) / sizeof(tests[0]));
    return 0;
}
