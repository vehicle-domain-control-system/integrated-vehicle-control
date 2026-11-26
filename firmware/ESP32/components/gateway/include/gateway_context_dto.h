#ifndef GATEWAY_CONTEXT_DTO_H
#define GATEWAY_CONTEXT_DTO_H

#include <stdbool.h>
#include <stdint.h>

#include "gateway_mapper.h"
#include "p6_session_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool available;
    uart_m_value_quality_t quality;
    uint32_t update_sequence;
    uint16_t age_ms;
} gateway_context_source_basis_t;

typedef struct
{
    gateway_context_source_basis_t registration_connection;
    gateway_context_source_basis_t app;
    bool proximity_available;
    uart_m_tri_state_t proximity;
    uart_m_validity_t proximity_validity;
    uart_m_quality_reason_t proximity_reason;
    uint32_t proximity_update_sequence;
    uint16_t proximity_age_ms;
} gateway_context_sources_t;

/*
 * Pure DTO preparation only: no UART frame, queue or transmission is created.
 * P6 identity/session facts must already be current and authorized. Missing
 * producer metadata is represented as NO_DATA/unknown, never as fresh data.
 */
gateway_mapper_status_t gateway_context_dto_from_p6_snapshot(
    const p6_authorization_snapshot_t *snapshot,
    const gateway_context_sources_t *sources,
    uart_m_context_t *out_context);

#ifdef __cplusplus
}
#endif

#endif
