#include "network/net_board_ports.h"
#include "network/net_host_mock.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static NetFrame fixture_frame(NetLink link, uint8_t value)
{
    NetFrame frame = {0};
    frame.link = link;
    frame.id = 23u;
    frame.length = 3u;
    frame.captured_at_ms = 17u;
    frame.bytes[0] = value;
    frame.bytes[1] = 0x22u;
    frame.bytes[2] = 0x33u;
    return frame;
}

static int check_clocks_and_instances(void)
{
    NetHostMock first;
    NetHostMock second;
    NetPort first_port;
    NetPort second_port;
    uint64_t now = 51u;
    net_host_mock_init(&first);
    net_host_mock_init(&second);
    first_port = net_host_mock_port(&first);
    second_port = net_host_mock_port(&second);
    CHECK(net_port_now(&first_port, &now) == NET_OK && now == 0u);
    CHECK(net_host_mock_set_time(&first, 99u) == NET_OK);
    CHECK(net_host_mock_advance_time(&first, 1u) == NET_OK);
    CHECK(net_port_now(&first_port, &now) == NET_OK && now == 100u);
    CHECK(net_port_now(&second_port, &now) == NET_OK && now == 0u);
    CHECK(net_host_mock_set_time(&first, UINT64_MAX) == NET_OK);
    CHECK(net_host_mock_advance_time(&first, 1u) == NET_INVALID_ARGUMENT);
    CHECK(net_port_now(&first_port, &now) == NET_OK && now == UINT64_MAX);
    CHECK(net_host_mock_advance_time(&first, 0u) == NET_OK);
    CHECK(net_host_mock_set_time(NULL, 1u) == NET_INVALID_ARGUMENT);
    CHECK(net_host_mock_advance_time(NULL, 1u) == NET_INVALID_ARGUMENT);
    CHECK(first_port.ops->now_ms(NULL, &now) == NET_INVALID_ARGUMENT);
    CHECK(first_port.ops->now_ms(&first, NULL) == NET_INVALID_ARGUMENT);
    return 0;
}

static int check_stable_rx(void)
{
    NetHostMock mock;
    NetHostMock second;
    NetPort port;
    NetPort second_port;
    NetPortEvent event;
    NetPortEvent unchanged;
    const NetLink links[] = {NET_LINK_CAN, NET_LINK_UART_M, NET_LINK_UART_C};
    NetFrame frame;
    size_t index;
    net_host_mock_init(&mock);
    net_host_mock_init(&second);
    port = net_host_mock_port(&mock);
    second_port = net_host_mock_port(&second);
    CHECK(net_host_mock_set_time(&mock, 9999u) == NET_OK);
    for (index = 0u; index < sizeof(links) / sizeof(links[0]); ++index) {
        frame = fixture_frame(links[index], 0x11u);
        CHECK(net_host_mock_inject_rx(&mock, &frame) == NET_OK);
        memset(&frame, 0, sizeof(frame));
        CHECK(net_port_poll(&port, &event) == NET_OK);
        CHECK(event.kind == NET_PORT_RX && event.frame.link == links[index]);
        CHECK(event.frame.id == 23u && event.frame.length == 3u);
        CHECK(event.frame.captured_at_ms == 17u);
        CHECK(event.frame.bytes[0] == 0x11u && event.frame.bytes[1] == 0x22u &&
              event.frame.bytes[2] == 0x33u);
    }
    memset(&event, 0xa5, sizeof(event));
    memcpy(&unchanged, &event, sizeof(event));
    CHECK(net_port_poll(&port, &event) == NET_NO_WORK);
    CHECK(memcmp(&event, &unchanged, sizeof(event)) == 0);
    CHECK(net_port_poll(&second_port, &event) == NET_NO_WORK);
    CHECK(memcmp(&event, &unchanged, sizeof(event)) == 0);
    frame = fixture_frame(NET_LINK_CAN, 0u);
    frame.length = NET_FRAME_CAPACITY + 1u;
    CHECK(net_host_mock_inject_rx(&mock, &frame) == NET_INVALID_ARGUMENT);
    frame.length = 1u;
    frame.link = (NetLink)99;
    CHECK(net_host_mock_inject_rx(&mock, &frame) == NET_INVALID_ARGUMENT);
    CHECK(net_host_mock_inject_rx(&mock, NULL) == NET_INVALID_ARGUMENT);
    CHECK(net_host_mock_inject_rx(NULL, &frame) == NET_INVALID_ARGUMENT);
    CHECK(net_host_mock_inject_tx(&mock, NULL) == NET_INVALID_ARGUMENT);
    CHECK(net_host_mock_inject_error(NULL, 1u) == NET_INVALID_ARGUMENT);
    CHECK(mock.event_count == 0u && net_host_mock_dropped_events(&mock) == 0u);
    return 0;
}

