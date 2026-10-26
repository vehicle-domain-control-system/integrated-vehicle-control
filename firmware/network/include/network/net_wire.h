#ifndef NETWORK_NET_WIRE_H
#define NETWORK_NET_WIRE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Host-only interpretation of the FINAL_98 candidate. Never product adoption. */
typedef enum {
    NET_WIRE_OK, NET_WIRE_INVALID_ARGUMENT, NET_WIRE_NOT_BOUND_STATUS,
    NET_WIRE_MALFORMED, NET_WIRE_UNSUPPORTED, NET_WIRE_CONTRACT_HOLD,
    NET_WIRE_NEED_MORE, NET_WIRE_NO_CAPACITY, NET_WIRE_TIME_HOLD
} NetWireStatus;
typedef enum { NET_WIRE_NOT_BOUND = 0, NET_WIRE_CANDIDATE_ONLY = 1 } NetWireProfile;
typedef enum { NET_WIRE_CAN, NET_WIRE_UART_M, NET_WIRE_UART_C } NetWireLink;
/* Uplink: ESP32->DOMAIN on M, PI->CIS on C. Not a wire field. */
typedef enum { NET_WIRE_UPLINK, NET_WIRE_DOWNLINK, NET_WIRE_CAN_DIRECTION } NetWireDirection;
enum {
    NET_WIRE_NODE_DOMAIN = 1, NET_WIRE_NODE_BCM = 2, NET_WIRE_NODE_CIS = 4,
    NET_WIRE_NODE_VSS = 8, NET_WIRE_NODE_WINDOW = 16,
    NET_WIRE_NODE_ESP32 = 32, NET_WIRE_NODE_PI = 64
};
typedef struct {
    NetWireLink link;
    uint16_t id_type, payload_length, frame_length;
    uint8_t dlc, version;
    NetWireDirection direction;
    uint32_t sender, receivers;
    bool typed_wave1; /* Stable historical seven; use query below for current coverage. */
    const char *name;
} NetWireRegistryEntry;
/* Metadata lookup is not permission to execute any registered message. */
size_t net_wire_registry_count(void);
const NetWireRegistryEntry *net_wire_registry_at(size_t index);
const NetWireRegistryEntry *net_wire_registry_find(NetWireLink link, uint16_t id_type);
/* Current candidate capability, NOT runtime permission/product adoption.
   Wave2B: CAN22, UART-M23 and UART-C3; unknown link/ID pairs return false. */
bool net_wire_registry_has_typed_codec(NetWireLink link, uint16_t id_type);

/* All candidate codec operations require explicit CANDIDATE_ONLY. */
NetWireStatus net_wire_profile_check(NetWireProfile profile);
/* Address-range test only; caller still supplies actual live buffers. */
bool net_wire_ranges_overlap(const void *a, size_t an, const void *b, size_t bn);
/* Copy / clear of the Network's own objects, word by word when dst, src and
 * size are 4-byte aligned (byte by byte otherwise; with a compiler other than
 * GCC / clang the C library memcpy / memset). For copies made with
 * interrupts masked and once per received byte: a board C library may copy
 * byte by byte (S32DS GCC 10.2 newlib: memcpy, memmove and memset are 1-byte
 * loops), which made those sections longer than one UART byte. The ranges
 * must not overlap; size 0 does nothing. */
void net_wire_copy(void *dst, const void *src, size_t size);
void net_wire_zero(void *dst, size_t size);
#endif
