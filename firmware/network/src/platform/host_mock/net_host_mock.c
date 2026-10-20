#include "network/net_host_mock.h"

#include <string.h>

static bool same_key(NetTxKey left, NetTxKey right)
{
    return left.epoch == right.epoch && left.token == right.token;
}

static bool valid_frame(const NetFrame *frame)
{
    return frame != NULL && frame->length <= NET_FRAME_CAPACITY &&
           (frame->link == NET_LINK_CAN || frame->link == NET_LINK_UART_M ||
            frame->link == NET_LINK_UART_C);
}

static NetStatus mock_now(void *context, uint64_t *out)
{
    const NetHostMock *mock = context;
    if (mock == NULL || out == NULL) {
        return NET_INVALID_ARGUMENT;
    }
    *out = mock->now_ms;
    return NET_OK;
}

static NetStatus mock_submit(void *context, const NetFrame *frame, NetTxKey key)
{
    NetHostMock *mock = context;
    size_t index;
    size_t free_index = NET_HOST_TX_CAPACITY;
    if (mock == NULL || !valid_frame(frame)) {
        return NET_INVALID_ARGUMENT;
    }
    if (mock->submit_status != NET_OK) {
        return mock->submit_status;
    }
    for (index = 0u; index < NET_HOST_TX_CAPACITY; ++index) {
        if (mock->borrows[index].occupied) {
            if (same_key(mock->borrows[index].key, key)) {
                return NET_DUPLICATE;
            }
        } else if (free_index == NET_HOST_TX_CAPACITY) {
            free_index = index;
        }
    }
    if (free_index == NET_HOST_TX_CAPACITY) {
        return NET_NO_CAPACITY;
    }
    mock->borrows[free_index].occupied = true;
    mock->borrows[free_index].key = key;
    mock->borrows[free_index].frame = frame;
    return NET_OK;
}

static NetStatus mock_poll(void *context, NetPortEvent *out)
{
    NetHostMock *mock = context;
    NetPortEvent event;
    size_t index;
    if (mock == NULL || out == NULL) {
        return NET_INVALID_ARGUMENT;
    }
    if (mock->event_count == 0u) {
        return NET_NO_WORK;
    }
    event = mock->events[mock->event_head];
    memset(&mock->events[mock->event_head], 0, sizeof(NetPortEvent));
    mock->event_head = (mock->event_head + 1u) % NET_HOST_EVENT_CAPACITY;
    --mock->event_count;
    if (event.kind == NET_PORT_TX && event.tx.release_observed) {
        for (index = 0u; index < NET_HOST_TX_CAPACITY; ++index) {
            if (mock->borrows[index].occupied &&
                same_key(mock->borrows[index].key, event.tx.key)) {
                /* This exact attempt's borrow ends; no newer key is affected. */
                memset(&mock->borrows[index], 0, sizeof(NetHostBorrow));
                break;
            }
        }
    }
    *out = event;
    return NET_OK;
}

static const NetPortOps mock_ops = {mock_now, mock_submit, mock_poll};

static NetStatus enqueue(NetHostMock *mock, const NetPortEvent *event)
{
    size_t tail;
    if (mock->event_count == NET_HOST_EVENT_CAPACITY) {
        if (mock->dropped_events != UINT32_MAX) {
            ++mock->dropped_events;
        }
        return NET_NO_CAPACITY;
    }
    tail = (mock->event_head + mock->event_count) % NET_HOST_EVENT_CAPACITY;
    mock->events[tail] = *event;
    ++mock->event_count;
    return NET_OK;
}

void net_host_mock_init(NetHostMock *mock)
{
    if (mock != NULL) {
        memset(mock, 0, sizeof(*mock));
        mock->submit_status = NET_OK;
    }
}

NetPort net_host_mock_port(NetHostMock *mock)
{
    NetPort port = {&mock_ops, mock};
    return port;
}

NetStatus net_host_mock_set_time(NetHostMock *mock, uint64_t now_ms)
{
    if (mock == NULL) {
        return NET_INVALID_ARGUMENT;
    }
    mock->now_ms = now_ms;
    return NET_OK;
}

NetStatus net_host_mock_advance_time(NetHostMock *mock, uint64_t delta_ms)
{
    if (mock == NULL || (UINT64_MAX - mock->now_ms) < delta_ms) {
        return NET_INVALID_ARGUMENT;
    }
    mock->now_ms += delta_ms;
    return NET_OK;
}

NetStatus net_host_mock_inject_rx(NetHostMock *mock, const NetFrame *frame)
{
    NetPortEvent event = {0};
    if (mock == NULL || !valid_frame(frame)) {
        return NET_INVALID_ARGUMENT;
    }
    event.kind = NET_PORT_RX;
    event.frame = *frame;
    return enqueue(mock, &event);
}

NetStatus net_host_mock_inject_tx(NetHostMock *mock, const NetTxEvent *tx)
{
    NetPortEvent event = {0};
    if (mock == NULL || tx == NULL) {
        return NET_INVALID_ARGUMENT;
    }
    event.kind = NET_PORT_TX;
    event.tx = *tx;
    return enqueue(mock, &event);
}

NetStatus net_host_mock_inject_error(NetHostMock *mock, uint32_t error_code)
{
    NetPortEvent event = {0};
    if (mock == NULL) {
        return NET_INVALID_ARGUMENT;
    }
    event.kind = NET_PORT_ERROR;
    event.error_code = error_code;
    return enqueue(mock, &event);
}

NetStatus net_host_mock_set_submit_status(NetHostMock *mock, NetStatus status)
{
    if (mock == NULL || (status != NET_OK && status != NET_BUSY &&
                         status != NET_REJECTED)) {
        return NET_INVALID_ARGUMENT;
    }
    mock->submit_status = status;
    return NET_OK;
}

const NetFrame *net_host_mock_borrowed_frame(const NetHostMock *mock, NetTxKey key)
{
    size_t index;
    if (mock == NULL) {
        return NULL;
    }
    for (index = 0u; index < NET_HOST_TX_CAPACITY; ++index) {
        if (mock->borrows[index].occupied && same_key(mock->borrows[index].key, key)) {
            return mock->borrows[index].frame;
        }
    }
    return NULL;
}

size_t net_host_mock_pending_tx_count(const NetHostMock *mock)
{
    size_t index;
    size_t count = 0u;
    if (mock == NULL) {
        return 0u;
    }
    for (index = 0u; index < NET_HOST_TX_CAPACITY; ++index) {
        if (mock->borrows[index].occupied) {
            ++count;
        }
    }
    return count;
}

uint32_t net_host_mock_dropped_events(const NetHostMock *mock)
{
    return mock != NULL ? mock->dropped_events : 0u;
}
