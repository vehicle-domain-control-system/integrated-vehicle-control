#include "can_core_private.h"

CanCoreStatus CanTransport_Init(CanTransport *self,const CanTransportConfig *config,CanTransportStorage *storage,CanLinkPort *port)
{
    size_t bytes=sizeof(*self),tx_bytes,rx_bytes;
    CAN_BRANCH("TI01");
    if (!self || !config || !storage || !port) return CAN_INVALID_ARG;
    CAN_BRANCH("TI02");
    if (self->owner_guard) return CAN_REENTRANT;
    if (self->initialized) return CAN_NOT_READY;
    CAN_BRANCH("TI03");
    if (!storage->tx_slots || !storage->rx_records || !config->tx_capacity || !config->rx_capacity ||
        config->tx_capacity>UINT16_MAX || config->tx_capacity!=port->config.tx_slots ||
        config->max_observation_gap_ms!=port->config.max_clock_gap_ms ||
        storage->tx_count<config->tx_capacity || storage->rx_count<config->rx_capacity) return CAN_INVALID_ARG;
    CAN_BRANCH("TI04");
    if (!config->watchdog_ms || config->watchdog_ms>=UINT32_C(0x80000000) ||
        config->horizon_proven!=1 || !config->max_observation_gap_ms ||
        config->max_observation_gap_ms>=UINT32_C(0x80000000)) return CAN_INVALID_ARG;
    CAN_BRANCH("TI05");
    if (!can_size_add(&bytes,config->tx_capacity,sizeof(CanTransportSlot)) ||
        !can_size_add(&bytes,config->rx_capacity,sizeof(CanRxRecord)) || bytes>config->memory_budget_bytes) return CAN_INVALID_ARG;
    tx_bytes=config->tx_capacity*sizeof(CanTransportSlot);rx_bytes=config->rx_capacity*sizeof(CanRxRecord);
    if (can_overlap(self,sizeof(*self),config,sizeof(*config)) || can_overlap(self,sizeof(*self),storage,sizeof(*storage)) ||
        can_overlap(self,sizeof(*self),port,sizeof(*port)) ||
        can_overlap(storage->tx_slots,tx_bytes,storage->rx_records,rx_bytes) ||
        can_overlap(self,sizeof(*self),storage->tx_slots,tx_bytes) || can_overlap(self,sizeof(*self),storage->rx_records,rx_bytes) ||
        can_overlap(config,sizeof(*config),storage->tx_slots,tx_bytes) || can_overlap(config,sizeof(*config),storage->rx_records,rx_bytes) ||
        can_overlap(storage,sizeof(*storage),storage->tx_slots,tx_bytes) || can_overlap(storage,sizeof(*storage),storage->rx_records,rx_bytes) ||
        can_port_alias(port,storage->tx_slots,tx_bytes) || can_port_alias(port,storage->rx_records,rx_bytes)) return CAN_INVALID_ARG;
    CAN_BRANCH("TI06");
    if (!port->initialized || !port->health.generation || config->generation_fresh_for_pair!=1) return CAN_NOT_READY;
    CAN_BRANCH("TI07");
    memset(storage->tx_slots,0,tx_bytes);memset(storage->rx_records,0,rx_bytes);memset(self,0,sizeof(*self));
    self->config=*config;self->storage=*storage;self->port=port;self->health=port->health;self->initialized=1;
    return CAN_OK;
}

