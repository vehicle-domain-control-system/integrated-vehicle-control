#include "p6_nvs_registration_store.h"
#include "p6_registration_record_codec.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mock_nvs.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

static p6_registration_record_t active_record(void)
{
    p6_registration_record_t record;
    size_t index;

    memset(&record, 0, sizeof(record));
    record.schema_version = P6_REGISTRATION_SCHEMA_VERSION;
    record.state = P6_REGISTRATION_RECORD_ACTIVE;
    record.bond_identity.address_type = 1U;
    for (index = 0U; index < P6_BOND_IDENTITY_BYTES; ++index) {
        record.bond_identity.bytes[index] = (uint8_t)(0x20U + index);
    }
    record.device_context_id = UINT32_C(0x12345678);
    record.registration_epoch = UINT32_C(0x10203040);
    return record;
}

static bool records_equal(const p6_registration_record_t *left,
                          const p6_registration_record_t *right)
{
    return left->schema_version == right->schema_version &&
           left->state == right->state &&
           p6_bond_identity_equal(&left->bond_identity,
                                  &right->bond_identity) &&
           left->device_context_id == right->device_context_id &&
           left->registration_epoch == right->registration_epoch;
}

static bool test_record_codec_independent_golden_vectors(void)
{
    static const uint8_t golden[P6_REGISTRATION_RECORD_BLOB_SIZE] = {
        0x50U, 0x36U, 0x52U, 0x32U, 0x02U, 0x00U, 0x00U, 0x00U,
        0x01U, 0x01U, 0x20U, 0x21U, 0x22U, 0x23U, 0x24U, 0x25U,
        0x78U, 0x56U, 0x34U, 0x12U, 0x40U, 0x30U, 0x20U, 0x10U,
        0x79U, 0x61U, 0x68U, 0xF7U,
    };
    static const uint8_t bad_schema[P6_REGISTRATION_RECORD_BLOB_SIZE] = {
        0x50U, 0x36U, 0x52U, 0x32U, 0x03U, 0x00U, 0x00U, 0x00U,
        0x01U, 0x01U, 0x20U, 0x21U, 0x22U, 0x23U, 0x24U, 0x25U,
        0x78U, 0x56U, 0x34U, 0x12U, 0x40U, 0x30U, 0x20U, 0x10U,
        0x3FU, 0x5AU, 0x0FU, 0x92U,
    };
    static const uint8_t bad_identity[P6_REGISTRATION_RECORD_BLOB_SIZE] = {
        0x50U, 0x36U, 0x52U, 0x32U, 0x02U, 0x00U, 0x00U, 0x00U,
        0x01U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x78U, 0x56U, 0x34U, 0x12U, 0x40U, 0x30U, 0x20U, 0x10U,
        0x26U, 0x43U, 0x40U, 0x31U,
    };
    static const uint8_t bad_epoch[P6_REGISTRATION_RECORD_BLOB_SIZE] = {
        0x50U, 0x36U, 0x52U, 0x32U, 0x02U, 0x00U, 0x00U, 0x00U,
        0x01U, 0x01U, 0x20U, 0x21U, 0x22U, 0x23U, 0x24U, 0x25U,
        0x78U, 0x56U, 0x34U, 0x12U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x12U, 0xE8U, 0x24U, 0xC0U,
    };
    p6_registration_record_t record = active_record();
    p6_registration_record_t decoded;
    uint8_t encoded[P6_REGISTRATION_RECORD_BLOB_SIZE];
    uint8_t bad_magic[P6_REGISTRATION_RECORD_BLOB_SIZE];

    CHECK(p6_registration_record_encode(&record, encoded));
    CHECK(memcmp(encoded, golden, sizeof(golden)) == 0);
    CHECK(p6_registration_record_decode(golden, sizeof(golden), &decoded));
    CHECK(records_equal(&record, &decoded));
    memcpy(bad_magic, golden, sizeof(bad_magic));
    bad_magic[0] ^= 0x01U;
    CHECK(!p6_registration_record_decode(bad_magic, sizeof(bad_magic),
                                         &decoded));
    CHECK(!p6_registration_record_decode(bad_schema, sizeof(bad_schema),
                                         &decoded));
    CHECK(!p6_registration_record_decode(bad_identity, sizeof(bad_identity),
                                         &decoded));
    CHECK(!p6_registration_record_decode(bad_epoch, sizeof(bad_epoch),
                                         &decoded));
    return true;
}

static bool test_empty_save_load_and_close(void)
{
    p6_nvs_registration_store_t nvs_store = {0};
    p6_registration_store_t port;
    p6_registration_record_t saved = active_record();
    p6_registration_record_t loaded;

    mock_nvs_reset();
    CHECK(p6_nvs_registration_store_open(&nvs_store) == ESP_OK);
    port = p6_nvs_registration_store_port(&nvs_store);
    CHECK(port.load(port.context, &loaded) == P6_STORE_READ_EMPTY);
    CHECK(port.save(port.context, &saved) == P6_STORE_COMMITTED);
    CHECK(port.load(port.context, &loaded) == P6_STORE_READ_OK);
    CHECK(records_equal(&saved, &loaded));
    p6_nvs_registration_store_close(&nvs_store);
    CHECK(!nvs_store.opened);
    return true;
}

