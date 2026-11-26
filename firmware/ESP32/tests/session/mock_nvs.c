#include "mock_nvs.h"

#include <string.h>

#include "nvs.h"

static mock_nvs_state_t state;

void mock_nvs_reset(void)
{
    memset(&state, 0, sizeof(state));
    state.open_result = ESP_OK;
    state.get_result = ESP_OK;
    state.set_result = ESP_OK;
    state.erase_result = ESP_OK;
    state.commit_result = ESP_OK;
}

mock_nvs_state_t *mock_nvs_state(void)
{
    return &state;
}

esp_err_t nvs_open(const char *namespace_name, nvs_open_mode_t open_mode,
                   nvs_handle_t *out_handle)
{
    (void)open_mode;
    state.open_calls++;
    if (namespace_name == NULL ||
        strlen(namespace_name) >= sizeof(state.opened_namespace)) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(state.opened_namespace, namespace_name,
           strlen(namespace_name) + 1U);
    if (state.open_result == ESP_OK && out_handle != NULL) {
        *out_handle = 1U;
    }
    return state.open_result;
}

void nvs_close(nvs_handle_t handle)
{
    (void)handle;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key,
                       void *out_value, size_t *length)
{
    (void)handle;
    (void)key;
    if (state.get_result != ESP_OK) {
        return state.get_result;
    }
    if (!state.has_blob) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if (length == NULL || *length < state.blob_length) {
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    memcpy(out_value, state.blob, state.blob_length);
    *length = state.blob_length;
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key,
                       const void *value, size_t length)
{
    (void)handle;
    (void)key;
    if (state.set_result != ESP_OK) {
        return state.set_result;
    }
    if (value == NULL || length > sizeof(state.blob)) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(state.blob, value, length);
    state.blob_length = length;
    state.has_blob = true;
    return ESP_OK;
}

esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key)
{
    (void)handle;
    (void)key;
    if (state.erase_result != ESP_OK) {
        return state.erase_result;
    }
    if (!state.has_blob) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    state.has_blob = false;
    state.blob_length = 0U;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void)handle;
    state.commit_calls++;
    return state.commit_result;
}
