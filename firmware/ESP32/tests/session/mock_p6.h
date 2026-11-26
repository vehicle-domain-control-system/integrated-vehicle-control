#ifndef MOCK_P6_H
#define MOCK_P6_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "p6_session_manager.h"

#define MOCK_P6_ENTROPY_VALUES 128U

typedef struct
{
    bool has_record;
    bool corrupt;
    bool fail_load;
    p6_store_commit_result_t save_result;
    p6_store_commit_result_t erase_result;
    bool apply_unknown_save;
    bool apply_unknown_erase;
    unsigned load_calls;
    unsigned save_calls;
    unsigned erase_calls;
    p6_registration_record_t record;
} mock_registration_store_t;

void mock_registration_store_init(mock_registration_store_t *store);
p6_registration_store_t mock_registration_store_port(
    mock_registration_store_t *store);

typedef struct
{
    uint32_t u32_values[MOCK_P6_ENTROPY_VALUES];
    uint64_t u64_values[MOCK_P6_ENTROPY_VALUES];
    size_t u32_count;
    size_t u64_count;
    size_t u32_index;
    size_t u64_index;
    bool fail_u32;
    bool fail_u64;
} mock_entropy_t;

void mock_entropy_init(mock_entropy_t *entropy);
void mock_entropy_push_u32(mock_entropy_t *entropy, uint32_t value);
void mock_entropy_push_u64(mock_entropy_t *entropy, uint64_t value);
p6_entropy_source_t mock_entropy_port(mock_entropy_t *entropy);

typedef struct
{
    uint64_t now_ms;
} mock_monotonic_clock_t;

void mock_monotonic_clock_init(mock_monotonic_clock_t *clock,
                               uint64_t now_ms);
p6_monotonic_clock_t mock_monotonic_clock_port(mock_monotonic_clock_t *clock);

#endif
