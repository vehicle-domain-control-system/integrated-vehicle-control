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
#include "nimble/nimble_npl.h"
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
#if CONFIG_P6_READ_ONLY_MONITORING
    struct ble_npl_callout peer_observer;
    bool peer_observer_initialized;
#endif
} server_state_t;

static const char *TAG = "ble_gatt";
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static server_state_t state;
static ble_connection_state_t connection_state;

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

static bool address_bytes_valid(const uint8_t address[6])
{
    size_t index;
    bool all_zero = true;
    bool all_ff = true;

    for (index = 0U; index < 6U; ++index) {
        all_zero = all_zero && address[index] == 0U;
        all_ff = all_ff && address[index] == UINT8_MAX;
    }
    return !all_zero && !all_ff;
}

static void observe_peer_identity(uint16_t connection_handle,
                                  ble_gatt_peer_identity_t *out_identity)
{
    struct ble_gap_conn_desc descriptor;
#if MYNEWT_VAL(BLE_STORE_MAX_BONDS) > 0
    ble_addr_t bonded_peers[MYNEWT_VAL(BLE_STORE_MAX_BONDS)];
    int bonded_peer_count = 0;
    int index;
#endif
    int result;

    memset(out_identity, 0, sizeof(*out_identity));
    result = ble_gap_conn_find(connection_handle, &descriptor);
    if (result != 0) {
        out_identity->lookup_error = true;
        return;
    }

    out_identity->ota_address_is_rpa = BLE_ADDR_IS_RPA(&descriptor.peer_ota_addr);
    if ((descriptor.peer_id_addr.type > BLE_ADDR_RANDOM) ||
        BLE_ADDR_IS_RPA(&descriptor.peer_id_addr) ||
        !address_bytes_valid(descriptor.peer_id_addr.val)) {
        return;
    }

    out_identity->identity_resolved = true;
    out_identity->address_type = descriptor.peer_id_addr.type;
    memcpy(out_identity->address, descriptor.peer_id_addr.val,
           sizeof(out_identity->address));

#if MYNEWT_VAL(BLE_STORE_MAX_BONDS) > 0
    result = ble_store_util_bonded_peers(bonded_peers, &bonded_peer_count,
                                         MYNEWT_VAL(BLE_STORE_MAX_BONDS));
    if (result != 0) {
        out_identity->lookup_error = true;
        return;
    }
    for (index = 0; index < bonded_peer_count; ++index) {
        if (ble_addr_cmp(&descriptor.peer_id_addr, &bonded_peers[index]) == 0) {
            out_identity->bond_store_present = true;
            break;
        }
    }
#else
    out_identity->lookup_error = true;
#endif
}

#if CONFIG_P6_READ_ONLY_MONITORING
static void observe_current_peer(struct ble_npl_event *unused)
{
    ble_gatt_server_event_t event = {0};
    ble_connection_snapshot_t before, after;
    bool active;
    (void)unused;

    portENTER_CRITICAL(&state_lock);
    active = state.started && !state.stopping;
    ble_connection_state_snapshot(&connection_state, &before);
    portEXIT_CRITICAL(&state_lock);
    if (!active || !before.available || !before.connected) return;

    /* Runs on NimBLE's Host event queue, not GatewayTask/ISR/timer callback.
     * Reuse exact identity + bounded Bond Store read, without any store write. */
    event.type = BLE_GATT_EVENT_PEER_OBSERVED;
    event.connection_handle = before.connection_handle;
    event.connection_generation = before.connection_generation;
    event.observation_epoch = before.connection_epoch;
    observe_peer_identity(before.connection_handle, &event.peer_identity);
    if (!connection_security(before.connection_handle,
            &event.data.security.encrypted, &event.data.security.authenticated,
            &event.data.security.bonded)) {
        event.peer_identity.lookup_error = true;
    }
    event.observed_at_ms = (uint64_t)(esp_timer_get_time() / INT64_C(1000));
    portENTER_CRITICAL(&state_lock);
    active = state.started && !state.stopping;
    ble_connection_state_snapshot(&connection_state, &after);
    portEXIT_CRITICAL(&state_lock);
    if (!active) return;
    if (after.available && after.connected &&
        after.connection_handle == before.connection_handle &&
        after.connection_generation == before.connection_generation &&
        after.connection_epoch == before.connection_epoch) {
        /* Queue loss cannot create an observation; old Source will expire. */
        (void)enqueue_event(&event);
    }
    if (after.connected && ble_npl_callout_reset(&state.peer_observer,
            ble_npl_time_ms_to_ticks32(BLE_GATT_MONITOR_OBSERVATION_PERIOD_MS)) != 0) {
        ESP_LOGW(TAG, "Peer observation timer unavailable; Source cannot refresh");
    }
}
#endif

