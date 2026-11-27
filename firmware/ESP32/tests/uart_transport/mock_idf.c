#include "mock_idf.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define MOCK_MAX_QUEUES 8U

typedef struct {
    size_t length;
    size_t item_size;
    size_t count;
    size_t head;
    uint8_t *storage;
    unsigned creation_index;
} mock_queue_t;

typedef struct { EventBits_t bits; } mock_event_group_t;

static mock_queue_t *s_queues[MOCK_MAX_QUEUES];
static unsigned s_queue_creations;
static size_t s_live_queues;
static size_t s_live_event_groups;
static bool s_fail_driver_install;
static bool s_driver_installed;
static QueueHandle_t s_driver_queue;
static unsigned s_driver_delete_count;
static unsigned s_fail_task_call;
static unsigned s_task_calls;

void mock_idf_reset(void)
{
    memset(s_queues, 0, sizeof(s_queues));
    s_queue_creations = 0U;
    s_live_queues = 0U;
    s_live_event_groups = 0U;
    s_fail_driver_install = false;
    s_driver_installed = false;
    s_driver_queue = NULL;
    s_driver_delete_count = 0U;
    s_fail_task_call = 0U;
    s_task_calls = 0U;
}

void mock_idf_fail_driver_install(bool fail) { s_fail_driver_install = fail; }
void mock_idf_fail_task_create_call(unsigned call_number) { s_fail_task_call = call_number; }
size_t mock_idf_live_queue_count(void) { return s_live_queues; }
size_t mock_idf_live_event_group_count(void) { return s_live_event_groups; }
bool mock_idf_driver_is_installed(void) { return s_driver_installed; }
unsigned mock_idf_driver_delete_count(void) { return s_driver_delete_count; }

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t item_size)
{
    mock_queue_t *queue;

    if ((length == 0U) || (item_size == 0U) || (s_queue_creations >= MOCK_MAX_QUEUES)) {
        return NULL;
    }
    queue = (mock_queue_t *)calloc(1U, sizeof(*queue));
    if (queue == NULL) return NULL;
    queue->storage = (uint8_t *)calloc((size_t)length, (size_t)item_size);
    if (queue->storage == NULL) { free(queue); return NULL; }
    queue->length = length;
    queue->item_size = item_size;
    queue->creation_index = s_queue_creations;
    s_queues[s_queue_creations++] = queue;
    s_live_queues++;
    return queue;
}

static BaseType_t queue_send(mock_queue_t *queue, const void *item, bool front)
{
    size_t index;
    if ((queue == NULL) || (item == NULL) || (queue->count == queue->length)) return pdFALSE;
    if (front) {
        queue->head = (queue->head + queue->length - 1U) % queue->length;
        index = queue->head;
    } else {
        index = (queue->head + queue->count) % queue->length;
    }
    memcpy(&queue->storage[index * queue->item_size], item, queue->item_size);
    queue->count++;
    return pdTRUE;
}

BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t wait)
{
    (void)wait;
    return queue_send((mock_queue_t *)queue, item, false);
}

BaseType_t xQueueSendToFront(QueueHandle_t queue, const void *item, TickType_t wait)
{
    (void)wait;
    return queue_send((mock_queue_t *)queue, item, true);
}

BaseType_t xQueueReceive(QueueHandle_t handle, void *item, TickType_t wait)
{
    mock_queue_t *queue = (mock_queue_t *)handle;
    (void)wait;
    if ((queue == NULL) || (item == NULL) || (queue->count == 0U)) return pdFALSE;
    memcpy(item, &queue->storage[queue->head * queue->item_size], queue->item_size);
    queue->head = (queue->head + 1U) % queue->length;
    queue->count--;
    return pdTRUE;
}

BaseType_t xQueueReset(QueueHandle_t handle)
{
    mock_queue_t *queue = (mock_queue_t *)handle;
    if (queue == NULL) return pdFALSE;
    queue->count = 0U;
    queue->head = 0U;
    return pdTRUE;
}