CanCoreStatus CanTransport_TryTx(CanTransport *self,const CanRawMessage *input,CanClockStamp now,CanTxToken *accepted)
{
    size_t i,index;
    uint8_t exhausted=0;
    CanTransportSlot *slot;
    CanFrame frame;
    CanCodecResult codec;
    CanTxToken token;
    CanCoreStatus status;
    CAN_BRANCH("TT01");
    if (!self || !input || !accepted || !self->initialized || now.valid>1 ||
        can_transport_alias(self,input,sizeof(*input)) || can_transport_alias(self,accepted,sizeof(*accepted)) ||
        can_overlap(input,sizeof(*input),accepted,sizeof(*accepted)) ||
        can_port_alias(self->port,input,sizeof(*input)) || can_port_alias(self->port,accepted,sizeof(*accepted))) return CAN_INVALID_ARG;
    CAN_BRANCH("TT02");
    if (self->owner_guard) return CAN_REENTRANT;
    self->owner_guard=1;
    CAN_BRANCH("TT03");
    if (now.valid!=1 || self->config.horizon_proven!=1) {self->owner_guard=0;return CAN_CLOCK_UNKNOWN;}
    CAN_BRANCH("TT04");
    if (!self->port->health.ready || !self->port->health.generation || self->port->health.critical_loss) {self->owner_guard=0;return CAN_NOT_READY;}
    CAN_BRANCH("TT05");
    index=self->config.tx_capacity;
    for (i=0;i<self->config.tx_capacity;i++) {
        if (self->storage.tx_slots[i].state==CAN_T_FREE) {
            if (self->storage.tx_slots[i].serial==UINT64_MAX) exhausted=1;
            else {index=i;break;}
        }
    }
    if (index==self->config.tx_capacity) {self->owner_guard=0;return exhausted ? CAN_NOT_READY : CAN_NO_CAPACITY;}
    CAN_BRANCH("TT06");
    codec=CanCodec_Encode(input,&frame);self->last_codec_result=codec;self->codec_result_valid=1;
    if (codec.status!=CAN_OK) {self->owner_guard=0;return codec.status;}
    CAN_BRANCH("TT07");
    slot=&self->storage.tx_slots[index];slot->serial++;
    token=(CanTxToken){self->port->health.generation,(uint16_t)index,slot->serial};
    slot->state=CAN_T_RESERVED;slot->frame=frame;slot->kind=input->kind;
    slot->source_update=input->kind==CAN_REAR ? input->fields.rear.update_sequence : input->fields.permission.update_sequence;
    CAN_BRANCH("TT08");
    status=CanLinkPort_TryTx(self->port,&slot->frame,token,now);
    if (status!=CAN_ACCEPTED) {
        uint64_t serial=slot->serial;memset(slot,0,sizeof(*slot));slot->serial=serial;
        self->owner_guard=0;return status;
    }
    CAN_BRANCH("TT09");
    slot->token=token;slot->accepted_at=now;slot->deadline_fenced=0;slot->state=CAN_T_INFLIGHT;
    *accepted=token;self->owner_guard=0;return CAN_ACCEPTED;
}

