#include "gateway_request_admission.h"

#include <string.h>

static bool request_key_equal(const uart_m_request_t *left,
                              const uart_m_request_t *right)
{
    return left->context.device_context_id == right->context.device_context_id &&
           left->context.session_id == right->context.session_id &&
           left->request_id == right->request_id;
}

static gateway_request_candidate_t take_oldest(
    gateway_request_admission_t *admission)
{
    gateway_request_candidate_t candidate = admission->candidates[admission->head];

    memset(&admission->candidates[admission->head], 0,
           sizeof(admission->candidates[admission->head]));
    admission->head =
        (admission->head + 1U) % GATEWAY_REQUEST_ADMISSION_CAPACITY;
    admission->count--;
    return candidate;
}

void gateway_request_admission_init(
    gateway_request_admission_t *admission,
    const gateway_request_admission_config_t *config)
{
    if (admission == NULL) {
        return;
    }
    memset(admission, 0, sizeof(*admission));
    if (config != NULL) {
        admission->config = *config;
    }
}

gateway_request_admission_status_t gateway_request_admission_capture(
    gateway_request_admission_t *admission,
    const gateway_request_candidate_t *candidate)
{
    size_t index;

    if (admission == NULL || candidate == NULL ||
        candidate->request.context.device_context_id == 0U ||
        candidate->request.context.session_id == 0U ||
        candidate->request.request_id == 0U ||
        candidate->connection_handle == UINT16_MAX ||
        candidate->connection_generation == 0U ||
        candidate->connection_epoch == 0U || candidate->domain_boot_id == 0U) {
        return GATEWAY_REQUEST_ADMISSION_INVALID_ARGUMENT;
    }
    if (admission->config.ttl_ms == 0U) {
        return GATEWAY_REQUEST_ADMISSION_POLICY_UNCONFIGURED;
    }
    if (!candidate->received_at_known) {
        return GATEWAY_REQUEST_ADMISSION_TIMESTAMP_UNKNOWN;
    }
    for (index = 0U; index < admission->count; ++index) {
        size_t slot = (admission->head + index) %
                      GATEWAY_REQUEST_ADMISSION_CAPACITY;
        if (request_key_equal(&admission->candidates[slot].request,
                              &candidate->request)) {
            return GATEWAY_REQUEST_ADMISSION_DUPLICATE;
        }
    }
    if (admission->count >= GATEWAY_REQUEST_ADMISSION_CAPACITY) {
        return GATEWAY_REQUEST_ADMISSION_QUEUE_FULL;
    }

    admission->candidates[admission->tail] = *candidate;
    admission->tail =
        (admission->tail + 1U) % GATEWAY_REQUEST_ADMISSION_CAPACITY;
    admission->count++;
    return GATEWAY_REQUEST_ADMISSION_OK;
}

gateway_request_admission_status_t gateway_request_admission_prepare(
    gateway_request_admission_t *admission,
    const gateway_request_admission_evidence_t *evidence,
    uart_m_request_t *out_request)
{
    gateway_request_candidate_t candidate;
    const p6_authorization_snapshot_t *authorization;

    if (admission == NULL || evidence == NULL || out_request == NULL) {
        return GATEWAY_REQUEST_ADMISSION_INVALID_ARGUMENT;
    }
    memset(out_request, 0, sizeof(*out_request));
    if (admission->count == 0U) {
        return GATEWAY_REQUEST_ADMISSION_NO_CANDIDATE;
    }
    candidate = take_oldest(admission);
    if (admission->config.ttl_ms == 0U) {
        return GATEWAY_REQUEST_ADMISSION_POLICY_UNCONFIGURED;
    }
    if (!candidate.received_at_known || !evidence->now_known) {
        return GATEWAY_REQUEST_ADMISSION_TIMESTAMP_UNKNOWN;
    }
    if (evidence->now_ms < candidate.received_at_ms) {
        return GATEWAY_REQUEST_ADMISSION_CLOCK_INVALID;
    }
    if ((evidence->now_ms - candidate.received_at_ms) >=
        (uint64_t)admission->config.ttl_ms) {
        return GATEWAY_REQUEST_ADMISSION_TTL_EXPIRED;
    }
    if (!evidence->connection.available || !evidence->connection.connected ||
        evidence->connection.connection_handle != candidate.connection_handle ||
        evidence->connection.connection_generation !=
            candidate.connection_generation ||
        evidence->connection.connection_epoch != candidate.connection_epoch) {
        return GATEWAY_REQUEST_ADMISSION_CONNECTION_CHANGED;
    }
    if (!evidence->domain_link_up || evidence->domain_boot_id == 0U ||
        evidence->domain_boot_id != candidate.domain_boot_id) {
        return GATEWAY_REQUEST_ADMISSION_DOMAIN_CHANGED;
    }

    authorization = &evidence->authorization;
    if (authorization->connection_generation != candidate.connection_generation ||
        authorization->device_context_id !=
            candidate.request.context.device_context_id ||
        authorization->session_id != candidate.request.context.session_id) {
        return GATEWAY_REQUEST_ADMISSION_SESSION_CHANGED;
    }
    if (authorization->state != P6_SESSION_STATE_AUTHORIZED ||
        !authorization->registered || !authorization->connected ||
        !authorization->bonded || !authorization->encrypted ||
        !authorization->authorization_verified ||
        !authorization->session_valid ||
        !authorization->registration_bond_reconciled ||
        !authorization->registration_bond_present ||
        authorization->storage_fault_latched ||
        authorization->reset_state != P6_RESET_STATE_NONE ||
        !authorization->domain_boot_known ||
        authorization->domain_boot_id != candidate.domain_boot_id) {
        return GATEWAY_REQUEST_ADMISSION_NOT_AUTHORIZED;
    }

    *out_request = candidate.request;
    return GATEWAY_REQUEST_ADMISSION_OK;
}

void gateway_request_admission_invalidate(
    gateway_request_admission_t *admission)
{
    gateway_request_admission_config_t config;

    if (admission == NULL) {
        return;
    }
    config = admission->config;
    memset(admission, 0, sizeof(*admission));
    admission->config = config;
}

size_t gateway_request_admission_pending(
    const gateway_request_admission_t *admission)
{
    return admission != NULL ? admission->count : 0U;
}
