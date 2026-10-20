#include "network/net_wire_can_nodes.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
#define CANDIDATE NET_WIRE_CANDIDATE_ONLY

/* Independent fixed literals from nodes_golden.py (whole-frame struct.pack).
 * Never initialized with codec output. DTOs below are input scalar fixtures. */
static const uint8_t golden_window_command[24] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0xcc,0xbb,0xaa,0x99,
    0x34,0x12,0x78,0x56,0x05,0x00,0x25,0x04,
};
static NetWireWindowCommandRaw example_window_command(void)
{
    NetWireWindowCommandRaw p = {0};
    p.boot_id = 16909060u;
    p.tx_sequence = 287454020u;
    p.target_boot_id = 1432778632u;
    p.command_id = 2578103244u;
    p.command_age_ms = 4660u;
    p.accept_limit_ms = 22136u;
    p.window_command = 5u;
    p.target_channel = 0u;
    p.target_position = 37u;
    p.source_context = 4u;
    return p;
}
static int equal_window_command(const NetWireWindowCommandRaw *a, const NetWireWindowCommandRaw *b)
{
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->target_boot_id == b->target_boot_id &&
        a->command_id == b->command_id &&
        a->command_age_ms == b->command_age_ms &&
        a->accept_limit_ms == b->accept_limit_ms &&
        a->window_command == b->window_command &&
        a->target_channel == b->target_channel &&
        a->target_position == b->target_position &&
        a->source_context == b->source_context;
}

static const uint8_t golden_window_state[24] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0x02,0x02,0x25,0x01,0x00,0x00,0x01,0x01,
    0xd4,0xc3,0xb2,0xa1,0xdc,0xfe,0x00,0x00,
};
static NetWireWindowStateRaw example_window_state(void)
{
    NetWireWindowStateRaw p = {0};
    p.boot_id = 16909060u;
    p.tx_sequence = 287454020u;
    p.motion_state = 2u;
    p.window_ecu_state = 2u;
    p.window_position = 37u;
    p.window_value_quality = 1u;
    p.fully_open_state = 0u;
    p.fully_closed_state = 0u;
    p.antipinch_protection_status = 1u;
    p.reverse_state = 1u;
    p.update_sequence = 2712847316u;
    p.source_age_ms = 65244u;
    return p;
}
static int equal_window_state(const NetWireWindowStateRaw *a, const NetWireWindowStateRaw *b)
{
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->motion_state == b->motion_state &&
        a->window_ecu_state == b->window_ecu_state &&
        a->window_position == b->window_position &&
        a->window_value_quality == b->window_value_quality &&
        a->fully_open_state == b->fully_open_state &&
        a->fully_closed_state == b->fully_closed_state &&
        a->antipinch_protection_status == b->antipinch_protection_status &&
        a->reverse_state == b->reverse_state &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}

static const uint8_t golden_window_event[32] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0x03,0x00,0xd4,0xc3,0xb2,0xa1,0x88,0x77,
    0x66,0x55,0xcc,0xbb,0xaa,0x99,0x05,0x34,0x12,0x02,0x00,0x00,0x00,0x00,0x00,0x00,
};
static NetWireWindowEventRaw example_window_event(void)
{
    NetWireWindowEventRaw p = {0};
    p.boot_id = 16909060u;
    p.tx_sequence = 287454020u;
    p.event_kind = 3u;
    p.target_channel = 0u;
    p.occurrence_id = 2712847316u;
    p.related_domain_boot = 1432778632u;
    p.related_request_sequence = 2578103244u;
    p.origin = 5u;
    p.event_age_ms = 4660u;
    p.event_quality = 2u;
    return p;
}
static int equal_window_event(const NetWireWindowEventRaw *a, const NetWireWindowEventRaw *b)
{
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->event_kind == b->event_kind &&
        a->target_channel == b->target_channel &&
        a->occurrence_id == b->occurrence_id &&
        a->related_domain_boot == b->related_domain_boot &&
        a->related_request_sequence == b->related_request_sequence &&
        a->origin == b->origin &&
        a->event_age_ms == b->event_age_ms &&
        a->event_quality == b->event_quality;
}

static const uint8_t golden_window_result[20] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0xcc,0xbb,0xaa,0x99,
    0x00,0x05,0x19,0x00,
};
static NetWireWindowResultRaw example_window_result(void)
{
    NetWireWindowResultRaw p = {0};
    p.boot_id = 16909060u;
    p.tx_sequence = 287454020u;
    p.request_domain_boot = 1432778632u;
    p.request_sequence = 2578103244u;
    p.target_channel = 0u;
    p.result = 5u;
    p.result_reason = 25u;
    return p;
}
static int equal_window_result(const NetWireWindowResultRaw *a, const NetWireWindowResultRaw *b)
{
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->request_domain_boot == b->request_domain_boot &&
        a->request_sequence == b->request_sequence &&
        a->target_channel == b->target_channel &&
        a->result == b->result &&
        a->result_reason == b->result_reason;
}

static const uint8_t golden_window_fault[24] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0x02,0xff,0x00,0x00,0x88,0x77,0x66,0x55,
    0xcc,0xbb,0xaa,0x99,0xff,0xff,0x00,0x00,
};
static NetWireWindowFaultRaw example_window_fault(void)
{
    NetWireWindowFaultRaw p = {0};
    p.boot_id = 16909060u;
    p.tx_sequence = 287454020u;
    p.fault_category = 2u;
    p.fault_code = 255u;
    p.fault_status = 0u;
    p.occurrence_id = 1432778632u;
    p.update_sequence = 2578103244u;
    p.source_age_ms = 65535u;
    return p;
}
static int equal_window_fault(const NetWireWindowFaultRaw *a, const NetWireWindowFaultRaw *b)
{
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->fault_category == b->fault_category &&
        a->fault_code == b->fault_code &&
        a->fault_status == b->fault_status &&
        a->occurrence_id == b->occurrence_id &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}

static const uint8_t golden_cis_environment[48] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0x60,0xf0,0x55,0x44,0x33,0x22,0x34,0x12,
    0x01,0x00,0x85,0x1a,0x66,0x55,0x44,0x33,0x56,0x34,0x01,0x00,0x40,0xe2,0x01,0x00,
    0x77,0x66,0x55,0x44,0x78,0x56,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static NetWireCisEnvironmentRaw example_cis_environment(void)
{
    NetWireCisEnvironmentRaw p = {0};
    p.boot_id = 16909060u;
    p.tx_sequence = 287454020u;
    p.temperature_centi_c = -4000;
    p.temperature_sequence = 573785173u;
    p.temperature_age_ms = 4660u;
    p.temperature_validity = 1u;
    p.temperature_reason = 0u;
    p.humidity_centi_percent = 6789u;
    p.humidity_sequence = 860116326u;
    p.humidity_age_ms = 13398u;
    p.humidity_validity = 1u;
    p.humidity_reason = 0u;
    p.illuminance_lux = 123456u;
    p.illuminance_sequence = 1146447479u;
    p.illuminance_age_ms = 22136u;
    p.illuminance_validity = 1u;
    p.illuminance_reason = 0u;
    return p;
}
static int equal_cis_environment(const NetWireCisEnvironmentRaw *a, const NetWireCisEnvironmentRaw *b)
{
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->temperature_centi_c == b->temperature_centi_c &&
        a->temperature_sequence == b->temperature_sequence &&
        a->temperature_age_ms == b->temperature_age_ms &&
        a->temperature_validity == b->temperature_validity &&
        a->temperature_reason == b->temperature_reason &&
        a->humidity_centi_percent == b->humidity_centi_percent &&
        a->humidity_sequence == b->humidity_sequence &&
        a->humidity_age_ms == b->humidity_age_ms &&
        a->humidity_validity == b->humidity_validity &&
        a->humidity_reason == b->humidity_reason &&
        a->illuminance_lux == b->illuminance_lux &&
        a->illuminance_sequence == b->illuminance_sequence &&
        a->illuminance_age_ms == b->illuminance_age_ms &&
        a->illuminance_validity == b->illuminance_validity &&
        a->illuminance_reason == b->illuminance_reason;
}

static const uint8_t golden_cis_occupant[32] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0xcc,0xbb,0xaa,0x99,
    0x34,0x12,0x01,0x01,0x00,0x03,0x01,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static NetWireCisOccupantRaw example_cis_occupant(void)
{
    NetWireCisOccupantRaw p = {0};
    p.boot_id = 16909060u;
    p.tx_sequence = 287454020u;
    p.pi_boot_id = 1432778632u;
    p.vision_sequence = 2578103244u;
    p.vision_age_ms = 4660u;
    p.occupant_presence = 1u;
    p.presence_validity = 1u;
    p.presence_reason = 0u;
    p.occupant_count = 3u;
    p.count_validity = 1u;
    p.count_reason = 0u;
    p.vision_function_status = 2u;
    return p;
}
static int equal_cis_occupant(const NetWireCisOccupantRaw *a, const NetWireCisOccupantRaw *b)
{
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->pi_boot_id == b->pi_boot_id &&
        a->vision_sequence == b->vision_sequence &&
        a->vision_age_ms == b->vision_age_ms &&
        a->occupant_presence == b->occupant_presence &&
        a->presence_validity == b->presence_validity &&
        a->presence_reason == b->presence_reason &&
        a->occupant_count == b->occupant_count &&
        a->count_validity == b->count_validity &&
        a->count_reason == b->count_reason &&
        a->vision_function_status == b->vision_function_status;
}

