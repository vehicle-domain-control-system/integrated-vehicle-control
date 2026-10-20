#include "network/net_wire.h"

/* L04/M05+M06 read-only candidate metadata from FINAL_98. These records do
 * not register callbacks, enable routes, or adopt a product wire profile.
 * In particular WINDOW remains HOLD and VSS_EVENT is sent by DOMAIN. */
#define NODE(n) NET_WIRE_NODE_##n
#define CAN_ROW(name_, id_, used_, frame_, dlc_, sender_, receivers_, typed_) \
    { NET_WIRE_CAN, id_, used_, frame_, dlc_, 0U, NET_WIRE_CAN_DIRECTION, \
      sender_, receivers_, typed_, name_ }
#define UART_ROW(link_, name_, type_, used_, frame_, version_, direction_, sender_, receiver_, typed_) \
    { link_, type_, used_, frame_, 0U, version_, direction_, \
      sender_, receiver_, typed_, name_ }

static const NetWireRegistryEntry entries[] = {
    CAN_ROW("DOMAIN_PERMISSION", 0x080U, 19U, 20U, 11U, NODE(DOMAIN), NODE(BCM) | NODE(CIS) | NODE(WINDOW) | NODE(VSS), true),
    CAN_ROW("BCM_DOOR_COMMAND", 0x100U, 22U, 24U, 12U, NODE(DOMAIN), NODE(BCM), false),
    CAN_ROW("BCM_CLIMATE_COMMAND", 0x110U, 23U, 24U, 12U, NODE(DOMAIN), NODE(BCM), false),
    CAN_ROW("BCM_LIGHT_COMMAND", 0x120U, 25U, 32U, 13U, NODE(DOMAIN), NODE(BCM), false),
    CAN_ROW("WINDOW_COMMAND", 0x090U, 24U, 24U, 12U, NODE(DOMAIN), NODE(WINDOW), false),
    CAN_ROW("BCM_DOOR_STATE", 0x200U, 20U, 20U, 11U, NODE(BCM), NODE(DOMAIN), false),
    CAN_ROW("BCM_CLIMATE_STATE", 0x210U, 24U, 24U, 12U, NODE(BCM), NODE(DOMAIN), false),
    CAN_ROW("BCM_LIGHT_STATE", 0x220U, 25U, 32U, 13U, NODE(BCM), NODE(DOMAIN), false),
    CAN_ROW("BCM_RESULT", 0x140U, 29U, 32U, 13U, NODE(BCM), NODE(DOMAIN), false),
    CAN_ROW("BCM_EVENT", 0x141U, 25U, 32U, 13U, NODE(BCM), NODE(DOMAIN), false),
    CAN_ROW("BCM_STATUS", 0x230U, 22U, 24U, 12U, NODE(BCM), NODE(DOMAIN), false),
    CAN_ROW("CIS_ENVIRONMENT", 0x300U, 40U, 48U, 14U, NODE(CIS), NODE(DOMAIN), false),
    CAN_ROW("CIS_OCCUPANT", 0x310U, 25U, 32U, 13U, NODE(CIS), NODE(DOMAIN), false),
    CAN_ROW("CIS_REAR", 0x0A0U, 19U, 20U, 11U, NODE(CIS), NODE(DOMAIN), true),
    CAN_ROW("CIS_STATUS", 0x320U, 34U, 48U, 14U, NODE(CIS), NODE(DOMAIN), false),
    CAN_ROW("WINDOW_STATE", 0x0B0U, 22U, 24U, 12U, NODE(WINDOW), NODE(DOMAIN), false),
    CAN_ROW("WINDOW_EVENT", 0x091U, 26U, 32U, 13U, NODE(WINDOW), NODE(DOMAIN), false),
    CAN_ROW("WINDOW_RESULT", 0x092U, 20U, 20U, 11U, NODE(WINDOW), NODE(DOMAIN), false),
    CAN_ROW("WINDOW_FAULT", 0x330U, 22U, 24U, 12U, NODE(WINDOW), NODE(DOMAIN), false),
    CAN_ROW("VSS_EVENT", 0x160U, 23U, 24U, 12U, NODE(DOMAIN), NODE(VSS), false),
    CAN_ROW("VSS_WARNING_STATE", 0x0C0U, 60U, 64U, 15U, NODE(DOMAIN), NODE(VSS), false),
    CAN_ROW("VSS_STATUS", 0x340U, 28U, 32U, 13U, NODE(VSS), NODE(DOMAIN), false),
    UART_ROW(NET_WIRE_UART_M, "M_CONTEXT", 0x01U, 38U, 48U, 2U, NET_WIRE_UPLINK, NODE(ESP32), NODE(DOMAIN), true),
    UART_ROW(NET_WIRE_UART_M, "M_REQUEST", 0x10U, 24U, 34U, 2U, NET_WIRE_UPLINK, NODE(ESP32), NODE(DOMAIN), true),
    UART_ROW(NET_WIRE_UART_M, "M_QUERY", 0x11U, 32U, 42U, 2U, NET_WIRE_UPLINK, NODE(ESP32), NODE(DOMAIN), false),
    UART_ROW(NET_WIRE_UART_M, "M_WARNING_ACK", 0x12U, 21U, 31U, 2U, NET_WIRE_UPLINK, NODE(ESP32), NODE(DOMAIN), false),
    UART_ROW(NET_WIRE_UART_M, "M_DOMAIN_ALIVE", 0x02U, 12U, 22U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_RESULT", 0x20U, 44U, 54U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), true),
    UART_ROW(NET_WIRE_UART_M, "M_WARNING", 0x21U, 40U, 50U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_AVAILABILITY", 0x22U, 62U, 72U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_DIGITAL_STATUS", 0x23U, 30U, 40U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_DIGITAL_RESULT", 0x24U, 35U, 45U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_USER_SETTINGS", 0x25U, 36U, 46U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_QUERY_END", 0x40U, 24U, 34U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_BCM_DOOR_STATE", 0x30U, 40U, 50U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_BCM_CLIMATE_STATE", 0x31U, 44U, 54U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_BCM_LIGHT_STATE", 0x32U, 45U, 55U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_BCM_STATUS", 0x33U, 42U, 52U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_CIS_ENVIRONMENT", 0x34U, 60U, 70U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_CIS_OCCUPANT", 0x35U, 45U, 55U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_CIS_REAR", 0x36U, 39U, 49U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_CIS_STATUS", 0x37U, 54U, 64U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_VSS_STATUS", 0x3AU, 48U, 58U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_WINDOW_STATE", 0x38U, 42U, 52U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_M, "M_WINDOW_FAULT", 0x39U, 42U, 52U, 2U, NET_WIRE_DOWNLINK, NODE(DOMAIN), NODE(ESP32), false),
    UART_ROW(NET_WIRE_UART_C, "P_VISION_RESULT", 0x60U, 19U, 29U, 1U, NET_WIRE_UPLINK, NODE(PI), NODE(CIS), true),
    UART_ROW(NET_WIRE_UART_C, "P_VISION_STATUS", 0x61U, 16U, 26U, 1U, NET_WIRE_UPLINK, NODE(PI), NODE(CIS), false),
    UART_ROW(NET_WIRE_UART_C, "P_PERMISSION", 0x62U, 15U, 25U, 1U, NET_WIRE_DOWNLINK, NODE(CIS), NODE(PI), true)
};

#undef UART_ROW
#undef CAN_ROW
#undef NODE

size_t net_wire_registry_count(void)
{
    return sizeof(entries) / sizeof(entries[0]);
}

const NetWireRegistryEntry *net_wire_registry_at(size_t index)
{
    return index < net_wire_registry_count() ? &entries[index] : NULL;
}

const NetWireRegistryEntry *net_wire_registry_find(NetWireLink link, uint16_t id_type)
{
    size_t index;
    for (index = 0U; index < net_wire_registry_count(); ++index) {
        if (entries[index].link == link && entries[index].id_type == id_type) {
            return &entries[index];
        }
    }
    return NULL;
}

bool net_wire_registry_has_typed_codec(NetWireLink link, uint16_t id_type)
{
    const NetWireRegistryEntry *entry = net_wire_registry_find(link, id_type);
    if (entry == NULL) return false;
    if (entry->link == NET_WIRE_CAN || entry->typed_wave1) return true;
    if (entry->link == NET_WIRE_UART_C) return id_type == 0x61U;
    /* Only the twenty independently tested Wave2B M candidates. Historical
     * flags and the registry's IDs/routes/lengths remain unchanged. */
    switch (id_type) {
    case 0x11U: case 0x12U: case 0x02U: case 0x21U: case 0x22U:
    case 0x23U: case 0x24U: case 0x25U: case 0x40U: case 0x30U:
    case 0x31U: case 0x32U: case 0x33U: case 0x34U: case 0x35U:
    case 0x36U: case 0x37U: case 0x3aU: case 0x38U: case 0x39U:
        return true;
    default: return false;
    }
}
