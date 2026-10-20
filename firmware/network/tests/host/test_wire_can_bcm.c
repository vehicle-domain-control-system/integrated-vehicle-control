#include "network/net_wire_can_bcm.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
#define CANDIDATE NET_WIRE_CANDIDATE_ONLY
/* Fixed literals from BCM_GOLDEN_ORACLE.py: independent Python whole-message
 * struct.pack formats from FINAL_98, never C codec output or roundtrip. */
typedef union {
    max_align_t alignment;
    NetWireBcmDoorCommandRaw bcm_door_command;
    NetWireBcmClimateCommandRaw bcm_climate_command;
    NetWireBcmLightCommandRaw bcm_light_command;
    NetWireBcmDoorStateRaw bcm_door_state;
    NetWireBcmClimateStateRaw bcm_climate_state;
    NetWireBcmLightStateRaw bcm_light_state;
    NetWireBcmResultRaw bcm_result;
    NetWireBcmEventRaw bcm_event;
    NetWireBcmStatusRaw bcm_status;
} Storage;

typedef NetWireStatus (*Encode)(NetWireProfile, const void *, NetWireCanEnvelope *);
typedef NetWireStatus (*Decode)(NetWireProfile, const NetWireCanEnvelope *, void *);
typedef struct { size_t offset, width; uint32_t value; } BadValue;
typedef struct {
    const char *name; uint16_t id; size_t used, length; uint8_t dlc;
    const uint8_t *golden; Encode encode; Decode decode;
    void (*sample)(void *); int (*equal)(const void *, const void *);
    void (*mutate)(void *, size_t); const BadValue *bad; size_t bad_count;
    void (*bounds)(void *, int); void (*randomize)(void *, uint32_t *);
} Spec;
static uint32_t next_random(uint32_t *state)
{
    uint32_t v = *state; v ^= v << 13u; v ^= v >> 17u; v ^= v << 5u; *state = v; return v;
}

static const uint8_t bcm_door_command_golden[24] = {
    0x40u, 0x30u, 0x20u, 0x10u, 0x80u, 0x70u, 0x60u, 0x50u, 0xc4u, 0xb3u, 0xa2u, 0x91u, 0x45u, 0x36u, 0x27u, 0x18u, 0x6au, 0x59u, 0x8cu, 0x7bu, 0x01u, 0x05u, 0x00u, 0x00u
};
static void bcm_door_command_sample(void *p)
{
    NetWireBcmDoorCommandRaw *v = (NetWireBcmDoorCommandRaw *)p;
    memset(v, 0, sizeof(*v));
    v->boot_id = 270544960u;
    v->tx_sequence = 1348497536u;
    v->target_boot_id = 2443359172u;
    v->command_id = 405222981u;
    v->command_age_ms = 22890u;
    v->accept_limit_ms = 31628u;
    v->door_lock_target = 1u;
    v->origin = 5u;
}
static int bcm_door_command_equal(const void *pa, const void *pb)
{
    const NetWireBcmDoorCommandRaw *a = (const NetWireBcmDoorCommandRaw *)pa, *b = (const NetWireBcmDoorCommandRaw *)pb;
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->target_boot_id == b->target_boot_id &&
        a->command_id == b->command_id &&
        a->command_age_ms == b->command_age_ms &&
        a->accept_limit_ms == b->accept_limit_ms &&
        a->door_lock_target == b->door_lock_target &&
        a->origin == b->origin;
}
static NetWireStatus bcm_door_command_encode(NetWireProfile p, const void *i, NetWireCanEnvelope *o)
{ return net_wire_can_encode_bcm_door_command(p, (const NetWireBcmDoorCommandRaw *)i, o); }
static NetWireStatus bcm_door_command_decode(NetWireProfile p, const NetWireCanEnvelope *i, void *o)
{ return net_wire_can_decode_bcm_door_command(p, i, (NetWireBcmDoorCommandRaw *)o); }
static const BadValue bcm_door_command_bad[] = {
    {0u, 4u, 0u},
    {20u, 1u, 255u},
    {21u, 1u, 6u},
};
static void bcm_door_command_mutate(void *p, size_t index)
{
    NetWireBcmDoorCommandRaw *v = (NetWireBcmDoorCommandRaw *)p;
    switch (index) {
    case 0u: v->boot_id = 0u; break;
    case 1u: v->door_lock_target = 255u; break;
    case 2u: v->origin = 6u; break;
    default: break;
    }
}
static void bcm_door_command_bounds(void *p, int high)
{
    NetWireBcmDoorCommandRaw *v = (NetWireBcmDoorCommandRaw *)p;
    v->boot_id = UINT32_MAX;
    v->tx_sequence = high ? UINT32_MAX : 0u;
    v->target_boot_id = high ? UINT32_MAX : 1u;
    v->command_id = high ? UINT32_MAX : 0u;
    v->command_age_ms = high ? UINT16_MAX : 0u;
    v->accept_limit_ms = high ? UINT16_MAX : 0u;
    v->door_lock_target = high ? 1u : 0u;
    v->origin = high ? 5u : 0u;
}
static void bcm_door_command_randomize(void *p, uint32_t *seed)
{
    NetWireBcmDoorCommandRaw *v = (NetWireBcmDoorCommandRaw *)p;
    v->boot_id = next_random(seed) | 1u;
    v->tx_sequence = next_random(seed);
    v->target_boot_id = next_random(seed) | 1u;
    v->command_id = next_random(seed);
    v->command_age_ms = (uint16_t)next_random(seed);
    v->accept_limit_ms = (uint16_t)next_random(seed);
}

