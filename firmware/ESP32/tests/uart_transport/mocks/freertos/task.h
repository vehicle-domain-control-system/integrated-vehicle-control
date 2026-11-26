#ifndef MOCK_TASK_H
#define MOCK_TASK_H

#include "freertos/FreeRTOS.h"

typedef void (*TaskFunction_t)(void *);
BaseType_t xTaskCreate(TaskFunction_t function, const char *name, uint32_t stack_size,
                       void *argument, UBaseType_t priority, TaskHandle_t *handle);
TickType_t xTaskGetTickCount(void);
void vTaskDelete(TaskHandle_t handle);

#endif
