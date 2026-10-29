#ifndef NETWORK_NET_CAPACITY_H
#define NETWORK_NET_CAPACITY_H
#include "network/net_upper_profile.h"

/* Buffer capacities of a Network node, chosen by the node profile of the
 * build (NETWORK_UPPER_NODE_PROFILE, net_upper_profile.h), all in this file.
 * Each one can be set for the whole build with -D (board project settings,
 * tests). Give every file of a build the same value: the queues are members
 * of structures that several files share.
 *
 *   NET_UPPER_TX_CAPACITY       M04 transmit queue (NetUpper.tx). A publish
 *                               entry holds at most one offered frame until
 *                               it is retired (net_upper_publish.h, in_flight),
 *                               so the publish table of the node bounds it.
 *   NET_UPPER_PUBLISH_CAPACITY  M04 publish entries (NetUpperPublisher.slot).
 *   NET_UPPER_RX_CAPACITY       service receive queue (NetUpper.rx): the
 *                               events of one step; a UART byte is one event.
 *   NET_UPPER_POLL_BUDGET       events one net_upper_step() takes from its
 *                               Port; NetNode_Step() runs two steps per pass.
 *   NET_HAL_RX_CAPACITY         adapter receive queue (NetHalAdapter.rx),
 *                               filled by the backend drain, emptied per pass.
 *   NET_BOARD_ISR_DRAIN_BUDGET  ISR ring entries moved to the adapter per main
 *                               pass: CAN frames first, then UART bytes.
 *   NET_HAL_TX_CAPACITY         transfers lent to the device at a time (CAN
 *                               TX message buffers, UART transmit queue).
 *   NET_BOARD_CAN_ISR_RING      received CAN frames the ISR ring holds between
 *                               two main passes. The RX message buffers accept
 *                               every ID, so the ring sees the whole bus; the
 *                               M04 registry drops the routes of other nodes.
 *   NET_BOARD_UART_ISR_RING     received bytes the ISR ring of one UART link
 *                               holds between two main passes.
 *
 * The CIS, BCM, VSS and DOMAIN values follow the traffic of the node
 * (docs/network/interface/03_COMMUNICATION/10_CAN_FD.md, 20_UART_M.md and
 * 30_UART_C.md, message lists): the periodic bus traffic is about 107
 * frames/s (WINDOW excluded) and up to 11 periodic frames start at the same
 * instant. HOST, CIS_CAN_ONLY and WINDOW keep the common values (HOST is the
 * reference of the product tests; WINDOW has no node table).
 *
 * Link conditions, each a compile error when a value breaks it:
 *   NET_UPPER_POLL_BUDGET <= NET_UPPER_RX_CAPACITY             (below)
 *   NET_UPPER_TX_CAPACITY >= entries of the node publish table
 *                                     (src/node/net_node_profile.h)
 *   NET_BOARD_ISR_DRAIN_BUDGET <= NET_HAL_RX_CAPACITY
 *                                     (src/platform/rtd/net_hal_rtd.c) */

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
/* CIS: publish table of 5 (CIS_REAR, CIS_ENVIRONMENT, CIS_OCCUPANT,
 * CIS_STATUS, P_PERMISSION). UART-C from the Pi: P_VISION_RESULT 29 B every
 * 100 ms and P_VISION_STATUS 26 B every 1 s; two steps of 24 events take a
 * whole frame per pass, also when frames follow each other at a short pass
 * period. The CAN ring of 16 frames holds the bus traffic of a pass of up to
 * about 45 ms together with the 11 frames that start at once. 256 B of UART
 * ring are about 22 ms of a fully loaded 115200 bit/s line. */
#define NET_CAPACITY_TX         5u
#define NET_CAPACITY_PUBLISH    5u
#define NET_CAPACITY_RX         24u
#define NET_CAPACITY_POLL       24u
#define NET_CAPACITY_HAL_RX     16u
#define NET_CAPACITY_DRAIN      16u
#define NET_CAPACITY_HAL_TX     4u
#define NET_CAPACITY_CAN_RING   16u
#define NET_CAPACITY_UART_RING  256u
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_BCM
/* BCM: publish table of 6 (BCM_RESULT, BCM_EVENT, BCM_DOOR_STATE,
 * BCM_CLIMATE_STATE, BCM_LIGHT_STATE, BCM_STATUS). CAN only: one event per
 * frame, 8 per step and 16 per pass; the drain of 8 frames per pass takes the
 * 11 frames that start at once in two passes. CAN ring as CIS. No UART link
 * (the UART ring value is not used). */
#define NET_CAPACITY_TX         6u
#define NET_CAPACITY_PUBLISH    6u
#define NET_CAPACITY_RX         8u
#define NET_CAPACITY_POLL       8u
#define NET_CAPACITY_HAL_RX     8u
#define NET_CAPACITY_DRAIN      8u
#define NET_CAPACITY_HAL_TX     4u
#define NET_CAPACITY_CAN_RING   16u
#define NET_CAPACITY_UART_RING  256u
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_VSS
/* VSS: publish table of 1 (VSS_STATUS), one transmitted message, so two TX
 * message buffers. CAN only: 8 events per step and 16 per pass; the drain of
 * 16 frames empties a burst of 12 DOMAIN frames in one pass (tests
 * vss_node.h14_rx_burst_*; VSS contract C8 (VSS_NODE_INTEGRATION.md):
 * Net_MainFunction stays short, the rest waits in the ring). The CAN ring holds 24 frames between two passes. No UART link
 * (the UART ring value is not used). */
