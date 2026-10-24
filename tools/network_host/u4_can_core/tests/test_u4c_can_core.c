#define __USE_MINGW_ANSI_STDIO 1
#include "mock_can_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include "golden_oracles.h"

/* Test-only instrumentation, never state used by the production Core. */
static const char *case_id;
static unsigned checks,failed,total_failed,total_checks;
static const char *branch_ids[]={ BRANCH_IDS };
static unsigned branch_hits[123];
static unsigned case_hits[123];
void CanTest_BranchHit(const char *id)
{
    size_t i;
    for (i=0;i<123;i++) if (!strcmp(branch_ids[i],id)) {
        branch_hits[i]++;case_hits[i]++;return;
    }
    fprintf(stderr,"Unknown branch ID %s\n",id);abort();
}
static void json_string(const char *s)
{
    putchar('"');
    while (*s) {if (*s=='"' || *s=='\\') putchar('\\');putchar((unsigned char)*s++);}
    putchar('"');
}
static void eq(long long actual,long long expected,const char *expression,unsigned line)
{
    checks++;if (actual!=expected) failed++;
    printf("{\"kind\":\"assert\",\"id\":\"%s\",\"line\":%u,\"expression\":",case_id,line);
    json_string(expression);
    printf(",\"actual\":%lld,\"expected\":%lld,\"pass\":%s}\n",actual,expected,actual==expected?"true":"false");
}
static void bytes(const void *actual,const void *expected,size_t n,const char *expression,unsigned line)
{
    const unsigned char *a=actual,*b=expected;size_t i;int ok=memcmp(a,b,n)==0;
    checks++;if (!ok) failed++;
    printf("{\"kind\":\"bytes\",\"id\":\"%s\",\"line\":%u,\"expression\":",case_id,line);json_string(expression);
    printf(",\"actual_hex\":\"");for(i=0;i<n;i++) printf("%02x",a[i]);
    printf("\",\"expected_hex\":\"");for(i=0;i<n;i++) printf("%02x",b[i]);
    printf("\",\"pass\":%s}\n",ok?"true":"false");
}
#define EQ(a,b) eq((long long)(a),(long long)(b),#a,__LINE__)
#define BYTES(a,b,n) bytes((a),(b),(n),#a,__LINE__)
typedef struct {
    CanLinkPort p;CanPortSlot ps[2];CanRxRecord pr[4];CanPortStorage pv;CanPortConfig pc;
    CanTransport t;CanTransportSlot ts[2];CanRxRecord tr[4];CanTransportStorage tv;CanTransportConfig tc;
    CanService s;CanSvcConfig sc;MockCanBackend m;
    CanSvcRxView received;unsigned received_count;uint8_t reentrant_sink;
} Fixture;
static CanClockStamp stamp(uint32_t ms) {return (CanClockStamp){1,ms,1};}
static CanRawMessage rear(uint32_t update,uint16_t age)
{
    CanRawMessage r={0};r.kind=CAN_REAR;
    r.fields.rear=(i1_cis_rear){1,99,update,age,1230,1,0,2};return r;
}
static CanRawMessage permission(void)
{
    CanRawMessage r={0};r.kind=CAN_PERMISSION;
    r.fields.permission=(i1_domain_permission){1,99,255,2,255,0,2,9,655};return r;
}
/* Hand-declared byte oracles; all mutations below are test-fixture LE writes,
 * never obtained from the implementation's encoder output. */
