#include "u3_raw_diag.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static u3_status profile(uint32_t id, uint8_t ide, uint8_t fdf,
    uint8_t brs, uint8_t rtr, uint8_t dlc)
{
    if (id > UINT32_C(0x7FF) || ide != 0) return U3_ERR_ID;
    if (id != I1_CIS_REAR_ID && id != I1_DOMAIN_PERMISSION_ID)
        return U3_ERR_UNSUPPORTED;
    if (fdf != 1 || brs != 1 || rtr != 0) return U3_ERR_PROFILE;
    if (dlc != I1_CAN_DLC) return U3_ERR_DLC;
    return U3_OK;
}

u3_status u3_raw_decode(const u3_raw_frame *f, u3_diagnostic *out)
{
    u3_diagnostic temp = {0};
    u3_status r;
    i1_status codec;
    if (f == NULL || out == NULL || f->payload == NULL) return U3_ERR_ARGUMENT;
    r = profile(f->can_id, f->ide, f->fdf, f->brs, f->rtr, f->dlc);
    if (r != U3_OK) return r;
    /* Validate before accessing any payload byte, including padding. */
    if (f->length != I1_CAN_BYTES) return U3_ERR_LENGTH;
    if (f->can_id == I1_CIS_REAR_ID)
        codec = i1_cis_rear_decode(f->can_id, f->dlc, f->payload,
            f->length, &temp.fields.rear);
    else
        codec = i1_domain_permission_decode(f->can_id, f->dlc, f->payload,
            f->length, &temp.fields.permission);
    if (codec != I1_OK) return (u3_status)codec;
    temp.can_id = f->can_id; temp.received_at_ms = f->received_at_ms;
    temp.ide = f->ide; temp.fdf = f->fdf; temp.brs = f->brs;
    temp.rtr = f->rtr; temp.dlc = f->dlc;
    memcpy(temp.raw, f->payload, I1_CAN_BYTES);
    *out = temp;
    return U3_OK;
}

u3_status u3_raw_encode(const u3_diagnostic *d, uint8_t *out,
    size_t capacity, size_t *written)
{
    u3_status r;
    if (written != NULL) *written = 0;
    if (d == NULL || out == NULL || written == NULL) return U3_ERR_ARGUMENT;
    r = profile(d->can_id, d->ide, d->fdf, d->brs, d->rtr, d->dlc);
    if (r != U3_OK) return r;
    if (d->can_id == I1_CIS_REAR_ID)
        return (u3_status)i1_cis_rear_encode(&d->fields.rear, out, capacity, written);
    return (u3_status)i1_domain_permission_encode(&d->fields.permission, out, capacity, written);
}

