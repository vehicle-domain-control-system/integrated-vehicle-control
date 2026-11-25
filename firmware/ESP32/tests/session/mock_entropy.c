#include "mock_p6.h"

#include <string.h>

static bool mock_next_u32(void *context, uint32_t *out_value)
{
    mock_entropy_t *entropy = (mock_entropy_t *)context;

    if (entropy->fail_u32 || (entropy->u32_index >= entropy->u32_count)) {
        return false;
    }
    *out_value = entropy->u32_values[entropy->u32_index++];
    return true;
}

static bool mock_next_u64(void *context, uint64_t *out_value)
{
    mock_entropy_t *entropy = (mock_entropy_t *)context;

    if (entropy->fail_u64 || (entropy->u64_index >= entropy->u64_count)) {
        return false;
    }
    *out_value = entropy->u64_values[entropy->u64_index++];
    return true;
}

void mock_entropy_init(mock_entropy_t *entropy)
{
    memset(entropy, 0, sizeof(*entropy));
}

void mock_entropy_push_u32(mock_entropy_t *entropy, uint32_t value)
{
    if (entropy->u32_count < MOCK_P6_ENTROPY_VALUES) {
        entropy->u32_values[entropy->u32_count++] = value;
    }
}

void mock_entropy_push_u64(mock_entropy_t *entropy, uint64_t value)
{
    if (entropy->u64_count < MOCK_P6_ENTROPY_VALUES) {
        entropy->u64_values[entropy->u64_count++] = value;
    }
}

p6_entropy_source_t mock_entropy_port(mock_entropy_t *entropy)
{
    p6_entropy_source_t port;

    port.next_u32 = mock_next_u32;
    port.next_u64 = mock_next_u64;
    port.context = entropy;
    return port;
}