void vQueueDelete(QueueHandle_t handle)
{
    mock_queue_t *queue = (mock_queue_t *)handle;
    if (queue == NULL) return;
    if (queue->creation_index < MOCK_MAX_QUEUES) s_queues[queue->creation_index] = NULL;
    free(queue->storage);
    free(queue);
    s_live_queues--;
}

bool mock_idf_queue_pop(unsigned creation_index, void *item, size_t item_size)
{
    mock_queue_t *queue;
    if ((creation_index >= MOCK_MAX_QUEUES) || (item == NULL)) return false;
    queue = s_queues[creation_index];
    if ((queue == NULL) || (queue->item_size != item_size)) return false;
    return xQueueReceive(queue, item, 0U) == pdTRUE;
}

EventGroupHandle_t xEventGroupCreate(void)
{
    mock_event_group_t *group = (mock_event_group_t *)calloc(1U, sizeof(*group));
    if (group != NULL) s_live_event_groups++;
    return group;
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t handle, EventBits_t bits)
{
    mock_event_group_t *group = (mock_event_group_t *)handle;
    if (group == NULL) return 0U;
    group->bits |= bits;
    return group->bits;
}

EventBits_t xEventGroupWaitBits(EventGroupHandle_t handle, EventBits_t bits,
                                BaseType_t clear, BaseType_t wait_all, TickType_t wait)
{
    mock_event_group_t *group = (mock_event_group_t *)handle;
    (void)wait_all; (void)wait;
    if (group == NULL) return 0U;
    group->bits |= bits;
    EventBits_t result = group->bits;
    if (clear != pdFALSE) group->bits &= ~bits;
    return result;
}

void vEventGroupDelete(EventGroupHandle_t handle)
{
    if (handle != NULL) { free(handle); s_live_event_groups--; }
}

BaseType_t xTaskCreate(TaskFunction_t function, const char *name, uint32_t stack_size,
                       void *argument, UBaseType_t priority, TaskHandle_t *handle)
{
    (void)function; (void)name; (void)stack_size; (void)argument; (void)priority;
    s_task_calls++;
    if ((s_fail_task_call != 0U) && (s_task_calls == s_fail_task_call)) return pdFALSE;
    *handle = (TaskHandle_t)(uintptr_t)s_task_calls;
    return pdPASS;
}

TickType_t xTaskGetTickCount(void) { return 0U; }
void vTaskDelete(TaskHandle_t handle) { (void)handle; }

esp_err_t uart_param_config(uart_port_t port, const uart_config_t *config)
{ (void)port; return (config == NULL) ? ESP_FAIL : ESP_OK; }
esp_err_t uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts)
{ (void)port; (void)rts; (void)cts; return ((tx < 0) || (rx < 0)) ? ESP_FAIL : ESP_OK; }
esp_err_t uart_driver_install(uart_port_t port, int rx_size, int tx_size, int queue_size,
                              QueueHandle_t *queue, int flags)
{
    (void)port; (void)rx_size; (void)tx_size; (void)flags;
    if (s_fail_driver_install) return ESP_FAIL;
    *queue = xQueueCreate((UBaseType_t)queue_size, (UBaseType_t)sizeof(uart_event_t));
    if (*queue == NULL) return ESP_FAIL;
    s_driver_installed = true;
    s_driver_queue = *queue;
    return ESP_OK;
}
esp_err_t uart_driver_delete(uart_port_t port)
{
    (void)port;
    if (s_driver_queue != NULL) {
        vQueueDelete(s_driver_queue);
        s_driver_queue = NULL;
    }
    s_driver_installed = false;
    s_driver_delete_count++;
    return ESP_OK;
}
int uart_read_bytes(uart_port_t port, void *buffer, uint32_t length, TickType_t wait)
{ (void)port; (void)buffer; (void)length; (void)wait; return 0; }
int uart_write_bytes(uart_port_t port, const void *buffer, size_t length)
{ (void)port; (void)buffer; return (int)length; }
esp_err_t uart_wait_tx_done(uart_port_t port, TickType_t wait)
{ (void)port; (void)wait; return ESP_OK; }
esp_err_t uart_flush_input(uart_port_t port) { (void)port; return ESP_OK; }
int64_t esp_timer_get_time(void) { return 1234; }
