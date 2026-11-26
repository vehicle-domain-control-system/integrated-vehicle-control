#ifndef MOCK_CAN_BACKEND_H
#define MOCK_CAN_BACKEND_H
#include "../can_core_private.h"
enum { MOCK_TX_CAPACITY=2 };
typedef struct {
    CanHostBackend binding;
    CanPortAdmissionObservation next_admission;
    const CanFrame *references[MOCK_TX_CAPACITY];
    CanTxToken tokens[MOCK_TX_CAPACITY];
    uint32_t admits,flushes,detached_references;
    uint8_t synchronous_terminal,allow_flush;
    CanTxResult synchronous_result;
    void (*during_admit)(void *,CanLinkPort *);
    void *callback_context;
} MockCanBackend;
void MockCan_Init(MockCanBackend *self);
CanCoreStatus MockCan_Terminal(MockCanBackend *self,CanLinkPort *port,CanTxToken token,CanTxResult result,CanClockStamp at);
CanCoreStatus MockCan_Rx(CanLinkPort *port,const CanFrame *frame,CanClockStamp at,uint64_t generation,uint32_t error);
void MockCan_Fault(CanLinkPort *port,uint8_t bus_off,uint8_t lost_terminal);
#endif