static void observe_bond_store(ble_gatt_server_event_t *event)
{
#if MYNEWT_VAL(BLE_STORE_MAX_BONDS) > 0
    ble_addr_t bonded_peers[BLE_GATT_BOND_SNAPSHOT_CAPACITY];
    int bonded_peer_count = 0;
    int result;
    int index;

    result = ble_store_util_bonded_peers(
        bonded_peers, &bonded_peer_count,
        (int)BLE_GATT_BOND_SNAPSHOT_CAPACITY);
    if (result != 0 || bonded_peer_count < 0 ||
        bonded_peer_count > (int)BLE_GATT_BOND_SNAPSHOT_CAPACITY) {
        event->data.bond_store.lookup_error = true;
        event->data.bond_store.overflow = result == BLE_HS_ENOMEM;
        return;
    }
    event->data.bond_store.count = (uint8_t)bonded_peer_count;
    for (index = 0; index < bonded_peer_count; ++index) {
        ble_gatt_peer_identity_t *peer =
            &event->data.bond_store.peers[index];

        if (bonded_peers[index].type > BLE_ADDR_RANDOM ||
            BLE_ADDR_IS_RPA(&bonded_peers[index]) ||
            !address_bytes_valid(bonded_peers[index].val)) {
            event->data.bond_store.lookup_error = true;
            event->data.bond_store.count = 0U;
            return;
        }
        peer->identity_resolved = true;
        peer->bond_store_present = true;
        peer->address_type = bonded_peers[index].type;
        memcpy(peer->address, bonded_peers[index].val,
               sizeof(peer->address));
    }
#else
    event->data.bond_store.lookup_error = true;
#endif
}

