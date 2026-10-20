#include "network/net_hal_adapter.h"
/* Private S32K144 binding entry; board/RTD operations remain injected. */
NetStatus net_s32k144_upper_port_init(NetHalAdapter *adapter, const NetHalConfig *config)
{ return net_hal_adapter_init(adapter, config, NET_HAL_S32K144); }