static const uint8_t rear_oracle[20]={1,0,0,0,1,0,0,0,7,0,0,0,10,0,206,4,1,0,2,0};
static const uint8_t permission_oracle[20]={1,0,0,0,1,0,0,0,255,2,255,0,2,9,0,0,0,143,2,0};
static void le32(uint8_t *p,uint32_t n) {size_t i;for(i=0;i<4;i++) p[i]=(uint8_t)(n>>(i*8));}
static void le16(uint8_t *p,uint16_t n) {p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static CanFrame rx_frame(uint32_t tx,uint32_t update,uint16_t age)
{
    CanFrame r={0};r.can_id=0xA0;r.fdf=r.brs=1;r.dlc=11;r.length=20;
    memcpy(r.bytes,rear_oracle,20);le32(r.bytes+4,tx);le32(r.bytes+8,update);le16(r.bytes+12,age);return r;
}
static int named_equal(const CanRawMessage *a,const CanRawMessage *b)
{
    if(a->kind!=b->kind) return 0;
    if(a->kind==CAN_REAR) {
        const i1_cis_rear *x=&a->fields.rear,*y=&b->fields.rear;
        return x->boot_id==y->boot_id && x->tx_sequence==y->tx_sequence && x->update_sequence==y->update_sequence &&
            x->source_age_ms==y->source_age_ms && x->rear_distance_deci_cm==y->rear_distance_deci_cm &&
            x->validity==y->validity && x->quality_reason==y->quality_reason && x->proximity_status==y->proximity_status;
    } else {
        const i1_domain_permission *x=&a->fields.permission,*y=&b->fields.permission;
        return x->boot_id==y->boot_id && x->tx_sequence==y->tx_sequence && x->update_sequence==y->update_sequence &&
            x->source_age_ms==y->source_age_ms && x->vehicle_power_permission==y->vehicle_power_permission &&
            x->power_permission_quality==y->power_permission_quality && x->power_operation_state==y->power_operation_state &&
            x->operation_allowed==y->operation_allowed && x->operation_quality==y->operation_quality;
    }
}
static void sink(void *ctx,const CanSvcRxView *v)
{
    Fixture *f=ctx;
    f->received=*v;f->received_count++;
    if (f->reentrant_sink) {
        CanRawMessage r=rear(7,10);CanTxToken token;CanTxOutcome outcome;CanRxRecord rx;CanPortEvent ev;
        EQ(CanSvc_Submit(&f->s,&r,stamp(100),stamp(100)),CAN_REENTRANT);
        EQ(CanSvc_Init(&f->s,&f->sc,&f->t),CAN_REENTRANT);
        EQ(CanSvc_Pump(&f->s,stamp(100),8),CAN_REENTRANT);
        EQ(CanSvc_BindSource(&f->s,CAN_REAR,1,2),CAN_REENTRANT);
        EQ(CanTransport_TryTx(&f->t,&r,stamp(100),&token),CAN_REENTRANT);
        EQ(CanTransport_Init(&f->t,&f->tc,&f->tv,&f->p),CAN_REENTRANT);
        EQ(CanTransport_Pump(&f->t,stamp(100),8),CAN_REENTRANT);
        EQ(CanTransport_TakeOutcome(&f->t,&outcome),CAN_REENTRANT);
        EQ(CanTransport_TakeRx(&f->t,&rx),CAN_REENTRANT);
        EQ(CanLinkPort_PollEvent(&f->p,&ev,stamp(100)),CAN_REENTRANT);
        EQ(CanLinkPort_Reset(&f->p,CAN_RESET_REQUESTED,stamp(100)),CAN_REENTRANT);
        EQ(CanLinkPort_Init(&f->p,&f->pc,&f->pv,stamp(100)),CAN_REENTRANT);
        {CanFrame frame=rx_frame(1,7,10);CanRxRecord received={frame,stamp(100),1,0};CanRawMessage decoded=rear(7,10);
         decoded.fields.rear.tx_sequence=1;
         EQ(CanLinkPort_TryTx(&f->p,&frame,(CanTxToken){1,1,1},stamp(100)),CAN_REENTRANT);
         EQ(CanSvc_OnDecoded(&f->s,&decoded,&received,stamp(100)),CAN_REENTRANT);}
        EQ(CanLinkPort_TryTx(&f->p,&f->ps[0].frame,(CanTxToken){1,0,1},stamp(100)),CAN_INVALID_ARG); /* owned alias is rejected first */
    }
}
static void setup_at(Fixture *f,uint32_t ms)
{
    memset(f,0,sizeof(*f));MockCan_Init(&f->m);
    f->pv=(CanPortStorage){f->ps,f->pr,2,4};
    f->pc=(CanPortConfig){2,4,1000,SIZE_MAX,CAN_PORT_HOST_PROFILE_ONLY,1,1,1,1,&f->m.binding};
    f->tv=(CanTransportStorage){f->ts,2,f->tr,4};
    f->tc=(CanTransportConfig){2,4,SIZE_MAX,1000,1000,1,1};
    f->sc=(CanSvcConfig){1,2,3,3,1000,{1000000,1000000},1,SIZE_MAX,sink,f};
    EQ(CanLinkPort_Init(&f->p,&f->pc,&f->pv,stamp(ms)),CAN_OK);
    EQ(CanTransport_Init(&f->t,&f->tc,&f->tv,&f->p),CAN_OK);
    EQ(CanSvc_Init(&f->s,&f->sc,&f->t),CAN_OK);
    EQ(CanSvc_BindSource(&f->s,CAN_REAR,1,1),CAN_OK);
    EQ(CanSvc_BindSource(&f->s,CAN_PERMISSION,1,1),CAN_OK);
}
static void setup(Fixture *f) {setup_at(f,100);}
static void cycle(Fixture *f,uint32_t ms)
{
    EQ(CanTransport_Pump(&f->t,stamp(ms),8),CAN_OK);
    (void)CanSvc_Pump(&f->s,stamp(ms),8);
}
static void receive(Fixture *f,CanFrame *frame,uint32_t captured,uint32_t consumed)
{
    EQ(MockCan_Rx(&f->p,frame,stamp(captured),f->p.health.generation,0),CAN_ACCEPTED);cycle(f,consumed);
}
static CanTxToken transmit_at(Fixture *f,uint32_t ms)
{
    CanTxToken t={0};CanRawMessage r=rear(7,10);
    EQ(CanTransport_TryTx(&f->t,&r,stamp(ms),&t),CAN_ACCEPTED);return t;
}
static CanTxToken transmit(Fixture *f) {return transmit_at(f,100);}
static void d01(void)
{
    Fixture f;CanRawMessage r=rear(7,10);setup(&f);
    EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(100)),CAN_ACCEPTED);
    r.fields.rear.rear_distance_deci_cm=42;
    EQ(f.s.pending[0].original.fields.rear.rear_distance_deci_cm,1230);EQ(f.m.admits,0);EQ(f.s.tx_next[0],1);
    EQ(CanSvc_Pump(&f.s,stamp(100),8),CAN_ACCEPTED);
    BYTES(f.ps[0].frame.bytes,rear_oracle,20);BYTES(f.ts[0].frame.bytes,rear_oracle,20);
    EQ(f.m.references[0]==&f.ps[0].frame,1);EQ(f.m.references[0]==&f.ts[0].frame,0);EQ(f.s.tx_next[0],2);
}
static void d02(void)
{
    Fixture f;CanRawMessage r=permission();setup(&f);
    EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(100)),CAN_ACCEPTED);EQ(CanSvc_Pump(&f.s,stamp(100),8),CAN_ACCEPTED);
    BYTES(f.ps[0].frame.bytes,permission_oracle,20);EQ(f.ps[0].frame.bytes[8],255);EQ(f.ps[0].frame.bytes[11],0);
    EQ(f.s.tx_next[CAN_REAR],1);EQ(f.s.tx_next[CAN_PERMISSION],2);EQ(f.received_count,0);
}
static void d03(void)
{
    Fixture f;CanRawMessage out,backup,r=rear(7,10);CanFrame fr=rx_frame(1,7,10),encoded,eb;
    CanTxToken token,tk;CanTxOutcome oc,ob;CanPortEvent pe,pb;CanRxRecord rx,rb;
    union {CanRawMessage r;CanFrame f;} alias;
    unsigned i;setup(&f);memset(&out,0xA5,sizeof(out));backup=out;
    EQ(CanCodec_Decode(NULL,&out).status,CAN_INVALID_ARG);BYTES(&out,&backup,sizeof(out));
    EQ(CanCodec_Encode(NULL,&encoded).status,CAN_INVALID_ARG);EQ(CanCodec_Decode(&fr,NULL).status,CAN_INVALID_ARG);
    alias.r=r;EQ(CanCodec_Encode(&alias.r,&alias.f).status,CAN_INVALID_ARG);
    for (i=0;i<8;i++) {
        CanFrame bad=fr;
        if(i==0) bad.length=19;
        if(i==1) bad.length=21;
        if(i==2) bad.dlc=10;
        if(i==3) bad.ide=1;
        if(i==4) bad.fdf=0;
        if(i==5) bad.brs=2;
        if(i==6) bad.rtr=1;
        if(i==7) bad.can_id=0x800;
        EQ(CanCodec_Decode(&bad,&out).status,CAN_PROFILE_REJECT);BYTES(&out,&backup,sizeof(out));
    }
    memset(&encoded,0xA5,sizeof(encoded));eb=encoded;r.fields.rear.boot_id=0;
    EQ(CanCodec_Encode(&r,&encoded).i1,I1_ERR_RESERVED);BYTES(&encoded,&eb,sizeof(encoded));
    memset(&token,0xA5,sizeof(token));tk=token;
    EQ(CanTransport_TryTx(&f.t,&r,stamp(100),&token),CAN_CODEC_REJECT);BYTES(&token,&tk,sizeof(token));
    EQ(CanSvc_Submit(NULL,&r,stamp(100),stamp(100)),CAN_INVALID_ARG);EQ(CanSvc_Init(NULL,&f.sc,&f.t),CAN_INVALID_ARG);
    EQ(CanSvc_Submit(&f.s,&f.s.pending[0].original,stamp(100),stamp(100)),CAN_INVALID_ARG);
    EQ(CanTransport_Init(NULL,&f.tc,&f.tv,&f.p),CAN_INVALID_ARG);EQ(CanLinkPort_Init(NULL,&f.pc,&f.pv,stamp(100)),CAN_INVALID_ARG);
    EQ(CanLinkPort_TryTx(&f.p,NULL,(CanTxToken){1,0,1},stamp(100)),CAN_INVALID_ARG);
    memset(&oc,0xA5,sizeof(oc));ob=oc;EQ(CanTransport_TakeOutcome(&f.t,&oc),CAN_EMPTY);BYTES(&oc,&ob,sizeof(oc));
    memset(&rx,0xA5,sizeof(rx));rb=rx;EQ(CanTransport_TakeRx(&f.t,&rx),CAN_EMPTY);BYTES(&rx,&rb,sizeof(rx));
    memset(&pe,0xA5,sizeof(pe));pb=pe;EQ(CanLinkPort_PollEvent(&f.p,&pe,stamp(100)),CAN_EMPTY);BYTES(&pe,&pb,sizeof(pe));
    EQ(CanTransport_TakeOutcome(&f.t,(CanTxOutcome *)&f.t),CAN_INVALID_ARG);
    EQ(CanTransport_TakeRx(&f.t,(CanRxRecord *)&f.tr[0]),CAN_INVALID_ARG);
    EQ(CanLinkPort_PollEvent(&f.p,(CanPortEvent *)&f.p,stamp(100)),CAN_INVALID_ARG);
}
static void d04(void)
{
    CanFrame fr=rx_frame(1,7,10);CanRawMessage r;CanCodecResult c;size_t i;
    fr.bytes[19]=1;c=CanCodec_Decode(&fr,&r);EQ(c.status,CAN_CODEC_REJECT);EQ(c.i1,I1_ERR_PADDING);EQ(c.i1_called,1);
    fr.bytes[19]=0;le32(fr.bytes,0);EQ(CanCodec_Decode(&fr,&r).i1,I1_ERR_RESERVED);
    le32(fr.bytes,1);fr.bytes[16]=2;EQ(CanCodec_Decode(&fr,&r).i1,I1_ERR_VALUE);
    fr.bytes[16]=0;fr.bytes[17]=1;EQ(CanCodec_Decode(&fr,&r).status,CAN_OK);EQ(r.fields.rear.validity,0);EQ(r.fields.rear.quality_reason,1);
    fr.can_id=0x80;memcpy(fr.bytes,permission_oracle,20);fr.bytes[9]=4;
    EQ(CanCodec_Decode(&fr,&r).i1,I1_ERR_VALUE);memcpy(fr.bytes,permission_oracle,20);fr.bytes[11]=2;
    EQ(CanCodec_Decode(&fr,&r).i1,I1_ERR_VALUE);memcpy(fr.bytes,permission_oracle,20);fr.bytes[8]=2;
    EQ(CanCodec_Decode(&fr,&r).i1,I1_ERR_VALUE);
    for (i=0;i<GOLDEN_COUNT;i++) {
        CanFrame encoded;CanRawMessage decoded;
        EQ(CanCodec_Encode(&golden_messages[i],&encoded).status,CAN_OK);BYTES(encoded.bytes,golden_bytes[i],20);
        encoded.can_id=golden_messages[i].kind==CAN_REAR?0xA0:0x80;
        memcpy(encoded.bytes,golden_bytes[i],20);
        EQ(CanCodec_Decode(&encoded,&decoded).status,CAN_OK);
        EQ(decoded.kind,golden_messages[i].kind);
        EQ(named_equal(&decoded,&golden_messages[i]),1);
        /* Compare all named fields via the independent I1 encoder oracle bytes. */
        EQ(CanCodec_Encode(&decoded,&encoded).status,CAN_OK);BYTES(encoded.bytes,golden_bytes[i],20);
    }
}
static void d05(void)
{
    Fixture f;CanRawMessage r=rear(7,10),p=permission();CanTxToken t,sentinel;CanTransport tmp={0};CanTransportConfig cfg;
    setup(&f);f.s.config.pending_capacity=1;
    EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(100)),CAN_ACCEPTED);
    EQ(CanSvc_Submit(&f.s,&p,stamp(100),stamp(100)),CAN_NO_CAPACITY);EQ(f.s.tx_origin[1].has_observation,0);
    f.m.next_admission=(CanPortAdmissionObservation){CAN_BUSY,1};
    EQ(CanSvc_Pump(&f.s,stamp(100),8),CAN_BUSY);EQ(f.s.tx_next[0],1);EQ(f.ps[0].state,CAN_PORT_FREE);EQ(f.ts[0].state,CAN_T_FREE);
    EQ(f.s.pending[0].occupied,1);
    f.m.next_admission=(CanPortAdmissionObservation){CAN_NO_CAPACITY,1};memset(&t,0xA5,sizeof(t));sentinel=t;
    EQ(CanTransport_TryTx(&f.t,&r,stamp(100),&t),CAN_NO_CAPACITY);BYTES(&t,&sentinel,sizeof(t));
    f.m.next_admission=(CanPortAdmissionObservation){CAN_NOT_READY,1};EQ(CanTransport_TryTx(&f.t,&r,stamp(100),&t),CAN_NOT_READY);
    f.m.next_admission=(CanPortAdmissionObservation){CAN_ACCEPTED,0};transmit(&f);transmit(&f);
    EQ(CanTransport_TryTx(&f.t,&r,stamp(100),&t),CAN_NO_CAPACITY);
    cfg=f.tc;cfg.rx_capacity=SIZE_MAX;{CanTransportStorage view=f.tv;view.rx_count=SIZE_MAX;EQ(CanTransport_Init(&tmp,&cfg,&view,&f.p),CAN_INVALID_ARG);}
    EQ(tmp.initialized,0);cfg=f.tc;cfg.memory_budget_bytes=1;EQ(CanTransport_Init(&tmp,&cfg,&f.tv,&f.p),CAN_INVALID_ARG);
    cfg=f.tc;cfg.tx_capacity=0;EQ(CanTransport_Init(&tmp,&cfg,&f.tv,&f.p),CAN_INVALID_ARG);
}
static void d06(void)
{
    Fixture f;CanRawMessage r=rear(7,10);uint8_t saved[20];setup(&f);
    EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(100)),CAN_ACCEPTED);r.fields.rear.update_sequence=8;r.fields.rear.rear_distance_deci_cm=900;
    EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(100)),CAN_ACCEPTED);EQ(f.s.counters.replaced,1);
    EQ(CanSvc_Pump(&f.s,stamp(100),8),CAN_ACCEPTED);memcpy(saved,f.ps[0].frame.bytes,20);
    r.fields.rear.update_sequence=9;r.fields.rear.rear_distance_deci_cm=800;
    EQ(CanSvc_Submit(&f.s,&r,stamp(101),stamp(101)),CAN_ACCEPTED);BYTES(f.ps[0].frame.bytes,saved,20);BYTES(f.ts[0].frame.bytes,saved,20);
}
static void d07(void)
{
    Fixture f;CanRawMessage r=rear(7,10),saved;CanFrame fr,old;setup(&f);r.kind=(CanMsgKind)99;saved=r;memset(&fr,0xA5,sizeof(fr));old=fr;
    EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(100)),CAN_UNSUPPORTED);BYTES(&r,&saved,sizeof(r));EQ(f.s.pending[0].occupied,0);
    EQ(CanCodec_Encode(&r,&fr).status,CAN_UNSUPPORTED);BYTES(&fr,&old,sizeof(fr));
    fr=rx_frame(1,7,10);fr.can_id=0x123;EQ(CanCodec_Decode(&fr,&r).status,CAN_UNSUPPORTED);
    EQ(CanSvc_BindSource(&f.s,(CanMsgKind)99,1,1),CAN_UNSUPPORTED);
    {CanRxRecord rx={rx_frame(1,7,10),stamp(100),1,0};r.kind=(CanMsgKind)99;
     EQ(CanSvc_OnDecoded(&f.s,&r,&rx,stamp(100)),CAN_UNSUPPORTED);EQ(f.s.guard,CAN_SVC_IDLE);}
}
static void d08(void)
{
    Fixture f;CanTxToken token;CanTxOutcome out;setup(&f);token=transmit(&f);
    EQ(f.ts[0].state,CAN_T_INFLIGHT);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_EMPTY);EQ(f.received_count,0);
    EQ(MockCan_Terminal(&f.m,&f.p,token,CAN_TX_COMPLETE,stamp(101)),CAN_OK);
    EQ(CanTransport_Pump(&f.t,stamp(101),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_OK);
    EQ(out.result,CAN_TX_COMPLETE);EQ(out.source_update,7);EQ(can_token_equal(out.token,token),1);
    EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_EMPTY);EQ(f.received_count,0);EQ(f.s.rx_origin[0].has_record,0);
}
static void d09(void)
{
    Fixture f;CanTxToken token;CanTxOutcome out;uint8_t saved[20];setup(&f);f.ts[0].serial=6;token=transmit(&f);EQ(token.serial,7);
    EQ(MockCan_Terminal(&f.m,&f.p,token,CAN_TX_CONFIRMED_FAIL,stamp(101)),CAN_OK);
    EQ(CanTransport_Pump(&f.t,stamp(101),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_OK);EQ(out.result,CAN_TX_CONFIRMED_FAIL);
    token=transmit_at(&f,101);memcpy(saved,f.ps[0].frame.bytes,20);
    EQ(MockCan_Terminal(&f.m,&f.p,token,CAN_TX_UNKNOWN,stamp(101)),CAN_OK);
    EQ(CanTransport_Pump(&f.t,stamp(101),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_OK);EQ(out.result,CAN_TX_UNKNOWN);
    EQ(f.ps[0].state,CAN_PORT_UNKNOWN_PINNED);EQ(f.p.health.ready,0);EQ(f.m.references[0]!=NULL,1);BYTES(f.ps[0].frame.bytes,saved,20);
    EQ(MockCan_Terminal(&f.m,&f.p,token,CAN_TX_COMPLETE,stamp(102)),CAN_EMPTY);
    EQ(CanTransport_Pump(&f.t,stamp(102),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_EMPTY);
    /* MAY_START without any terminal is accepted UNKNOWN, never NO_START rollback. */
    setup(&f);f.m.next_admission=(CanPortAdmissionObservation){CAN_BUSY,0};
    {CanRawMessage r=rear(7,10);EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(100)),CAN_ACCEPTED);}
    EQ(CanSvc_Pump(&f.s,stamp(100),8),CAN_ACCEPTED);EQ(f.s.tx_next[0],2);EQ(f.s.pending[0].occupied,0);
    EQ(f.ps[0].terminal_result,CAN_TX_UNKNOWN);EQ(f.ps[0].release_safe,0);EQ(f.m.references[0]!=NULL,1);
    cycle(&f,101);EQ(f.s.last_outcome[0].result,CAN_TX_UNKNOWN);EQ(f.ps[0].state,CAN_PORT_UNKNOWN_PINNED);EQ(f.m.admits,1);
}
static void d10(void)
{
    Fixture f;CanRawMessage r=rear(7,10);CanTxToken token;CanTxOutcome out;setup(&f);
    EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(100)),CAN_ACCEPTED);EQ(CanSvc_Pump(&f.s,stamp(100),8),CAN_ACCEPTED);token=f.ts[0].token;
    r.fields.rear.update_sequence=8;EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(100)),CAN_ACCEPTED);
    MockCan_Fault(&f.p,1,0);cycle(&f,105);EQ(f.p.health.bus_off,1);EQ(f.s.pending[0].occupied,1);EQ(f.s.has_outcome[0],1);EQ(f.s.last_outcome[0].result,CAN_TX_UNKNOWN);
    EQ(f.ps[0].state,CAN_PORT_UNKNOWN_PINNED);EQ(f.m.references[0]!=NULL,1);EQ(f.m.admits,1);
    EQ(CanLinkPort_Reset(&f.p,CAN_RESET_BUS_OFF,stamp(106)),CAN_NOT_READY);
    f.m.allow_flush=1;cycle(&f,110);cycle(&f,111);
    EQ(f.m.detached_references,1);EQ(f.p.health.bus_off,0);EQ(f.m.flushes>0,1);
    EQ(f.m.admits,2);EQ(f.ps[0].frame.bytes[12],20);EQ(f.ps[0].frame.bytes[8],8);EQ(f.s.tx_next[0],3);
    EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_EMPTY);EQ(token.backend_generation,1);
}
static void d11(void)
{
    Fixture f;CanTxToken old,current;CanTxOutcome out;uint64_t generation;setup(&f);old=transmit(&f);
    EQ(CanLinkPort_Reset(&f.p,CAN_RESET_REQUESTED,stamp(100)),CAN_NOT_READY);generation=f.p.health.generation;
    EQ(CanLinkPort_Reset(&f.p,CAN_RESET_REQUESTED,stamp(100)),CAN_NOT_READY);EQ(f.p.health.generation,generation);
    EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_OK);EQ(out.result,CAN_TX_UNKNOWN);EQ(can_token_equal(old,out.token),1);
    f.m.allow_flush=1;EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);
    current=transmit(&f);EQ(current.backend_generation,2);EQ(current.serial>old.serial,1);
    EQ(MockCan_Terminal(&f.m,&f.p,old,CAN_TX_COMPLETE,stamp(100)),CAN_ORDER_REJECT);EQ(f.ts[0].state,CAN_T_INFLIGHT);
    EQ(MockCan_Terminal(&f.m,&f.p,current,CAN_TX_COMPLETE,stamp(100)),CAN_OK);EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_OK);
    f.ts[0].serial=f.ts[1].serial=UINT64_MAX;{CanRawMessage r=rear(7,10);EQ(CanTransport_TryTx(&f.t,&r,stamp(100),&current),CAN_NOT_READY);}
    f.p.health.generation=UINT64_MAX;EQ(CanLinkPort_Reset(&f.p,CAN_RESET_REQUESTED,stamp(100)),CAN_NOT_READY);EQ(f.p.health.generation==UINT64_MAX,1);
    setup(&f);transmit(&f);f.m.allow_flush=1;EQ(CanLinkPort_Reset(&f.p,CAN_RESET_REQUESTED,stamp(100)),CAN_OK);
    EQ(f.ps[0].terminal_result,CAN_TX_UNKNOWN);EQ(f.ps[0].release_safe,1);EQ(f.m.detached_references,1);
}
static void d12(void)
{
    Fixture f;CanFrame fr=rx_frame(1,7,10);setup(&f);le32(fr.bytes,2);receive(&f,&fr,100,100);EQ(f.received_count,0);
    le32(fr.bytes,1);receive(&f,&fr,100,100);EQ(f.received_count,1);
    receive(&f,&fr,101,101);EQ(f.received_count,1);EQ(f.s.rx_origin[0].last_valid_frame.ms,100);
    le32(fr.bytes+4,0);receive(&f,&fr,102,102);EQ(f.received_count,1);EQ(f.s.rx_origin[1].has_record,0);
    fr=rx_frame(2,6,10);receive(&f,&fr,103,103);EQ(f.received_count,1);EQ(f.s.rx_origin[0].last_valid_frame.ms,100);
    EQ(CanSvc_BindSource(&f.s,CAN_REAR,1,2),CAN_OK);EQ(f.s.rx_origin[0].last.original.fields.rear.update_sequence,7);
    EQ(CanSvc_BindSource(&f.s,CAN_REAR,2,2),CAN_ORIGIN_CONFLICT);EQ(CanSvc_BindSource(&f.s,CAN_REAR,1,1),CAN_ORDER_REJECT);
}
static void d13(void)
{
    Fixture f;CanFrame fr=rx_frame(UINT32_MAX-1,UINT32_MAX-1,10);CanRawMessage r;setup_at(&f,UINT32_C(0xFFFFFFF0));
    receive(&f,&fr,UINT32_C(0xFFFFFFF0),UINT32_C(0xFFFFFFF0));fr=rx_frame(1,1,10);receive(&f,&fr,0x10,0x10);
    EQ(f.received_count,2);EQ(f.s.rx_origin[0].last.new_observation,1);
    fr=rx_frame(UINT32_C(0x80000001),1,0);receive(&f,&fr,0x11,0x11);EQ(f.received_count,2);
    r=rear(7,10);EQ(CanSvc_Submit(&f.s,&r,stamp(0x11),stamp(0x11)),CAN_ACCEPTED);EQ(CanSvc_Pump(&f.s,stamp(0x12),8),CAN_ACCEPTED);
    EQ(f.ps[0].frame.bytes[12],11);
    {CanClockStamp bad={2,0x13,1};EQ(CanSvc_Submit(&f.s,&r,bad,bad),CAN_CLOCK_UNKNOWN);EQ(f.s.tx_origin[0].age_anchor.valid,0);}
    f.p.config.horizon_proven=0;EQ(CanLinkPort_TryTx(&f.p,&fr,(CanTxToken){1,1,1},stamp(0x13)),CAN_CLOCK_UNKNOWN);
    setup(&f);r=rear(7,10);EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(100)),CAN_ACCEPTED);
    EQ(CanSvc_Pump(&f.s,stamp(1101),8),CAN_CLOCK_UNKNOWN);EQ(f.s.pending[0].occupied,1);EQ(f.s.pending[0].age_state,CAN_AGE_UNCERTAIN);EQ(f.m.admits,0);
    EQ(CanSvc_Submit(&f.s,&r,stamp(1102),stamp(1102)),CAN_CLOCK_UNKNOWN);
    r.fields.rear.update_sequence=8;EQ(CanSvc_Submit(&f.s,&r,stamp(1102),stamp(1102)),CAN_ACCEPTED);
    EQ(f.s.pending[0].age_at_anchor,10);EQ(f.s.pending[0].original.fields.rear.update_sequence,8);
    /* A valid-clock diagnostic watchdog fences unresolved accepted bytes once. */
    setup(&f);f.t.config.watchdog_ms=100;{CanTxToken t=transmit(&f);CanTxOutcome o;
        EQ(CanTransport_Pump(&f.t,stamp(200),8),CAN_OK);EQ(f.p.health.generation,2);EQ(f.ts[0].deadline_fenced,1);
        EQ(CanTransport_Pump(&f.t,stamp(201),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&o),CAN_OK);
        EQ(o.result,CAN_TX_UNKNOWN);EQ(can_token_equal(o.token,t),1);EQ(f.ps[0].state,CAN_PORT_UNKNOWN_PINNED);}
    /* An already captured conclusive result is retained even beyond that watchdog. */
    setup(&f);f.t.config.watchdog_ms=100;{CanTxToken t=transmit(&f);CanTxOutcome o;
        EQ(MockCan_Terminal(&f.m,&f.p,t,CAN_TX_COMPLETE,stamp(201)),CAN_OK);
        EQ(CanTransport_Pump(&f.t,stamp(201),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&o),CAN_OK);
        EQ(o.result,CAN_TX_COMPLETE);EQ(f.p.health.generation,1);}
}
static void d14(void)
{
    Fixture f;CanRawMessage r=rear(7,65534);CanFrame fr=rx_frame(1,7,65535);setup(&f);
    EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(102)),CAN_ACCEPTED);EQ(f.s.pending[0].age_at_anchor,65535);
    EQ(CanSvc_Pump(&f.s,stamp(103),8),CAN_ACCEPTED);EQ(f.ps[0].frame.bytes[12],255);EQ(f.ps[0].frame.bytes[13],255);
    receive(&f,&fr,103,104);EQ(f.s.rx_origin[0].last.age_state,CAN_AGE_SATURATED);EQ(f.s.rx_origin[0].last.effective_age_ms,65535);
    {CanClockStamp bad={1,105,0};r.fields.rear.update_sequence=8;EQ(CanSvc_Submit(&f.s,&r,bad,bad),CAN_CLOCK_UNKNOWN);}
    setup(&f);fr=rx_frame(1,7,10);
    EQ(MockCan_Rx(&f.p,&fr,(CanClockStamp){1,100,0},1,0),CAN_ACCEPTED);cycle(&f,100);
    EQ(f.received.age_state,CAN_AGE_UNCERTAIN);EQ(f.s.rx_origin[0].last_valid_frame.valid,0);
}
static void d15(void)
{
    Fixture f;CanRawMessage r=rear(7,10);CanFrame fr=rx_frame(1,7,10);setup(&f);
    EQ(CanSvc_Submit(&f.s,&r,stamp(100),stamp(100)),CAN_ACCEPTED);r.fields.rear.source_age_ms=0;
    EQ(CanSvc_Submit(&f.s,&r,stamp(200),stamp(200)),CAN_ACCEPTED);EQ(f.s.pending[0].age_at_anchor,110);
    receive(&f,&fr,200,200);fr=rx_frame(2,7,0);receive(&f,&fr,300,300);
    EQ(f.s.rx_origin[0].last.effective_age_ms,110);EQ(f.s.rx_origin[0].last.new_observation,0);EQ(f.s.rx_origin[0].last_new_update.ms,200);
    EQ(f.s.rx_origin[0].last.original.fields.rear.source_age_ms,0);
    r.fields.rear.rear_distance_deci_cm=5;EQ(CanSvc_Submit(&f.s,&r,stamp(300),stamp(300)),CAN_ORIGIN_CONFLICT);
    fr=rx_frame(3,7,0);fr.bytes[14]=3;receive(&f,&fr,301,301);EQ(f.received_count,2);
}
static void d16(void)
{
    Fixture f;CanFrame fr=rx_frame(1,7,10);CanTxToken token;CanTxOutcome out;CanRxRecord rx;unsigned i;setup(&f);token=transmit(&f);
    for(i=0;i<4;i++) {fr.bytes[14]=(uint8_t)i;EQ(MockCan_Rx(&f.p,&fr,stamp(100),1,0),CAN_ACCEPTED);}
    fr.bytes[14]=99;EQ(MockCan_Rx(&f.p,&fr,stamp(100),1,0),CAN_NO_CAPACITY);EQ(f.p.health.rx_dropped,1);
    EQ(MockCan_Terminal(&f.m,&f.p,token,CAN_TX_COMPLETE,stamp(100)),CAN_OK);
    EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_OK);EQ(out.result,CAN_TX_COMPLETE);
    EQ(f.t.rx_used,4);EQ(CanTransport_TakeRx(&f.t,&rx),CAN_OK);EQ(rx.frame.bytes[14],0);
    /* Fill the remaining T capacity, then independently overflow RT. */
    EQ(MockCan_Rx(&f.p,&fr,stamp(100),1,0),CAN_ACCEPTED);EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);
    EQ(MockCan_Rx(&f.p,&fr,stamp(100),1,0),CAN_ACCEPTED);EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);
    EQ(f.t.rx_dropped,1);EQ(f.t.rx_used,4);
    EQ(MockCan_Rx(&f.p,&fr,stamp(100),1,17),CAN_ACCEPTED);EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);
    EQ(f.t.last_rejected_rx.controller_error,17);EQ(f.received_count,0);
}
static void d17(void)
{
    Fixture a,b;CanFrame ra=rx_frame(1,7,10),rb=ra;unsigned i;setup(&a);setup(&b);
    for(i=0;i<3;i++) {
        ra=rx_frame(i+1,7,(uint16_t)i);rb=ra;
        EQ(MockCan_Rx(&a.p,&ra,stamp(100+i),1,0),CAN_ACCEPTED);EQ(MockCan_Rx(&b.p,&rb,stamp(100+i),1,0),CAN_ACCEPTED);
        cycle(&a,100+i); /* superloop */
        cycle(&b,100+i); /* worker wake; identical owner order, no wake-count-derived records */
    }
    EQ(a.received_count,3);EQ(b.received_count,3);EQ(a.s.rx_origin[0].last.effective_age_ms,b.s.rx_origin[0].last.effective_age_ms);
    EQ(a.p.rx_count,b.p.rx_count);EQ(a.t.rx_used,b.t.rx_used);EQ(a.s.rx_origin[0].last_new_update.ms,b.s.rx_origin[0].last_new_update.ms);
    EQ(CanSvc_Pump(&a.s,stamp(104),2),CAN_INVALID_ARG);EQ(CanTransport_Pump(&a.t,stamp(104),0),CAN_INVALID_ARG);
}
static void d18(void)
{
    Fixture f;CanFrame fr=rx_frame(1,7,10);setup(&f);fr.bytes[19]=1;receive(&f,&fr,100,100);
    EQ(f.received_count,0);EQ(f.s.last_codec_result.i1,I1_ERR_PADDING);EQ(f.s.last_rejected_rx.frame.bytes[19],1);
    fr.bytes[19]=0;fr.bytes[16]=0;fr.bytes[17]=1;receive(&f,&fr,101,101);
    EQ(f.received_count,1);EQ(f.received.original.fields.rear.validity,0);EQ(f.received.original.fields.rear.quality_reason,1);
    fr.bytes[19]=1;receive(&f,&fr,102,102);EQ(f.s.rx_origin[0].last_valid_frame.ms,101);EQ(f.received_count,1);
}
static void during_admit(void *ctx,CanLinkPort *p)
{
    Fixture *f=ctx;CanPortEvent event;CanTxOutcome outcome;
    EQ(CanLinkPort_PollEvent(p,&event,stamp(100)),CAN_REENTRANT);
    EQ(CanLinkPort_Reset(p,CAN_RESET_REQUESTED,stamp(100)),CAN_REENTRANT);
    EQ(CanTransport_TakeOutcome(&f->t,&outcome),CAN_REENTRANT);
}
static void d19(void)
{
    Fixture f;CanTxToken token;CanTxOutcome out;CanFrame fr=rx_frame(1,7,10);setup(&f);
    f.m.synchronous_terminal=1;f.m.during_admit=during_admit;f.m.callback_context=&f;token=transmit(&f);
    EQ(f.ps[0].state,CAN_PORT_TERMINAL_READY);EQ(f.ps[0].terminal_result,CAN_TX_COMPLETE);
    EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_OK);EQ(out.result,CAN_TX_COMPLETE);
    EQ(MockCan_Terminal(&f.m,&f.p,token,CAN_TX_COMPLETE,stamp(100)),CAN_ORDER_REJECT);
    f.reentrant_sink=1;receive(&f,&fr,100,100);EQ(f.received_count,1);EQ(f.s.guard,CAN_SVC_IDLE);EQ(f.t.owner_guard,0);EQ(f.p.owner_active,0);
    /* A synchronous conclusive terminal survives contradictory MAY_START observation. */
    f.m.during_admit=NULL;f.m.next_admission=(CanPortAdmissionObservation){CAN_BUSY,0};token=transmit(&f);
    EQ(f.ps[0].terminal_result,CAN_TX_COMPLETE);EQ(f.ps[0].release_safe,1);EQ(f.p.health.ready,0);
    EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_OK);EQ(out.result,CAN_TX_COMPLETE);
    setup(&f);token=transmit(&f);MockCan_Fault(&f.p,0,1);
    EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_OK);EQ(out.result,CAN_TX_UNKNOWN);
    EQ(f.ps[0].state,CAN_PORT_UNKNOWN_PINNED);EQ(f.m.references[0]!=NULL,1);
    /* Port-unit terminal with no corresponding Transport reservation is diagnostic only. */
    setup(&f);token=(CanTxToken){1,1,77};EQ(CanLinkPort_TryTx(&f.p,&fr,token,stamp(100)),CAN_ACCEPTED);
    EQ(MockCan_Terminal(&f.m,&f.p,token,CAN_TX_COMPLETE,stamp(100)),CAN_OK);EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_OK);
    EQ(f.t.late_event_count,1);EQ(CanTransport_TakeOutcome(&f.t,&out),CAN_EMPTY);
    /* Whitebox structural-corruption fixture, never claimed as a normal owner path. */
    f.p.initialized=0;EQ(CanTransport_Pump(&f.t,stamp(100),8),CAN_NOT_READY);EQ(f.t.owner_guard,0);
    (void)token;
}
static void d20(void)
{
    Fixture f;CanFrame fr=rx_frame(1,7,10);setup(&f);le16(fr.bytes+14,65535);fr.bytes[18]=1;receive(&f,&fr,100,100);
    EQ(f.received.original.fields.rear.rear_distance_deci_cm,65535);EQ(f.received.original.fields.rear.proximity_status,1);
    fr=rx_frame(2,8,10);le16(fr.bytes+14,65535);fr.bytes[16]=0;fr.bytes[17]=1;fr.bytes[18]=5;receive(&f,&fr,101,101);
    EQ(f.received.original.fields.rear.validity,0);EQ(f.received.original.fields.rear.proximity_status,5);EQ(f.received.original.fields.rear.rear_distance_deci_cm,65535);
}
static void d21(void)
{
    Fixture f;CanFrame fr={0};CanRawMessage r=permission();setup(&f);
    fr.can_id=0x80;fr.fdf=fr.brs=1;fr.dlc=11;fr.length=20;memcpy(fr.bytes,permission_oracle,20);
    EQ(MockCan_Rx(&f.p,&fr,stamp(100),1,0),CAN_ACCEPTED);cycle(&f,300);
    EQ(f.received.original.fields.permission.vehicle_power_permission,255);EQ(f.received.original.fields.permission.operation_allowed,0);
    EQ(f.received.effective_age_ms,855);EQ(f.received_count,1);
    f.s.config.tx_mask=1;EQ(CanSvc_Submit(&f.s,&r,stamp(300),stamp(300)),CAN_NOT_READY);
    f.s.config.rx_mask=1;le32(fr.bytes+4,2);EQ(MockCan_Rx(&f.p,&fr,stamp(301),1,0),CAN_ACCEPTED);cycle(&f,301);EQ(f.received_count,1);
}
static void d22(void)
{
    Fixture a,b;CanRawMessage r=rear(7,10),decoded;CanFrame fr=rx_frame(1,7,10);CanSvcRxContext snapshot;
    CanTransport tmp={0};CanTransportConfig cfg;CanTransportStorage alias;
    setup(&a);setup(&b);EQ(CanSvc_Submit(&a.s,&r,stamp(100),stamp(100)),CAN_ACCEPTED);EQ(b.s.pending[0].occupied,0);
    EQ(CanSvc_Pump(&a.s,stamp(100),8),CAN_ACCEPTED);EQ(b.s.tx_next[0],1);EQ(b.m.admits,0);
    MockCan_Fault(&a.p,1,0);EQ(CanTransport_Pump(&a.t,stamp(100),8),CAN_OK);EQ(b.p.health.ready,1);EQ(b.p.health.generation,1);
    EQ(CanCodec_Decode(&fr,&decoded).status,CAN_OK);snapshot=b.s.rx_origin[0];decoded.fields.rear.update_sequence=123;
    EQ(CanSvc_OnDecoded(&b.s,&decoded,&(CanRxRecord){fr,stamp(100),1,0},stamp(100)),CAN_INVALID_ARG);
    BYTES(&b.s.rx_origin[0],&snapshot,sizeof(snapshot));EQ(b.received_count,0);
    cfg=b.tc;alias=b.tv;alias.rx_records=(CanRxRecord *)alias.tx_slots;
    EQ(CanTransport_Init(&tmp,&cfg,&alias,&b.p),CAN_INVALID_ARG);EQ(tmp.initialized,0);
    cfg=b.tc;cfg.generation_fresh_for_pair=0;EQ(CanTransport_Init(&tmp,&cfg,&b.tv,&b.p),CAN_NOT_READY);
    EQ(CanSvc_Init(&b.s,&b.sc,&b.t),CAN_NOT_READY);EQ(CanLinkPort_Init(&b.p,&b.pc,&b.pv,stamp(100)),CAN_NOT_READY);
    {CanService uninit={0};CanSvcConfig bad=b.sc;bad.tx_mask=4;EQ(CanSvc_Init(&uninit,&bad,&b.t),CAN_INVALID_ARG);EQ(uninit.initialized,0);}
    {CanService uninit={0};CanTransport empty={0};EQ(CanSvc_Init(&uninit,&b.sc,&empty),CAN_NOT_READY);EQ(uninit.initialized,0);}
}
int main(void)
{
    void (*tests[22])(void)={d01,d02,d03,d04,d05,d06,d07,d08,d09,d10,d11,d12,d13,d14,d15,d16,d17,d18,d19,d20,d21,d22};
    unsigned n;size_t i;char id[16];
    for(n=0;n<22;n++) {
        snprintf(id,sizeof(id),"CAN-D%02u",n+1);case_id=id;checks=failed=0;memset(case_hits,0,sizeof(case_hits));
        tests[n]();total_checks+=checks;total_failed+=failed;
        printf("{\"kind\":\"case\",\"id\":\"%s\",\"assertions\":%u,\"failed\":%u,\"status\":\"%s\",\"branches\":[",id,checks,failed,failed?"FAIL":"PASS");
        {unsigned comma=0;for(i=0;i<123;i++) if(case_hits[i]) {if(comma++) putchar(',');json_string(branch_ids[i]);}}
        printf("]}\n");
    }
    printf("{\"kind\":\"summary\",\"cases\":22,\"assertions\":%u,\"failed\":%u,\"memory\":{\"service\":%llu,\"transport\":%llu,\"port\":%llu}}\n",
        total_checks,total_failed,(unsigned long long)sizeof(CanService),(unsigned long long)sizeof(CanTransport),(unsigned long long)sizeof(CanLinkPort));
    return total_failed?1:0;
}
