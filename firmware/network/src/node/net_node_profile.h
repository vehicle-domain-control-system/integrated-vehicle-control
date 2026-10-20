#ifndef NETWORK_NET_NODE_PROFILE_H
#define NETWORK_NET_NODE_PROFILE_H

/* Composition-internal: what the node root (net_node.c) takes from the node
 * profile table. Exactly one net_node_profile_<node>.c defines
 * net_node_profile for the profile of the build (NETWORK_UPPER_NODE_PROFILE);
 * the others compile to nothing. */
#include "network/net_node.h"
#include "network/net_ecu_adapter.h"

typedef struct {
    NetUpper *service;
    NetEcuAdapter *ecu;
    NetUpperPublisher *publisher;
    const NetNodeConfig *config;
} NetNodeParts;

typedef struct {
    uint32_t node;                  /* NET_WIRE_NODE_* of this node */
    bool uart_m, uart_c;            /* UART links the node uses (need a port epoch) */
    const NetEcuCallbacks *(*callbacks)(void); /* App Port RX table for the typed adapter */
    /* Node part of the service configuration, after the common defaults
     * (every route bound, 2000 ms delivery age bound, epochs set): receive
     * policies of the node's routes and its UART stream / LINK_SEQUENCE. */
    void (*upper)(NetUpperConfig *c);
    /* M04 publish table: message, repetition period (0 = one frame per
     * submission) and the freshness bound of a submission. */
    const NetUpperPublishEntry *publish;
    size_t publish_count;
    NetStatus (*bind)(const NetNodeParts *parts);   /* App Port facade */
    /* DOMAIN_PERMISSION supervision, after the common 150 ms receive timeout. */
    void (*permission)(NetPermissionPolicy *p);
    void (*diag)(const NetUpperPublisher *publisher, NetNodeDiag *out);
} NetNodeProfile;

extern const NetNodeProfile net_node_profile;

#endif