static const uint8_t bcm_climate_command_golden[24] = {
    0x40u, 0x30u, 0x20u, 0x10u, 0x80u, 0x70u, 0x60u, 0x50u, 0xc4u, 0xb3u, 0xa2u, 0x91u, 0x45u, 0x36u, 0x27u, 0x18u, 0x6au, 0x59u, 0x8cu, 0x7bu, 0x03u, 0x02u, 0x64u, 0x00u
};
static void bcm_climate_command_sample(void *p)
{
    NetWireBcmClimateCommandRaw *v = (NetWireBcmClimateCommandRaw *)p;
    memset(v, 0, sizeof(*v));
    v->boot_id = 270544960u;
    v->tx_sequence = 1348497536u;
    v->target_boot_id = 2443359172u;
    v->command_id = 405222981u;
    v->command_age_ms = 22890u;
    v->accept_limit_ms = 31628u;
    v->fan_target_level = 3u;
    v->thermal_direction = 2u;
    v->thermal_target_percent = 100u;
}
static int bcm_climate_command_equal(const void *pa, const void *pb)
{
    const NetWireBcmClimateCommandRaw *a = (const NetWireBcmClimateCommandRaw *)pa, *b = (const NetWireBcmClimateCommandRaw *)pb;
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->target_boot_id == b->target_boot_id &&
        a->command_id == b->command_id &&
        a->command_age_ms == b->command_age_ms &&
        a->accept_limit_ms == b->accept_limit_ms &&
        a->fan_target_level == b->fan_target_level &&
        a->thermal_direction == b->thermal_direction &&
        a->thermal_target_percent == b->thermal_target_percent;
}
static NetWireStatus bcm_climate_command_encode(NetWireProfile p, const void *i, NetWireCanEnvelope *o)
{ return net_wire_can_encode_bcm_climate_command(p, (const NetWireBcmClimateCommandRaw *)i, o); }
static NetWireStatus bcm_climate_command_decode(NetWireProfile p, const NetWireCanEnvelope *i, void *o)
{ return net_wire_can_decode_bcm_climate_command(p, i, (NetWireBcmClimateCommandRaw *)o); }
static const BadValue bcm_climate_command_bad[] = {
    {0u, 4u, 0u},
    {20u, 1u, 255u},
    {21u, 1u, 3u},
    {22u, 1u, 101u},
};
static void bcm_climate_command_mutate(void *p, size_t index)
{
    NetWireBcmClimateCommandRaw *v = (NetWireBcmClimateCommandRaw *)p;
    switch (index) {
    case 0u: v->boot_id = 0u; break;
    case 1u: v->fan_target_level = 255u; break;
    case 2u: v->thermal_direction = 3u; break;
    case 3u: v->thermal_target_percent = 101u; break;
    default: break;
    }
}
static void bcm_climate_command_bounds(void *p, int high)
{
    NetWireBcmClimateCommandRaw *v = (NetWireBcmClimateCommandRaw *)p;
    v->boot_id = UINT32_MAX;
    v->tx_sequence = high ? UINT32_MAX : 0u;
    v->target_boot_id = high ? UINT32_MAX : 1u;
    v->command_id = high ? UINT32_MAX : 0u;
    v->command_age_ms = high ? UINT16_MAX : 0u;
    v->accept_limit_ms = high ? UINT16_MAX : 0u;
    v->fan_target_level = high ? 3u : 0u;
    v->thermal_direction = high ? 2u : 0u;
    v->thermal_target_percent = high ? 100u : 0u;
}
static void bcm_climate_command_randomize(void *p, uint32_t *seed)
{
    NetWireBcmClimateCommandRaw *v = (NetWireBcmClimateCommandRaw *)p;
    v->boot_id = next_random(seed) | 1u;
    v->tx_sequence = next_random(seed);
    v->target_boot_id = next_random(seed) | 1u;
    v->command_id = next_random(seed);
    v->command_age_ms = (uint16_t)next_random(seed);
    v->accept_limit_ms = (uint16_t)next_random(seed);
}

static const uint8_t bcm_light_command_golden[32] = {
    0x40u, 0x30u, 0x20u, 0x10u, 0x80u, 0x70u, 0x60u, 0x50u, 0xc4u, 0xb3u, 0xa2u, 0x91u, 0x45u, 0x36u, 0x27u, 0x18u, 0x6au, 0x59u, 0x8cu, 0x7bu, 0x02u, 0x25u, 0x12u, 0x34u, 0x56u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
};
static void bcm_light_command_sample(void *p)
{
    NetWireBcmLightCommandRaw *v = (NetWireBcmLightCommandRaw *)p;
    memset(v, 0, sizeof(*v));
    v->boot_id = 270544960u;
    v->tx_sequence = 1348497536u;
    v->target_boot_id = 2443359172u;
    v->command_id = 405222981u;
    v->command_age_ms = 22890u;
    v->accept_limit_ms = 31628u;
    v->interior_light_type = 2u;
    v->light_level_percent = 37u;
    v->red = 18u;
    v->green = 52u;
    v->blue = 86u;
}
static int bcm_light_command_equal(const void *pa, const void *pb)
{
    const NetWireBcmLightCommandRaw *a = (const NetWireBcmLightCommandRaw *)pa, *b = (const NetWireBcmLightCommandRaw *)pb;
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->target_boot_id == b->target_boot_id &&
        a->command_id == b->command_id &&
        a->command_age_ms == b->command_age_ms &&
        a->accept_limit_ms == b->accept_limit_ms &&
        a->interior_light_type == b->interior_light_type &&
        a->light_level_percent == b->light_level_percent &&
        a->red == b->red &&
        a->green == b->green &&
        a->blue == b->blue;
}
static NetWireStatus bcm_light_command_encode(NetWireProfile p, const void *i, NetWireCanEnvelope *o)
{ return net_wire_can_encode_bcm_light_command(p, (const NetWireBcmLightCommandRaw *)i, o); }
static NetWireStatus bcm_light_command_decode(NetWireProfile p, const NetWireCanEnvelope *i, void *o)
{ return net_wire_can_decode_bcm_light_command(p, i, (NetWireBcmLightCommandRaw *)o); }
static const BadValue bcm_light_command_bad[] = {
    {0u, 4u, 0u},
    {20u, 1u, 4u},
    {21u, 1u, 101u},
};
static void bcm_light_command_mutate(void *p, size_t index)
{
    NetWireBcmLightCommandRaw *v = (NetWireBcmLightCommandRaw *)p;
    switch (index) {
    case 0u: v->boot_id = 0u; break;
    case 1u: v->interior_light_type = 4u; break;
    case 2u: v->light_level_percent = 101u; break;
    default: break;
    }
}
static void bcm_light_command_bounds(void *p, int high)
{
    NetWireBcmLightCommandRaw *v = (NetWireBcmLightCommandRaw *)p;
    v->boot_id = UINT32_MAX;
    v->tx_sequence = high ? UINT32_MAX : 0u;
    v->target_boot_id = high ? UINT32_MAX : 1u;
    v->command_id = high ? UINT32_MAX : 0u;
    v->command_age_ms = high ? UINT16_MAX : 0u;
    v->accept_limit_ms = high ? UINT16_MAX : 0u;
    v->red = high ? UINT8_MAX : 0u; v->green = high ? 0u : UINT8_MAX; v->blue = high ? UINT8_MAX : 0u;
    v->interior_light_type = high ? 3u : 0u;
    v->light_level_percent = high ? 100u : 0u;
}
static void bcm_light_command_randomize(void *p, uint32_t *seed)
{
    NetWireBcmLightCommandRaw *v = (NetWireBcmLightCommandRaw *)p;
    v->boot_id = next_random(seed) | 1u;
    v->tx_sequence = next_random(seed);
    v->target_boot_id = next_random(seed) | 1u;
    v->command_id = next_random(seed);
    v->command_age_ms = (uint16_t)next_random(seed);
    v->accept_limit_ms = (uint16_t)next_random(seed);
    v->red = (uint8_t)next_random(seed); v->green = (uint8_t)next_random(seed); v->blue = (uint8_t)next_random(seed);
}

