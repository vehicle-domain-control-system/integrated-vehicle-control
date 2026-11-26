#ifndef P6_REGISTRATION_STORE_H
#define P6_REGISTRATION_STORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define P6_BOND_IDENTITY_BYTES 6U
#define P6_REGISTRATION_SCHEMA_VERSION 2U

typedef struct
{
    uint8_t address_type;
    uint8_t bytes[P6_BOND_IDENTITY_BYTES];
} p6_bond_identity_t;

typedef enum
{
    P6_REGISTRATION_RECORD_ACTIVE = 1,
    P6_REGISTRATION_RECORD_RESET_PENDING = 2
} p6_registration_record_state_t;

typedef struct
{
    uint32_t schema_version;
    p6_registration_record_state_t state;
    p6_bond_identity_t bond_identity;
    uint32_t device_context_id;
    uint32_t registration_epoch;
} p6_registration_record_t;

typedef enum
{
    P6_STORE_READ_OK = 0,
    P6_STORE_READ_EMPTY,
    P6_STORE_READ_CORRUPT,
    P6_STORE_READ_IO_ERROR
} p6_store_read_result_t;

/*
 * A mutation returning UNKNOWN may or may not be durable.  Callers must fail
 * closed in the current boot.  No API can prove a reset intent after reboot
 * when the store reports NOT_COMMITTED or UNKNOWN and leaves an old ACTIVE
 * record; a production adapter therefore needs a transactional reset gate.
 */
typedef enum
{
    P6_STORE_COMMITTED = 0,
    P6_STORE_NOT_COMMITTED,
    P6_STORE_COMMIT_UNKNOWN
} p6_store_commit_result_t;

typedef p6_store_read_result_t (*p6_registration_store_load_fn)(
    void *context,
    p6_registration_record_t *out_record);
typedef p6_store_commit_result_t (*p6_registration_store_save_fn)(
    void *context,
    const p6_registration_record_t *record);
typedef p6_store_commit_result_t (*p6_registration_store_erase_fn)(void *context);

typedef struct
{
    p6_registration_store_load_fn load;
    p6_registration_store_save_fn save;
    p6_registration_store_erase_fn erase;
    void *context;
} p6_registration_store_t;

bool p6_bond_identity_is_valid(const p6_bond_identity_t *identity);
bool p6_bond_identity_equal(const p6_bond_identity_t *left,
                            const p6_bond_identity_t *right);
bool p6_registration_record_is_valid(const p6_registration_record_t *record);

#ifdef __cplusplus
}
#endif

#endif
