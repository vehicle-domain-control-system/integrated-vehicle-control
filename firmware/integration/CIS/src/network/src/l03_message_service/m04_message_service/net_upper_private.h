#ifndef NET_UPPER_PRIVATE_H
#define NET_UPPER_PRIVATE_H
#include "network/net_upper.h"
NetUpperTx *net_upper_find_tx(NetUpper *, NetTxKey);
NetStatus net_upper_observe_tx(NetUpper *, const NetTxEvent *);
NetStatus net_upper_read_clock(NetUpper *, uint64_t *);
void net_upper_reject_queued(NetUpperTx *, NetStatus);
void net_upper_failure_changed(NetUpperTx *);
NetStatus net_upper_can_original_age(const NetUpperCanMessage *, const NetUpperCanMessage *, uint64_t);
NetStatus net_upper_uart_original_age(const NetUpperUartMessage *, const NetUpperUartMessage *, uint64_t);
bool net_upper_can_same_observation(const NetUpperCanMessage *, const NetUpperCanMessage *);
bool net_upper_uart_same_observation(const NetUpperUartMessage *, const NetUpperUartMessage *);
bool net_upper_uart_same_source(const NetUpperUartMessage *, const NetUpperUartMessage *);
NetStatus net_upper_can_repeat_age(const NetUpperCanMessage *, NetUpperCanMessage *, uint64_t, uint16_t, bool *);
NetStatus net_upper_uart_repeat_age(const NetUpperUartMessage *, NetUpperUartMessage *, uint64_t, uint16_t, bool *);
size_t net_upper_admission_index(size_t registry_index);
const NetUpperAdmission *net_upper_admission_at(const NetUpper *, size_t registry_index);
NetUpperAdmission *net_upper_admission_at_mut(NetUpper *, size_t registry_index);
const NetUpperUartReceiver *net_upper_receiver(const NetUpper *, size_t logical_uart_index);
NetUpperUartReceiver *net_upper_receiver_mut(NetUpper *, size_t logical_uart_index);
bool net_upper_profile_link(NetWireLink);
bool net_upper_uart_sequence_unused(NetUpper *, NetWireLink, uint16_t);
bool net_upper_profile_matches(uint32_t node);
#endif
