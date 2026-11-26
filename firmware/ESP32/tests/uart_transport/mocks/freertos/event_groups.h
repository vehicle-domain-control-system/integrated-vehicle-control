#ifndef MOCK_EVENT_GROUPS_H
#define MOCK_EVENT_GROUPS_H

#include "freertos/FreeRTOS.h"

EventGroupHandle_t xEventGroupCreate(void);
EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t group, EventBits_t bits,
                                BaseType_t clear, BaseType_t wait_all, TickType_t wait);
void vEventGroupDelete(EventGroupHandle_t group);

#endif
