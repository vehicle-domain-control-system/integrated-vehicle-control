#ifndef NET_BOARD_NET_VSS_BOARD_H
#define NET_BOARD_NET_VSS_BOARD_H

/* Network side of the VSS board project (S32K344): the node configuration
 * hook of Net_Init(). Project define:
 *   NET_BOARD_NODE_CONFIG_HOOK=VssBoard_NetNodeConfig
 * It uses the Network type NetNodeConfig, so it lives here and not in the VSS
 * sources. Only Network headers: never include vss_net_api.h in this file
 * (it declares the same application types as network/app/net_if_*.h; the two
 * meet at link time). */
#include <stdbool.h>
#include "network/net_node.h"

bool VssBoard_NetNodeConfig(NetNodeConfig *out);

#endif