static int check_ring_overflow_and_wrap(void)
{
    NetHostMock mock;
    NetPort port;
    NetPortEvent event;
    NetFrame frame = fixture_frame(NET_LINK_CAN, 0u);
    NetTxEvent tx = {0};
    size_t index;
    net_host_mock_init(&mock);
    port = net_host_mock_port(&mock);
    for (index = 0u; index < NET_HOST_EVENT_CAPACITY; ++index) {
        CHECK(net_host_mock_inject_error(&mock, (uint32_t)index) == NET_OK);
    }
    CHECK(net_host_mock_inject_rx(&mock, &frame) == NET_NO_CAPACITY);
    CHECK(net_host_mock_inject_tx(&mock, &tx) == NET_NO_CAPACITY);
    CHECK(net_host_mock_inject_error(&mock, 99u) == NET_NO_CAPACITY);
    CHECK(net_host_mock_dropped_events(&mock) == 3u);
    mock.dropped_events = UINT32_MAX - 1u;
    CHECK(net_host_mock_inject_error(&mock, 99u) == NET_NO_CAPACITY);
    CHECK(net_host_mock_dropped_events(&mock) == UINT32_MAX);
    CHECK(net_host_mock_inject_error(&mock, 99u) == NET_NO_CAPACITY);
    CHECK(net_host_mock_dropped_events(&mock) == UINT32_MAX);
    for (index = 0u; index < 3u; ++index) {
        CHECK(net_port_poll(&port, &event) == NET_OK);
        CHECK(event.kind == NET_PORT_ERROR && event.error_code == (uint32_t)index);
    }
    for (index = 0u; index < 3u; ++index) {
        CHECK(net_host_mock_inject_error(&mock,
              (uint32_t)(NET_HOST_EVENT_CAPACITY + index)) == NET_OK);
    }
    for (index = 3u; index < NET_HOST_EVENT_CAPACITY + 3u; ++index) {
        CHECK(net_port_poll(&port, &event) == NET_OK);
        CHECK(event.kind == NET_PORT_ERROR && event.error_code == (uint32_t)index);
    }
    CHECK(net_port_poll(&port, &event) == NET_NO_WORK);
    return 0;
}