CanCoreStatus CanTransport_Pump(CanTransport *self,CanClockStamp now,uint16_t budget)
{
    uint16_t n;
    size_t i;
    uint8_t reset_needed=0;
    CanPortEvent event;
    CAN_BRANCH("TP01");
    if (!self || !self->initialized || !budget || now.valid>1) return CAN_INVALID_ARG;
    CAN_BRANCH("TP02");
    if (self->owner_guard) return CAN_REENTRANT;
    self->owner_guard=2;
    for (n=0;n<budget;n++) {
        CanCoreStatus status=CanLinkPort_PollEvent(self->port,&event,now);
        CAN_BRANCH("TP03");
        if (status==CAN_EMPTY || status==CAN_NOT_READY) break;
        if (status!=CAN_OK) {
            CAN_BRANCH("TP04");CanLinkPort_Reset(self->port,CAN_RESET_EVENT_LOSS,now);
            self->health=self->port->health;self->owner_guard=0;return CAN_NOT_READY;
        }
        if (event.kind==CAN_PORT_HEALTH) {
            CAN_BRANCH("TP05");self->health=event.data.health;continue;
        }
        if (event.kind==CAN_PORT_TX_TERMINAL) {
            CanTxToken token=event.data.terminal.token;
            if (token.slot<self->config.tx_capacity) {
                CanTransportSlot *s=&self->storage.tx_slots[token.slot];
                if (s->state==CAN_T_INFLIGHT && can_token_equal(s->token,token)) {
                    CAN_BRANCH("TP06");
                    s->outcome=(CanTxOutcome){token,s->kind,s->source_update,event.data.terminal.result,event.data.terminal.cause,event.data.terminal.observed_at};
                    s->state=CAN_T_OUTCOME_READY;continue;
                }
            }
            CAN_BRANCH("TP07");self->late_event_count=can_sat_inc(self->late_event_count);continue;
        }
        if (event.kind==CAN_PORT_RX_FRAME) {
            if (event.data.rx.controller_error || event.data.rx.backend_generation!=self->port->health.generation ||
                self->rx_used==self->config.rx_capacity) {
                CAN_BRANCH("TP08");self->last_rejected_rx=event.data.rx;self->rejected_rx_valid=1;
                self->rx_dropped=can_sat_inc(self->rx_dropped);continue;
            }
            CAN_BRANCH("TP09");self->storage.rx_records[self->rx_tail]=event.data.rx;
            self->rx_tail=(self->rx_tail+1)%self->config.rx_capacity;self->rx_used++;
        }
    }
    CAN_BRANCH("TP10");
    for (i=0;i<self->config.tx_capacity;i++) {
        uint32_t delta;
        CanTransportSlot *s=&self->storage.tx_slots[i];
        if (s->state==CAN_T_INFLIGHT && !s->deadline_fenced &&
            (self->config.horizon_proven!=1 || !can_clock_delta(s->accepted_at,now,&delta) || delta>=self->config.watchdog_ms)) reset_needed=1;
    }
    if (reset_needed) {
        for (i=0;i<self->config.tx_capacity;i++) if (self->storage.tx_slots[i].state==CAN_T_INFLIGHT) self->storage.tx_slots[i].deadline_fenced=1;
        CanLinkPort_Reset(self->port,CAN_RESET_REQUESTED,now);self->health=self->port->health;
    }
    CAN_BRANCH("TP11");self->owner_guard=0;return CAN_OK;
}

CanCoreStatus CanTransport_TakeOutcome(CanTransport *self,CanTxOutcome *out)
{
    size_t n;
    CAN_BRANCH("TO01");
    if (!self || !out || !self->initialized || can_transport_alias(self,out,sizeof(*out)) || can_port_alias(self->port,out,sizeof(*out))) return CAN_INVALID_ARG;
    CAN_BRANCH("TO02");if (self->owner_guard) return CAN_REENTRANT;
    CAN_BRANCH("TO03");
    for (n=0;n<self->config.tx_capacity;n++) {
        size_t i=(self->outcome_cursor+n)%self->config.tx_capacity;CanTransportSlot *s=&self->storage.tx_slots[i];
        if (s->state==CAN_T_OUTCOME_READY) {
            uint64_t serial=s->serial;
            CAN_BRANCH("TO04");*out=s->outcome;memset(s,0,sizeof(*s));s->serial=serial;
            self->outcome_cursor=(i+1)%self->config.tx_capacity;return CAN_OK;
        }
    }
    return CAN_EMPTY;
}

CanCoreStatus CanTransport_TakeRx(CanTransport *self,CanRxRecord *out)
{
    CAN_BRANCH("TR01");
    if (!self || !out || !self->initialized || can_transport_alias(self,out,sizeof(*out)) || can_port_alias(self->port,out,sizeof(*out))) return CAN_INVALID_ARG;
    CAN_BRANCH("TR02");if (self->owner_guard) return CAN_REENTRANT;
    CAN_BRANCH("TR03");
    while (self->rx_used && self->storage.rx_records[self->rx_head].backend_generation!=self->port->health.generation) {
        self->rx_head=(self->rx_head+1)%self->config.rx_capacity;self->rx_used--;self->rx_dropped=can_sat_inc(self->rx_dropped);
    }
    CAN_BRANCH("TR04");if (!self->rx_used) return CAN_EMPTY;
    CAN_BRANCH("TR05");*out=self->storage.rx_records[self->rx_head];
    self->rx_head=(self->rx_head+1)%self->config.rx_capacity;self->rx_used--;return CAN_OK;
}
