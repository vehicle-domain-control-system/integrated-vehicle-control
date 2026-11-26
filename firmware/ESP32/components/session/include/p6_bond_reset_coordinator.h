#ifndef P6_BOND_RESET_COORDINATOR_H
#define P6_BOND_RESET_COORDINATOR_H

#include "p6_session_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define P6_DEVELOPER_BOND_RESET_ENABLED 0

typedef enum
{
    P6_BOND_DELETE_OK = 0,
    P6_BOND_DELETE_NOT_FOUND,
    P6_BOND_DELETE_FAILED,
    P6_BOND_DELETE_UNKNOWN
} p6_bond_delete_result_t;

typedef p6_bond_delete_result_t (*p6_delete_bond_fn)(
    void *context,
    const p6_bond_identity_t *identity);

typedef struct
{
    p6_delete_bond_fn delete_bond;
    void *context;
} p6_bond_reset_port_t;

typedef enum
{
    P6_BOND_RESET_COMPLETE = 0,
    P6_BOND_RESET_REQUIRED,
    P6_BOND_RESET_REJECTED,
    P6_BOND_RESET_INVALID_ARGUMENT
} p6_bond_reset_status_t;

/* Single-owner calls only. No BLE command is bound to either API. */
p6_bond_reset_status_t p6_bond_reset_begin(
    p6_session_manager_t *manager,
    const p6_bond_reset_port_t *port);
p6_bond_reset_status_t p6_bond_reset_resume(
    p6_session_manager_t *manager,
    const p6_bond_reset_port_t *port);

#ifdef __cplusplus
}
#endif

#endif
