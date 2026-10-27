#ifndef NETWORK_TEST_VIRTUAL_DOMAIN_NET_BOARD_CFG_H
#define NETWORK_TEST_VIRTUAL_DOMAIN_NET_BOARD_CFG_H
/* TEST ONLY. Board configuration of the HAL-less DOMAIN node worker
 * (tests/virtual_ecu/virtual_domain_node.c). nodes/domain/src/node/net_node_domain.c takes
 * from its board configuration only the MCU (NETWORK_NODE_PLATFORM) and the C2
 * hook (NET_BOARD_DOMAIN_C2_HOOK); the worker build passes both as compile
 * definitions. There is no FlexCAN / LPUART layout, time hook or critical
 * section here: the Host virtual backend replaces the RTD backend, so the
 * board configuration of src/platform/rtd stays off the include path. */
#ifndef NETWORK_NODE_PLATFORM
#error "virtual DOMAIN node: NETWORK_NODE_PLATFORM must be given by the build"
#endif
#ifndef NET_BOARD_DOMAIN_C2_HOOK
#error "virtual DOMAIN node: NET_BOARD_DOMAIN_C2_HOOK must be given by the build"
#endif
#endif
