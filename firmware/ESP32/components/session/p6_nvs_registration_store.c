#include "p6_nvs_registration_store.h"

#include <string.h>

#include "p6_registration_record_codec.h"

static p6_store_read_result_t load_record(
    void *context,
    p6_registration_record_t *out_record)
{
    p6_nvs_registration_store_t *store = context;
    uint8_t blob[P6_REGISTRATION_RECORD_BLOB_SIZE];
    size_t length = sizeof(blob);
    esp_err_t result;

    if (store == NULL || !store->opened || out_record == NULL) {
        return P6_STORE_READ_IO_ERROR;
    }
    memset(out_record, 0, sizeof(*out_record));
    result = nvs_get_blob(store->handle, P6_NVS_REGISTRATION_KEY,
                          blob, &length);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return P6_STORE_READ_EMPTY;
    }
    if (result == ESP_ERR_NVS_INVALID_LENGTH ||
        (result == ESP_OK && length != sizeof(blob))) {
        return P6_STORE_READ_CORRUPT;
    }
    if (result != ESP_OK) {
        return P6_STORE_READ_IO_ERROR;
    }
    return p6_registration_record_decode(blob, length, out_record)
               ? P6_STORE_READ_OK
               : P6_STORE_READ_CORRUPT;
}

static p6_store_commit_result_t save_record(
    void *context,
    const p6_registration_record_t *record)
{
    p6_nvs_registration_store_t *store = context;
    uint8_t blob[P6_REGISTRATION_RECORD_BLOB_SIZE];

    if (store == NULL || !store->opened ||
        !p6_registration_record_encode(record, blob)) {
        return P6_STORE_NOT_COMMITTED;
    }
    if (nvs_set_blob(store->handle, P6_NVS_REGISTRATION_KEY,
                     blob, sizeof(blob)) != ESP_OK) {
        return P6_STORE_COMMIT_UNKNOWN;
    }
    return nvs_commit(store->handle) == ESP_OK
               ? P6_STORE_COMMITTED
               : P6_STORE_COMMIT_UNKNOWN;
}

static p6_store_commit_result_t erase_record(void *context)
{
    p6_nvs_registration_store_t *store = context;
    esp_err_t result;

    if (store == NULL || !store->opened) {
        return P6_STORE_NOT_COMMITTED;
    }
    result = nvs_erase_key(store->handle, P6_NVS_REGISTRATION_KEY);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return P6_STORE_COMMITTED;
    }
    if (result != ESP_OK) {
        return P6_STORE_COMMIT_UNKNOWN;
    }
    return nvs_commit(store->handle) == ESP_OK
               ? P6_STORE_COMMITTED
               : P6_STORE_COMMIT_UNKNOWN;
}

esp_err_t p6_nvs_registration_store_open(
    p6_nvs_registration_store_t *store)
{
    return p6_nvs_registration_store_open_namespace(
        store, P6_NVS_REGISTRATION_NAMESPACE);
}

esp_err_t p6_nvs_registration_store_open_namespace(
    p6_nvs_registration_store_t *store,
    const char *namespace_name)
{
    esp_err_t result;

    if (store == NULL || store->opened || namespace_name == NULL ||
        namespace_name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    memset(store, 0, sizeof(*store));
    result = nvs_open(namespace_name, NVS_READWRITE, &store->handle);
    if (result == ESP_OK) {
        store->opened = true;
    }
    return result;
}

void p6_nvs_registration_store_close(
    p6_nvs_registration_store_t *store)
{
    if (store != NULL && store->opened) {
        nvs_close(store->handle);
        memset(store, 0, sizeof(*store));
    }
}

p6_registration_store_t p6_nvs_registration_store_port(
    p6_nvs_registration_store_t *store)
{
    p6_registration_store_t port = {
        .load = load_record,
        .save = save_record,
        .erase = erase_record,
        .context = store,
    };
    return port;
}
