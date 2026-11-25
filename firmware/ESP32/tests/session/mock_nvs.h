#ifndef MOCK_NVS_BACKEND_H
#define MOCK_NVS_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define MOCK_NVS_MAX_BLOB 64U
#define MOCK_NVS_MAX_NAMESPACE 16U

typedef struct
{
    bool has_blob;
    uint8_t blob[MOCK_NVS_MAX_BLOB];
    size_t blob_length;
    esp_err_t open_result;
    esp_err_t get_result;
    esp_err_t set_result;
    esp_err_t erase_result;
    esp_err_t commit_result;
    unsigned commit_calls;
    unsigned open_calls;
    char opened_namespace[MOCK_NVS_MAX_NAMESPACE];
} mock_nvs_state_t;

void mock_nvs_reset(void);
mock_nvs_state_t *mock_nvs_state(void);

#endif
