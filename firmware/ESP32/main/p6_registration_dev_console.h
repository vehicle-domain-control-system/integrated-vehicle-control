#ifndef P6_REGISTRATION_DEV_CONSOLE_H
#define P6_REGISTRATION_DEV_CONSOLE_H

#include "esp_err.h"
#include "gateway_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t p6_registration_dev_console_start(gateway_adapter_t *adapter);

#ifdef __cplusplus
}
#endif

#endif
