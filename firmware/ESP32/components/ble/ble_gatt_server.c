#include "ble_gatt_server.h"

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define BLE_GATT_DEFAULT_QUEUE_DEPTH 12U
#define BLE_GATT_DEFAULT_TASK_STACK 4096U
#define BLE_GATT_DEFAULT_TASK_PRIORITY 5U
#define BLE_GATT_STOPPED_BIT BIT0
#define BLE_GATT_APP_COMMAND_MAX_FRAME (BLE_GATT_FRAGMENT_HEADER_SIZE + BLE_GATT_MAX_UPLINK_PAYLOAD)

typedef enum {
    ATTR_GATEWAY_STATUS = 1,
    ATTR_APP_COMMAND,
    ATTR_VEHICLE_DATA,
    ATTR_APP_STATE,
} attribute_id_t;

typedef struct {
    bool started;
    bool stopping;
    uint8_t own_address_type;
    uint16_t connection_handle;
    uint32_t connection_generation;
    uint16_t mtu;
    bool gateway_status_subscribed;
    bool vehicle_data_subscribed;
    uint16_t next_transfer_id;
    uint8_t gateway_status[BLE_GATT_GATEWAY_STATUS_LENGTH];
    bool app_state_seen;
    bool last_app_active;
    ble_gatt_reassembler_t reassembler;
    ble_gatt_server_config_t config;
    ble_gatt_server_stats_t stats;
    QueueHandle_t event_queue;
    EventGroupHandle_t lifecycle_events;
    TaskHandle_t event_task;
} server_state_t;

static const char *TAG = "ble_gatt";
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static server_state_t state;

static uint16_t gateway_status_handle;
static uint16_t vehicle_data_handle;

static const ble_uuid128_t service_uuid = BLE_UUID128_INIT(
    0x4c, 0x49, 0x42, 0x4f, 0x4d, 0x5f, 0x32, 0x9e,
    0x5a, 0x4b, 0x21, 0x7c, 0x00, 0x00, 0xf0, 0xa0);
static const ble_uuid128_t gateway_status_uuid = BLE_UUID128_INIT(
    0x4c, 0x49, 0x42, 0x4f, 0x4d, 0x5f, 0x32, 0x9e,
    0x5a, 0x4b, 0x21, 0x7c, 0x01, 0x00, 0xf0, 0xa0);
static const ble_uuid128_t app_command_uuid = BLE_UUID128_INIT(
    0x4c, 0x49, 0x42, 0x4f, 0x4d, 0x5f, 0x32, 0x9e,
    0x5a, 0x4b, 0x21, 0x7c, 0x02, 0x00, 0xf0, 0xa0);
static const ble_uuid128_t vehicle_data_uuid = BLE_UUID128_INIT(
    0x4c, 0x49, 0x42, 0x4f, 0x4d, 0x5f, 0x32, 0x9e,
    0x5a, 0x4b, 0x21, 0x7c, 0x03, 0x00, 0xf0, 0xa0);
static const ble_uuid128_t app_state_uuid = BLE_UUID128_INIT(
    0x4c, 0x49, 0x42, 0x4f, 0x4d, 0x5f, 0x32, 0x9e,
    0x5a, 0x4b, 0x21, 0x7c, 0x04, 0x00, 0xf0, 0xa0);

void ble_store_config_init(void);

static int gap_event(struct ble_gap_event *event, void *arg);
static int gatt_access(uint16_t connection_handle,
                       uint16_t attribute_handle,
                       struct ble_gatt_access_ctxt *context,
                       void *arg);

static const struct ble_gatt_svc_def services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &gateway_status_uuid.u,
                .access_cb = gatt_access,
                .arg = (void *)ATTR_GATEWAY_STATUS,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY |
                         BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY_INDICATE_ENC,
                .val_handle = &gateway_status_handle,
            },
            {
                .uuid = &app_command_uuid.u,
                .access_cb = gatt_access,
                .arg = (void *)ATTR_APP_COMMAND,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
            },
            {
                .uuid = &vehicle_data_uuid.u,
                .access_cb = gatt_access,
                .arg = (void *)ATTR_VEHICLE_DATA,
                .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_NOTIFY_INDICATE_ENC,
                .val_handle = &vehicle_data_handle,
            },
            {
                .uuid = &app_state_uuid.u,
                .access_cb = gatt_access,
                .arg = (void *)ATTR_APP_STATE,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
            },
            {0},
        },
    },
    {0},
};