static const uint8_t bcm_door_state_golden[20] = {
    0x40u, 0x30u, 0x20u, 0x10u, 0x80u, 0x70u, 0x60u, 0x50u, 0xffu, 0x01u, 0x02u, 0x03u, 0x01u, 0x02u, 0xaau, 0xbbu, 0xccu, 0xddu, 0x9au, 0xbcu
};
static void bcm_door_state_sample(void *p)
{
    NetWireBcmDoorStateRaw *v = (NetWireBcmDoorStateRaw *)p;
    memset(v, 0, sizeof(*v));
    v->boot_id = 270544960u;
    v->tx_sequence = 1348497536u;
    v->door_lock_state = 255u;
    v->door_open_state = 1u;
    v->door_composite_state = 2u;
    v->lock_quality = 3u;
    v->open_quality = 1u;
    v->composite_quality = 2u;
    v->update_sequence = 3721182122u;
    v->source_age_ms = 48282u;
}
static int bcm_door_state_equal(const void *pa, const void *pb)
{
    const NetWireBcmDoorStateRaw *a = (const NetWireBcmDoorStateRaw *)pa, *b = (const NetWireBcmDoorStateRaw *)pb;
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->door_lock_state == b->door_lock_state &&
        a->door_open_state == b->door_open_state &&
        a->door_composite_state == b->door_composite_state &&
        a->lock_quality == b->lock_quality &&
        a->open_quality == b->open_quality &&
        a->composite_quality == b->composite_quality &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus bcm_door_state_encode(NetWireProfile p, const void *i, NetWireCanEnvelope *o)
{ return net_wire_can_encode_bcm_door_state(p, (const NetWireBcmDoorStateRaw *)i, o); }
static NetWireStatus bcm_door_state_decode(NetWireProfile p, const NetWireCanEnvelope *i, void *o)
{ return net_wire_can_decode_bcm_door_state(p, i, (NetWireBcmDoorStateRaw *)o); }
static const BadValue bcm_door_state_bad[] = {
    {0u, 4u, 0u},
    {8u, 1u, 2u},
    {9u, 1u, 254u},
    {10u, 1u, 3u},
    {11u, 1u, 4u},
    {12u, 1u, 255u},
    {13u, 1u, 4u},
};
static void bcm_door_state_mutate(void *p, size_t index)
{
    NetWireBcmDoorStateRaw *v = (NetWireBcmDoorStateRaw *)p;
    switch (index) {
    case 0u: v->boot_id = 0u; break;
    case 1u: v->door_lock_state = 2u; break;
    case 2u: v->door_open_state = 254u; break;
    case 3u: v->door_composite_state = 3u; break;
    case 4u: v->lock_quality = 4u; break;
    case 5u: v->open_quality = 255u; break;
    case 6u: v->composite_quality = 4u; break;
    default: break;
    }
}
static void bcm_door_state_bounds(void *p, int high)
{
    NetWireBcmDoorStateRaw *v = (NetWireBcmDoorStateRaw *)p;
    v->boot_id = UINT32_MAX;
    v->tx_sequence = high ? UINT32_MAX : 0u;
    v->update_sequence = high ? UINT32_MAX : 0u;
    v->source_age_ms = high ? UINT16_MAX : 0u;
    v->door_lock_state = high ? 255u : 0u;
    v->door_open_state = high ? 255u : 0u;
    v->door_composite_state = high ? 2u : 0u;
    v->lock_quality = high ? 3u : 0u;
    v->open_quality = high ? 3u : 0u;
    v->composite_quality = high ? 3u : 0u;
}
static void bcm_door_state_randomize(void *p, uint32_t *seed)
{
    NetWireBcmDoorStateRaw *v = (NetWireBcmDoorStateRaw *)p;
    v->boot_id = next_random(seed) | 1u;
    v->tx_sequence = next_random(seed);
    v->update_sequence = next_random(seed);
    v->source_age_ms = (uint16_t)next_random(seed);
}

static const uint8_t bcm_climate_state_golden[24] = {
    0x40u, 0x30u, 0x20u, 0x10u, 0x80u, 0x70u, 0x60u, 0x50u, 0x02u, 0xffu, 0x01u, 0x25u, 0xffu, 0x00u, 0x03u, 0x01u, 0x02u, 0x03u, 0xaau, 0xbbu, 0xccu, 0xddu, 0x9au, 0xbcu
};
static void bcm_climate_state_sample(void *p)
{
    NetWireBcmClimateStateRaw *v = (NetWireBcmClimateStateRaw *)p;
    memset(v, 0, sizeof(*v));
    v->boot_id = 270544960u;
    v->tx_sequence = 1348497536u;
    v->fan_command_level = 2u;
    v->fan_measured_level = 255u;
    v->thermal_direction_state = 1u;
    v->thermal_output_percent = 37u;
    v->heat_removal_state = 255u;
    v->fan_command_quality = 0u;
    v->fan_measured_quality = 3u;
    v->thermal_direction_quality = 1u;
    v->thermal_output_quality = 2u;
    v->heat_removal_quality = 3u;
    v->update_sequence = 3721182122u;
    v->source_age_ms = 48282u;
}
static int bcm_climate_state_equal(const void *pa, const void *pb)
{
    const NetWireBcmClimateStateRaw *a = (const NetWireBcmClimateStateRaw *)pa, *b = (const NetWireBcmClimateStateRaw *)pb;
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->fan_command_level == b->fan_command_level &&
        a->fan_measured_level == b->fan_measured_level &&
        a->thermal_direction_state == b->thermal_direction_state &&
        a->thermal_output_percent == b->thermal_output_percent &&
        a->heat_removal_state == b->heat_removal_state &&
        a->fan_command_quality == b->fan_command_quality &&
        a->fan_measured_quality == b->fan_measured_quality &&
        a->thermal_direction_quality == b->thermal_direction_quality &&
        a->thermal_output_quality == b->thermal_output_quality &&
        a->heat_removal_quality == b->heat_removal_quality &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus bcm_climate_state_encode(NetWireProfile p, const void *i, NetWireCanEnvelope *o)
{ return net_wire_can_encode_bcm_climate_state(p, (const NetWireBcmClimateStateRaw *)i, o); }
static NetWireStatus bcm_climate_state_decode(NetWireProfile p, const NetWireCanEnvelope *i, void *o)
{ return net_wire_can_decode_bcm_climate_state(p, i, (NetWireBcmClimateStateRaw *)o); }
static const BadValue bcm_climate_state_bad[] = {
    {0u, 4u, 0u},
    {8u, 1u, 255u},
    {9u, 1u, 4u},
    {10u, 1u, 3u},
    {11u, 1u, 101u},
    {12u, 1u, 2u},
    {13u, 1u, 4u},
    {14u, 1u, 4u},
    {15u, 1u, 4u},
    {16u, 1u, 4u},
    {17u, 1u, 255u},
};
static void bcm_climate_state_mutate(void *p, size_t index)
{
    NetWireBcmClimateStateRaw *v = (NetWireBcmClimateStateRaw *)p;
    switch (index) {
    case 0u: v->boot_id = 0u; break;
    case 1u: v->fan_command_level = 255u; break;
    case 2u: v->fan_measured_level = 4u; break;
    case 3u: v->thermal_direction_state = 3u; break;
    case 4u: v->thermal_output_percent = 101u; break;
    case 5u: v->heat_removal_state = 2u; break;
    case 6u: v->fan_command_quality = 4u; break;
    case 7u: v->fan_measured_quality = 4u; break;
    case 8u: v->thermal_direction_quality = 4u; break;
    case 9u: v->thermal_output_quality = 4u; break;
    case 10u: v->heat_removal_quality = 255u; break;
    default: break;
    }
}
static void bcm_climate_state_bounds(void *p, int high)
{
    NetWireBcmClimateStateRaw *v = (NetWireBcmClimateStateRaw *)p;
    v->boot_id = UINT32_MAX;
    v->tx_sequence = high ? UINT32_MAX : 0u;
    v->update_sequence = high ? UINT32_MAX : 0u;
    v->source_age_ms = high ? UINT16_MAX : 0u;
    v->fan_command_level = high ? 3u : 0u;
    v->fan_measured_level = high ? 255u : 0u;
    v->thermal_direction_state = high ? 2u : 0u;
    v->thermal_output_percent = high ? 100u : 0u;
    v->heat_removal_state = high ? 255u : 0u;
    v->fan_command_quality = high ? 3u : 0u;
    v->fan_measured_quality = high ? 3u : 0u;
    v->thermal_direction_quality = high ? 3u : 0u;
    v->thermal_output_quality = high ? 3u : 0u;
    v->heat_removal_quality = high ? 3u : 0u;
}
static void bcm_climate_state_randomize(void *p, uint32_t *seed)
{
    NetWireBcmClimateStateRaw *v = (NetWireBcmClimateStateRaw *)p;
    v->boot_id = next_random(seed) | 1u;
    v->tx_sequence = next_random(seed);
    v->update_sequence = next_random(seed);
    v->source_age_ms = (uint16_t)next_random(seed);
}

static const uint8_t bcm_light_state_golden[32] = {
    0x40u, 0x30u, 0x20u, 0x10u, 0x80u, 0x70u, 0x60u, 0x50u, 0x03u, 0x49u, 0x12u, 0x34u, 0x56u, 0xffu, 0x00u, 0x01u, 0x02u, 0x03u, 0x03u, 0xaau, 0xbbu, 0xccu, 0xddu, 0x9au, 0xbcu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
};
static void bcm_light_state_sample(void *p)
{
    NetWireBcmLightStateRaw *v = (NetWireBcmLightStateRaw *)p;
    memset(v, 0, sizeof(*v));
    v->boot_id = 270544960u;
    v->tx_sequence = 1348497536u;
    v->interior_light_type_state = 3u;
    v->light_level_percent = 73u;
    v->red = 18u;
    v->green = 52u;
    v->blue = 86u;
    v->interior_light_apply_result = 255u;
    v->light_physical_feedback_capability = 0u;
    v->light_type_quality = 1u;
    v->light_level_quality = 2u;
    v->light_color_quality = 3u;
    v->light_apply_quality = 3u;
    v->update_sequence = 3721182122u;
    v->source_age_ms = 48282u;
}
static int bcm_light_state_equal(const void *pa, const void *pb)
{
    const NetWireBcmLightStateRaw *a = (const NetWireBcmLightStateRaw *)pa, *b = (const NetWireBcmLightStateRaw *)pb;
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->interior_light_type_state == b->interior_light_type_state &&
        a->light_level_percent == b->light_level_percent &&
        a->red == b->red &&
        a->green == b->green &&
        a->blue == b->blue &&
        a->interior_light_apply_result == b->interior_light_apply_result &&
        a->light_physical_feedback_capability == b->light_physical_feedback_capability &&
        a->light_type_quality == b->light_type_quality &&
        a->light_level_quality == b->light_level_quality &&
        a->light_color_quality == b->light_color_quality &&
        a->light_apply_quality == b->light_apply_quality &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus bcm_light_state_encode(NetWireProfile p, const void *i, NetWireCanEnvelope *o)
{ return net_wire_can_encode_bcm_light_state(p, (const NetWireBcmLightStateRaw *)i, o); }
static NetWireStatus bcm_light_state_decode(NetWireProfile p, const NetWireCanEnvelope *i, void *o)
{ return net_wire_can_decode_bcm_light_state(p, i, (NetWireBcmLightStateRaw *)o); }
static const BadValue bcm_light_state_bad[] = {
    {0u, 4u, 0u},
    {8u, 1u, 4u},
    {9u, 1u, 101u},
    {13u, 1u, 2u},
    {14u, 1u, 2u},
    {15u, 1u, 4u},
    {16u, 1u, 4u},
    {17u, 1u, 4u},
    {18u, 1u, 255u},
};
static void bcm_light_state_mutate(void *p, size_t index)
{
    NetWireBcmLightStateRaw *v = (NetWireBcmLightStateRaw *)p;
    switch (index) {
    case 0u: v->boot_id = 0u; break;
    case 1u: v->interior_light_type_state = 4u; break;
    case 2u: v->light_level_percent = 101u; break;
    case 3u: v->interior_light_apply_result = 2u; break;
    case 4u: v->light_physical_feedback_capability = 2u; break;
    case 5u: v->light_type_quality = 4u; break;
    case 6u: v->light_level_quality = 4u; break;
    case 7u: v->light_color_quality = 4u; break;
    case 8u: v->light_apply_quality = 255u; break;
    default: break;
    }
}
static void bcm_light_state_bounds(void *p, int high)
{
    NetWireBcmLightStateRaw *v = (NetWireBcmLightStateRaw *)p;
    v->boot_id = UINT32_MAX;
    v->tx_sequence = high ? UINT32_MAX : 0u;
    v->update_sequence = high ? UINT32_MAX : 0u;
    v->source_age_ms = high ? UINT16_MAX : 0u;
    v->red = high ? UINT8_MAX : 0u; v->green = high ? 0u : UINT8_MAX; v->blue = high ? UINT8_MAX : 0u;
    v->interior_light_type_state = high ? 3u : 0u;
    v->light_level_percent = high ? 100u : 0u;
    v->interior_light_apply_result = high ? 255u : 0u;
    v->light_physical_feedback_capability = high ? 1u : 0u;
    v->light_type_quality = high ? 3u : 0u;
    v->light_level_quality = high ? 3u : 0u;
    v->light_color_quality = high ? 3u : 0u;
    v->light_apply_quality = high ? 3u : 0u;
}
static void bcm_light_state_randomize(void *p, uint32_t *seed)
{
    NetWireBcmLightStateRaw *v = (NetWireBcmLightStateRaw *)p;
    v->boot_id = next_random(seed) | 1u;
    v->tx_sequence = next_random(seed);
    v->update_sequence = next_random(seed);
    v->source_age_ms = (uint16_t)next_random(seed);
    v->red = (uint8_t)next_random(seed); v->green = (uint8_t)next_random(seed); v->blue = (uint8_t)next_random(seed);
}

static const uint8_t bcm_result_golden[32] = {
    0x40u, 0x30u, 0x20u, 0x10u, 0x80u, 0x70u, 0x60u, 0x50u, 0xc4u, 0xb3u, 0xa2u, 0x91u, 0x45u, 0x36u, 0x27u, 0x18u, 0x01u, 0x02u, 0x09u, 0x00u, 0x03u, 0xb4u, 0xc3u, 0xd2u, 0xe1u, 0x9au, 0x78u, 0x01u, 0x01u, 0x00u, 0x00u, 0x00u
};
static void bcm_result_sample(void *p)
{
    NetWireBcmResultRaw *v = (NetWireBcmResultRaw *)p;
    memset(v, 0, sizeof(*v));
    v->boot_id = 270544960u;
    v->tx_sequence = 1348497536u;
    v->command_domain_boot = 2443359172u;
    v->command_id = 405222981u;
    v->command_kind = 1u;
    v->result = 2u;
    v->result_reason = 9u;
    v->door_event = 3u;
    v->occurrence_id = 3788686260u;
    v->event_age_ms = 30874u;
    v->event_quality = 1u;
    v->door_target = 1u;
}
static int bcm_result_equal(const void *pa, const void *pb)
{
    const NetWireBcmResultRaw *a = (const NetWireBcmResultRaw *)pa, *b = (const NetWireBcmResultRaw *)pb;
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->command_domain_boot == b->command_domain_boot &&
        a->command_id == b->command_id &&
        a->command_kind == b->command_kind &&
        a->result == b->result &&
        a->result_reason == b->result_reason &&
        a->door_event == b->door_event &&
        a->occurrence_id == b->occurrence_id &&
        a->event_age_ms == b->event_age_ms &&
        a->event_quality == b->event_quality &&
        a->door_target == b->door_target;
}
static NetWireStatus bcm_result_encode(NetWireProfile p, const void *i, NetWireCanEnvelope *o)
{ return net_wire_can_encode_bcm_result(p, (const NetWireBcmResultRaw *)i, o); }
static NetWireStatus bcm_result_decode(NetWireProfile p, const NetWireCanEnvelope *i, void *o)
{ return net_wire_can_decode_bcm_result(p, i, (NetWireBcmResultRaw *)o); }
static const BadValue bcm_result_bad[] = {
    {0u, 4u, 0u},
    {16u, 1u, 0u},
    {16u, 1u, 4u},
    {17u, 1u, 6u},
    {18u, 2u, 26u},
    {20u, 1u, 6u},
    {27u, 1u, 4u},
    {28u, 1u, 2u},
};
static void bcm_result_mutate(void *p, size_t index)
{
    NetWireBcmResultRaw *v = (NetWireBcmResultRaw *)p;
    switch (index) {
    case 0u: v->boot_id = 0u; break;
    case 1u: v->command_kind = 0u; break;
    case 2u: v->command_kind = 4u; break;
    case 3u: v->result = 6u; break;
    case 4u: v->result_reason = 26u; break;
    case 5u: v->door_event = 6u; break;
    case 6u: v->event_quality = 4u; break;
    case 7u: v->door_target = 2u; break;
    default: break;
    }
}
static void bcm_result_bounds(void *p, int high)
{
    NetWireBcmResultRaw *v = (NetWireBcmResultRaw *)p;
    v->boot_id = UINT32_MAX;
    v->tx_sequence = high ? UINT32_MAX : 0u;
    v->command_domain_boot = high ? UINT32_MAX : 1u;
    v->command_id = high ? UINT32_MAX : 0u;
    v->occurrence_id = high ? UINT32_MAX : 0u;
    v->door_event = high ? 3u : 0u;
    v->event_age_ms = high ? UINT16_MAX : 0u;
    v->result = high ? 5u : 0u;
    v->result_reason = high ? 25u : 0u;
    v->event_quality = high ? 3u : 0u;
    v->door_target = high ? 1u : 0u;
}
static void bcm_result_randomize(void *p, uint32_t *seed)
{
    NetWireBcmResultRaw *v = (NetWireBcmResultRaw *)p;
    v->boot_id = next_random(seed) | 1u;
    v->tx_sequence = next_random(seed);
    v->command_domain_boot = next_random(seed) | 1u;
    v->command_id = next_random(seed);
    v->occurrence_id = next_random(seed) | 1u;
    v->event_age_ms = (uint16_t)next_random(seed);
}

static const uint8_t bcm_event_golden[32] = {
    0x40u, 0x30u, 0x20u, 0x10u, 0x80u, 0x70u, 0x60u, 0x50u, 0x02u, 0x31u, 0x32u, 0x33u, 0x34u, 0x61u, 0x62u, 0x02u, 0x71u, 0x72u, 0x73u, 0x74u, 0x81u, 0x82u, 0x83u, 0x84u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
};
static void bcm_event_sample(void *p)
{
    NetWireBcmEventRaw *v = (NetWireBcmEventRaw *)p;
    memset(v, 0, sizeof(*v));
    v->boot_id = 270544960u;
    v->tx_sequence = 1348497536u;
    v->event_kind = 2u;
    v->occurrence_id = 875770417u;
    v->event_age_ms = 25185u;
    v->event_quality = 2u;
    v->related_domain_boot = 1953722993u;
    v->related_command_id = 2223211137u;
    v->affected_function = 2u;
}
static int bcm_event_equal(const void *pa, const void *pb)
{
    const NetWireBcmEventRaw *a = (const NetWireBcmEventRaw *)pa, *b = (const NetWireBcmEventRaw *)pb;
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->event_kind == b->event_kind &&
        a->occurrence_id == b->occurrence_id &&
        a->event_age_ms == b->event_age_ms &&
        a->event_quality == b->event_quality &&
        a->related_domain_boot == b->related_domain_boot &&
        a->related_command_id == b->related_command_id &&
        a->affected_function == b->affected_function;
}
static NetWireStatus bcm_event_encode(NetWireProfile p, const void *i, NetWireCanEnvelope *o)
{ return net_wire_can_encode_bcm_event(p, (const NetWireBcmEventRaw *)i, o); }
static NetWireStatus bcm_event_decode(NetWireProfile p, const NetWireCanEnvelope *i, void *o)
{ return net_wire_can_decode_bcm_event(p, i, (NetWireBcmEventRaw *)o); }
static const BadValue bcm_event_bad[] = {
    {0u, 4u, 0u},
    {8u, 1u, 0u},
    {8u, 1u, 4u},
    {15u, 1u, 4u},
    {24u, 1u, 8u},
};
static void bcm_event_mutate(void *p, size_t index)
{
    NetWireBcmEventRaw *v = (NetWireBcmEventRaw *)p;
    switch (index) {
    case 0u: v->boot_id = 0u; break;
    case 1u: v->event_kind = 0u; break;
    case 2u: v->event_kind = 4u; break;
    case 3u: v->event_quality = 4u; break;
    case 4u: v->affected_function = 8u; break;
    default: break;
    }
}
static void bcm_event_bounds(void *p, int high)
{
    NetWireBcmEventRaw *v = (NetWireBcmEventRaw *)p;
    v->boot_id = UINT32_MAX;
    v->tx_sequence = high ? UINT32_MAX : 0u;
    v->occurrence_id = high ? UINT32_MAX : 0u;
    v->event_age_ms = high ? UINT16_MAX : 0u;
    v->related_domain_boot = high ? UINT32_MAX : 0u;
    v->related_command_id = high ? UINT32_MAX : 0u;
    v->event_kind = high ? 3u : 1u;
    v->event_quality = high ? 3u : 0u;
    v->affected_function = high ? 7u : 0u;
}
static void bcm_event_randomize(void *p, uint32_t *seed)
{
    NetWireBcmEventRaw *v = (NetWireBcmEventRaw *)p;
    v->boot_id = next_random(seed) | 1u;
    v->tx_sequence = next_random(seed);
    v->occurrence_id = next_random(seed) | 1u;
    v->event_age_ms = (uint16_t)next_random(seed);
    v->related_domain_boot = next_random(seed) | 1u;
    v->related_command_id = next_random(seed) | 1u;
}

static const uint8_t bcm_status_golden[24] = {
    0x40u, 0x30u, 0x20u, 0x10u, 0x80u, 0x70u, 0x60u, 0x50u, 0x03u, 0xa5u, 0x00u, 0x03u, 0x08u, 0x00u, 0x07u, 0x01u, 0xd4u, 0xc3u, 0xb2u, 0xa1u, 0xefu, 0xcdu, 0x00u, 0x00u
};
static void bcm_status_sample(void *p)
{
    NetWireBcmStatusRaw *v = (NetWireBcmStatusRaw *)p;
    memset(v, 0, sizeof(*v));
    v->boot_id = 270544960u;
    v->tx_sequence = 1348497536u;
    v->bcm_ecu_state = 3u;
    v->fault_mask = 165u;
    v->active_fault_category = 3u;
    v->active_fault_code = 8u;
    v->affected_function = 7u;
    v->recovering = 1u;
    v->update_sequence = 2712847316u;
    v->source_age_ms = 52719u;
}
static int bcm_status_equal(const void *pa, const void *pb)
{
    const NetWireBcmStatusRaw *a = (const NetWireBcmStatusRaw *)pa, *b = (const NetWireBcmStatusRaw *)pb;
    return a->boot_id == b->boot_id &&
        a->tx_sequence == b->tx_sequence &&
        a->bcm_ecu_state == b->bcm_ecu_state &&
        a->fault_mask == b->fault_mask &&
        a->active_fault_category == b->active_fault_category &&
        a->active_fault_code == b->active_fault_code &&
        a->affected_function == b->affected_function &&
        a->recovering == b->recovering &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus bcm_status_encode(NetWireProfile p, const void *i, NetWireCanEnvelope *o)
{ return net_wire_can_encode_bcm_status(p, (const NetWireBcmStatusRaw *)i, o); }
static NetWireStatus bcm_status_decode(NetWireProfile p, const NetWireCanEnvelope *i, void *o)
{ return net_wire_can_decode_bcm_status(p, i, (NetWireBcmStatusRaw *)o); }
static const BadValue bcm_status_bad[] = {
    {0u, 4u, 0u},
    {8u, 1u, 4u},
    {9u, 2u, 256u},
    {11u, 1u, 4u},
    {12u, 2u, 9u},
    {14u, 1u, 8u},
    {15u, 1u, 2u},
};
static void bcm_status_mutate(void *p, size_t index)
{
    NetWireBcmStatusRaw *v = (NetWireBcmStatusRaw *)p;
    switch (index) {
    case 0u: v->boot_id = 0u; break;
    case 1u: v->bcm_ecu_state = 4u; break;
    case 2u: v->fault_mask = 256u; break;
    case 3u: v->active_fault_category = 4u; break;
    case 4u: v->active_fault_code = 9u; break;
    case 5u: v->affected_function = 8u; break;
    case 6u: v->recovering = 2u; break;
    default: break;
    }
}
static void bcm_status_bounds(void *p, int high)
{
    NetWireBcmStatusRaw *v = (NetWireBcmStatusRaw *)p;
    v->boot_id = UINT32_MAX;
    v->tx_sequence = high ? UINT32_MAX : 0u;
    v->update_sequence = high ? UINT32_MAX : 0u;
    v->source_age_ms = high ? UINT16_MAX : 0u;
    v->bcm_ecu_state = high ? 3u : 0u;
    v->fault_mask = high ? 255u : 0u;
    v->active_fault_category = high ? 3u : 0u;
    v->active_fault_code = high ? 8u : 0u;
    v->affected_function = high ? 7u : 0u;
    v->recovering = high ? 1u : 0u;
}
static void bcm_status_randomize(void *p, uint32_t *seed)
{
    NetWireBcmStatusRaw *v = (NetWireBcmStatusRaw *)p;
    v->boot_id = next_random(seed) | 1u;
    v->tx_sequence = next_random(seed);
    v->update_sequence = next_random(seed);
    v->source_age_ms = (uint16_t)next_random(seed);
}

static const Spec specs[] = {
    {"bcm_door_command", 0x100u, 22u, 24u, 12u, bcm_door_command_golden,
     bcm_door_command_encode, bcm_door_command_decode, bcm_door_command_sample, bcm_door_command_equal,
     bcm_door_command_mutate, bcm_door_command_bad, 3u, bcm_door_command_bounds, bcm_door_command_randomize},
    {"bcm_climate_command", 0x110u, 23u, 24u, 12u, bcm_climate_command_golden,
     bcm_climate_command_encode, bcm_climate_command_decode, bcm_climate_command_sample, bcm_climate_command_equal,
     bcm_climate_command_mutate, bcm_climate_command_bad, 4u, bcm_climate_command_bounds, bcm_climate_command_randomize},
    {"bcm_light_command", 0x120u, 25u, 32u, 13u, bcm_light_command_golden,
     bcm_light_command_encode, bcm_light_command_decode, bcm_light_command_sample, bcm_light_command_equal,
     bcm_light_command_mutate, bcm_light_command_bad, 3u, bcm_light_command_bounds, bcm_light_command_randomize},
    {"bcm_door_state", 0x200u, 20u, 20u, 11u, bcm_door_state_golden,
     bcm_door_state_encode, bcm_door_state_decode, bcm_door_state_sample, bcm_door_state_equal,
     bcm_door_state_mutate, bcm_door_state_bad, 7u, bcm_door_state_bounds, bcm_door_state_randomize},
    {"bcm_climate_state", 0x210u, 24u, 24u, 12u, bcm_climate_state_golden,
     bcm_climate_state_encode, bcm_climate_state_decode, bcm_climate_state_sample, bcm_climate_state_equal,
     bcm_climate_state_mutate, bcm_climate_state_bad, 11u, bcm_climate_state_bounds, bcm_climate_state_randomize},
    {"bcm_light_state", 0x220u, 25u, 32u, 13u, bcm_light_state_golden,
     bcm_light_state_encode, bcm_light_state_decode, bcm_light_state_sample, bcm_light_state_equal,
     bcm_light_state_mutate, bcm_light_state_bad, 9u, bcm_light_state_bounds, bcm_light_state_randomize},
    {"bcm_result", 0x140u, 29u, 32u, 13u, bcm_result_golden,
     bcm_result_encode, bcm_result_decode, bcm_result_sample, bcm_result_equal,
     bcm_result_mutate, bcm_result_bad, 8u, bcm_result_bounds, bcm_result_randomize},
    {"bcm_event", 0x141u, 25u, 32u, 13u, bcm_event_golden,
     bcm_event_encode, bcm_event_decode, bcm_event_sample, bcm_event_equal,
     bcm_event_mutate, bcm_event_bad, 5u, bcm_event_bounds, bcm_event_randomize},
    {"bcm_status", 0x230u, 22u, 24u, 12u, bcm_status_golden,
     bcm_status_encode, bcm_status_decode, bcm_status_sample, bcm_status_equal,
     bcm_status_mutate, bcm_status_bad, 7u, bcm_status_bounds, bcm_status_randomize},
};

static NetWireCanEnvelope literal_frame(const Spec *s)
{
    NetWireCanEnvelope f = {0}; f.standard_id = s->id;
    f.fdf = true; f.brs = true; f.dlc = s->dlc; f.length = s->length;
    memcpy(f.bytes, s->golden, s->length); return f;
}
static int fail_decode(const Spec *s, const NetWireCanEnvelope *f, NetWireStatus status)
{
    Storage out, before; memset(&out, 0xa6, sizeof(out)); memcpy(&before, &out, sizeof(before));
    CHECK(s->decode(CANDIDATE, f, &out) == status);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0); return 0;
}
static int fail_encode(const Spec *s, const void *input, NetWireStatus status)
{
    NetWireCanEnvelope out, before; memset(&out, 0xb7, sizeof(out)); memcpy(&before, &out, sizeof(before));
    CHECK(s->encode(CANDIDATE, input, &out) == status);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0); return 0;
}
static int run_message(const Spec *s)
{
    Storage expected, decoded; NetWireCanEnvelope f, g = literal_frame(s); size_t i, k;
    s->sample(&expected); memset(&decoded, 0x81, sizeof(decoded)); memset(&f, 0x57, sizeof(f));
    CHECK(s->decode(CANDIDATE, &g, &decoded) == NET_WIRE_OK);
    CHECK(s->equal(&expected, &decoded));
    CHECK(s->encode(CANDIDATE, &expected, &f) == NET_WIRE_OK);
    CHECK(f.standard_id == s->id && f.dlc == s->dlc && f.length == s->length);
    CHECK(!f.extended && f.fdf && f.brs && !f.rtr && !f.driver_error);
    CHECK(memcmp(f.bytes, s->golden, s->length) == 0);
    for (i = s->used; i < s->length; ++i) CHECK(f.bytes[i] == 0u);
    /* Each ID independently receives every envelope error, with full atomicity. */
    for (i = 0u; i < 14u; ++i) {
        f = g;
        switch (i) {
        case 0: f.standard_id ^= 1u; break;
        case 1: f.standard_id |= 0x800u; break;
        case 2: f.extended = true; break;
        case 3: f.fdf = false; break;
        case 4: f.brs = false; break;
        case 5: f.rtr = true; break;
        case 6: f.driver_error = true; break;
        case 7: f.dlc = (uint8_t)(s->dlc - 1u); break;
        case 8: f.dlc = (uint8_t)(s->dlc + 1u); break;
        case 9: f.dlc = (uint8_t)s->length; break;
        case 10: f.length = s->length - 1u; break;
        case 11: f.length = s->length + 1u; break;
        case 12: f.length = SIZE_MAX; break;
        default: f.length = 0u; break;
        }
        CHECK(fail_decode(s, &f, NET_WIRE_MALFORMED) == 0);
    }
    for (i = s->used; i < s->length; ++i) {
        f = g; f.bytes[i] = 1u; CHECK(fail_decode(s, &f, NET_WIRE_MALFORMED) == 0);
    }
    /* Byte after actual frame is outside wire and is deliberately ignored. */
    f = g; f.bytes[s->length] = 0x97u;
    CHECK(s->decode(CANDIDATE, &f, &decoded) == NET_WIRE_OK);
    CHECK(s->equal(&expected, &decoded));
    for (i = 0u; i < s->bad_count; ++i) {
        s->sample(&expected); s->mutate(&expected, i);
        CHECK(fail_encode(s, &expected, NET_WIRE_MALFORMED) == 0);
        f = g;
        for (k = 0; k < s->bad[i].width; ++k)
            f.bytes[s->bad[i].offset + k] = (uint8_t)(s->bad[i].value >> (8u * k));
        CHECK(fail_decode(s, &f, NET_WIRE_MALFORMED) == 0);
    }
    /* Unselected and invalid profiles preserve both kinds of complete output. */
    for (i = 0u; i < 2u; ++i) {
        NetWireCanEnvelope before; Storage d_before;
        NetWireProfile profile = i == 0u ? NET_WIRE_NOT_BOUND : (NetWireProfile)99;
        NetWireStatus status = i == 0u ? NET_WIRE_NOT_BOUND_STATUS : NET_WIRE_INVALID_ARGUMENT;
        s->sample(&expected); memset(&f, 0x46, sizeof(f)); memcpy(&before, &f, sizeof(f));
        memset(&decoded, 0x65, sizeof(decoded)); memcpy(&d_before, &decoded, sizeof(decoded));
        CHECK(s->encode(profile, &expected, &f) == status);
        CHECK(s->decode(profile, &g, &decoded) == status);
        CHECK(memcmp(&f, &before, sizeof(f)) == 0);
        CHECK(memcmp(&decoded, &d_before, sizeof(decoded)) == 0);
    }
    CHECK(fail_encode(s, NULL, NET_WIRE_INVALID_ARGUMENT) == 0);
    CHECK(fail_decode(s, NULL, NET_WIRE_INVALID_ARGUMENT) == 0);
    CHECK(s->encode(CANDIDATE, &expected, NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(s->decode(CANDIDATE, &g, NULL) == NET_WIRE_INVALID_ARGUMENT);
    for (i = 0u; i < 2u; ++i) {
        union { max_align_t alignment; uint8_t bytes[256]; } buffer;
        uint8_t before[256]; size_t offset = i * _Alignof(max_align_t);
        NetWireCanEnvelope *wire = (NetWireCanEnvelope *)(void *)buffer.bytes;
        void *dto = (void *)(buffer.bytes + offset);
        memset(buffer.bytes, 0x59, sizeof(buffer.bytes)); memcpy(before, buffer.bytes, sizeof(before));
        CHECK(s->encode(CANDIDATE, dto, wire) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(s->decode(CANDIDATE, wire, dto) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(before, buffer.bytes, sizeof(before)) == 0);
        /* Reverse partial overlap checks the other direction as well. */
        wire = (NetWireCanEnvelope *)(void *)(buffer.bytes + offset); dto = buffer.bytes;
        CHECK(s->encode(CANDIDATE, dto, wire) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(s->decode(CANDIDATE, wire, dto) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(before, buffer.bytes, sizeof(before)) == 0);
    }
    for (i = 0u; i < 2u; ++i) {
        s->sample(&expected); s->bounds(&expected, (int)i);
        CHECK(s->encode(CANDIDATE, &expected, &f) == NET_WIRE_OK);
        CHECK(s->decode(CANDIDATE, &f, &decoded) == NET_WIRE_OK);
        CHECK(s->equal(&expected, &decoded));
    }
    printf("PASS %s: independent golden, fields, envelope, null/alias, atomicity, boundaries\n", s->name);
    return 0;
}

static int semantic_holds(void)
{
    NetWireBcmResultRaw r; NetWireBcmEventRaw e; NetWireCanEnvelope f;
    NetWireBcmDoorCommandRaw door; NetWireBcmClimateCommandRaw climate; NetWireBcmLightCommandRaw light;
    bcm_door_command_sample(&door); door.target_boot_id = 0u;
    CHECK(fail_encode(&specs[0], &door, NET_WIRE_CONTRACT_HOLD) == 0);
    f = literal_frame(&specs[0]); memset(f.bytes + 8u, 0, 4u);
    CHECK(fail_decode(&specs[0], &f, NET_WIRE_CONTRACT_HOLD) == 0);
    bcm_climate_command_sample(&climate); climate.target_boot_id = 0u;
    CHECK(fail_encode(&specs[1], &climate, NET_WIRE_CONTRACT_HOLD) == 0);
    f = literal_frame(&specs[1]); memset(f.bytes + 8u, 0, 4u);
    CHECK(fail_decode(&specs[1], &f, NET_WIRE_CONTRACT_HOLD) == 0);
    bcm_light_command_sample(&light); light.target_boot_id = 0u;
    CHECK(fail_encode(&specs[2], &light, NET_WIRE_CONTRACT_HOLD) == 0);
    f = literal_frame(&specs[2]); memset(f.bytes + 8u, 0, 4u);
    CHECK(fail_decode(&specs[2], &f, NET_WIRE_CONTRACT_HOLD) == 0);
    bcm_result_sample(&r); r.command_domain_boot = 0u;
    CHECK(fail_encode(&specs[6], &r, NET_WIRE_CONTRACT_HOLD) == 0);
    f = literal_frame(&specs[6]); memset(f.bytes + 8u, 0, 4u);
    CHECK(fail_decode(&specs[6], &f, NET_WIRE_CONTRACT_HOLD) == 0);
    bcm_result_sample(&r); r.result = UINT8_MAX;
    CHECK(fail_encode(&specs[6], &r, NET_WIRE_CONTRACT_HOLD) == 0);
    f = literal_frame(&specs[6]); f.bytes[17] = 255u;
    CHECK(fail_decode(&specs[6], &f, NET_WIRE_CONTRACT_HOLD) == 0);
    bcm_result_sample(&r); r.occurrence_id = 0u;
    CHECK(fail_encode(&specs[6], &r, NET_WIRE_CONTRACT_HOLD) == 0);
    f = literal_frame(&specs[6]); memset(f.bytes + 21u, 0, 4u);
    CHECK(fail_decode(&specs[6], &f, NET_WIRE_CONTRACT_HOLD) == 0);
    bcm_result_sample(&r); r.command_kind = 2u;
    CHECK(fail_encode(&specs[6], &r, NET_WIRE_MALFORMED) == 0);
    f = literal_frame(&specs[6]); f.bytes[16] = 2u;
    CHECK(fail_decode(&specs[6], &f, NET_WIRE_MALFORMED) == 0);
    r.door_event = 0u; r.occurrence_id = 0u; r.door_target = UINT8_MAX;
    CHECK(net_wire_can_encode_bcm_result(CANDIDATE, &r, &f) == NET_WIRE_OK);
    CHECK(f.bytes[16] == 2u && f.bytes[20] == 0u && f.bytes[28] == 255u);
    /* No confirmation field exists and DONE is not execution success proof. */
    bcm_result_sample(&r); r.door_event = 0u;
    CHECK(fail_encode(&specs[6], &r, NET_WIRE_MALFORMED) == 0);
    f = literal_frame(&specs[6]); f.bytes[20] = 0u;
    CHECK(fail_decode(&specs[6], &f, NET_WIRE_MALFORMED) == 0);
    bcm_result_sample(&r); r.door_target = UINT8_MAX;
    CHECK(fail_encode(&specs[6], &r, NET_WIRE_MALFORMED) == 0);
    f = literal_frame(&specs[6]); f.bytes[28] = 255u;
    CHECK(fail_decode(&specs[6], &f, NET_WIRE_MALFORMED) == 0);
    bcm_event_sample(&e); e.related_domain_boot = 0u;
    CHECK(fail_encode(&specs[7], &e, NET_WIRE_CONTRACT_HOLD) == 0);
    f = literal_frame(&specs[7]); memset(f.bytes + 16u, 0, 4u);
    CHECK(fail_decode(&specs[7], &f, NET_WIRE_CONTRACT_HOLD) == 0);
    bcm_event_sample(&e); e.related_command_id = 0u;
    CHECK(fail_encode(&specs[7], &e, NET_WIRE_CONTRACT_HOLD) == 0);
    f = literal_frame(&specs[7]); memset(f.bytes + 20u, 0, 4u);
    CHECK(fail_decode(&specs[7], &f, NET_WIRE_CONTRACT_HOLD) == 0);
    puts("PASS bcm_semantic_holds: unresolved result/evidence identities remain HOLD");
    return 0;
}
static int seeded_boundaries(void)
{
    uint32_t seed = UINT32_C(0xbc2a2026); unsigned iteration; size_t i;
    for (iteration = 0u; iteration < 1024u; ++iteration) {
        for (i = 0u; i < sizeof(specs) / sizeof(specs[0]); ++i) {
            Storage input, output; NetWireCanEnvelope f;
            specs[i].sample(&input); specs[i].randomize(&input, &seed);
            CHECK(specs[i].encode(CANDIDATE, &input, &f) == NET_WIRE_OK);
            CHECK(specs[i].decode(CANDIDATE, &f, &output) == NET_WIRE_OK);
            CHECK(specs[i].equal(&input, &output));
        }
    }
    puts("PASS bcm_seeded_boundaries seed=0xbc2a2026 iterations=1024 messages=9216");
    return 0;
}
int main(int argc, char **argv)
{
    size_t i; int found = 0;
    if (argc > 2) return 2;
    for (i = 0u; i < sizeof(specs) / sizeof(specs[0]); ++i) {
        if (argc == 1 || strcmp(argv[1], specs[i].name) == 0) {
            if (run_message(&specs[i]) != 0) return 1;
            found = 1;
        }
    }
    if (argc == 1 || strcmp(argv[1], "bcm_semantic_holds") == 0) {
        if (semantic_holds() != 0) return 1;
        found = 1;
    }
    if (argc == 1 || strcmp(argv[1], "bcm_seeded_boundaries") == 0) {
        if (seeded_boundaries() != 0) return 1;
        found = 1;
    }
    return found ? 0 : 2;
}