static int check_explicit_tx_lifetime(void)
{
    NetHostMock mock;
    NetPort port;
    NetFrame frames[NET_HOST_TX_CAPACITY + 1u];
    NetTxKey keys[NET_HOST_TX_CAPACITY + 1u];
    NetPortEvent event;
    NetTxEvent tx = {0};
    size_t index;
    net_host_mock_init(&mock);
    port = net_host_mock_port(&mock);
    for (index = 0u; index < NET_HOST_TX_CAPACITY + 1u; ++index) {
        frames[index] = fixture_frame(NET_LINK_CAN, (uint8_t)index);
        keys[index].epoch = 3u;
        keys[index].token = index + 1u;
    }
    CHECK(net_host_mock_set_submit_status(&mock, NET_BUSY) == NET_OK);
    CHECK(net_port_submit(&port, &frames[0], keys[0]) == NET_BUSY);
    CHECK(net_host_mock_borrowed_frame(&mock, keys[0]) == NULL);
    CHECK(net_host_mock_set_submit_status(&mock, NET_REJECTED) == NET_OK);
    CHECK(net_port_submit(&port, &frames[0], keys[0]) == NET_REJECTED);
    CHECK(net_host_mock_set_submit_status(&mock, NET_HOLD) == NET_INVALID_ARGUMENT);
    CHECK(net_host_mock_pending_tx_count(&mock) == 0u);
    CHECK(net_port_poll(&port, &event) == NET_NO_WORK);
    CHECK(net_host_mock_set_submit_status(&mock, NET_OK) == NET_OK);
    for (index = 0u; index < NET_HOST_TX_CAPACITY; ++index) {
        CHECK(net_port_submit(&port, &frames[index], keys[index]) == NET_OK);
        CHECK(net_host_mock_borrowed_frame(&mock, keys[index]) == &frames[index]);
    }
    CHECK(net_host_mock_pending_tx_count(&mock) == NET_HOST_TX_CAPACITY);
    CHECK(net_port_submit(&port, &frames[0], keys[0]) == NET_DUPLICATE);
    CHECK(net_port_submit(&port, &frames[NET_HOST_TX_CAPACITY],
                          keys[NET_HOST_TX_CAPACITY]) == NET_NO_CAPACITY);
    CHECK(net_port_poll(&port, &event) == NET_NO_WORK);
    tx.key = keys[0];
    tx.admission_observed = true;
    tx.accepted = true;
    CHECK(net_host_mock_inject_tx(&mock, &tx) == NET_OK);
    tx.accepted = false;
    CHECK(net_port_poll(&port, &event) == NET_OK);
    CHECK(event.kind == NET_PORT_TX && event.tx.accepted);
    CHECK(event.tx.admission_observed && !event.tx.release_observed);
    CHECK(event.tx.outcome == NET_TX_UNKNOWN);
    CHECK(net_host_mock_borrowed_frame(&mock, keys[0]) == &frames[0]);
    memset(&tx, 0, sizeof(tx));
    tx.key = keys[0];
    tx.outcome = NET_TX_SUCCEEDED;
    CHECK(net_host_mock_inject_tx(&mock, &tx) == NET_OK);
    CHECK(net_port_poll(&port, &event) == NET_OK);
    CHECK(event.tx.outcome == NET_TX_SUCCEEDED && !event.tx.release_observed);
    CHECK(net_host_mock_borrowed_frame(&mock, keys[0]) == &frames[0]);
    tx.outcome = NET_TX_UNKNOWN;
    tx.release_observed = true;
    CHECK(net_host_mock_inject_tx(&mock, &tx) == NET_OK);
    CHECK(net_host_mock_borrowed_frame(&mock, keys[0]) == &frames[0]);
    CHECK(net_port_poll(&port, &event) == NET_OK);
    CHECK(event.tx.release_observed && event.tx.outcome == NET_TX_UNKNOWN);
    CHECK(net_host_mock_borrowed_frame(&mock, keys[0]) == NULL);
    CHECK(net_port_submit(&port, &frames[NET_HOST_TX_CAPACITY],
                          keys[NET_HOST_TX_CAPACITY]) == NET_OK);
    /* Old release must not release the new occupant of the same local slot. */
    CHECK(net_host_mock_inject_tx(&mock, &tx) == NET_OK);
    CHECK(net_port_poll(&port, &event) == NET_OK);
    CHECK(net_host_mock_borrowed_frame(&mock, keys[NET_HOST_TX_CAPACITY]) ==
          &frames[NET_HOST_TX_CAPACITY]);
    tx.key = keys[NET_HOST_TX_CAPACITY];
    --tx.key.epoch;
    CHECK(net_host_mock_inject_tx(&mock, &tx) == NET_OK);
    CHECK(net_port_poll(&port, &event) == NET_OK);
    CHECK(net_host_mock_borrowed_frame(&mock, keys[NET_HOST_TX_CAPACITY]) ==
          &frames[NET_HOST_TX_CAPACITY]);
    for (index = 0u; index < NET_HOST_EVENT_CAPACITY; ++index) {
        CHECK(net_host_mock_inject_error(&mock, 1u) == NET_OK);
    }
    tx.key = keys[NET_HOST_TX_CAPACITY];
    CHECK(net_host_mock_inject_tx(&mock, &tx) == NET_NO_CAPACITY);
    CHECK(net_host_mock_borrowed_frame(&mock, tx.key) == &frames[NET_HOST_TX_CAPACITY]);
    for (index = 0u; index < NET_HOST_EVENT_CAPACITY; ++index) {
        CHECK(net_port_poll(&port, &event) == NET_OK);
    }
    for (index = 1u; index < NET_HOST_TX_CAPACITY + 1u; ++index) {
        tx.key = keys[index];
        CHECK(net_host_mock_inject_tx(&mock, &tx) == NET_OK);
        CHECK(net_port_poll(&port, &event) == NET_OK);
        CHECK(net_host_mock_borrowed_frame(&mock, keys[index]) == NULL);
    }
    CHECK(net_host_mock_pending_tx_count(&mock) == 0u);
    return 0;
}