static const char *peer_identity_state_name(
    const ble_gatt_peer_identity_t *identity)
{
    if (identity->lookup_error) {
        return "LOOKUP_ERROR";
    }
    if (!identity->identity_resolved) {
        return "UNRESOLVED";
    }
    return identity->bond_store_present ? "RESOLVED_BONDED" :
                                          "RESOLVED_NOT_BONDED";
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

esp_err_t ble_gatt_server_get_connection_snapshot(
    ble_connection_snapshot_t *out_snapshot)
{
    if (out_snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&state_lock);
    ble_connection_state_snapshot(&connection_state, out_snapshot);
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
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
    ble_gatt_server_event_t bond_event = {
        .type = BLE_GATT_EVENT_BOND_STORE_READY,
        .connection_handle = BLE_HS_CONN_HANDLE_NONE,
    };
    int result = ble_hs_util_ensure_addr(0);
    if (result == 0) {
        result = ble_hs_id_infer_auto(0, &state.own_address_type);
    }
    if (result == 0) {
        observe_bond_store(&bond_event);
        if (!enqueue_event(&bond_event)) {
            ESP_LOGW(TAG, "Bond Store snapshot dropped; P6 remains fail-closed");
        } else if (bond_event.data.bond_store.lookup_error) {
            ESP_LOGW(TAG, "Bond Store snapshot failed; P6 remains fail-closed");
        } else {
            ESP_LOGI(TAG, "Bond Store snapshot ready: peers=%u; identities withheld",
                     bond_event.data.bond_store.count);
        }
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
    (void)ble_connection_state_disconnect(&connection_state,
                                          connection_handle);
    state.connection_handle = BLE_HS_CONN_HANDLE_NONE;
    state.mtu = BLE_ATT_MTU_DFLT;
    state.gateway_status_subscribed = false;
    state.vehicle_data_subscribed = false;
    state.app_state_seen = false;
    portEXIT_CRITICAL(&state_lock);
    ble_gatt_reassembler_reset(&state.reassembler);
#if CONFIG_P6_READ_ONLY_MONITORING
    if (state.peer_observer_initialized) ble_npl_callout_stop(&state.peer_observer);
#endif
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
        if (!ble_connection_state_connect(&connection_state,
                                          event->connect.conn_handle)) {
            portEXIT_CRITICAL(&state_lock);
            ESP_LOGE(TAG, "BLE connection state rejected new connection=%u",
                     event->connect.conn_handle);
            (void)ble_gap_terminate(event->connect.conn_handle,
                                    BLE_ERR_REM_USER_CONN_TERM);
            return 0;
        }
        state.connection_generation =
            connection_state.connection_generation;
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
        observe_peer_identity(event->connect.conn_handle,
                              &app_event.peer_identity);
        (void)enqueue_event(&app_event);
#if CONFIG_P6_READ_ONLY_MONITORING
        if (state.peer_observer_initialized && ble_npl_callout_reset(&state.peer_observer,
                ble_npl_time_ms_to_ticks32(BLE_GATT_MONITOR_OBSERVATION_PERIOD_MS)) != 0) {
            ESP_LOGW(TAG, "Peer observer not scheduled; Source cannot refresh");
        }
#endif
        ESP_LOGI(TAG, "BLE_CONNECTED identity=%s ota_rpa=%d; registration remains separate",
                 peer_identity_state_name(&app_event.peer_identity),
                 app_event.peer_identity.ota_address_is_rpa);
        if (app_event.peer_identity.identity_resolved &&
            app_event.peer_identity.bond_store_present &&
            !app_event.peer_identity.lookup_error) {
            ESP_LOGI(TAG, "BOND_IDENTITY_RESOLVED; identity bytes withheld");
        }
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
        observe_peer_identity(event->enc_change.conn_handle,
                              &app_event.peer_identity);
        (void)enqueue_event(&app_event);
        ESP_LOGI(TAG, "%s identity=%s ota_rpa=%d",
                 encrypted && bonded ? "SECURITY_VERIFIED" : "SECURITY_PENDING",
                 peer_identity_state_name(&app_event.peer_identity),
                 app_event.peer_identity.ota_address_is_rpa);
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
#if CONFIG_P6_READ_ONLY_MONITORING
        if (state.peer_observer_initialized) ble_npl_callout_deinit(&state.peer_observer);
#endif
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
#if CONFIG_P6_READ_ONLY_MONITORING
    if (ble_npl_callout_init(&state.peer_observer, nimble_port_get_dflt_eventq(),
                            observe_current_peer, NULL) != 0) {
        cleanup_start_failure(true);
        return ESP_ERR_NO_MEM;
    }
    state.peer_observer_initialized = true;
#endif
    portENTER_CRITICAL(&state_lock);
    if (!connection_state.initialized) {
        ble_connection_state_init(&connection_state);
    }
    ble_connection_state_set_available(&connection_state, true);
    state.started = true;
    portEXIT_CRITICAL(&state_lock);
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
    portENTER_CRITICAL(&state_lock);
    ble_connection_state_set_available(&connection_state, false);
    state.stopping = true;
    portEXIT_CRITICAL(&state_lock);
    (void)ble_gap_adv_stop();
    if (state.connection_handle != BLE_HS_CONN_HANDLE_NONE) {
        (void)ble_gap_terminate(state.connection_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    if (nimble_port_stop() != 0) {
        return ESP_FAIL;
    }
#if CONFIG_P6_READ_ONLY_MONITORING
    if (state.peer_observer_initialized) {
        ble_npl_callout_stop(&state.peer_observer);
        ble_npl_callout_deinit(&state.peer_observer);
        state.peer_observer_initialized = false;
    }
#endif

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
