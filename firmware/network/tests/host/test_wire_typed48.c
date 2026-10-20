#include "network/net_wire.h"
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int main(void)
{
    static const struct { NetWireLink link; uint16_t type; } added[] = {
        {NET_WIRE_UART_M, 0x11u}, {NET_WIRE_UART_M, 0x12u},
        {NET_WIRE_UART_M, 0x02u}, {NET_WIRE_UART_M, 0x21u},
        {NET_WIRE_UART_M, 0x22u}, {NET_WIRE_UART_M, 0x23u},
        {NET_WIRE_UART_M, 0x24u}, {NET_WIRE_UART_M, 0x25u},
        {NET_WIRE_UART_M, 0x40u}, {NET_WIRE_UART_M, 0x30u},
        {NET_WIRE_UART_M, 0x31u}, {NET_WIRE_UART_M, 0x32u},
        {NET_WIRE_UART_M, 0x33u}, {NET_WIRE_UART_M, 0x34u},
        {NET_WIRE_UART_M, 0x35u}, {NET_WIRE_UART_M, 0x36u},
        {NET_WIRE_UART_M, 0x37u}, {NET_WIRE_UART_M, 0x3au},
        {NET_WIRE_UART_M, 0x38u}, {NET_WIRE_UART_M, 0x39u},
        {NET_WIRE_UART_C, 0x61u}
    };
    size_t i, can = 0u, m = 0u, c = 0u, historical = 0u;
    CHECK(net_wire_registry_count() == 48u);
    for (i = 0u; i < net_wire_registry_count(); ++i) {
        const NetWireRegistryEntry *entry = net_wire_registry_at(i);
        CHECK(entry != NULL && net_wire_registry_has_typed_codec(entry->link, entry->id_type));
        if (entry->typed_wave1) ++historical;
        if (entry->link == NET_WIRE_CAN) ++can;
        else if (entry->link == NET_WIRE_UART_M) { ++m; CHECK(entry->version == 2u); }
        else if (entry->link == NET_WIRE_UART_C) { ++c; CHECK(entry->version == 1u); }
        else CHECK(false);
    }
    CHECK(can == 22u && m == 23u && c == 3u && historical == 7u);
    for (i = 0u; i < sizeof(added) / sizeof(added[0]); ++i) {
        const NetWireRegistryEntry *entry = net_wire_registry_find(added[i].link, added[i].type);
        CHECK(entry != NULL && !entry->typed_wave1);
        CHECK(net_wire_registry_has_typed_codec(added[i].link, added[i].type));
    }
    CHECK(!net_wire_registry_has_typed_codec(NET_WIRE_CAN, 0x7ffu));
    CHECK(!net_wire_registry_has_typed_codec(NET_WIRE_UART_M, 0x61u));
    CHECK(!net_wire_registry_has_typed_codec(NET_WIRE_UART_C, 0x11u));
    CHECK(!net_wire_registry_has_typed_codec(NET_WIRE_UART_C, 0xffu));
    CHECK(!net_wire_registry_has_typed_codec((NetWireLink)99, 0x61u));
    puts("PASS CAN22/UART-M23/UART-C3 raw typed capability; historical seven unchanged; no product adoption");
    return 0;
}
