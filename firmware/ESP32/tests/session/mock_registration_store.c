#include "mock_p6.h"

#include <string.h>

static p6_store_read_result_t mock_load(void *context,
                                        p6_registration_record_t *out_record)
{
    mock_registration_store_t *store = (mock_registration_store_t *)context;

    store->load_calls++;
    if (store->fail_load) {
        return P6_STORE_READ_IO_ERROR;
    }
    if (store->corrupt) {
        return P6_STORE_READ_CORRUPT;
    }
    if (!store->has_record) {
        return P6_STORE_READ_EMPTY;
    }
    *out_record = store->record;
    return P6_STORE_READ_OK;
}

static p6_store_commit_result_t mock_save(
    void *context,
    const p6_registration_record_t *record)
{
    mock_registration_store_t *store = (mock_registration_store_t *)context;

    store->save_calls++;
    if (store->save_result == P6_STORE_NOT_COMMITTED) {
        return P6_STORE_NOT_COMMITTED;
    }
    if ((store->save_result == P6_STORE_COMMIT_UNKNOWN) &&
        !store->apply_unknown_save) {
        return P6_STORE_COMMIT_UNKNOWN;
    }
    store->record = *record;
    store->has_record = true;
    store->corrupt = false;
    return store->save_result;
}

static p6_store_commit_result_t mock_erase(void *context)
{
    mock_registration_store_t *store = (mock_registration_store_t *)context;

    store->erase_calls++;
    if (store->erase_result == P6_STORE_NOT_COMMITTED) {
        return P6_STORE_NOT_COMMITTED;
    }
    if ((store->erase_result == P6_STORE_COMMIT_UNKNOWN) &&
        !store->apply_unknown_erase) {
        return P6_STORE_COMMIT_UNKNOWN;
    }
    memset(&store->record, 0, sizeof(store->record));
    store->has_record = false;
    store->corrupt = false;
    return store->erase_result;
}

void mock_registration_store_init(mock_registration_store_t *store)
{
    memset(store, 0, sizeof(*store));
    store->save_result = P6_STORE_COMMITTED;
    store->erase_result = P6_STORE_COMMITTED;
}

p6_registration_store_t mock_registration_store_port(
    mock_registration_store_t *store)
{
    p6_registration_store_t port;

    port.load = mock_load;
    port.save = mock_save;
    port.erase = mock_erase;
    port.context = store;
    return port;
}