u3_status u3_raw_format(const u3_diagnostic *d, char *out,
    size_t capacity, size_t *written)
{
    static const char *const status_names[] = {
        "VALID_DISTANCE", "NO_OBJECT", "UNAVAILABLE", "FAULT", "RECOVERING", "INACTIVE"
    };
    static const char *const reason_names[] = {
        "NONE", "NOT_READY", "OUT_OF_RANGE", "SENSOR_FAULT", "VISION_FAULT",
        "STALE", "NO_DATA", "COMMUNICATION_FAULT", "RECOVERING", "DATA_INVALID"
    };
    static const char *const quality_names[] = {"OK", "STALE", "INVALID", "NO_DATA"};
    char temp[1024], hex[2 * I1_CAN_BYTES + 1];
    uint8_t encoded[I1_CAN_BYTES];
    size_t i, n;
    int prefix, suffix;
    u3_status r;
    if (written != NULL) *written = 0;
    if (d == NULL || out == NULL || written == NULL) return U3_ERR_ARGUMENT;
    /* Also validate code ranges before indexing name tables. Keep text and raw
     * provenance consistent: callers cannot silently edit a decoded snapshot. */
    r = u3_raw_encode(d, encoded, sizeof encoded, &n);
    if (r != U3_OK) return r;
    if (memcmp(encoded, d->raw, I1_CAN_BYTES) != 0) return U3_ERR_VALUE;
    for (i = 0; i < I1_CAN_BYTES; ++i) {
        static const char digits[] = "0123456789ABCDEF";
        hex[2*i] = digits[d->raw[i] >> 4]; hex[2*i+1] = digits[d->raw[i] & 15u];
    }
    hex[2*I1_CAN_BYTES] = '\0';
    prefix = snprintf(temp, sizeof temp,
        "WIRE_PROPOSED id=0x%03" PRIX32 " ide=%u fdf=%u brs=%u rtr=%u dlc=%u bytes=20 used=19 pad=0 rx_local_ms=%" PRIu32 " raw=%s ",
        d->can_id, (unsigned)d->ide, (unsigned)d->fdf, (unsigned)d->brs,
        (unsigned)d->rtr, (unsigned)d->dlc, d->received_at_ms, hex);
    if (prefix < 0 || (size_t)prefix >= sizeof temp) return U3_ERR_CAPACITY;
    if (d->can_id == I1_CIS_REAR_ID) {
        const i1_cis_rear *f = &d->fields.rear;
        suffix = snprintf(temp + prefix, sizeof temp - (size_t)prefix,
            "CIS_REAR boot=%" PRIu32 " tx=%" PRIu32 " update=%" PRIu32 " source_age_ms=%u age=%s distance_raw_0_1cm=%u sentinel=%u validity=%u(%s) reason=%u(%s) proximity=%u(%s) degraded=%u unit_binding=UNRESOLVED_CONTRACT policy=UNRESOLVED_CONTRACT",
            f->boot_id, f->tx_sequence, f->update_sequence, (unsigned)f->source_age_ms,
            f->source_age_ms == UINT16_MAX ? "SATURATED_LOWER_BOUND" : "RAW_UNASSESSED",
            (unsigned)f->rear_distance_deci_cm,
            (unsigned)(f->rear_distance_deci_cm == UINT16_MAX), (unsigned)f->validity,
            f->validity ? "VALID" : "INVALID", (unsigned)f->quality_reason,
            reason_names[f->quality_reason], (unsigned)f->proximity_status,
            status_names[f->proximity_status],
            (unsigned)(f->validity == 0 || f->quality_reason != 0 ||
                f->proximity_status >= 2 || f->source_age_ms == UINT16_MAX));
    } else {
        const i1_domain_permission *f = &d->fields.permission;
        suffix = snprintf(temp + prefix, sizeof temp - (size_t)prefix,
            "DOMAIN_PERMISSION boot=%" PRIu32 " tx=%" PRIu32 " update=%" PRIu32 " source_age_ms=%u age=%s power_raw=%u(%s) power_quality=%u(%s) operation_state_raw=%u(%s) operation_allowed_raw=%u operation_quality=%u(%s) degraded=%u policy=UNRESOLVED_CONTRACT producer_coherence_source_limit=UNRESOLVED_CONTRACT",
            f->boot_id, f->tx_sequence, f->update_sequence, (unsigned)f->source_age_ms,
            f->source_age_ms == UINT16_MAX ? "SATURATED_LOWER_BOUND" : "RAW_UNASSESSED",
            (unsigned)f->vehicle_power_permission,
            f->vehicle_power_permission == 255 ? "UNKNOWN" : "RAW_BIT",
            (unsigned)f->power_permission_quality, quality_names[f->power_permission_quality],
            (unsigned)f->power_operation_state,
            f->power_operation_state == 255 ? "UNKNOWN" : "RAW_BIT",
            (unsigned)f->operation_allowed, (unsigned)f->operation_quality,
            quality_names[f->operation_quality],
            (unsigned)(f->power_permission_quality != 0 || f->operation_quality != 0 ||
                f->vehicle_power_permission == 255 || f->power_operation_state == 255 ||
                f->source_age_ms == UINT16_MAX));
    }
    if (suffix < 0 || (size_t)suffix >= sizeof temp - (size_t)prefix)
        return U3_ERR_CAPACITY;
    n = (size_t)prefix + (size_t)suffix;
    if (capacity <= n) return U3_ERR_CAPACITY;
    memcpy(out, temp, n + 1); *written = n;
    return U3_OK;
}