#define NET_CAPACITY_TX         1u
#define NET_CAPACITY_PUBLISH    1u
#define NET_CAPACITY_RX         8u
#define NET_CAPACITY_POLL       8u
#define NET_CAPACITY_HAL_RX     16u
#define NET_CAPACITY_DRAIN      16u
#define NET_CAPACITY_HAL_TX     2u
#define NET_CAPACITY_CAN_RING   24u
#define NET_CAPACITY_UART_RING  256u
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_DOMAIN
/* DOMAIN: publish table of 3 (DOMAIN_PERMISSION, VSS_WARNING_STATE,
 * M_DOMAIN_ALIVE). It receives 15 CAN routes (67 periodic frames/s, WINDOW
 * excluded, up to 9 starting at the same instant) and UART-M from the ESP32
 * (M_CONTEXT 48 B every 100 ms; M_REQUEST, M_QUERY, M_WARNING_ACK on
 * occurrence): two steps of 24 events take a whole UART-M frame per pass, as
 * in CIS. The C2 adapter sends the commands, the UART-M state relays (200 ms
 * and 1 s groups, up to 13 at one instant) and the query answers: the
 * transmit queue of 16 holds such a group, and a frame the device has no room
 * for waits there (HAL TX 4: CAN MBs and the UART-M transfer queue). The CAN
 * ring of 24 holds the 9 frames of one instant and about 200 ms of the
 * periodic traffic; 256 B of UART ring are about 22 ms of a fully loaded
 * 115200 bit/s line. RAM budget of Network + RTD backend on the DOMAIN board:
 * 44 KB. */
#define NET_CAPACITY_TX         16u
#define NET_CAPACITY_PUBLISH    3u
#define NET_CAPACITY_RX         24u
#define NET_CAPACITY_POLL       24u
#define NET_CAPACITY_HAL_RX     16u
#define NET_CAPACITY_DRAIN      16u
#define NET_CAPACITY_HAL_TX     4u
#define NET_CAPACITY_CAN_RING   24u
#define NET_CAPACITY_UART_RING  256u
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_HOST || \
      NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS_CAN_ONLY || \
      NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_WINDOW
/* Common values: bounded implementation capacities, not approved ECU sizing. */
#define NET_CAPACITY_TX         16u
#define NET_CAPACITY_PUBLISH    8u
#define NET_CAPACITY_RX         32u
#define NET_CAPACITY_POLL       32u
#define NET_CAPACITY_HAL_RX     16u
#define NET_CAPACITY_DRAIN      16u
#define NET_CAPACITY_HAL_TX     4u
#define NET_CAPACITY_CAN_RING   32u
#define NET_CAPACITY_UART_RING  256u
#else
#error "net_capacity.h: no capacity values for this NETWORK_UPPER_NODE_PROFILE"
#endif

#ifndef NET_UPPER_TX_CAPACITY
#define NET_UPPER_TX_CAPACITY       NET_CAPACITY_TX
#endif
#ifndef NET_UPPER_PUBLISH_CAPACITY
#define NET_UPPER_PUBLISH_CAPACITY  NET_CAPACITY_PUBLISH
#endif
#ifndef NET_UPPER_RX_CAPACITY
#define NET_UPPER_RX_CAPACITY       NET_CAPACITY_RX
#endif
#ifndef NET_UPPER_POLL_BUDGET
#define NET_UPPER_POLL_BUDGET       NET_CAPACITY_POLL
#endif
#ifndef NET_HAL_RX_CAPACITY
#define NET_HAL_RX_CAPACITY         NET_CAPACITY_HAL_RX
#endif
#ifndef NET_BOARD_ISR_DRAIN_BUDGET
#define NET_BOARD_ISR_DRAIN_BUDGET  NET_CAPACITY_DRAIN
#endif
#ifndef NET_HAL_TX_CAPACITY
#define NET_HAL_TX_CAPACITY         NET_CAPACITY_HAL_TX
#endif
#ifndef NET_BOARD_CAN_ISR_RING
#define NET_BOARD_CAN_ISR_RING      NET_CAPACITY_CAN_RING
#endif
#ifndef NET_BOARD_UART_ISR_RING
#define NET_BOARD_UART_ISR_RING     NET_CAPACITY_UART_RING
#endif

/* net_upper_step() refuses a budget above its receive queue
 * (NET_INVALID_ARGUMENT) and NetNode_Step() passes NET_UPPER_POLL_BUDGET, so a
 * larger budget would receive nothing at all. */
typedef char net_capacity_NET_UPPER_POLL_BUDGET_le_NET_UPPER_RX_CAPACITY[
    (NET_UPPER_POLL_BUDGET >= 1u && NET_UPPER_POLL_BUDGET <= NET_UPPER_RX_CAPACITY) ? 1 : -1];

#endif
