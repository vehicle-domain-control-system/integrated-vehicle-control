#include "gateway_context_dto.h"

#include <limits.h>
#include <string.h>

static void apply_basis(const gateway_context_source_basis_t *basis,
                        uart_m_value_quality_t *quality,
                        uint32_t *sequence,
                        uint16_t *age_ms)
{
    if (!basis->available) {
        *quality = UART_M_QUALITY_NO_DATA;
        *sequence = 0U;
        *age_ms = UINT16_MAX;
        return;
    }
    *quality = basis->quality;
    *sequence = basis->update_sequence;
    *age_ms = basis->age_ms;
}

static bool snapshot_is_current(const p6_authorization_snapshot_t *snapshot)
{
    return snapshot->state == P6_SESSION_STATE_AUTHORIZED &&
           snapshot->reset_state == P6_RESET_STATE_NONE &&
           snapshot->registered && snapshot->registration_bond_reconciled &&
           snapshot->registration_bond_present && snapshot->connected &&
           snapshot->bonded && snapshot->encrypted &&
           snapshot->authorization_verified && snapshot->session_valid &&
           !snapshot->storage_fault_latched &&
           snapshot->device_context_id != 0U && snapshot->session_id != 0U &&
           snapshot->connection_generation != 0U;
}

gateway_mapper_status_t gateway_context_dto_from_p6_snapshot(
    const p6_authorization_snapshot_t *snapshot,
    const gateway_context_sources_t *sources,
    uart_m_context_t *out_context)
{
    uint8_t validation_payload[GATEWAY_MAPPER_CONTEXT_PAYLOAD_LENGTH];
    size_t validation_length = 0U;

    if (snapshot == NULL || sources == NULL || out_context == NULL) {
        return GATEWAY_MAPPER_INVALID_ARGUMENT;
    }
    if (!snapshot_is_current(snapshot)) {
        return GATEWAY_MAPPER_INVALID_CONTEXT;
    }

    memset(out_context, 0, sizeof(*out_context));
    out_context->context.device_context_id = snapshot->device_context_id;
    out_context->context.session_id = snapshot->session_id;
    out_context->registration = UART_M_TRI_YES;
    out_context->connection = UART_M_TRI_YES;
    out_context->app_active = snapshot->app_instance_known
                                  ? (snapshot->app_active ? UART_M_TRI_YES
                                                          : UART_M_TRI_NO)
                                  : UART_M_TRI_UNKNOWN;

    apply_basis(&sources->registration_connection,
                &out_context->registration_connection_quality,
                &out_context->registration_connection_update_sequence,
                &out_context->registration_connection_age_ms);
    apply_basis(&sources->app, &out_context->app_quality,
                &out_context->app_update_sequence,
                &out_context->app_age_ms);

    if (sources->proximity_available) {
        out_context->proximity = sources->proximity;
        out_context->proximity_validity = sources->proximity_validity;
        out_context->proximity_reason = sources->proximity_reason;
        out_context->proximity_update_sequence =
            sources->proximity_update_sequence;
        out_context->proximity_age_ms = sources->proximity_age_ms;
    } else {
        out_context->proximity = UART_M_TRI_UNKNOWN;
        out_context->proximity_validity = UART_M_VALIDITY_INVALID;
        out_context->proximity_reason = UART_M_QUALITY_REASON_NO_DATA;
        out_context->proximity_update_sequence = 0U;
        out_context->proximity_age_ms = UINT16_MAX;
    }

    return gateway_mapper_encode_context_schema(
        out_context, validation_payload, sizeof(validation_payload),
        &validation_length);
}
