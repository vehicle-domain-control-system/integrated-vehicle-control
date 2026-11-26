#ifndef GATEWAY_REQUEST_ADMISSION_H
#define GATEWAY_REQUEST_ADMISSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ble_connection_state.h"
#include "p6_session_manager.h"
#include "uart_m_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GATEWAY_REQUEST_ADMISSION_CAPACITY 4U

typedef enum {
    GATEWAY_REQUEST_ADMISSION_OK = 0,
    GATEWAY_REQUEST_ADMISSION_NO_CANDIDATE,
    GATEWAY_REQUEST_ADMISSION_INVALID_ARGUMENT,
    GATEWAY_REQUEST_ADMISSION_POLICY_UNCONFIGURED,
    GATEWAY_REQUEST_ADMISSION_QUEUE_FULL,
    GATEWAY_REQUEST_ADMISSION_DUPLICATE,
    GATEWAY_REQUEST_ADMISSION_TIMESTAMP_UNKNOWN,
    GATEWAY_REQUEST_ADMISSION_CLOCK_INVALID,
    GATEWAY_REQUEST_ADMISSION_TTL_EXPIRED,
    GATEWAY_REQUEST_ADMISSION_CONNECTION_CHANGED,
    GATEWAY_REQUEST_ADMISSION_SESSION_CHANGED,
    GATEWAY_REQUEST_ADMISSION_DOMAIN_CHANGED,
    GATEWAY_REQUEST_ADMISSION_NOT_AUTHORIZED,
} gateway_request_admission_status_t;

typedef struct {
    uint32_t ttl_ms;
} gateway_request_admission_config_t;

typedef struct {
    uart_m_request_t request;
    bool received_at_known;
    uint64_t received_at_ms;
    uint16_t connection_handle;
    uint32_t connection_generation;
    uint32_t connection_epoch;
    uint32_t domain_boot_id;
} gateway_request_candidate_t;

typedef struct {
    bool now_known;
    uint64_t now_ms;
    bool domain_link_up;
    uint32_t domain_boot_id;
    ble_connection_snapshot_t connection;
    p6_authorization_snapshot_t authorization;
} gateway_request_admission_evidence_t;

typedef struct {
    gateway_request_admission_config_t config;
    gateway_request_candidate_t candidates[GATEWAY_REQUEST_ADMISSION_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
} gateway_request_admission_t;

/*
 * Observation-only admission boundary. It never calls UART Transport and does
 * not create source Quality/Age/Sequence. ttl_ms=0 intentionally keeps the
 * boundary fail-closed until an explicit policy is supplied.
 */
void gateway_request_admission_init(
    gateway_request_admission_t *admission,
    const gateway_request_admission_config_t *config);

gateway_request_admission_status_t gateway_request_admission_capture(
    gateway_request_admission_t *admission,
    const gateway_request_candidate_t *candidate);

/*
 * Revalidates and consumes the oldest copied candidate immediately before a
 * possible future UART enqueue. Any rejected candidate is consumed and cannot
 * be replayed. A successful result is still only a Host-observable candidate.
 */
gateway_request_admission_status_t gateway_request_admission_prepare(
    gateway_request_admission_t *admission,
    const gateway_request_admission_evidence_t *evidence,
    uart_m_request_t *out_request);

void gateway_request_admission_invalidate(
    gateway_request_admission_t *admission);
size_t gateway_request_admission_pending(
    const gateway_request_admission_t *admission);

#ifdef __cplusplus
}
#endif

#endif