static const uint8_t golden_cis_status[48] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0x03,0x02,0x01,0x04,0x03,0x00,0x02,0x01,
    0xa5,0x01,0x09,0x00,0x01,0x08,0x00,0x88,0x77,0x66,0x55,0x2d,0xcc,0xbb,0xaa,0x99,
    0xdc,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static NetWireCisStatusRaw example_cis_status(void)
{
    NetWireCisStatusRaw p = {0};
    p.boot_id = 16909060u;
    p.tx_sequence = 287454020u;
    p.cis_state = 3u;
    p.vision_status = 2u;
    p.temperature_status = 1u;
    p.humidity_status = 4u;
    p.illuminance_status = 3u;
    p.rear_status = 0u;
    p.pi_uart_status = 2u;
    p.can_interface_status = 1u;
    p.fault_mask = 421u;
    p.active_fault = 9u;
    p.recovering = 1u;
    p.last_fault = 8u;
    p.last_fault_occurrence = 1432778632u;
    p.affected_function = 45u;
    p.update_sequence = 2578103244u;
    p.source_age_ms = 65244u;
    return p;
}
static int equal_cis_status(const NetWireCisStatusRaw *a, const NetWireCisStatusRaw *b)
{
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->cis_state == b->cis_state &&
        a->vision_status == b->vision_status &&
        a->temperature_status == b->temperature_status &&
        a->humidity_status == b->humidity_status &&
        a->illuminance_status == b->illuminance_status &&
        a->rear_status == b->rear_status &&
        a->pi_uart_status == b->pi_uart_status &&
        a->can_interface_status == b->can_interface_status &&
        a->fault_mask == b->fault_mask &&
        a->active_fault == b->active_fault &&
        a->recovering == b->recovering &&
        a->last_fault == b->last_fault &&
        a->last_fault_occurrence == b->last_fault_occurrence &&
        a->affected_function == b->affected_function &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}

static const uint8_t golden_vss_event[24] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0x05,0x88,0x77,0x66,0x55,0x34,0x12,0x78,
    0x56,0x01,0x01,0xcc,0xbb,0xaa,0x99,0x00,
};
static NetWireVssEventRaw example_vss_event(void)
{
    NetWireVssEventRaw p = {0};
    p.boot_id = 16909060u;
    p.tx_sequence = 287454020u;
    p.event_kind = 5u;
    p.occurrence_id = 1432778632u;
    p.original_age_ms = 4660u;
    p.use_limit_ms = 22136u;
    p.value_quality = 1u;
    p.input_availability = 1u;
    p.related_command_id = 2578103244u;
    return p;
}
static int equal_vss_event(const NetWireVssEventRaw *a, const NetWireVssEventRaw *b)
{
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->event_kind == b->event_kind &&
        a->occurrence_id == b->occurrence_id &&
        a->original_age_ms == b->original_age_ms &&
        a->use_limit_ms == b->use_limit_ms &&
        a->value_quality == b->value_quality &&
        a->input_availability == b->input_availability &&
        a->related_command_id == b->related_command_id;
}

static const uint8_t golden_vss_warning_state[64] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0x00,0x03,0x06,0x02,0x55,0x44,0x33,0x22,0x66,0x55,0x44,0x33,0x34,0x12,
    0x01,0x00,0x00,0x01,0x88,0x77,0x66,0x55,0xcc,0xbb,0xaa,0x99,0x45,0x23,0x02,0x00,
    0x00,0x00,0x01,0x01,0x00,0x00,0xaa,0x99,0x88,0x77,0x56,0x34,0x00,0x00,0x00,0x00,
};
static NetWireVssWarningStateRaw example_vss_warning_state(void)
{
    NetWireVssWarningStateRaw p = {0};
    p.boot_id = 16909060u;
    p.tx_sequence = 287454020u;
    p.antipinch_source_boot = 0u;
    p.antipinch_source_sequence = 0u;
    p.antipinch_original_age_ms = 65535u;
    p.antipinch_state = 0u;
    p.antipinch_quality = 3u;
    p.antipinch_reason = 6u;
    p.antipinch_availability = 2u;
    p.occupant_hazard_source_boot = 573785173u;
    p.occupant_hazard_source_sequence = 860116326u;
    p.occupant_hazard_original_age_ms = 4660u;
    p.occupant_hazard_state = 1u;
    p.occupant_hazard_quality = 0u;
    p.occupant_hazard_reason = 0u;
    p.occupant_hazard_availability = 1u;
    p.rear_obstacle_source_boot = 1432778632u;
    p.rear_obstacle_source_sequence = 2578103244u;
    p.rear_obstacle_original_age_ms = 9029u;
    p.rear_obstacle_state = 2u;
    p.rear_obstacle_quality = 0u;
    p.rear_obstacle_reason = 0u;
    p.rear_obstacle_availability = 0u;
    p.rear_detection_activation = 1u;
    p.activation_meta_format = 1u;
    p.rear_activation_quality = 0u;
    p.rear_activation_reason = 0u;
    p.rear_activation_sequence = 2005440938u;
    p.rear_activation_age_ms = 13398u;
    return p;
}
static int equal_vss_warning_state(const NetWireVssWarningStateRaw *a, const NetWireVssWarningStateRaw *b)
{
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->antipinch_source_boot == b->antipinch_source_boot &&
        a->antipinch_source_sequence == b->antipinch_source_sequence &&
        a->antipinch_original_age_ms == b->antipinch_original_age_ms &&
        a->antipinch_state == b->antipinch_state &&
        a->antipinch_quality == b->antipinch_quality &&
        a->antipinch_reason == b->antipinch_reason &&
        a->antipinch_availability == b->antipinch_availability &&
        a->occupant_hazard_source_boot == b->occupant_hazard_source_boot &&
        a->occupant_hazard_source_sequence == b->occupant_hazard_source_sequence &&
        a->occupant_hazard_original_age_ms == b->occupant_hazard_original_age_ms &&
        a->occupant_hazard_state == b->occupant_hazard_state &&
        a->occupant_hazard_quality == b->occupant_hazard_quality &&
        a->occupant_hazard_reason == b->occupant_hazard_reason &&
        a->occupant_hazard_availability == b->occupant_hazard_availability &&
        a->rear_obstacle_source_boot == b->rear_obstacle_source_boot &&
        a->rear_obstacle_source_sequence == b->rear_obstacle_source_sequence &&
        a->rear_obstacle_original_age_ms == b->rear_obstacle_original_age_ms &&
        a->rear_obstacle_state == b->rear_obstacle_state &&
        a->rear_obstacle_quality == b->rear_obstacle_quality &&
        a->rear_obstacle_reason == b->rear_obstacle_reason &&
        a->rear_obstacle_availability == b->rear_obstacle_availability &&
        a->rear_detection_activation == b->rear_detection_activation &&
        a->activation_meta_format == b->activation_meta_format &&
        a->rear_activation_quality == b->rear_activation_quality &&
        a->rear_activation_reason == b->rear_activation_reason &&
        a->rear_activation_sequence == b->rear_activation_sequence &&
        a->rear_activation_age_ms == b->rear_activation_age_ms;
}

static const uint8_t golden_vss_status[32] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0x02,0x01,0x01,0x00,0x00,0x03,0x00,0x88,
    0x77,0x66,0x55,0xff,0xff,0x00,0xcc,0xbb,0xaa,0x99,0x34,0x12,0x00,0x00,0x00,0x00,
};
static NetWireVssStatusRaw example_vss_status(void)
{
    NetWireVssStatusRaw p = {0};
    p.boot_id = 16909060u;
    p.tx_sequence = 287454020u;
    p.vss_state = 2u;
    p.vss_availability = 1u;
    p.vss_accepting_events = 1u;
    p.vss_fault_active = 0u;
    p.fault_mask = 0u;
    p.vss_last_fault = 3u;
    p.last_fault_occurrence = 1432778632u;
    p.last_fault_age_ms = 65535u;
    p.last_fault_quality = 0u;
    p.update_sequence = 2578103244u;
    p.source_age_ms = 4660u;
    return p;
}
static int equal_vss_status(const NetWireVssStatusRaw *a, const NetWireVssStatusRaw *b)
{
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->vss_state == b->vss_state &&
        a->vss_availability == b->vss_availability &&
        a->vss_accepting_events == b->vss_accepting_events &&
        a->vss_fault_active == b->vss_fault_active &&
        a->fault_mask == b->fault_mask &&
        a->vss_last_fault == b->vss_last_fault &&
        a->last_fault_occurrence == b->last_fault_occurrence &&
        a->last_fault_age_ms == b->last_fault_age_ms &&
        a->last_fault_quality == b->last_fault_quality &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}