static void increment_stat(uint32_t *counter)
{
    portENTER_CRITICAL(&state_lock);
    (*counter)++;
    portEXIT_CRITICAL(&state_lock);
}

static bool enqueue_event(const ble_gatt_server_event_t *event)
{
    QueueHandle_t queue = state.event_queue;

    if (queue == NULL || xQueueSend(queue, event, 0U) != pdTRUE) {
        increment_stat(&state.stats.event_queue_overflows);
        return false;
    }
    return true;
}

static void application_event_task(void *argument)
{
    ble_gatt_server_event_t event;
    (void)argument;

    while (xQueueReceive(state.event_queue, &event, portMAX_DELAY) == pdTRUE) {
        if (state.stopping) {
            break;
        }
        if (state.config.event_callback != NULL) {
            state.config.event_callback(&event, state.config.event_context);
        }
    }
    xEventGroupSetBits(state.lifecycle_events, BLE_GATT_STOPPED_BIT);
    vTaskDelete(NULL);
}

static bool connection_security(uint16_t connection_handle,
                                bool *encrypted,
                                bool *authenticated,
                                bool *bonded)
{
    struct ble_gap_conn_desc descriptor;

    if (ble_gap_conn_find(connection_handle, &descriptor) != 0) {
        return false;
    }
    *encrypted = descriptor.sec_state.encrypted != 0U;
    *authenticated = descriptor.sec_state.authenticated != 0U;
    *bonded = descriptor.sec_state.bonded != 0U;
    return true;
}

static bool current_connection_generation(uint16_t connection_handle,
                                          uint32_t *generation)
{
    bool current;

    portENTER_CRITICAL(&state_lock);
    current = state.connection_handle == connection_handle;
    if (current && generation != NULL) {
        *generation = state.connection_generation;
    }
    portEXIT_CRITICAL(&state_lock);
    return current;
}

bool ble_gatt_server_is_connected(void)
{
    bool connected;
    portENTER_CRITICAL(&state_lock);
    connected = state.connection_handle != BLE_HS_CONN_HANDLE_NONE;
    portEXIT_CRITICAL(&state_lock);
    return connected;
}

bool ble_gatt_server_is_secure(void)
{
    uint16_t connection_handle;
    bool encrypted = false;
    bool authenticated = false;
    bool bonded = false;

    portENTER_CRITICAL(&state_lock);
    connection_handle = state.connection_handle;
    portEXIT_CRITICAL(&state_lock);
    if (connection_handle == BLE_HS_CONN_HANDLE_NONE) {
        return false;
    }
    return connection_security(connection_handle, &encrypted, &authenticated, &bonded) &&
           encrypted && bonded;
}

static int start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    struct ble_hs_adv_fields scan_response = {0};
    struct ble_gap_adv_params parameters = {0};
    int result;

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&service_uuid;
    fields.num_uuids128 = 1U;
    fields.uuids128_is_complete = 1U;
    result = ble_gap_adv_set_fields(&fields);
    if (result != 0) {
        return result;
    }

    scan_response.name = (uint8_t *)BLE_GATT_DEVICE_NAME;
    scan_response.name_len = (uint8_t)strlen(BLE_GATT_DEVICE_NAME);
    scan_response.name_is_complete = 1U;
    result = ble_gap_adv_rsp_set_fields(&scan_response);
    if (result != 0) {
        return result;
    }

    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    return ble_gap_adv_start(state.own_address_type, NULL, BLE_HS_FOREVER,
                             &parameters, gap_event, NULL);
}

static void host_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE host reset; reason=%d", reason);
}

