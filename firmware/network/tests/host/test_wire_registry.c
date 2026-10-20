#include "network/net_wire.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

/* Independently transcribed from FINAL_98 interface/03_COMMUNICATION tables.
 * Numeric node masks are expected public constants, not inferred from names.
 * This is a metadata oracle, separate from the seven wire byte-vector tests. */
typedef struct {
    NetWireLink link;
    const char *name;
    uint16_t id, used, frame;
    uint8_t dlc, version;
    NetWireDirection direction;
    uint32_t sender, receivers;
    bool typed;
} Expected;

static const Expected expected[] = {
    {NET_WIRE_CAN, "DOMAIN_PERMISSION", 0x080, 19, 20, 11, 0, NET_WIRE_CAN_DIRECTION, 1, 30, true},
    {NET_WIRE_CAN, "BCM_DOOR_COMMAND", 0x100, 22, 24, 12, 0, NET_WIRE_CAN_DIRECTION, 1, 2, false},
    {NET_WIRE_CAN, "BCM_CLIMATE_COMMAND", 0x110, 23, 24, 12, 0, NET_WIRE_CAN_DIRECTION, 1, 2, false},
    {NET_WIRE_CAN, "BCM_LIGHT_COMMAND", 0x120, 25, 32, 13, 0, NET_WIRE_CAN_DIRECTION, 1, 2, false},
    {NET_WIRE_CAN, "WINDOW_COMMAND", 0x090, 24, 24, 12, 0, NET_WIRE_CAN_DIRECTION, 1, 16, false},
    {NET_WIRE_CAN, "BCM_DOOR_STATE", 0x200, 20, 20, 11, 0, NET_WIRE_CAN_DIRECTION, 2, 1, false},
    {NET_WIRE_CAN, "BCM_CLIMATE_STATE", 0x210, 24, 24, 12, 0, NET_WIRE_CAN_DIRECTION, 2, 1, false},
    {NET_WIRE_CAN, "BCM_LIGHT_STATE", 0x220, 25, 32, 13, 0, NET_WIRE_CAN_DIRECTION, 2, 1, false},
    {NET_WIRE_CAN, "BCM_RESULT", 0x140, 29, 32, 13, 0, NET_WIRE_CAN_DIRECTION, 2, 1, false},
    {NET_WIRE_CAN, "BCM_EVENT", 0x141, 25, 32, 13, 0, NET_WIRE_CAN_DIRECTION, 2, 1, false},
    {NET_WIRE_CAN, "BCM_STATUS", 0x230, 22, 24, 12, 0, NET_WIRE_CAN_DIRECTION, 2, 1, false},
    {NET_WIRE_CAN, "CIS_ENVIRONMENT", 0x300, 40, 48, 14, 0, NET_WIRE_CAN_DIRECTION, 4, 1, false},
    {NET_WIRE_CAN, "CIS_OCCUPANT", 0x310, 25, 32, 13, 0, NET_WIRE_CAN_DIRECTION, 4, 1, false},
    {NET_WIRE_CAN, "CIS_REAR", 0x0A0, 19, 20, 11, 0, NET_WIRE_CAN_DIRECTION, 4, 1, true},
    {NET_WIRE_CAN, "CIS_STATUS", 0x320, 34, 48, 14, 0, NET_WIRE_CAN_DIRECTION, 4, 1, false},
    {NET_WIRE_CAN, "WINDOW_STATE", 0x0B0, 22, 24, 12, 0, NET_WIRE_CAN_DIRECTION, 16, 1, false},
    {NET_WIRE_CAN, "WINDOW_EVENT", 0x091, 26, 32, 13, 0, NET_WIRE_CAN_DIRECTION, 16, 1, false},
    {NET_WIRE_CAN, "WINDOW_RESULT", 0x092, 20, 20, 11, 0, NET_WIRE_CAN_DIRECTION, 16, 1, false},
    {NET_WIRE_CAN, "WINDOW_FAULT", 0x330, 22, 24, 12, 0, NET_WIRE_CAN_DIRECTION, 16, 1, false},
    {NET_WIRE_CAN, "VSS_EVENT", 0x160, 23, 24, 12, 0, NET_WIRE_CAN_DIRECTION, 1, 8, false},
    {NET_WIRE_CAN, "VSS_WARNING_STATE", 0x0C0, 60, 64, 15, 0, NET_WIRE_CAN_DIRECTION, 1, 8, false},
    {NET_WIRE_CAN, "VSS_STATUS", 0x340, 28, 32, 13, 0, NET_WIRE_CAN_DIRECTION, 8, 1, false},
    {NET_WIRE_UART_M, "M_CONTEXT", 0x01, 38, 48, 0, 2, NET_WIRE_UPLINK, 32, 1, true},
    {NET_WIRE_UART_M, "M_REQUEST", 0x10, 24, 34, 0, 2, NET_WIRE_UPLINK, 32, 1, true},
    {NET_WIRE_UART_M, "M_QUERY", 0x11, 32, 42, 0, 2, NET_WIRE_UPLINK, 32, 1, false},
    {NET_WIRE_UART_M, "M_WARNING_ACK", 0x12, 21, 31, 0, 2, NET_WIRE_UPLINK, 32, 1, false},
    {NET_WIRE_UART_M, "M_DOMAIN_ALIVE", 0x02, 12, 22, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_RESULT", 0x20, 44, 54, 0, 2, NET_WIRE_DOWNLINK, 1, 32, true},
    {NET_WIRE_UART_M, "M_WARNING", 0x21, 40, 50, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_AVAILABILITY", 0x22, 62, 72, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_DIGITAL_STATUS", 0x23, 30, 40, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_DIGITAL_RESULT", 0x24, 35, 45, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_USER_SETTINGS", 0x25, 36, 46, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_QUERY_END", 0x40, 24, 34, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_BCM_DOOR_STATE", 0x30, 40, 50, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_BCM_CLIMATE_STATE", 0x31, 44, 54, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_BCM_LIGHT_STATE", 0x32, 45, 55, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_BCM_STATUS", 0x33, 42, 52, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_CIS_ENVIRONMENT", 0x34, 60, 70, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_CIS_OCCUPANT", 0x35, 45, 55, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_CIS_REAR", 0x36, 39, 49, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_CIS_STATUS", 0x37, 54, 64, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_VSS_STATUS", 0x3A, 48, 58, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_WINDOW_STATE", 0x38, 42, 52, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_M, "M_WINDOW_FAULT", 0x39, 42, 52, 0, 2, NET_WIRE_DOWNLINK, 1, 32, false},
    {NET_WIRE_UART_C, "P_VISION_RESULT", 0x60, 19, 29, 0, 1, NET_WIRE_UPLINK, 64, 4, true},
    {NET_WIRE_UART_C, "P_VISION_STATUS", 0x61, 16, 26, 0, 1, NET_WIRE_UPLINK, 64, 4, false},
    {NET_WIRE_UART_C, "P_PERMISSION", 0x62, 15, 25, 0, 1, NET_WIRE_DOWNLINK, 4, 64, true}
};

static int registry_layout(void)
{
    static const uint16_t dlc_lengths[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};
    size_t i, j;
    CHECK(net_wire_registry_count() == sizeof(expected) / sizeof(expected[0]));
    for (i = 0U; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        const Expected *want = &expected[i];
        const NetWireRegistryEntry *got = net_wire_registry_find(want->link, want->id);
        CHECK(got != NULL);
        CHECK(strcmp(got->name, want->name) == 0);
        CHECK(got->payload_length == want->used && got->frame_length == want->frame);
        CHECK(got->dlc == want->dlc && got->version == want->version);
        CHECK(got->payload_length <= got->frame_length);
        if (got->link == NET_WIRE_CAN) {
            CHECK(got->id_type <= 0x7FFU && got->dlc < 16U);
            CHECK(got->frame_length == dlc_lengths[got->dlc]);
        } else {
            CHECK(got->id_type <= 0xFFU && got->payload_length <= 128U);
            CHECK(got->frame_length == got->payload_length + 10U);
        }
        for (j = i + 1U; j < net_wire_registry_count(); ++j) {
            const NetWireRegistryEntry *other = net_wire_registry_at(j);
            CHECK(other != NULL);
            CHECK(!(net_wire_registry_at(i)->link == other->link &&
                    net_wire_registry_at(i)->id_type == other->id_type));
        }
    }
    return 0;
}

static int registry_routes(void)
{
    size_t i;
    CHECK(NET_WIRE_NODE_DOMAIN == 1 && NET_WIRE_NODE_BCM == 2 && NET_WIRE_NODE_CIS == 4);
    CHECK(NET_WIRE_NODE_VSS == 8 && NET_WIRE_NODE_WINDOW == 16);
    CHECK(NET_WIRE_NODE_ESP32 == 32 && NET_WIRE_NODE_PI == 64);
    for (i = 0U; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        const Expected *want = &expected[i];
        const NetWireRegistryEntry *got = net_wire_registry_find(want->link, want->id);
        CHECK(got != NULL);
        CHECK(got->direction == want->direction);
        CHECK(got->sender == want->sender && got->receivers == want->receivers);
        CHECK((got->sender & got->receivers) == 0U);
    }
    return 0;
}

static int registry_coverage(void)
{
    size_t i, counts[3] = {0U, 0U, 0U}, typed = 0U;
    CHECK(net_wire_registry_count() == 48U);
    for (i = 0U; i < net_wire_registry_count(); ++i) {
        const NetWireRegistryEntry *entry = net_wire_registry_at(i);
        const Expected *want = &expected[i];
        CHECK(entry != NULL);
        CHECK(entry == net_wire_registry_find(entry->link, entry->id_type));
        CHECK(entry->link >= NET_WIRE_CAN && entry->link <= NET_WIRE_UART_C);
        ++counts[(size_t)entry->link];
        CHECK(entry->link == want->link && entry->id_type == want->id);
        CHECK(entry->typed_wave1 == want->typed);
        if (entry->typed_wave1) { ++typed; }
    }
    CHECK(counts[0] == 22U && counts[1] == 23U && counts[2] == 3U && typed == 7U);
    CHECK(net_wire_registry_at(48U) == NULL);
    CHECK(net_wire_registry_at(SIZE_MAX) == NULL);
    CHECK(net_wire_registry_find(NET_WIRE_CAN, 0x7FFU) == NULL);
    CHECK(net_wire_registry_find(NET_WIRE_UART_M, 0x60U) == NULL);
    CHECK(net_wire_registry_find(NET_WIRE_UART_C, 0x01U) == NULL);
    CHECK(net_wire_registry_find((NetWireLink)99, 0x080U) == NULL);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "registry_layout") == 0) { return registry_layout(); }
        if (strcmp(argv[1], "registry_routes") == 0) { return registry_routes(); }
        if (strcmp(argv[1], "registry_coverage") == 0) { return registry_coverage(); }
    }
    (void)fprintf(stderr, "expected registry_layout, registry_routes, or registry_coverage\n");
    return 2;
}
