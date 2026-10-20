#include "network/net_wire.h"
#include <stdio.h>
int main(void)
{
    size_t i, can = 0u, uart = 0u, uart_wave1 = 0u, wave1 = 0u;
    if (net_wire_registry_count() != 48u) return 1;
    for (i = 0u; i < net_wire_registry_count(); ++i) {
        const NetWireRegistryEntry *e = net_wire_registry_at(i);
        bool implemented = net_wire_registry_has_typed_codec(e->link, e->id_type);
        if (e->typed_wave1) ++wave1;
        if (e->link == NET_WIRE_CAN) {
            if (!implemented) return 2;
            ++can;
        } else if (implemented) {
            if (e->typed_wave1) ++uart_wave1;
            ++uart;
        } else return 3;
    }
    if (can != 22u || uart != 26u || uart_wave1 != 5u || wave1 != 7u) return 4;
    if (net_wire_registry_has_typed_codec(NET_WIRE_CAN, 0x7ffu) ||
        net_wire_registry_has_typed_codec((NetWireLink)99, 0x100u) ||
        !net_wire_registry_has_typed_codec(NET_WIRE_UART_C, 0x61u)) return 5;
    puts("PASS candidate capability CAN22/UART26; historical Wave1 seven including UART5 preserved");
    return 0;
}
