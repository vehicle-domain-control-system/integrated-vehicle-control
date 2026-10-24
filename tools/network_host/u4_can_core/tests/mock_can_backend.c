#include "mock_can_backend.h"
static CanPortAdmissionObservation mock_admit(void *context,CanLinkPort *p,const CanFrame *frame,CanTxToken token,CanClockStamp now)
{
    MockCanBackend *m=context;
    m->admits++;
    if (token.slot>=MOCK_TX_CAPACITY) return (CanPortAdmissionObservation){CAN_NOT_READY,1};
    if (!m->next_admission.no_start_proven || m->next_admission.status==CAN_ACCEPTED || m->synchronous_terminal) {
        m->references[token.slot]=frame;m->tokens[token.slot]=token;
    }
    if (m->during_admit) m->during_admit(m->callback_context,p);
    if (m->synchronous_terminal) MockCan_Terminal(m,p,token,m->synchronous_result,now);
    return m->next_admission;
}
static uint8_t mock_flush(void *context,CanLinkPort *p,CanResetReason reason,uint64_t generation,CanClockStamp now)
{
    MockCanBackend *m=context;
    size_t i;
    (void)p;(void)reason;(void)generation;(void)now;
    m->flushes++;
    if (!m->allow_flush) return 0;
    /* No event queue exists: retained historical tokens own no bytes.
     * Explicit detach removes every live read reference before proof is returned. */
    for (i=0;i<MOCK_TX_CAPACITY;i++) {if (m->references[i]) m->detached_references++;m->references[i]=NULL;}
    for (i=0;i<MOCK_TX_CAPACITY;i++) if (m->references[i]) return 0;
    return 1;
}
void MockCan_Init(MockCanBackend *m)
{
    memset(m,0,sizeof(*m));m->binding=(CanHostBackend){m,mock_admit,mock_flush};
    m->next_admission=(CanPortAdmissionObservation){CAN_ACCEPTED,0};m->synchronous_result=CAN_TX_COMPLETE;
}
CanCoreStatus MockCan_Terminal(MockCanBackend *m,CanLinkPort *p,CanTxToken token,CanTxResult result,CanClockStamp at)
{
    if (token.slot<MOCK_TX_CAPACITY && can_token_equal(m->tokens[token.slot],token) && result!=CAN_TX_UNKNOWN) m->references[token.slot]=NULL;
    return can_host_terminal(p,token,result,result==CAN_TX_COMPLETE ? CAN_OK : CAN_NOT_READY,at);
}
CanCoreStatus MockCan_Rx(CanLinkPort *p,const CanFrame *f,CanClockStamp at,uint64_t gen,uint32_t error)
{
    return can_host_rx(p,f,at,gen,error);
}
void MockCan_Fault(CanLinkPort *p,uint8_t bus_off,uint8_t lost)
{
    can_host_fault(p,bus_off,lost);
}
