#include "p6_bond_reset_coordinator.h"

#include <stddef.h>

static p6_bond_reset_status_t finish_reset(
    p6_session_manager_t *manager,
    const p6_bond_reset_port_t *port)
{
    p6_bond_delete_result_t delete_result;
    p6_session_status_t complete_result;

    if (!p6_bond_identity_is_valid(&manager->registered_identity)) {
        return P6_BOND_RESET_REQUIRED;
    }
    delete_result = port->delete_bond(port->context,
                                      &manager->registered_identity);
    if (delete_result != P6_BOND_DELETE_OK &&
        delete_result != P6_BOND_DELETE_NOT_FOUND) {
        return P6_BOND_RESET_REQUIRED;
    }
    complete_result = p6_session_manager_complete_reset(manager);
    return complete_result == P6_SESSION_OK
               ? P6_BOND_RESET_COMPLETE
               : P6_BOND_RESET_REQUIRED;
}

p6_bond_reset_status_t p6_bond_reset_begin(
    p6_session_manager_t *manager,
    const p6_bond_reset_port_t *port)
{
    p6_session_status_t request_result;

    if (manager == NULL || port == NULL || port->delete_bond == NULL) {
        return P6_BOND_RESET_INVALID_ARGUMENT;
    }
    request_result = p6_session_manager_request_reset(manager);
    if (request_result != P6_SESSION_RESET_REQUIRED) {
        return P6_BOND_RESET_REJECTED;
    }
    return finish_reset(manager, port);
}

p6_bond_reset_status_t p6_bond_reset_resume(
    p6_session_manager_t *manager,
    const p6_bond_reset_port_t *port)
{
    if (manager == NULL || port == NULL || port->delete_bond == NULL) {
        return P6_BOND_RESET_INVALID_ARGUMENT;
    }
    if (!manager->initialized || !manager->reset_marker_loaded ||
        manager->reset_state != P6_RESET_STATE_REQUIRED) {
        return P6_BOND_RESET_REJECTED;
    }
    return finish_reset(manager, port);
}
