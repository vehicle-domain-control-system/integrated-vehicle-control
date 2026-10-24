/* Documented periodic traffic for the node load cases (cis_node, bcm_node,
 * vss_node load_doc_traffic): the periodic CAN messages of
 * docs/network/interface/03_COMMUNICATION/10_CAN_FD.md (message list, WINDOW
 * excluded: about 107 frames/s, 11 of them start together every second),
 * valid frames of every sender built with the repository codec, and the
 * check of the node's own periodic frames on the bus.
 * Host test code only, linked into the node test programs. */
#ifndef NET_LOAD_TRAFFIC_H
#define NET_LOAD_TRAFFIC_H
#include "network/net_wire_can.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { NET_LOAD_DOMAIN, NET_LOAD_CIS, NET_LOAD_BCM, NET_LOAD_VSS } NetLoadSender;

typedef struct {
    uint16_t id;
    uint32_t period_ms;    /* 10_CAN_FD.md message list; phase 0 for all */
    NetLoadSender sender;
} NetLoadMessage;

#define NET_LOAD_MESSAGE_COUNT 11u
#define NET_LOAD_DURATION_MS 10000u   /* simulated time of one load case */
extern const NetLoadMessage net_load_messages[NET_LOAD_MESSAGE_COUNT];

/* BOOT_ID of a sender run on the bus (nonzero, one per sender). */
uint32_t net_load_boot(NetLoadSender sender);

/* Frame n (0-based) of message k as its sender puts it on the bus: the
 * sender's BOOT_ID, TX_SEQUENCE n + 1, valid content. DOMAIN_PERMISSION is a
 * new evaluation every 200 ms (10_CAN_FD.md: re-evaluated about every 200 ms,
 * sent every 50 ms) and its AGE grows by 50 ms on each repetition; every
 * other message is a new observation. false if the codec refuses it. */
bool net_load_frame(size_t k, uint32_t n, NetWireCanEnvelope *out);

/* One periodic message the node under test sends. */
typedef struct {
    uint16_t id;
    uint32_t period_ms;
    uint32_t frames;          /* frames of this ID seen on the bus */
    uint32_t sequence_gaps;   /* frames whose TX_SEQUENCE is not the previous + 1 (first: 1) */
    uint32_t last_sequence;
    uint64_t limit_ms;        /* period + allowed slack */
    uint64_t first_ms, last_ms;
    uint64_t max_gap_ms;      /* largest interval between two frames of it */
    uint32_t violations;      /* intervals between two frames above limit_ms */
} NetLoadTx;

/* slack_ms: what a frame may be late by, one main pass of the node. */
void net_load_tx_init(NetLoadTx *t, uint16_t id, uint32_t period_ms, uint32_t slack_ms);
/* A frame the node put on the bus at at_ms; frames of other IDs are ignored.
 * Returns true when the frame belongs to one of the count entries. */
bool net_load_tx_seen(NetLoadTx *t, size_t count, const NetWireCanEnvelope *e, uint64_t at_ms);
/* Intervals above period + slack: between two frames of a message and from
 * its last frame to end_ms; a message without any frame counts once. */
uint32_t net_load_tx_violations(const NetLoadTx *t, size_t count, uint64_t end_ms);
uint32_t net_load_tx_frames(const NetLoadTx *t, size_t count);
uint32_t net_load_tx_gaps(const NetLoadTx *t, size_t count);

/* A named count of the node's result (a loss cause, a period violation). */
typedef struct { const char *name; uint32_t value; } NetLoadCount;
/* Sum of the counts (saturating); prints "<label>: name=value ..." with the
 * nonzero ones, or "<label>: none". */
uint32_t net_load_report(const char *label, const NetLoadCount *c, size_t count);

#endif