static NetWireCanEnvelope literal(uint32_t id, size_t length, uint8_t dlc, const uint8_t *bytes)
{
    NetWireCanEnvelope f = {0};
    f.standard_id = id; f.fdf = true; f.brs = true; f.dlc = dlc; f.length = length;
    memcpy(f.bytes,bytes,length);
    return f;
}
static void corrupt_envelope(NetWireCanEnvelope *f, unsigned which)
{
    switch (which) {
    case 0: f->standard_id ^= 1u; break;
    case 1: f->extended = true; break;
    case 2: f->fdf = false; break;
    case 3: f->brs = false; break;
    case 4: f->rtr = true; break;
    case 5: f->driver_error = true; break;
    case 6: --f->dlc; break;
    case 7: --f->length; break;
    case 8: ++f->length; break;
    case 9: f->length = 0u; break;
    case 10: f->length = SIZE_MAX; break;
    case 11: memset(f->bytes,0,4u); break;
    default: f->standard_id = 0x800u; break;
    }
}
#define DEFINE_ALIAS_TEST(NAME, TYPE) \
static int alias_##NAME(void) \
{ \
    union { max_align_t alignment; unsigned char bytes[512]; } storage; \
    unsigned char before[sizeof(storage)]; \
    TYPE p = example_##NAME(); \
    NetWireCanEnvelope frame = {0}; \
    size_t step; \
    CHECK(net_wire_can_encode_##NAME(CANDIDATE,&p,&frame) == NET_WIRE_OK); \
    for (step=0u; step<=_Alignof(max_align_t); step+=_Alignof(max_align_t)) { \
        memset(&storage,0x5a,sizeof(storage)); memcpy(storage.bytes,&p,sizeof(p)); \
        memcpy(before,&storage,sizeof(storage)); \
        CHECK(net_wire_can_encode_##NAME(CANDIDATE,(const TYPE *)(const void *)storage.bytes, \
            (NetWireCanEnvelope *)(void *)(storage.bytes+step)) == NET_WIRE_INVALID_ARGUMENT); \
        CHECK(memcmp(before,&storage,sizeof(storage)) == 0); \
        memset(&storage,0x5a,sizeof(storage)); memcpy(storage.bytes,&frame,sizeof(frame)); \
        memcpy(before,&storage,sizeof(storage)); \
        CHECK(net_wire_can_decode_##NAME(CANDIDATE,(const NetWireCanEnvelope *)(const void *)storage.bytes, \
            (TYPE *)(void *)(storage.bytes+step)) == NET_WIRE_INVALID_ARGUMENT); \
        CHECK(memcmp(before,&storage,sizeof(storage)) == 0); \
        /* Also reject overlaps where the output begins before the input. */ \
        memset(&storage,0x5a,sizeof(storage)); memcpy(storage.bytes+step,&p,sizeof(p)); \
        memcpy(before,&storage,sizeof(storage)); \
        CHECK(net_wire_can_encode_##NAME(CANDIDATE,(const TYPE *)(const void *)(storage.bytes+step), \
            (NetWireCanEnvelope *)(void *)storage.bytes) == NET_WIRE_INVALID_ARGUMENT); \
        CHECK(memcmp(before,&storage,sizeof(storage)) == 0); \
        memset(&storage,0x5a,sizeof(storage)); memcpy(storage.bytes+step,&frame,sizeof(frame)); \
        memcpy(before,&storage,sizeof(storage)); \
        CHECK(net_wire_can_decode_##NAME(CANDIDATE,(const NetWireCanEnvelope *)(const void *)(storage.bytes+step), \
            (TYPE *)(void *)storage.bytes) == NET_WIRE_INVALID_ARGUMENT); \
        CHECK(memcmp(before,&storage,sizeof(storage)) == 0); \
    } \
    return 0; \
}