static void host_sync(void)
{
    int result = ble_hs_util_ensure_addr(0);
    if (result == 0) {
        result = ble_hs_id_infer_auto(0, &state.own_address_type);
    }
    if (result == 0) {
        result = start_advertising();
    }
    if (result != 0) {
        ESP_LOGE(TAG, "Advertising start failed; rc=%d", result);
    } else {
        ESP_LOGI(TAG, "Advertising %s with Vehicle Gateway Service UUID", BLE_GATT_DEVICE_NAME);
    }
}

static void host_task(void *argument)
{
    (void)argument;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void clear_connection(uint16_t connection_handle, uint16_t reason)
{
    ble_gatt_server_event_t event = {0};

    portENTER_CRITICAL(&state_lock);
    if (state.connection_handle != connection_handle) {
        portEXIT_CRITICAL(&state_lock);
        ESP_LOGW(TAG, "Ignoring disconnect for stale connection=%u", connection_handle);
        return;
    }
    event.type = BLE_GATT_EVENT_DISCONNECTED;
    event.connection_handle = state.connection_handle;
    event.connection_generation = state.connection_generation;
    event.data.disconnected.reason = reason;
    state.connection_handle = BLE_HS_CONN_HANDLE_NONE;
    state.mtu = BLE_ATT_MTU_DFLT;
    state.gateway_status_subscribed = false;
    state.vehicle_data_subscribed = false;
    state.app_state_seen = false;
    portEXIT_CRITICAL(&state_lock);
    ble_gatt_reassembler_reset(&state.reassembler);
    increment_stat(&state.stats.disconnections);
    (void)enqueue_event(&event);
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    ble_gatt_server_event_t app_event = {0};
    bool encrypted = false;
    bool authenticated = false;
    bool bonded = false;
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            if (!state.stopping) {
                (void)start_advertising();
            }
            return 0;
        }
        portENTER_CRITICAL(&state_lock);
        state.connection_generation++;
        if (state.connection_generation == 0U) {
            state.connection_generation = 1U;
        }
        state.connection_handle = event->connect.conn_handle;
        state.mtu = ble_att_mtu(event->connect.conn_handle);
        state.gateway_status_subscribed = false;
        state.vehicle_data_subscribed = false;
        state.app_state_seen = false;
        app_event.connection_generation = state.connection_generation;
        portEXIT_CRITICAL(&state_lock);
        ble_gatt_reassembler_reset(&state.reassembler);
        increment_stat(&state.stats.connections);
        app_event.type = BLE_GATT_EVENT_CONNECTED;
        app_event.connection_handle = event->connect.conn_handle;
        (void)enqueue_event(&app_event);
        ESP_LOGI(TAG, "BLE central connected; security and registration remain separate");
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        clear_connection(event->disconnect.conn.conn_handle,
                         (uint16_t)event->disconnect.reason);
        ESP_LOGI(TAG, "BLE central disconnected; advertising will restart");
        if (!state.stopping) {
            (void)start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (!state.stopping && !ble_gatt_server_is_connected()) {
            (void)start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (!current_connection_generation(event->enc_change.conn_handle,
                                           &app_event.connection_generation)) {
            ESP_LOGW(TAG, "Ignoring security event for stale connection=%u",
                     event->enc_change.conn_handle);
            return 0;
        }
        app_event.type = BLE_GATT_EVENT_SECURITY_CHANGED;
        app_event.connection_handle = event->enc_change.conn_handle;
        if (event->enc_change.status == 0 &&
            connection_security(event->enc_change.conn_handle, &encrypted, &authenticated, &bonded)) {
            app_event.data.security.encrypted = encrypted;
            app_event.data.security.authenticated = authenticated;
            app_event.data.security.bonded = bonded;
        }
        (void)enqueue_event(&app_event);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        portENTER_CRITICAL(&state_lock);
        if (event->subscribe.conn_handle != state.connection_handle) {
            portEXIT_CRITICAL(&state_lock);
            ESP_LOGW(TAG, "Ignoring subscription for stale connection=%u",
                     event->subscribe.conn_handle);
            return 0;
        }
        if (event->subscribe.attr_handle == gateway_status_handle) {
            state.gateway_status_subscribed = event->subscribe.cur_notify != 0U;
        } else if (event->subscribe.attr_handle == vehicle_data_handle) {
            state.vehicle_data_subscribed = event->subscribe.cur_notify != 0U;
        }
        app_event.data.subscription.gateway_status_notify = state.gateway_status_subscribed;
        app_event.data.subscription.vehicle_data_notify = state.vehicle_data_subscribed;
        app_event.connection_generation = state.connection_generation;
        portEXIT_CRITICAL(&state_lock);
        app_event.type = BLE_GATT_EVENT_SUBSCRIPTION_CHANGED;
        app_event.connection_handle = event->subscribe.conn_handle;
        (void)enqueue_event(&app_event);
        ESP_LOGI(TAG, "Subscription changed: connection=%u generation=%" PRIu32
                 " gateway_status=%d vehicle_data=%d",
                 app_event.connection_handle, app_event.connection_generation,
                 app_event.data.subscription.gateway_status_notify,
                 app_event.data.subscription.vehicle_data_notify);
        return 0;

    case BLE_GAP_EVENT_MTU:
        portENTER_CRITICAL(&state_lock);
        if (event->mtu.conn_handle != state.connection_handle) {
            portEXIT_CRITICAL(&state_lock);
            ESP_LOGW(TAG, "Ignoring MTU event for stale connection=%u",
                     event->mtu.conn_handle);
            return 0;
        }
        state.mtu = event->mtu.value;
        app_event.connection_generation = state.connection_generation;
        portEXIT_CRITICAL(&state_lock);
        app_event.type = BLE_GATT_EVENT_MTU_CHANGED;
        app_event.connection_handle = event->mtu.conn_handle;
        app_event.data.mtu.mtu = event->mtu.value;
        (void)enqueue_event(&app_event);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        ESP_LOGW(TAG, "Repeat pairing ignored; stored bond is not deleted automatically");
        return BLE_GAP_REPEAT_PAIRING_IGNORE;

    default:
        return 0;
    }
}

static int read_gateway_status(uint16_t connection_handle,
                               struct ble_gatt_access_ctxt *context)
{
    uint8_t snapshot[BLE_GATT_GATEWAY_STATUS_LENGTH];

    portENTER_CRITICAL(&state_lock);
    memcpy(snapshot, state.gateway_status, sizeof(snapshot));
    portEXIT_CRITICAL(&state_lock);
    if (os_mbuf_append(context->om, snapshot, sizeof(snapshot)) != 0) {
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    ESP_LOGI(TAG, "Gateway Status READ: connection=%u length=%u",
             connection_handle, (unsigned)sizeof(snapshot));
    return 0;
}

static int write_app_state(uint16_t connection_handle, struct os_mbuf *value)
{
    uint8_t raw[BLE_GATT_APP_STATE_LENGTH];
    uint16_t copied = 0U;
    ble_gatt_server_event_t event = {
        .type = BLE_GATT_EVENT_APP_STATE,
        .connection_handle = connection_handle,
    };
    uint8_t error;
    bool log_state;

    if (!current_connection_generation(connection_handle,
                                       &event.connection_generation)) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (OS_MBUF_PKTLEN(value) != sizeof(raw) ||
        ble_hs_mbuf_to_flat(value, raw, sizeof(raw), &copied) != 0 || copied != sizeof(raw)) {
        increment_stat(&state.stats.rejected_writes);
        return BLE_GATT_ATT_ERR_PAYLOAD_LENGTH_INVALID;
    }
    error = ble_gatt_protocol_decode_app_state(raw, copied, &event.data.app_state);
    if (error != 0U || !enqueue_event(&event)) {
        increment_stat(&state.stats.rejected_writes);
        return error != 0U ? error : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    portENTER_CRITICAL(&state_lock);
    log_state = !state.app_state_seen ||
                state.last_app_active != event.data.app_state.active;
    state.app_state_seen = true;
    state.last_app_active = event.data.app_state.active;
    portEXIT_CRITICAL(&state_lock);
    if (log_state) {
        ESP_LOGI(TAG, "App State WRITE accepted: connection=%u active=%d",
                 connection_handle, event.data.app_state.active);
    }
    return 0;
}

static int write_app_command(uint16_t connection_handle, struct os_mbuf *value)
{
    uint8_t raw[BLE_GATT_APP_COMMAND_MAX_FRAME];
    uint16_t copied = 0U;
    bool subscribed;
    bool complete = false;
    ble_gatt_message_t message;
    ble_gatt_server_event_t event = {
        .type = BLE_GATT_EVENT_APP_COMMAND,
        .connection_handle = connection_handle,
    };
    uint8_t error;

    if (!current_connection_generation(connection_handle,
                                       &event.connection_generation)) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    portENTER_CRITICAL(&state_lock);
    subscribed = state.vehicle_data_subscribed;
    portEXIT_CRITICAL(&state_lock);
    if (!subscribed) {
        increment_stat(&state.stats.rejected_writes);
        return BLE_GATT_ATT_ERR_SUBSCRIPTION_REQUIRED;
    }
    if (OS_MBUF_PKTLEN(value) > sizeof(raw) ||
        ble_hs_mbuf_to_flat(value, raw, sizeof(raw), &copied) != 0) {
        increment_stat(&state.stats.rejected_writes);
        return BLE_GATT_ATT_ERR_FRAGMENT_INVALID;
    }

    error = ble_gatt_protocol_accept_app_command(&state.reassembler, raw, copied,
                                                  (uint64_t)(esp_timer_get_time() / 1000),
                                                  &complete, &message);
    if (error != 0U) {
        increment_stat(&state.stats.rejected_writes);
        return error;
    }
    if (!complete) {
        return 0;
    }

    event.data.app_command.message_type = message.message_type;
    event.data.app_command.payload_length = message.payload_length;
    memcpy(event.data.app_command.payload, message.payload, message.payload_length);
    if (!enqueue_event(&event)) {
        increment_stat(&state.stats.rejected_writes);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return 0;
}

static int gatt_access(uint16_t connection_handle,
                       uint16_t attribute_handle,
                       struct ble_gatt_access_ctxt *context,
                       void *arg)
{
    attribute_id_t attribute = (attribute_id_t)(uintptr_t)arg;
    (void)attribute_handle;

    if (attribute == ATTR_GATEWAY_STATUS && context->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return read_gateway_status(connection_handle, context);
    }
    if (attribute == ATTR_APP_COMMAND && context->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return write_app_command(connection_handle, context->om);
    }
    if (attribute == ATTR_APP_STATE && context->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return write_app_state(connection_handle, context->om);
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int register_services(void)
{
    int result;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    result = ble_gatts_count_cfg(services);
    return result == 0 ? ble_gatts_add_svcs(services) : result;
}

ble_gatt_server_config_t ble_gatt_server_development_config(void)
{
    ble_gatt_server_config_t config = {
        .event_queue_depth = BLE_GATT_DEFAULT_QUEUE_DEPTH,
        .event_task_stack_size = BLE_GATT_DEFAULT_TASK_STACK,
        .event_task_priority = BLE_GATT_DEFAULT_TASK_PRIORITY,
        .bonding_enabled = true,
        .mitm_required = false,
        .secure_connections_enabled = true,
        .io_capability = BLE_HS_IO_NO_INPUT_OUTPUT,
        .event_callback = NULL,
        .event_context = NULL,
    };
    return config;
}

static void cleanup_start_failure(bool nimble_initialized)
{
    state.stopping = true;
    if (state.event_task != NULL) {
        vTaskDelete(state.event_task);
    }
    if (state.event_queue != NULL) {
        vQueueDelete(state.event_queue);
    }
    if (state.lifecycle_events != NULL) {
        vEventGroupDelete(state.lifecycle_events);
    }
    if (nimble_initialized) {
        (void)nimble_port_deinit();
    }
    memset(&state, 0, sizeof(state));
    state.connection_handle = BLE_HS_CONN_HANDLE_NONE;
}

esp_err_t ble_gatt_server_start(const ble_gatt_server_config_t *config)
{
    esp_err_t result;
    int host_result;

    if (config == NULL || config->event_queue_depth == 0U ||
        config->event_task_stack_size < 2048U || config->event_task_priority == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (state.started) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&state, 0, sizeof(state));
    state.config = *config;
    state.connection_handle = BLE_HS_CONN_HANDLE_NONE;
    state.mtu = BLE_ATT_MTU_DFLT;
    state.next_transfer_id = 1U;
    state.gateway_status[0] = BLE_GATT_PROTOCOL_VERSION;
    state.gateway_status[1] = 0xffU;
    state.gateway_status[20] = 0xffU;

    state.event_queue = xQueueCreate(config->event_queue_depth, sizeof(ble_gatt_server_event_t));
    state.lifecycle_events = xEventGroupCreate();
    if (state.event_queue == NULL || state.lifecycle_events == NULL ||
        xTaskCreate(application_event_task, "ble_app_events", config->event_task_stack_size,
                    NULL, config->event_task_priority, &state.event_task) != pdPASS) {
        cleanup_start_failure(false);
        return ESP_ERR_NO_MEM;
    }

    result = nimble_port_init();
    if (result != ESP_OK) {
        cleanup_start_failure(false);
        return result;
    }

    ble_hs_cfg.reset_cb = host_reset;
    ble_hs_cfg.sync_cb = host_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_bonding = config->bonding_enabled ? 1U : 0U;
    ble_hs_cfg.sm_mitm = config->mitm_required ? 1U : 0U;
    ble_hs_cfg.sm_sc = config->secure_connections_enabled ? 1U : 0U;
    ble_hs_cfg.sm_io_cap = config->io_capability;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    host_result = register_services();
    if (host_result == 0) {
        host_result = ble_svc_gap_device_name_set(BLE_GATT_DEVICE_NAME);
    }
    if (host_result != 0) {
        cleanup_start_failure(true);
        return ESP_FAIL;
    }

    ble_store_config_init();
    state.started = true;
    nimble_port_freertos_init(host_task);
    ESP_LOGW(TAG, "Development security profile active; TBD-SEC-01 is not production-approved");
    return ESP_OK;
}

esp_err_t ble_gatt_server_stop(void)
{
    ble_gatt_server_event_t wake = {0};
    EventBits_t stopped;

    if (!state.started) {
        return ESP_ERR_INVALID_STATE;
    }
    state.stopping = true;
    (void)ble_gap_adv_stop();
    if (state.connection_handle != BLE_HS_CONN_HANDLE_NONE) {
        (void)ble_gap_terminate(state.connection_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    if (nimble_port_stop() != 0) {
        return ESP_FAIL;
    }

    (void)xQueueSend(state.event_queue, &wake, 0U);
    stopped = xEventGroupWaitBits(state.lifecycle_events, BLE_GATT_STOPPED_BIT,
                                  pdFALSE, pdTRUE, pdMS_TO_TICKS(1000U));
    if ((stopped & BLE_GATT_STOPPED_BIT) == 0U) {
        return ESP_ERR_TIMEOUT;
    }
    vQueueDelete(state.event_queue);
    vEventGroupDelete(state.lifecycle_events);
    if (nimble_port_deinit() != ESP_OK) {
        return ESP_FAIL;
    }
    memset(&state, 0, sizeof(state));
    state.connection_handle = BLE_HS_CONN_HANDLE_NONE;
    return ESP_OK;
}

esp_err_t ble_gatt_server_set_gateway_status(const uint8_t status[BLE_GATT_GATEWAY_STATUS_LENGTH])
{
    if (status == NULL || status[0] != BLE_GATT_PROTOCOL_VERSION) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&state_lock);
    memcpy(state.gateway_status, status, BLE_GATT_GATEWAY_STATUS_LENGTH);
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

static esp_err_t notify_value(uint16_t value_handle, const uint8_t *data, size_t length)
{
    struct os_mbuf *packet;
    uint16_t connection_handle;
    int result;

    portENTER_CRITICAL(&state_lock);
    connection_handle = state.connection_handle;
    portEXIT_CRITICAL(&state_lock);
    if (connection_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGW(TAG, "Notify rejected: no active connection");
        return ESP_ERR_INVALID_STATE;
    }
    if (!ble_gatt_server_is_secure()) {
        ESP_LOGW(TAG, "Notify rejected: encrypted bonded link unavailable; connection=%u",
                 connection_handle);
        return ESP_ERR_INVALID_STATE;
    }

    packet = ble_hs_mbuf_from_flat(data, (uint16_t)length);
    if (packet == NULL) {
        return ESP_ERR_NO_MEM;
    }
    result = ble_gatts_notify_custom(connection_handle, value_handle, packet);
    if (result != 0) {
        increment_stat(&state.stats.notification_failures);
        return ESP_FAIL;
    }
    increment_stat(&state.stats.notifications_sent);
    return ESP_OK;
}

esp_err_t ble_gatt_server_send_gateway_status(ble_gatt_send_mode_t mode)
{
    uint8_t snapshot[BLE_GATT_GATEWAY_STATUS_LENGTH];
    bool subscribed;
    uint16_t mtu;

    if (mode != BLE_GATT_SEND_NOTIFY) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    portENTER_CRITICAL(&state_lock);
    subscribed = state.gateway_status_subscribed;
    mtu = state.mtu;
    memcpy(snapshot, state.gateway_status, sizeof(snapshot));
    portEXIT_CRITICAL(&state_lock);
    if (!subscribed || mtu < BLE_GATT_MIN_APP_MTU) {
        ESP_LOGW(TAG, "Gateway Status Notify rejected: subscribed=%d mtu=%u minimum=%u",
                 subscribed, mtu, BLE_GATT_MIN_APP_MTU);
        return ESP_ERR_INVALID_STATE;
    }
    return notify_value(gateway_status_handle, snapshot, sizeof(snapshot));
}

esp_err_t ble_gatt_server_send_vehicle_data(uint8_t message_type,
                                            const uint8_t *payload,
                                            size_t payload_length,
                                            ble_gatt_send_mode_t mode)
{
    uint8_t frame[BLE_GATT_MAX_FRAME_SIZE];
    size_t frame_length;
    size_t offset = 0U;
    size_t fragment_data_max;
    uint8_t fragment_count;
    uint8_t fragment_index;
    uint16_t mtu;
    uint16_t transfer_id;
    bool subscribed;
    esp_err_t result;

    if (mode != BLE_GATT_SEND_NOTIFY) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (payload == NULL || payload_length != ble_gatt_protocol_payload_length(message_type, false)) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&state_lock);
    mtu = state.mtu;
    subscribed = state.vehicle_data_subscribed;
    portEXIT_CRITICAL(&state_lock);
    if (!subscribed || mtu <= BLE_GATT_FRAGMENT_HEADER_SIZE + 3U) {
        return ESP_ERR_INVALID_STATE;
    }
    portENTER_CRITICAL(&state_lock);
    (void)ble_gatt_protocol_next_transfer_id(&state.next_transfer_id, &transfer_id);
    portEXIT_CRITICAL(&state_lock);

    fragment_data_max = (size_t)mtu - 3U - BLE_GATT_FRAGMENT_HEADER_SIZE;
    fragment_count = (uint8_t)((payload_length + fragment_data_max - 1U) / fragment_data_max);
    for (fragment_index = 0U; fragment_index < fragment_count; ++fragment_index) {
        size_t remaining = payload_length - offset;
        size_t fragment_length = remaining < fragment_data_max ? remaining : fragment_data_max;
        if (!ble_gatt_protocol_encode_fragment(message_type, transfer_id, (uint16_t)payload_length,
                                               fragment_index, fragment_count, &payload[offset],
                                               fragment_length, frame, sizeof(frame), &frame_length)) {
            return ESP_ERR_INVALID_ARG;
        }
        result = notify_value(vehicle_data_handle, frame, frame_length);
        if (result != ESP_OK) {
            return result;
        }
        offset += fragment_length;
    }
    return ESP_OK;
}

void ble_gatt_server_get_stats(ble_gatt_server_stats_t *out_stats)
{
    if (out_stats == NULL) {
        return;
    }
    portENTER_CRITICAL(&state_lock);
    *out_stats = state.stats;
    portEXIT_CRITICAL(&state_lock);
}
