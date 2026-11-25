#include "p6_nvs_dev_test.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "p6_nvs_registration_store.h"

#define P6_NVS_DEV_TEST_NAMESPACE "p6_nvstest"
#define P6_NVS_DEV_TEST_PHASE_KEY "phase"
#define P6_NVS_DEV_TEST_PHASE_REBOOT_PENDING UINT8_C(0xA1)
#define P6_NVS_DEV_TEST_RESTART_DELAY_MS 1000U

static const char *TAG = "p6_nvs_dev_test";

static p6_registration_record_t test_record(void)
{
    static const uint8_t identity[P6_BOND_IDENTITY_BYTES] = {
        0xA2U, 0xB3U, 0xC4U, 0xD5U, 0xE6U, 0xF7U,
    };
    p6_registration_record_t record;

    memset(&record, 0, sizeof(record));
    record.schema_version = P6_REGISTRATION_SCHEMA_VERSION;
    record.state = P6_REGISTRATION_RECORD_RESET_PENDING;
    record.bond_identity.address_type = 1U;
    memcpy(record.bond_identity.bytes, identity, sizeof(identity));
    record.device_context_id = UINT32_C(0x54535431);
    record.registration_epoch = UINT32_C(1);
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

static esp_err_t open_test_store(p6_nvs_registration_store_t *store,
                                 p6_registration_store_t *port)
{
    esp_err_t result = p6_nvs_registration_store_open_namespace(
        store, P6_NVS_DEV_TEST_NAMESPACE);

    if (result == ESP_OK) {
        *port = p6_nvs_registration_store_port(store);
    }
    return result;
}

static esp_err_t verify_loaded_record(
    const p6_registration_store_t *port,
    const p6_registration_record_t *expected)
{
    p6_registration_record_t loaded;

    if (port->load(port->context, &loaded) != P6_STORE_READ_OK ||
        !records_equal(expected, &loaded)) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static esp_err_t cleanup_and_verify(
    p6_nvs_registration_store_t *store,
    p6_registration_store_t *port)
{
    p6_registration_record_t loaded;
    uint8_t phase = 0U;
    esp_err_t result;

    if (port->erase(port->context) != P6_STORE_COMMITTED) {
        return ESP_FAIL;
    }
    result = nvs_erase_key(store->handle, P6_NVS_DEV_TEST_PHASE_KEY);
    if (result != ESP_OK || nvs_commit(store->handle) != ESP_OK) {
        return ESP_FAIL;
    }
    p6_nvs_registration_store_close(store);

    result = open_test_store(store, port);
    if (result != ESP_OK ||
        port->load(port->context, &loaded) != P6_STORE_READ_EMPTY ||
        nvs_get_u8(store->handle, P6_NVS_DEV_TEST_PHASE_KEY, &phase) !=
            ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_INVALID_STATE;
    }
    p6_nvs_registration_store_close(store);
    return ESP_OK;
}

esp_err_t p6_nvs_dev_test_run(void)
{
    p6_nvs_registration_store_t store = {0};
    p6_registration_store_t port;
    p6_registration_record_t expected = test_record();
    p6_registration_record_t loaded;
    uint8_t phase = 0U;
    esp_err_t result;

    ESP_LOGW(TAG, "DEVELOPMENT-ONLY isolated NVS test enabled; production Registration and Bond data are not used");
    result = open_test_store(&store, &port);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "TEST_NAMESPACE_OPEN_FAIL: %s", esp_err_to_name(result));
        return result;
    }

    result = nvs_get_u8(store.handle, P6_NVS_DEV_TEST_PHASE_KEY, &phase);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        if (port.load(port.context, &loaded) != P6_STORE_READ_EMPTY) {
            ESP_LOGE(TAG, "TEST_NAMESPACE_NOT_EMPTY; refusing to overwrite orphaned test data");
            p6_nvs_registration_store_close(&store);
            return ESP_ERR_INVALID_STATE;
        }
        if (port.save(port.context, &expected) != P6_STORE_COMMITTED) {
            ESP_LOGE(TAG, "SAVE_COMMIT_FAIL");
            p6_nvs_registration_store_close(&store);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "SAVE_COMMIT_PASS");
        if (verify_loaded_record(&port, &expected) != ESP_OK) {
            ESP_LOGE(TAG, "IMMEDIATE_READ_FAIL");
            p6_nvs_registration_store_close(&store);
            return ESP_ERR_INVALID_STATE;
        }
        ESP_LOGI(TAG, "IMMEDIATE_READ_PASS");

        p6_nvs_registration_store_close(&store);
        result = open_test_store(&store, &port);
        if (result != ESP_OK ||
            verify_loaded_record(&port, &expected) != ESP_OK) {
            ESP_LOGE(TAG, "HANDLE_REOPEN_FAIL; this is not a reboot result");
            p6_nvs_registration_store_close(&store);
            return ESP_ERR_INVALID_STATE;
        }
        ESP_LOGI(TAG, "HANDLE_REOPEN_PASS_NOT_REBOOT");
        if (nvs_set_u8(store.handle, P6_NVS_DEV_TEST_PHASE_KEY,
                       P6_NVS_DEV_TEST_PHASE_REBOOT_PENDING) != ESP_OK ||
            nvs_commit(store.handle) != ESP_OK) {
            ESP_LOGE(TAG, "REBOOT_PHASE_COMMIT_FAIL");
            p6_nvs_registration_store_close(&store);
            return ESP_FAIL;
        }
        p6_nvs_registration_store_close(&store);
        ESP_LOGW(TAG, "REBOOT_PHASE_COMMITTED; invoking esp_restart for real boot-boundary verification");
        vTaskDelay(pdMS_TO_TICKS(P6_NVS_DEV_TEST_RESTART_DELAY_MS));
        esp_restart();
        return ESP_FAIL;
    }

    if (result != ESP_OK || phase != P6_NVS_DEV_TEST_PHASE_REBOOT_PENDING) {
        ESP_LOGE(TAG, "REBOOT_PHASE_INVALID; refusing cleanup outside the expected test state");
        p6_nvs_registration_store_close(&store);
        return ESP_ERR_INVALID_STATE;
    }
    if (verify_loaded_record(&port, &expected) != ESP_OK) {
        ESP_LOGE(TAG, "REBOOT_RESTORE_FAIL: reset_reason=%d",
                 (int)esp_reset_reason());
        p6_nvs_registration_store_close(&store);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "REBOOT_RESTORE_PASS: reset_reason=%d",
             (int)esp_reset_reason());
    result = cleanup_and_verify(&store, &port);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "TEST_DATA_CLEANUP_FAIL: %s", esp_err_to_name(result));
        p6_nvs_registration_store_close(&store);
        return result;
    }
    ESP_LOGI(TAG, "TEST_DATA_CLEANUP_PASS; production namespaces and bonds were not touched");
    return ESP_OK;
}
