#ifndef NETWORK_NET_HOST_MOCK_H
#define NETWORK_NET_HOST_MOCK_H

#include "network/net_port.h"

/* Local host fixture capacities, not product Driver or wire constraints. */
#define NET_HOST_EVENT_CAPACITY 8u
#define NET_HOST_TX_CAPACITY 4u

typedef struct {
    bool occupied;
    NetTxKey key;
    const NetFrame *frame;
} NetHostBorrow;

/* Single-owner, allocation-free test fixture. No concurrent ISR guarantee.
   Initialize only before use or after all borrowed frames have been released. */
typedef struct {
    uint64_t now_ms;
    NetPortEvent events[NET_HOST_EVENT_CAPACITY];
    size_t event_head;
    size_t event_count;
    uint32_t dropped_events;
    NetHostBorrow borrows[NET_HOST_TX_CAPACITY];
    NetStatus submit_status;
} NetHostMock;

void net_host_mock_init(NetHostMock *mock);
NetPort net_host_mock_port(NetHostMock *mock);
NetStatus net_host_mock_set_time(NetHostMock *mock, uint64_t now_ms);
NetStatus net_host_mock_advance_time(NetHostMock *mock, uint64_t delta_ms);

/* RX copies the entire frame, including its original captured_at_ms value.
   TX events are injected explicitly; submit never invents any TX event. */
NetStatus net_host_mock_inject_rx(NetHostMock *mock, const NetFrame *frame);
NetStatus net_host_mock_inject_tx(NetHostMock *mock, const NetTxEvent *event);
NetStatus net_host_mock_inject_error(NetHostMock *mock, uint32_t error_code);
NetStatus net_host_mock_set_submit_status(NetHostMock *mock, NetStatus status);

/* A matching release event removes the retained pointer when it is polled.
   Unsolicited/stale events are allowed so the runtime can reject them. */
const NetFrame *net_host_mock_borrowed_frame(const NetHostMock *mock, NetTxKey key);
size_t net_host_mock_pending_tx_count(const NetHostMock *mock);
uint32_t net_host_mock_dropped_events(const NetHostMock *mock);

#endif