static int check_boards_fail_closed(void)
{
    NetPort boards[2];
    NetFrame frame = fixture_frame(NET_LINK_CAN, 1u);
    NetTxKey key = {7u, 1u};
    NetPortEvent event;
    NetPortEvent unchanged;
    uint64_t now;
    size_t index;
    boards[0] = net_s32k144_port();
    boards[1] = net_s32k344_port();
    CHECK(boards[0].ops != boards[1].ops);
    for (index = 0u; index < 2u; ++index) {
        memset(&event, 0xa5, sizeof(event));
        memcpy(&unchanged, &event, sizeof(event));
        now = 314u;
        CHECK(net_port_validate(&boards[index]) == NET_OK);
        CHECK(net_port_now(&boards[index], &now) == NET_NOT_CONFIGURED);
        CHECK(now == 314u);
        CHECK(net_port_submit(&boards[index], &frame, key) == NET_NOT_CONFIGURED);
        CHECK(net_port_poll(&boards[index], &event) == NET_NOT_CONFIGURED);
        CHECK(memcmp(&event, &unchanged, sizeof(event)) == 0);
        CHECK(boards[index].ops->now_ms(NULL, NULL) == NET_NOT_CONFIGURED);
        CHECK(boards[index].ops->submit(NULL, NULL, key) == NET_NOT_CONFIGURED);
        CHECK(boards[index].ops->poll(NULL, NULL) == NET_NOT_CONFIGURED);
    }
    return 0;
}

int main(int argc, char **argv)
{
    static const struct {
        const char *name;
        int (*run)(void);
    } cases[] = {
        {"clocks_and_instances", check_clocks_and_instances},
        {"stable_rx", check_stable_rx},
        {"ring_overflow_and_wrap", check_ring_overflow_and_wrap},
        {"explicit_tx_lifetime", check_explicit_tx_lifetime},
        {"boards_fail_closed", check_boards_fail_closed}
    };
    size_t index;
    bool matched = false;
    if (argc > 2) {
        fprintf(stderr, "usage: %s [case_name]\n", argv[0]);
        return 2;
    }
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        if (argc == 1 || strcmp(argv[1], cases[index].name) == 0) {
            matched = true;
            if (cases[index].run() != 0) {
                return 1;
            }
            printf("platform.%s: PASS\n", cases[index].name);
        }
    }
    if (!matched) {
        fprintf(stderr, "unknown platform case: %s\n", argv[1]);
        return 2;
    }
    printf("platform: %u checks passed (Host fixture; boards NOT_CONFIGURED)\n", checks);
    return 0;
}