DEFINE_ALIAS_TEST(window_command, NetWireWindowCommandRaw)
static int test_window_command(void)
{
    NetWireWindowCommandRaw p = example_window_command(), decoded, before, bad;
    NetWireCanEnvelope out, out_before, f, good = literal(0x090u,24u,12u,golden_window_command);
    unsigned i;
    memset(&out,0xa5,sizeof(out));
    CHECK(net_wire_can_encode_window_command(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(out.standard_id == good.standard_id && out.dlc == good.dlc && out.length == good.length);
    CHECK(!out.extended && out.fdf && out.brs && !out.rtr && !out.driver_error);
    CHECK(memcmp(out.bytes,golden_window_command,24u) == 0);
    CHECK(net_wire_can_decode_window_command(CANDIDATE,&good,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_command(&p,&decoded));
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before));
    for (i=0u;i<13u;++i) {
        f=good; corrupt_envelope(&f,i);
        CHECK(net_wire_can_decode_window_command(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    memset(&out,0x5a,sizeof(out)); memcpy(&out_before,&out,sizeof(out));
    CHECK(net_wire_can_encode_window_command(CANDIDATE,NULL,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_window_command(CANDIDATE,&p,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_window_command(NET_WIRE_NOT_BOUND,&p,&out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_window_command((NetWireProfile)99,&p,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_decode_window_command(CANDIDATE,NULL,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_window_command(CANDIDATE,&good,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_window_command(NET_WIRE_NOT_BOUND,&good,&decoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_window_command((NetWireProfile)99,&good,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.boot_id=0u;
    CHECK(net_wire_can_encode_window_command(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    bad=p; bad.window_command=0u;
    CHECK(net_wire_can_encode_window_command(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[20]=0u;
    CHECK(net_wire_can_decode_window_command(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.source_context=5u;
    CHECK(net_wire_can_encode_window_command(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[23]=5u;
    CHECK(net_wire_can_decode_window_command(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(alias_window_command() == 0);
    /* Header boundaries retain raw0/MAX sequence and MAX boot. */
    p.boot_id=UINT32_MAX; p.tx_sequence=0u;
    CHECK(net_wire_can_encode_window_command(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_command(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_command(&p,&decoded));
    p.tx_sequence=UINT32_MAX;
    CHECK(net_wire_can_encode_window_command(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_command(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_command(&p,&decoded));
    return 0;
}

DEFINE_ALIAS_TEST(window_state, NetWireWindowStateRaw)
static int test_window_state(void)
{
    NetWireWindowStateRaw p = example_window_state(), decoded, before, bad;
    NetWireCanEnvelope out, out_before, f, good = literal(0x0b0u,24u,12u,golden_window_state);
    unsigned i;
    memset(&out,0xa5,sizeof(out));
    CHECK(net_wire_can_encode_window_state(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(out.standard_id == good.standard_id && out.dlc == good.dlc && out.length == good.length);
    CHECK(!out.extended && out.fdf && out.brs && !out.rtr && !out.driver_error);
    CHECK(memcmp(out.bytes,golden_window_state,24u) == 0);
    CHECK(net_wire_can_decode_window_state(CANDIDATE,&good,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_state(&p,&decoded));
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before));
    for (i=0u;i<13u;++i) {
        f=good; corrupt_envelope(&f,i);
        CHECK(net_wire_can_decode_window_state(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    for (i=22u;i<24u;++i) {
        f=good; f.bytes[i]=1u;
        CHECK(net_wire_can_decode_window_state(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    memset(&out,0x5a,sizeof(out)); memcpy(&out_before,&out,sizeof(out));
    CHECK(net_wire_can_encode_window_state(CANDIDATE,NULL,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_window_state(CANDIDATE,&p,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_window_state(NET_WIRE_NOT_BOUND,&p,&out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_window_state((NetWireProfile)99,&p,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_decode_window_state(CANDIDATE,NULL,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_window_state(CANDIDATE,&good,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_window_state(NET_WIRE_NOT_BOUND,&good,&decoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_window_state((NetWireProfile)99,&good,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.boot_id=0u;
    CHECK(net_wire_can_encode_window_state(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    bad=p; bad.motion_state=4u;
    CHECK(net_wire_can_encode_window_state(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[8]=4u;
    CHECK(net_wire_can_decode_window_state(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.window_value_quality=4u;
    CHECK(net_wire_can_encode_window_state(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[11]=4u;
    CHECK(net_wire_can_decode_window_state(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(alias_window_state() == 0);
    /* Header boundaries retain raw0/MAX sequence and MAX boot. */
    p.boot_id=UINT32_MAX; p.tx_sequence=0u;
    CHECK(net_wire_can_encode_window_state(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_state(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_state(&p,&decoded));
    p.tx_sequence=UINT32_MAX;
    CHECK(net_wire_can_encode_window_state(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_state(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_state(&p,&decoded));
    return 0;
}

DEFINE_ALIAS_TEST(window_event, NetWireWindowEventRaw)
static int test_window_event(void)
{
    NetWireWindowEventRaw p = example_window_event(), decoded, before, bad;
    NetWireCanEnvelope out, out_before, f, good = literal(0x091u,32u,13u,golden_window_event);
    unsigned i;
    memset(&out,0xa5,sizeof(out));
    CHECK(net_wire_can_encode_window_event(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(out.standard_id == good.standard_id && out.dlc == good.dlc && out.length == good.length);
    CHECK(!out.extended && out.fdf && out.brs && !out.rtr && !out.driver_error);
    CHECK(memcmp(out.bytes,golden_window_event,32u) == 0);
    CHECK(net_wire_can_decode_window_event(CANDIDATE,&good,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_event(&p,&decoded));
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before));
    for (i=0u;i<13u;++i) {
        f=good; corrupt_envelope(&f,i);
        CHECK(net_wire_can_decode_window_event(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    for (i=26u;i<32u;++i) {
        f=good; f.bytes[i]=1u;
        CHECK(net_wire_can_decode_window_event(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    memset(&out,0x5a,sizeof(out)); memcpy(&out_before,&out,sizeof(out));
    CHECK(net_wire_can_encode_window_event(CANDIDATE,NULL,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_window_event(CANDIDATE,&p,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_window_event(NET_WIRE_NOT_BOUND,&p,&out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_window_event((NetWireProfile)99,&p,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_decode_window_event(CANDIDATE,NULL,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_window_event(CANDIDATE,&good,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_window_event(NET_WIRE_NOT_BOUND,&good,&decoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_window_event((NetWireProfile)99,&good,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.boot_id=0u;
    CHECK(net_wire_can_encode_window_event(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    bad=p; bad.event_kind=0u;
    CHECK(net_wire_can_encode_window_event(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[8]=0u;
    CHECK(net_wire_can_decode_window_event(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.origin=6u;
    CHECK(net_wire_can_encode_window_event(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[22]=6u;
    CHECK(net_wire_can_decode_window_event(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(alias_window_event() == 0);
    /* Header boundaries retain raw0/MAX sequence and MAX boot. */
    p.boot_id=UINT32_MAX; p.tx_sequence=0u;
    CHECK(net_wire_can_encode_window_event(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_event(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_event(&p,&decoded));
    p.tx_sequence=UINT32_MAX;
    CHECK(net_wire_can_encode_window_event(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_event(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_event(&p,&decoded));
    return 0;
}

DEFINE_ALIAS_TEST(window_result, NetWireWindowResultRaw)
static int test_window_result(void)
{
    NetWireWindowResultRaw p = example_window_result(), decoded, before, bad;
    NetWireCanEnvelope out, out_before, f, good = literal(0x092u,20u,11u,golden_window_result);
    unsigned i;
    memset(&out,0xa5,sizeof(out));
    CHECK(net_wire_can_encode_window_result(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(out.standard_id == good.standard_id && out.dlc == good.dlc && out.length == good.length);
    CHECK(!out.extended && out.fdf && out.brs && !out.rtr && !out.driver_error);
    CHECK(memcmp(out.bytes,golden_window_result,20u) == 0);
    CHECK(net_wire_can_decode_window_result(CANDIDATE,&good,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_result(&p,&decoded));
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before));
    for (i=0u;i<13u;++i) {
        f=good; corrupt_envelope(&f,i);
        CHECK(net_wire_can_decode_window_result(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    memset(&out,0x5a,sizeof(out)); memcpy(&out_before,&out,sizeof(out));
    CHECK(net_wire_can_encode_window_result(CANDIDATE,NULL,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_window_result(CANDIDATE,&p,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_window_result(NET_WIRE_NOT_BOUND,&p,&out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_window_result((NetWireProfile)99,&p,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_decode_window_result(CANDIDATE,NULL,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_window_result(CANDIDATE,&good,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_window_result(NET_WIRE_NOT_BOUND,&good,&decoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_window_result((NetWireProfile)99,&good,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.boot_id=0u;
    CHECK(net_wire_can_encode_window_result(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    bad=p; bad.result=6u;
    CHECK(net_wire_can_encode_window_result(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[17]=6u;
    CHECK(net_wire_can_decode_window_result(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.result_reason=26u;
    CHECK(net_wire_can_encode_window_result(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[18]=26u;
    CHECK(net_wire_can_decode_window_result(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(alias_window_result() == 0);
    /* Header boundaries retain raw0/MAX sequence and MAX boot. */
    p.boot_id=UINT32_MAX; p.tx_sequence=0u;
    CHECK(net_wire_can_encode_window_result(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_result(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_result(&p,&decoded));
    p.tx_sequence=UINT32_MAX;
    CHECK(net_wire_can_encode_window_result(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_result(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_result(&p,&decoded));
    return 0;
}

DEFINE_ALIAS_TEST(window_fault, NetWireWindowFaultRaw)
static int test_window_fault(void)
{
    NetWireWindowFaultRaw p = example_window_fault(), decoded, before, bad;
    NetWireCanEnvelope out, out_before, f, good = literal(0x330u,24u,12u,golden_window_fault);
    unsigned i;
    memset(&out,0xa5,sizeof(out));
    CHECK(net_wire_can_encode_window_fault(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(out.standard_id == good.standard_id && out.dlc == good.dlc && out.length == good.length);
    CHECK(!out.extended && out.fdf && out.brs && !out.rtr && !out.driver_error);
    CHECK(memcmp(out.bytes,golden_window_fault,24u) == 0);
    CHECK(net_wire_can_decode_window_fault(CANDIDATE,&good,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_fault(&p,&decoded));
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before));
    for (i=0u;i<13u;++i) {
        f=good; corrupt_envelope(&f,i);
        CHECK(net_wire_can_decode_window_fault(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    for (i=22u;i<24u;++i) {
        f=good; f.bytes[i]=1u;
        CHECK(net_wire_can_decode_window_fault(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    memset(&out,0x5a,sizeof(out)); memcpy(&out_before,&out,sizeof(out));
    CHECK(net_wire_can_encode_window_fault(CANDIDATE,NULL,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_window_fault(CANDIDATE,&p,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_window_fault(NET_WIRE_NOT_BOUND,&p,&out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_window_fault((NetWireProfile)99,&p,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_decode_window_fault(CANDIDATE,NULL,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_window_fault(CANDIDATE,&good,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_window_fault(NET_WIRE_NOT_BOUND,&good,&decoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_window_fault((NetWireProfile)99,&good,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.boot_id=0u;
    CHECK(net_wire_can_encode_window_fault(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    bad=p; bad.fault_category=4u;
    CHECK(net_wire_can_encode_window_fault(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[8]=4u;
    CHECK(net_wire_can_decode_window_fault(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.fault_status=3u;
    CHECK(net_wire_can_encode_window_fault(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[11]=3u;
    CHECK(net_wire_can_decode_window_fault(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(alias_window_fault() == 0);
    /* Header boundaries retain raw0/MAX sequence and MAX boot. */
    p.boot_id=UINT32_MAX; p.tx_sequence=0u;
    CHECK(net_wire_can_encode_window_fault(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_fault(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_fault(&p,&decoded));
    p.tx_sequence=UINT32_MAX;
    CHECK(net_wire_can_encode_window_fault(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_fault(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_window_fault(&p,&decoded));
    return 0;
}

DEFINE_ALIAS_TEST(cis_environment, NetWireCisEnvironmentRaw)
static int test_cis_environment(void)
{
    NetWireCisEnvironmentRaw p = example_cis_environment(), decoded, before, bad;
    NetWireCanEnvelope out, out_before, f, good = literal(0x300u,48u,14u,golden_cis_environment);
    unsigned i;
    memset(&out,0xa5,sizeof(out));
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(out.standard_id == good.standard_id && out.dlc == good.dlc && out.length == good.length);
    CHECK(!out.extended && out.fdf && out.brs && !out.rtr && !out.driver_error);
    CHECK(memcmp(out.bytes,golden_cis_environment,48u) == 0);
    CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&good,&decoded) == NET_WIRE_OK);
    CHECK(equal_cis_environment(&p,&decoded));
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before));
    for (i=0u;i<13u;++i) {
        f=good; corrupt_envelope(&f,i);
        CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    for (i=40u;i<48u;++i) {
        f=good; f.bytes[i]=1u;
        CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    memset(&out,0x5a,sizeof(out)); memcpy(&out_before,&out,sizeof(out));
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,NULL,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_cis_environment(NET_WIRE_NOT_BOUND,&p,&out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_cis_environment((NetWireProfile)99,&p,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_decode_cis_environment(CANDIDATE,NULL,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&good,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_cis_environment(NET_WIRE_NOT_BOUND,&good,&decoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_cis_environment((NetWireProfile)99,&good,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.boot_id=0u;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    bad=p; bad.temperature_validity=2u;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[16]=2u;
    CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.temperature_reason=10u;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[17]=10u;
    CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(alias_cis_environment() == 0);
    /* Header boundaries retain raw0/MAX sequence and MAX boot. */
    p.boot_id=UINT32_MAX; p.tx_sequence=0u;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_cis_environment(&p,&decoded));
    p.tx_sequence=UINT32_MAX;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_cis_environment(&p,&decoded));
    return 0;
}

DEFINE_ALIAS_TEST(cis_occupant, NetWireCisOccupantRaw)
static int test_cis_occupant(void)
{
    NetWireCisOccupantRaw p = example_cis_occupant(), decoded, before, bad;
    NetWireCanEnvelope out, out_before, f, good = literal(0x310u,32u,13u,golden_cis_occupant);
    unsigned i;
    memset(&out,0xa5,sizeof(out));
    CHECK(net_wire_can_encode_cis_occupant(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(out.standard_id == good.standard_id && out.dlc == good.dlc && out.length == good.length);
    CHECK(!out.extended && out.fdf && out.brs && !out.rtr && !out.driver_error);
    CHECK(memcmp(out.bytes,golden_cis_occupant,32u) == 0);
    CHECK(net_wire_can_decode_cis_occupant(CANDIDATE,&good,&decoded) == NET_WIRE_OK);
    CHECK(equal_cis_occupant(&p,&decoded));
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before));
    for (i=0u;i<13u;++i) {
        f=good; corrupt_envelope(&f,i);
        CHECK(net_wire_can_decode_cis_occupant(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    for (i=25u;i<32u;++i) {
        f=good; f.bytes[i]=1u;
        CHECK(net_wire_can_decode_cis_occupant(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    memset(&out,0x5a,sizeof(out)); memcpy(&out_before,&out,sizeof(out));
    CHECK(net_wire_can_encode_cis_occupant(CANDIDATE,NULL,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_cis_occupant(CANDIDATE,&p,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_cis_occupant(NET_WIRE_NOT_BOUND,&p,&out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_cis_occupant((NetWireProfile)99,&p,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_decode_cis_occupant(CANDIDATE,NULL,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_cis_occupant(CANDIDATE,&good,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_cis_occupant(NET_WIRE_NOT_BOUND,&good,&decoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_cis_occupant((NetWireProfile)99,&good,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.boot_id=0u;
    CHECK(net_wire_can_encode_cis_occupant(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    bad=p; bad.occupant_presence=2u;
    CHECK(net_wire_can_encode_cis_occupant(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[18]=2u;
    CHECK(net_wire_can_decode_cis_occupant(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.vision_function_status=5u;
    CHECK(net_wire_can_encode_cis_occupant(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[24]=5u;
    CHECK(net_wire_can_decode_cis_occupant(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(alias_cis_occupant() == 0);
    /* Header boundaries retain raw0/MAX sequence and MAX boot. */
    p.boot_id=UINT32_MAX; p.tx_sequence=0u;
    CHECK(net_wire_can_encode_cis_occupant(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_cis_occupant(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_cis_occupant(&p,&decoded));
    p.tx_sequence=UINT32_MAX;
    CHECK(net_wire_can_encode_cis_occupant(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_cis_occupant(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_cis_occupant(&p,&decoded));
    return 0;
}

DEFINE_ALIAS_TEST(cis_status, NetWireCisStatusRaw)
static int test_cis_status(void)
{
    NetWireCisStatusRaw p = example_cis_status(), decoded, before, bad;
    NetWireCanEnvelope out, out_before, f, good = literal(0x320u,48u,14u,golden_cis_status);
    unsigned i;
    memset(&out,0xa5,sizeof(out));
    CHECK(net_wire_can_encode_cis_status(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(out.standard_id == good.standard_id && out.dlc == good.dlc && out.length == good.length);
    CHECK(!out.extended && out.fdf && out.brs && !out.rtr && !out.driver_error);
    CHECK(memcmp(out.bytes,golden_cis_status,48u) == 0);
    CHECK(net_wire_can_decode_cis_status(CANDIDATE,&good,&decoded) == NET_WIRE_OK);
    CHECK(equal_cis_status(&p,&decoded));
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before));
    for (i=0u;i<13u;++i) {
        f=good; corrupt_envelope(&f,i);
        CHECK(net_wire_can_decode_cis_status(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    for (i=34u;i<48u;++i) {
        f=good; f.bytes[i]=1u;
        CHECK(net_wire_can_decode_cis_status(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    memset(&out,0x5a,sizeof(out)); memcpy(&out_before,&out,sizeof(out));
    CHECK(net_wire_can_encode_cis_status(CANDIDATE,NULL,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_cis_status(CANDIDATE,&p,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_cis_status(NET_WIRE_NOT_BOUND,&p,&out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_cis_status((NetWireProfile)99,&p,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_decode_cis_status(CANDIDATE,NULL,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_cis_status(CANDIDATE,&good,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_cis_status(NET_WIRE_NOT_BOUND,&good,&decoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_cis_status((NetWireProfile)99,&good,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.boot_id=0u;
    CHECK(net_wire_can_encode_cis_status(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    bad=p; bad.cis_state=4u;
    CHECK(net_wire_can_encode_cis_status(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[8]=4u;
    CHECK(net_wire_can_decode_cis_status(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.recovering=2u;
    CHECK(net_wire_can_encode_cis_status(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[20]=2u;
    CHECK(net_wire_can_decode_cis_status(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(alias_cis_status() == 0);
    /* Header boundaries retain raw0/MAX sequence and MAX boot. */
    p.boot_id=UINT32_MAX; p.tx_sequence=0u;
    CHECK(net_wire_can_encode_cis_status(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_cis_status(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_cis_status(&p,&decoded));
    p.tx_sequence=UINT32_MAX;
    CHECK(net_wire_can_encode_cis_status(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_cis_status(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_cis_status(&p,&decoded));
    return 0;
}

DEFINE_ALIAS_TEST(vss_event, NetWireVssEventRaw)
static int test_vss_event(void)
{
    NetWireVssEventRaw p = example_vss_event(), decoded, before, bad;
    NetWireCanEnvelope out, out_before, f, good = literal(0x160u,24u,12u,golden_vss_event);
    unsigned i;
    memset(&out,0xa5,sizeof(out));
    CHECK(net_wire_can_encode_vss_event(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(out.standard_id == good.standard_id && out.dlc == good.dlc && out.length == good.length);
    CHECK(!out.extended && out.fdf && out.brs && !out.rtr && !out.driver_error);
    CHECK(memcmp(out.bytes,golden_vss_event,24u) == 0);
    CHECK(net_wire_can_decode_vss_event(CANDIDATE,&good,&decoded) == NET_WIRE_OK);
    CHECK(equal_vss_event(&p,&decoded));
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before));
    for (i=0u;i<13u;++i) {
        f=good; corrupt_envelope(&f,i);
        CHECK(net_wire_can_decode_vss_event(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    for (i=23u;i<24u;++i) {
        f=good; f.bytes[i]=1u;
        CHECK(net_wire_can_decode_vss_event(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    memset(&out,0x5a,sizeof(out)); memcpy(&out_before,&out,sizeof(out));
    CHECK(net_wire_can_encode_vss_event(CANDIDATE,NULL,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_vss_event(CANDIDATE,&p,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_vss_event(NET_WIRE_NOT_BOUND,&p,&out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_vss_event((NetWireProfile)99,&p,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_decode_vss_event(CANDIDATE,NULL,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_vss_event(CANDIDATE,&good,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_vss_event(NET_WIRE_NOT_BOUND,&good,&decoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_vss_event((NetWireProfile)99,&good,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.boot_id=0u;
    CHECK(net_wire_can_encode_vss_event(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    bad=p; bad.event_kind=0u;
    CHECK(net_wire_can_encode_vss_event(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[8]=0u;
    CHECK(net_wire_can_decode_vss_event(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.input_availability=3u;
    CHECK(net_wire_can_encode_vss_event(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[18]=3u;
    CHECK(net_wire_can_decode_vss_event(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(alias_vss_event() == 0);
    /* Header boundaries retain raw0/MAX sequence and MAX boot. */
    p.boot_id=UINT32_MAX; p.tx_sequence=0u;
    CHECK(net_wire_can_encode_vss_event(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_vss_event(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_vss_event(&p,&decoded));
    p.tx_sequence=UINT32_MAX;
    CHECK(net_wire_can_encode_vss_event(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_vss_event(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_vss_event(&p,&decoded));
    return 0;
}

DEFINE_ALIAS_TEST(vss_warning_state, NetWireVssWarningStateRaw)
static int test_vss_warning_state(void)
{
    NetWireVssWarningStateRaw p = example_vss_warning_state(), decoded, before, bad;
    NetWireCanEnvelope out, out_before, f, good = literal(0x0c0u,64u,15u,golden_vss_warning_state);
    unsigned i;
    memset(&out,0xa5,sizeof(out));
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(out.standard_id == good.standard_id && out.dlc == good.dlc && out.length == good.length);
    CHECK(!out.extended && out.fdf && out.brs && !out.rtr && !out.driver_error);
    CHECK(memcmp(out.bytes,golden_vss_warning_state,64u) == 0);
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&good,&decoded) == NET_WIRE_OK);
    CHECK(equal_vss_warning_state(&p,&decoded));
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before));
    for (i=0u;i<13u;++i) {
        f=good; corrupt_envelope(&f,i);
        CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    for (i=60u;i<64u;++i) {
        f=good; f.bytes[i]=1u;
        CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    memset(&out,0x5a,sizeof(out)); memcpy(&out_before,&out,sizeof(out));
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,NULL,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&p,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_vss_warning_state(NET_WIRE_NOT_BOUND,&p,&out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_vss_warning_state((NetWireProfile)99,&p,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,NULL,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&good,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_vss_warning_state(NET_WIRE_NOT_BOUND,&good,&decoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_vss_warning_state((NetWireProfile)99,&good,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.boot_id=0u;
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    bad=p; bad.activation_meta_format=2u;
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[51]=2u;
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.rear_obstacle_state=3u;
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[46]=3u;
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(alias_vss_warning_state() == 0);
    /* Header boundaries retain raw0/MAX sequence and MAX boot. */
    p.boot_id=UINT32_MAX; p.tx_sequence=0u;
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_vss_warning_state(&p,&decoded));
    p.tx_sequence=UINT32_MAX;
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_vss_warning_state(&p,&decoded));
    return 0;
}

DEFINE_ALIAS_TEST(vss_status, NetWireVssStatusRaw)
static int test_vss_status(void)
{
    NetWireVssStatusRaw p = example_vss_status(), decoded, before, bad;
    NetWireCanEnvelope out, out_before, f, good = literal(0x340u,32u,13u,golden_vss_status);
    unsigned i;
    memset(&out,0xa5,sizeof(out));
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(out.standard_id == good.standard_id && out.dlc == good.dlc && out.length == good.length);
    CHECK(!out.extended && out.fdf && out.brs && !out.rtr && !out.driver_error);
    CHECK(memcmp(out.bytes,golden_vss_status,32u) == 0);
    CHECK(net_wire_can_decode_vss_status(CANDIDATE,&good,&decoded) == NET_WIRE_OK);
    CHECK(equal_vss_status(&p,&decoded));
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before));
    for (i=0u;i<13u;++i) {
        f=good; corrupt_envelope(&f,i);
        CHECK(net_wire_can_decode_vss_status(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    for (i=28u;i<32u;++i) {
        f=good; f.bytes[i]=1u;
        CHECK(net_wire_can_decode_vss_status(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    }
    memset(&out,0x5a,sizeof(out)); memcpy(&out_before,&out,sizeof(out));
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,NULL,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&p,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_vss_status(NET_WIRE_NOT_BOUND,&p,&out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_encode_vss_status((NetWireProfile)99,&p,&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    CHECK(net_wire_can_decode_vss_status(CANDIDATE,NULL,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_vss_status(CANDIDATE,&good,NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_vss_status(NET_WIRE_NOT_BOUND,&good,&decoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(net_wire_can_decode_vss_status((NetWireProfile)99,&good,&decoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.boot_id=0u;
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    bad=p; bad.fault_mask=32u;
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[12]=32u;
    CHECK(net_wire_can_decode_vss_status(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    bad=p; bad.last_fault_quality=2u;
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&bad,&out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out,&out_before,sizeof(out)) == 0);
    f=good; f.bytes[21]=2u;
    CHECK(net_wire_can_decode_vss_status(CANDIDATE,&f,&decoded) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&before,&decoded,sizeof(before)) == 0);
    CHECK(alias_vss_status() == 0);
    /* Header boundaries retain raw0/MAX sequence and MAX boot. */
    p.boot_id=UINT32_MAX; p.tx_sequence=0u;
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_vss_status(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_vss_status(&p,&decoded));
    p.tx_sequence=UINT32_MAX;
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&p,&out) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_vss_status(CANDIDATE,&out,&decoded) == NET_WIRE_OK);
    CHECK(equal_vss_status(&p,&decoded));
    return 0;
}

static int nodes_environment_boundaries(void)
{
    NetWireCisEnvironmentRaw p=example_cis_environment(), decoded, saved;
    NetWireCanEnvelope f, old, bad;
    NetWireCisOccupantRaw occupant=example_cis_occupant(), occupant_out;
    NetWireCisStatusRaw status=example_cis_status(), status_out;
    int16_t temperatures[3]={-4000,0,12500};
    unsigned i;
    for (i=0;i<3u;++i) {
        p.temperature_centi_c=temperatures[i];
        p.humidity_centi_percent=(i==0u)?0u:10000u;
        p.illuminance_lux=(i==0u)?0u:UINT32_C(200000);
        p.temperature_age_ms=0u; p.humidity_age_ms=UINT16_MAX; p.illuminance_age_ms=0x1357u;
        p.temperature_sequence=0u; p.humidity_sequence=UINT32_MAX; p.illuminance_sequence=0x2468ace0u;
        CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,&f)==NET_WIRE_OK);
        CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&f,&decoded)==NET_WIRE_OK);
        CHECK(equal_cis_environment(&p,&decoded));
        CHECK(f.bytes[24]==0xffu && f.bytes[25]==0xffu && f.bytes[36]==0x57u && f.bytes[37]==0x13u);
        if(i==0u) CHECK(f.bytes[8]==0x60u && f.bytes[9]==0xf0u);
        if(i==2u) CHECK(f.bytes[8]==0xd4u && f.bytes[9]==0x30u);
    }
    p.temperature_centi_c=INT16_MIN; p.temperature_validity=0u; p.temperature_reason=6u;
    p.humidity_centi_percent=UINT16_MAX; p.humidity_validity=0u; p.humidity_reason=6u;
    p.illuminance_lux=UINT32_MAX; p.illuminance_validity=0u; p.illuminance_reason=6u;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,&f)==NET_WIRE_OK);
    CHECK(f.bytes[8]==0u && f.bytes[9]==0x80u && f.bytes[18]==0xffu && f.bytes[19]==0xffu);
    CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&f,&decoded)==NET_WIRE_OK);
    CHECK(equal_cis_environment(&p,&decoded));
    /* Invalid sentinels do not erase sibling raw data, IDs or ages. */
    p=example_cis_environment(); memset(&f,0xa5,sizeof(f)); memcpy(&old,&f,sizeof(old));
    p.temperature_centi_c=-4001;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,&f)==NET_WIRE_MALFORMED);
    CHECK(memcmp(&f,&old,sizeof(f))==0);
    p.temperature_centi_c=12501;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,&f)==NET_WIRE_MALFORMED);
    CHECK(memcmp(&f,&old,sizeof(f))==0);
    p.temperature_centi_c=INT16_MIN;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,&f)==NET_WIRE_MALFORMED);
    CHECK(memcmp(&f,&old,sizeof(f))==0);
    p=example_cis_environment(); p.humidity_centi_percent=10001u;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,&f)==NET_WIRE_MALFORMED);
    CHECK(memcmp(&f,&old,sizeof(f))==0);
    p=example_cis_environment(); p.illuminance_lux=200001u;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,&f)==NET_WIRE_MALFORMED);
    CHECK(memcmp(&f,&old,sizeof(f))==0);
    p=example_cis_environment(); p.temperature_validity=0u;
    CHECK(net_wire_can_encode_cis_environment(CANDIDATE,&p,&f)==NET_WIRE_CONTRACT_HOLD);
    CHECK(memcmp(&f,&old,sizeof(f))==0);
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&saved,&decoded,sizeof(saved));
    bad=literal(0x300u,48u,14u,golden_cis_environment); bad.bytes[8]=0u; bad.bytes[9]=0x80u;
    CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&bad,&decoded)==NET_WIRE_MALFORMED);
    CHECK(memcmp(&decoded,&saved,sizeof(saved))==0);
    bad=literal(0x300u,48u,14u,golden_cis_environment); bad.bytes[16]=0u;
    CHECK(net_wire_can_decode_cis_environment(CANDIDATE,&bad,&decoded)==NET_WIRE_CONTRACT_HOLD);
    CHECK(memcmp(&decoded,&saved,sizeof(saved))==0);
    occupant.occupant_presence=0u; occupant.occupant_count=UINT8_MAX;
    occupant.count_validity=0u; occupant.count_reason=6u;
    CHECK(net_wire_can_encode_cis_occupant(CANDIDATE,&occupant,&f)==NET_WIRE_OK);
    CHECK(net_wire_can_decode_cis_occupant(CANDIDATE,&f,&occupant_out)==NET_WIRE_OK);
    CHECK(equal_cis_occupant(&occupant,&occupant_out)); /* Independent missing count. */
    occupant=example_cis_occupant(); occupant.occupant_count=0u;
    CHECK(net_wire_can_encode_cis_occupant(CANDIDATE,&occupant,&f)==NET_WIRE_MALFORMED);
    occupant=example_cis_occupant(); occupant.pi_boot_id=0u;
    CHECK(net_wire_can_encode_cis_occupant(CANDIDATE,&occupant,&f)==NET_WIRE_CONTRACT_HOLD);
    status.fault_mask=0x1ffu; status.active_fault=9u; status.last_fault=9u;
    status.affected_function=0x3fu; status.update_sequence=UINT32_MAX; status.source_age_ms=UINT16_MAX;
    CHECK(net_wire_can_encode_cis_status(CANDIDATE,&status,&f)==NET_WIRE_OK);
    CHECK(net_wire_can_decode_cis_status(CANDIDATE,&f,&status_out)==NET_WIRE_OK);
    CHECK(equal_cis_status(&status,&status_out));
    status.fault_mask=0x200u;
    CHECK(net_wire_can_encode_cis_status(CANDIDATE,&status,&f)==NET_WIRE_MALFORMED);
    return 0;
}

static int nodes_warning60_formats(void)
{
    NetWireVssWarningStateRaw p=example_vss_warning_state(), decoded, saved;
    NetWireCanEnvelope f, absent, bad, old;
    unsigned i;
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&p,&f)==NET_WIRE_OK);
    CHECK(f.length==64u && f.dlc==15u && f.bytes[51]==1u);
    CHECK(f.bytes[54]==0xaau && f.bytes[55]==0x99u && f.bytes[56]==0x88u && f.bytes[57]==0x77u);
    p.rear_detection_activation=0u; p.activation_meta_format=0u;
    p.rear_activation_quality=0u; p.rear_activation_reason=0u;
    p.rear_activation_sequence=0u; p.rear_activation_age_ms=0u;
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&p,&absent)==NET_WIRE_OK);
    CHECK(memcmp(absent.bytes,golden_vss_warning_state,50u)==0);
    for(i=50u;i<64u;++i) CHECK(absent.bytes[i]==0u);
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&absent,&decoded)==NET_WIRE_OK);
    CHECK(equal_vss_warning_state(&p,&decoded));
    /* No semantic grant is returned: format0 is raw absence, not fresh DISABLED/OK.
     * A legacy producer's byte-identical64B frame cannot be detected from bytes.
     * The caller must explicitly select WARNING60 candidate binding. */
    memset(&decoded,0x5a,sizeof(decoded)); memcpy(&saved,&decoded,sizeof(saved));
    for(i=50u;i<60u;++i) {
        if(i==51u) continue;
        bad=absent; bad.bytes[i]=1u;
        CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&bad,&decoded)==NET_WIRE_MALFORMED);
        CHECK(memcmp(&decoded,&saved,sizeof(saved))==0);
    }
    bad=absent; bad.bytes[51]=2u;
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&bad,&decoded)==NET_WIRE_MALFORMED);
    CHECK(memcmp(&decoded,&saved,sizeof(saved))==0);
    bad=absent; bad.length=51u; /* Not a legal WARNING60 envelope, no autodetection. */
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&bad,&decoded)==NET_WIRE_MALFORMED);
    CHECK(memcmp(&decoded,&saved,sizeof(saved))==0);
    p.rear_activation_sequence=1u; memset(&f,0xa5,sizeof(f)); memcpy(&old,&f,sizeof(old));
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&p,&f)==NET_WIRE_MALFORMED);
    CHECK(memcmp(&f,&old,sizeof(f))==0);
    p=example_vss_warning_state(); p.antipinch_original_age_ms=0u;
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&p,&f)==NET_WIRE_CONTRACT_HOLD);
    CHECK(memcmp(&f,&old,sizeof(f))==0);
    bad=literal(0x0c0u,64u,15u,golden_vss_warning_state); bad.bytes[16]=0u; bad.bytes[17]=0u;
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&bad,&decoded)==NET_WIRE_CONTRACT_HOLD);
    CHECK(memcmp(&decoded,&saved,sizeof(saved))==0);
    p=example_vss_warning_state(); p.rear_activation_reason=7u;
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&p,&f)==NET_WIRE_CONTRACT_HOLD);
    CHECK(memcmp(&f,&old,sizeof(f))==0);
    /* Confirmed non-OK retained values remain raw; no CLEAR synthesis. */
    p=example_vss_warning_state(); p.occupant_hazard_quality=2u; p.occupant_hazard_reason=9u;
    p.occupant_hazard_availability=2u; p.rear_activation_age_ms=UINT16_MAX;
    CHECK(net_wire_can_encode_vss_warning_state(CANDIDATE,&p,&f)==NET_WIRE_OK);
    CHECK(net_wire_can_decode_vss_warning_state(CANDIDATE,&f,&decoded)==NET_WIRE_OK);
    CHECK(equal_vss_warning_state(&p,&decoded));
    return 0;
}

static int nodes_vss_status_history_u8(void)
{
    NetWireVssStatusRaw p=example_vss_status(), decoded, saved;
    NetWireCanEnvelope f, old, bad;
    NetWireVssEventRaw event=example_vss_event(), event_out;
    unsigned i;
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&p,&f)==NET_WIRE_OK);
    CHECK(f.bytes[12]==0u && f.bytes[13]==3u && f.bytes[14]==0u);
    CHECK(net_wire_can_decode_vss_status(CANDIDATE,&f,&decoded)==NET_WIRE_OK);
    CHECK(decoded.vss_fault_active==0u && decoded.fault_mask==0u && decoded.vss_last_fault==3u);
    CHECK(decoded.last_fault_occurrence==0x55667788u && decoded.last_fault_age_ms==UINT16_MAX);
    /* All five current bits are exactly one byte; following history remains independent. */
    for(i=0;i<32u;++i) {
        p.fault_mask=(uint8_t)i; p.vss_fault_active=(i==0u)?0u:1u;
        CHECK(net_wire_can_encode_vss_status(CANDIDATE,&p,&f)==NET_WIRE_OK);
        CHECK(f.bytes[12]==i && f.bytes[13]==3u && f.bytes[14]==0u);
        CHECK(net_wire_can_decode_vss_status(CANDIDATE,&f,&decoded)==NET_WIRE_OK);
        CHECK(equal_vss_status(&p,&decoded));
    }
    p.vss_state=3u; p.vss_availability=2u; p.vss_accepting_events=1u;
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&p,&f)==NET_WIRE_OK);
    p=example_vss_status(); p.last_fault_quality=3u; p.vss_last_fault=0u;
    p.last_fault_occurrence=0u; p.last_fault_age_ms=0u;
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&p,&f)==NET_WIRE_OK);
    CHECK(net_wire_can_decode_vss_status(CANDIDATE,&f,&decoded)==NET_WIRE_OK);
    CHECK(equal_vss_status(&p,&decoded));
    memset(&f,0xa5,sizeof(f)); memcpy(&old,&f,sizeof(f));
    p.last_fault_age_ms=1u;
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&p,&f)==NET_WIRE_MALFORMED);
    CHECK(memcmp(&f,&old,sizeof(f))==0);
    p=example_vss_status(); p.vss_last_fault=0u;
    CHECK(net_wire_can_encode_vss_status(CANDIDATE,&p,&f)==NET_WIRE_CONTRACT_HOLD);
    CHECK(memcmp(&f,&old,sizeof(f))==0);
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&saved,&decoded,sizeof(saved));
    bad=literal(0x340u,32u,13u,golden_vss_status); bad.bytes[21]=3u;
    CHECK(net_wire_can_decode_vss_status(CANDIDATE,&bad,&decoded)==NET_WIRE_MALFORMED);
    CHECK(memcmp(&decoded,&saved,sizeof(saved))==0);
    for(i=1u;i<=2u;++i) {
        bad=literal(0x340u,32u,13u,golden_vss_status); bad.bytes[21]=(uint8_t)i;
        CHECK(net_wire_can_decode_vss_status(CANDIDATE,&bad,&decoded)==NET_WIRE_MALFORMED);
        CHECK(memcmp(&decoded,&saved,sizeof(saved))==0);
    }
    event.use_limit_ms=0u; event.original_age_ms=UINT16_MAX; event.related_command_id=0u;
    CHECK(net_wire_can_encode_vss_event(CANDIDATE,&event,&f)==NET_WIRE_OK);
    CHECK(net_wire_can_decode_vss_event(CANDIDATE,&f,&event_out)==NET_WIRE_OK);
    CHECK(equal_vss_event(&event,&event_out)); /* Unusable raw event, no output start. */
    event.use_limit_ms=UINT16_MAX; event.original_age_ms=0u; event.related_command_id=UINT32_MAX;
    CHECK(net_wire_can_encode_vss_event(CANDIDATE,&event,&f)==NET_WIRE_OK);
    CHECK(net_wire_can_decode_vss_event(CANDIDATE,&f,&event_out)==NET_WIRE_OK);
    CHECK(equal_vss_event(&event,&event_out));
    return 0;
}

static int nodes_window_execution_hold(void)
{
    NetWireWindowCommandRaw p=example_window_command(), decoded, saved;
    NetWireWindowStateRaw state=example_window_state(), state_out;
    NetWireWindowResultRaw result=example_window_result(), result_out, result_before;
    NetWireWindowFaultRaw fault=example_window_fault(), fault_out;
    NetWireCanEnvelope f, old, bad;
    unsigned i;
    /* All5 APIs are raw-only. No port, runtime, schedule or actuator is linked.
     * This test selects candidate serialization; it cannot clear execution HOLD. */
    for(i=1u;i<=5u;++i) {
        p.window_command=(uint8_t)i; p.target_position=(i==1u)?0u:100u;
        p.command_age_ms=UINT16_MAX; p.accept_limit_ms=0u; p.command_id=UINT32_MAX;
        CHECK(net_wire_can_encode_window_command(CANDIDATE,&p,&f)==NET_WIRE_OK);
        CHECK(net_wire_can_decode_window_command(CANDIDATE,&f,&decoded)==NET_WIRE_OK);
        CHECK(equal_window_command(&p,&decoded));
    }
    p.window_command=4u;
    p.target_position=70u; p.accept_limit_ms=200u;
    CHECK(net_wire_can_encode_window_command(CANDIDATE,&p,&f)==NET_WIRE_OK);
    CHECK(f.bytes[22]==70u && f.bytes[18]==200u);
    p.target_position=80u; p.accept_limit_ms=1000u;
    CHECK(net_wire_can_encode_window_command(CANDIDATE,&p,&f)==NET_WIRE_OK);
    CHECK(f.bytes[22]==80u && f.bytes[18]==0xe8u && f.bytes[19]==3u);
    memcpy(&old,&f,sizeof(f));
    CHECK(net_wire_can_encode_window_command(NET_WIRE_NOT_BOUND,&p,&f)==NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&old,&f,sizeof(f))==0);
    p.target_boot_id=0u;
    CHECK(net_wire_can_encode_window_command(CANDIDATE,&p,&f)==NET_WIRE_CONTRACT_HOLD);
    CHECK(memcmp(&old,&f,sizeof(f))==0);
    memset(&decoded,0xa5,sizeof(decoded)); memcpy(&saved,&decoded,sizeof(saved));
    bad=literal(0x090u,24u,12u,golden_window_command); memset(bad.bytes+8,0,4u);
    CHECK(net_wire_can_decode_window_command(CANDIDATE,&bad,&decoded)==NET_WIRE_CONTRACT_HOLD);
    CHECK(memcmp(&decoded,&saved,sizeof(saved))==0);
    state.motion_state=UINT8_MAX; state.window_position=UINT8_MAX; state.window_value_quality=3u;
    state.fully_open_state=UINT8_MAX; state.fully_closed_state=UINT8_MAX; state.reverse_state=UINT8_MAX;
    state.source_age_ms=UINT16_MAX; state.update_sequence=0u;
    CHECK(net_wire_can_encode_window_state(CANDIDATE,&state,&f)==NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_state(CANDIDATE,&f,&state_out)==NET_WIRE_OK);
    CHECK(equal_window_state(&state,&state_out));
    state.window_value_quality=0u;
    CHECK(net_wire_can_encode_window_state(CANDIDATE,&state,&f)==NET_WIRE_CONTRACT_HOLD);
    result.request_sequence=0u;
    CHECK(net_wire_can_encode_window_result(CANDIDATE,&result,&f)==NET_WIRE_OK);
    CHECK(net_wire_can_decode_window_result(CANDIDATE,&f,&result_out)==NET_WIRE_OK);
    CHECK(equal_window_result(&result,&result_out));
    result.result=UINT8_MAX;
    CHECK(net_wire_can_encode_window_result(CANDIDATE,&result,&f)==NET_WIRE_CONTRACT_HOLD);
    memset(&result_out,0xa5,sizeof(result_out)); memcpy(&result_before,&result_out,sizeof(result_out));
    bad=literal(0x092u,20u,11u,golden_window_result); bad.bytes[17]=UINT8_MAX;
    CHECK(net_wire_can_decode_window_result(CANDIDATE,&bad,&result_out)==NET_WIRE_CONTRACT_HOLD);
    CHECK(memcmp(&result_out,&result_before,sizeof(result_out))==0);
    for(i=0u;i<=6u;++i) {
        fault.fault_code=(uint16_t)i; fault.occurrence_id=UINT32_MAX; fault.update_sequence=0u;
        CHECK(net_wire_can_encode_window_fault(CANDIDATE,&fault,&f)==NET_WIRE_OK);
        CHECK(net_wire_can_decode_window_fault(CANDIDATE,&f,&fault_out)==NET_WIRE_OK);
        CHECK(equal_window_fault(&fault,&fault_out));
    }
    fault.fault_code=7u;
    CHECK(net_wire_can_encode_window_fault(CANDIDATE,&fault,&f)==NET_WIRE_MALFORMED);
    return 0;
}

static int nodes_alias_atomicity(void)
{
    CHECK(alias_window_command()==0); CHECK(alias_window_state()==0);
    CHECK(alias_window_event()==0); CHECK(alias_window_result()==0); CHECK(alias_window_fault()==0);
    CHECK(alias_cis_environment()==0); CHECK(alias_cis_occupant()==0); CHECK(alias_cis_status()==0);
    CHECK(alias_vss_event()==0); CHECK(alias_vss_warning_state()==0); CHECK(alias_vss_status()==0);
    return 0;
}

/* Metadata oracle patches fixed independent goldens directly. It does not
 * derive expected bytes with the candidate codec or re-encode a decoded DTO. */
static uint32_t metadata_random(uint32_t *seed)
{
    uint32_t v=*seed;
    v^=v<<13; v^=v>>17; v^=v<<5;
    *seed=v;
    return v;
}
#define METADATA_CASE(NAME,TYPE,ID,LENGTH,DLC) \
static int metadata_##NAME(void) \
{ \
    uint32_t seed=UINT32_C(0x7531ca92); \
    unsigned round, j; \
    for(round=0u;round<256u;++round) { \
        TYPE p=example_##NAME(), decoded, before; \
        NetWireCanEnvelope expected=literal(ID,LENGTH,DLC,golden_##NAME), encoded, old; \
        p.boot_id=metadata_random(&seed); p.tx_sequence=metadata_random(&seed); \
        if(round==0u) { p.boot_id=0u; p.tx_sequence=0u; } \
        if(round==1u) { p.boot_id=UINT32_MAX; p.tx_sequence=UINT32_MAX; } \
        if(round==2u) { p.boot_id=1u; p.tx_sequence=0u; } \
        for(j=0u;j<4u;++j) { \
            uint64_t divisor=UINT64_C(1)<<(8u*j); \
            expected.bytes[j]=(uint8_t)((uint64_t)p.boot_id/divisor); \
            expected.bytes[4u+j]=(uint8_t)((uint64_t)p.tx_sequence/divisor); \
        } \
        memset(&encoded,0xa5,sizeof(encoded)); memcpy(&old,&encoded,sizeof(old)); \
        memset(&decoded,0x5a,sizeof(decoded)); memcpy(&before,&decoded,sizeof(before)); \
        if(p.boot_id==0u) { \
            CHECK(net_wire_can_encode_##NAME(CANDIDATE,&p,&encoded)==NET_WIRE_MALFORMED); \
            CHECK(memcmp(&encoded,&old,sizeof(old))==0); \
            CHECK(net_wire_can_decode_##NAME(CANDIDATE,&expected,&decoded)==NET_WIRE_MALFORMED); \
            CHECK(memcmp(&decoded,&before,sizeof(before))==0); \
        } else { \
            CHECK(net_wire_can_encode_##NAME(CANDIDATE,&p,&encoded)==NET_WIRE_OK); \
            CHECK(memcmp(encoded.bytes,expected.bytes,LENGTH)==0); \
            CHECK(net_wire_can_decode_##NAME(CANDIDATE,&expected,&decoded)==NET_WIRE_OK); \
            CHECK(equal_##NAME(&p,&decoded)); \
        } \
    } \
    return 0; \
}
METADATA_CASE(window_command,NetWireWindowCommandRaw,0x090u,24u,12u)
METADATA_CASE(window_state,NetWireWindowStateRaw,0x0b0u,24u,12u)
METADATA_CASE(window_event,NetWireWindowEventRaw,0x091u,32u,13u)
METADATA_CASE(window_result,NetWireWindowResultRaw,0x092u,20u,11u)
METADATA_CASE(window_fault,NetWireWindowFaultRaw,0x330u,24u,12u)
METADATA_CASE(cis_environment,NetWireCisEnvironmentRaw,0x300u,48u,14u)
METADATA_CASE(cis_occupant,NetWireCisOccupantRaw,0x310u,32u,13u)
METADATA_CASE(cis_status,NetWireCisStatusRaw,0x320u,48u,14u)
METADATA_CASE(vss_event,NetWireVssEventRaw,0x160u,24u,12u)
METADATA_CASE(vss_warning_state,NetWireVssWarningStateRaw,0x0c0u,64u,15u)
METADATA_CASE(vss_status,NetWireVssStatusRaw,0x340u,32u,13u)
#undef METADATA_CASE
static int nodes_seeded_metadata(void)
{
    puts("seed=0x7531ca92, 256 rounds x 11 messages; independent fixed-golden metadata oracle");
    CHECK(metadata_window_command()==0); CHECK(metadata_window_state()==0);
    CHECK(metadata_window_event()==0); CHECK(metadata_window_result()==0); CHECK(metadata_window_fault()==0);
    CHECK(metadata_cis_environment()==0); CHECK(metadata_cis_occupant()==0); CHECK(metadata_cis_status()==0);
    CHECK(metadata_vss_event()==0); CHECK(metadata_vss_warning_state()==0); CHECK(metadata_vss_status()==0);
    return 0;
}

typedef struct { const char *name; int (*run)(void); } Test;
static const Test tests[]={
    {"window_command",test_window_command}, {"window_state",test_window_state},
    {"window_event",test_window_event}, {"window_result",test_window_result},
    {"window_fault",test_window_fault}, {"cis_environment",test_cis_environment},
    {"cis_occupant",test_cis_occupant}, {"cis_status",test_cis_status},
    {"vss_event",test_vss_event}, {"vss_warning_state",test_vss_warning_state},
    {"vss_status",test_vss_status},
    {"nodes_environment_boundaries",nodes_environment_boundaries},
    {"nodes_warning60_formats",nodes_warning60_formats},
    {"nodes_vss_status_history_u8",nodes_vss_status_history_u8},
    {"nodes_window_execution_hold",nodes_window_execution_hold},
    {"nodes_alias_atomicity",nodes_alias_atomicity},
    {"nodes_seeded_metadata",nodes_seeded_metadata}
};
int main(int argc, char **argv)
{
    size_t i;
    if(argc>2) return 2;
    for(i=0u;i<sizeof(tests)/sizeof(tests[0]);++i) {
        if(argc==2 && strcmp(argv[1],tests[i].name)!=0) continue;
        if(tests[i].run()!=0) return 1;
        printf("PASS %s\n",tests[i].name);
        if(argc==2) return 0;
    }
    return argc==1?0:2;
}
