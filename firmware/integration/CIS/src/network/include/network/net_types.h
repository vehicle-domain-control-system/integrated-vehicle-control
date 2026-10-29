#ifndef NETWORK_NET_TYPES_H
#define NETWORK_NET_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Local scaffold vocabulary only: no wire enum, C2 ABI, or vehicle policy. */
typedef enum {
    NET_OK, NET_INVALID_ARGUMENT, NET_NO_CAPACITY, NET_NOT_CONFIGURED,
    NET_UNSUPPORTED, NET_HOLD, NET_NO_WORK, NET_EXPIRED, NET_REJECTED,
    NET_DUPLICATE, NET_STALE, NET_BUSY
} NetStatus;
typedef enum { NET_LINK_CAN, NET_LINK_UART_M, NET_LINK_UART_C } NetLink;
#define NET_FRAME_CAPACITY 160u
typedef struct {
    NetLink link;
    uint32_t id;
    size_t length;
    uint64_t captured_at_ms;
    uint8_t bytes[NET_FRAME_CAPACITY];
} NetFrame;
/* Local attempt identity, independent of all wire BOOT/sequence fields. */
typedef struct { uint64_t epoch; uint64_t token; } NetTxKey;
typedef enum { NET_TX_UNKNOWN, NET_TX_SUCCEEDED, NET_TX_FAILED } NetTxOutcome;

#endif
