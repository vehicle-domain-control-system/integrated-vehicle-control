#include "mock_p6.h"

static uint64_t mock_now_ms(void *context)
{
    const mock_monotonic_clock_t *clock =
        (const mock_monotonic_clock_t *)context;
    return clock->now_ms;
}

void mock_monotonic_clock_init(mock_monotonic_clock_t *clock,
                               uint64_t now_ms)
{
    clock->now_ms = now_ms;
}

p6_monotonic_clock_t mock_monotonic_clock_port(mock_monotonic_clock_t *clock)
{
    p6_monotonic_clock_t port;

    port.now_ms = mock_now_ms;
    port.context = clock;
    return port;
}
