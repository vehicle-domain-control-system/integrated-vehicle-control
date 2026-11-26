#include "network/net_runtime.h"
#include "network/net_host_mock.h"
#include <stdio.h>

static NetStatus spy(void *context, const NetFrame *frame)
{
    unsigned *received = context;
    ++*received;
    printf("Host spy: link=%u bytes=%u captured=%llu\n", (unsigned)frame->link,
           (unsigned)frame->length, (unsigned long long)frame->captured_at_ms);
    return NET_OK;
}
int main(void)
{
    NetHostMock mock;
    NetRuntime runtime;
    NetRuntimeConfig config;
    NetFrame frame = {0};
    NetSourceStamp source = {1u, 10u, 5u};
    NetAgePolicy policy = {true, 100u, true};
    NetTxKey key;
    NetTxEvent event = {0};
    NetObservation observation;
    unsigned received = 0u;
    net_host_mock_init(&mock);
    config.port = net_host_mock_port(&mock);
    config.local_epoch = 1u;
    config.consumer = spy;
    config.consumer_context = &received;
    config.host_fixture_enabled = true;
    frame.link = NET_LINK_CAN;
    frame.id = 0x123u;
    frame.length = 2u;
    frame.bytes[0] = 0x41u;
    frame.bytes[1] = 0x42u;
    frame.captured_at_ms = 10u;
    if (net_runtime_init(&runtime, &config) != NET_OK ||
        net_host_mock_set_time(&mock, 20u) != NET_OK ||
        net_host_mock_inject_rx(&mock, &frame) != NET_OK ||
        net_runtime_step(&runtime, 1u) != NET_OK || received != 1u ||
        net_runtime_submit(&runtime, &frame, &source, &policy, &key) != NET_OK) return 1;
    event.key = key;
    event.admission_observed = true;
    event.accepted = true;
    event.release_observed = true;
    if (net_host_mock_inject_tx(&mock, &event) != NET_OK ||
        net_runtime_step(&runtime, 0u) != NET_OK ||
        net_runtime_retire(&runtime, key) != NET_HOLD) return 2;
    puts("Host release observed; terminal remains UNKNOWN.");
    event.admission_observed = false;
    event.release_observed = false;
    event.outcome = NET_TX_SUCCEEDED;
    if (net_host_mock_inject_tx(&mock, &event) != NET_OK ||
        net_runtime_step(&runtime, 0u) != NET_OK ||
        net_runtime_retire(&runtime, key) != NET_OK ||
        net_runtime_observation(&runtime, &observation) != NET_OK ||
        observation.tx_succeeded != 1u) return 3;
    puts("Host fixture PASS; product wire/API HOLD; MCU hardware NOT_RUN.");
    return 0;
}
