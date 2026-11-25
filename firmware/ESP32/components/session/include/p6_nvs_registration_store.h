#ifndef P6_NVS_REGISTRATION_STORE_H
#define P6_NVS_REGISTRATION_STORE_H

#include <stdbool.h>

#include "esp_err.h"
#include "nvs.h"
#include "p6_registration_store.h"

#ifdef __cplusplus
extern "C" {
#endif

#define P6_NVS_REGISTRATION_NAMESPACE "p6_reg"
#define P6_NVS_REGISTRATION_KEY "record"

typedef struct
{
    nvs_handle_t handle;
    bool opened;
} p6_nvs_registration_store_t;

esp_err_t p6_nvs_registration_store_open(
    p6_nvs_registration_store_t *store);
esp_err_t p6_nvs_registration_store_open_namespace(
    p6_nvs_registration_store_t *store,
    const char *namespace_name);
void p6_nvs_registration_store_close(
    p6_nvs_registration_store_t *store);
p6_registration_store_t p6_nvs_registration_store_port(
    p6_nvs_registration_store_t *store);

#ifdef __cplusplus
}
#endif

#endif