static bool test_custom_namespace_isolated_open(void)
{
    p6_nvs_registration_store_t nvs_store = {0};

    mock_nvs_reset();
    CHECK(p6_nvs_registration_store_open_namespace(
              &nvs_store, "p6_nvstest") == ESP_OK);
    CHECK(mock_nvs_state()->open_calls == 1U);
    CHECK(strcmp(mock_nvs_state()->opened_namespace, "p6_nvstest") == 0);
    p6_nvs_registration_store_close(&nvs_store);

    CHECK(p6_nvs_registration_store_open(&nvs_store) == ESP_OK);
    CHECK(mock_nvs_state()->open_calls == 2U);
    CHECK(strcmp(mock_nvs_state()->opened_namespace,
                 P6_NVS_REGISTRATION_NAMESPACE) == 0);
    p6_nvs_registration_store_close(&nvs_store);
    CHECK(p6_nvs_registration_store_open_namespace(
              &nvs_store, NULL) == ESP_ERR_INVALID_ARG);
    return true;
}

static bool test_blob_corruption_and_schema_rejected(void)
{
    p6_nvs_registration_store_t nvs_store = {0};
    p6_registration_store_t port;
    p6_registration_record_t record = active_record();
    p6_registration_record_t loaded;

    mock_nvs_reset();
    CHECK(p6_nvs_registration_store_open(&nvs_store) == ESP_OK);
    port = p6_nvs_registration_store_port(&nvs_store);
    CHECK(port.save(port.context, &record) == P6_STORE_COMMITTED);
    mock_nvs_state()->blob[10] ^= 0x01U;
    CHECK(port.load(port.context, &loaded) == P6_STORE_READ_CORRUPT);
    record.schema_version++;
    CHECK(port.save(port.context, &record) == P6_STORE_NOT_COMMITTED);
    return true;
}

static bool test_write_and_commit_failures_are_unknown(void)
{
    p6_nvs_registration_store_t nvs_store = {0};
    p6_registration_store_t port;
    p6_registration_record_t record = active_record();

    mock_nvs_reset();
    CHECK(p6_nvs_registration_store_open(&nvs_store) == ESP_OK);
    port = p6_nvs_registration_store_port(&nvs_store);
    mock_nvs_state()->set_result = ESP_FAIL;
    CHECK(port.save(port.context, &record) == P6_STORE_COMMIT_UNKNOWN);
    mock_nvs_state()->set_result = ESP_OK;
    mock_nvs_state()->commit_result = ESP_FAIL;
    CHECK(port.save(port.context, &record) == P6_STORE_COMMIT_UNKNOWN);
    CHECK(mock_nvs_state()->has_blob);
    return true;
}

static bool test_reset_marker_survives_reboot_until_explicit_erase(void)
{
    p6_nvs_registration_store_t first = {0};
    p6_nvs_registration_store_t rebooted = {0};
    p6_registration_store_t first_port;
    p6_registration_store_t rebooted_port;
    p6_registration_record_t marker = active_record();
    p6_registration_record_t loaded;

    marker.state = P6_REGISTRATION_RECORD_RESET_PENDING;
    mock_nvs_reset();
    CHECK(p6_nvs_registration_store_open(&first) == ESP_OK);
    first_port = p6_nvs_registration_store_port(&first);
    CHECK(first_port.save(first_port.context, &marker) == P6_STORE_COMMITTED);
    p6_nvs_registration_store_close(&first);

    CHECK(p6_nvs_registration_store_open(&rebooted) == ESP_OK);
    rebooted_port = p6_nvs_registration_store_port(&rebooted);
    CHECK(rebooted_port.load(rebooted_port.context, &loaded) == P6_STORE_READ_OK);
    CHECK(loaded.state == P6_REGISTRATION_RECORD_RESET_PENDING);
    CHECK(rebooted_port.erase(rebooted_port.context) == P6_STORE_COMMITTED);
    CHECK(rebooted_port.load(rebooted_port.context, &loaded) ==
          P6_STORE_READ_EMPTY);
    return true;
}

static bool test_erase_failure_keeps_fail_closed_marker(void)
{
    p6_nvs_registration_store_t nvs_store = {0};
    p6_registration_store_t port;
    p6_registration_record_t marker = active_record();
    p6_registration_record_t loaded;

    marker.state = P6_REGISTRATION_RECORD_RESET_PENDING;
    mock_nvs_reset();
    CHECK(p6_nvs_registration_store_open(&nvs_store) == ESP_OK);
    port = p6_nvs_registration_store_port(&nvs_store);
    CHECK(port.save(port.context, &marker) == P6_STORE_COMMITTED);
    mock_nvs_state()->erase_result = ESP_FAIL;
    CHECK(port.erase(port.context) == P6_STORE_COMMIT_UNKNOWN);
    mock_nvs_state()->erase_result = ESP_OK;
    CHECK(port.load(port.context, &loaded) == P6_STORE_READ_OK);
    CHECK(loaded.state == P6_REGISTRATION_RECORD_RESET_PENDING);
    return true;
}

int main(void)
{
    static const struct {
        const char *name;
        bool (*run)(void);
    } tests[] = {
        {"record_codec_golden_vectors", test_record_codec_independent_golden_vectors},
        {"empty_save_load_close", test_empty_save_load_and_close},
        {"custom_namespace", test_custom_namespace_isolated_open},
        {"corruption_schema", test_blob_corruption_and_schema_rejected},
        {"write_commit_unknown", test_write_and_commit_failures_are_unknown},
        {"reset_marker_reboot", test_reset_marker_survives_reboot_until_explicit_erase},
        {"erase_failure_marker", test_erase_failure_keeps_fail_closed_marker},
    };
    size_t index;

    for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (!tests[index].run()) {
            printf("FAILED: %s\n", tests[index].name);
            return 1;
        }
    }
    printf("PASS: %zu P6 NVS registration store tests\n",
           sizeof(tests) / sizeof(tests[0]));
    return 0;
}
