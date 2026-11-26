#include "network/net_hal_adapter.h"
/* Private S32K344 binding entry; cache/DMA and board/RTD binding remain HOLD. */
NetStatus net_s32k344_upper_port_init(NetHalAdapter *adapter, const NetHalConfig *config)
{ return net_hal_adapter_init(adapter, config, NET_HAL_S32K344); }
