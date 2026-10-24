#include "network/net_upper_control.h"
#include "network/net_upper_query.h"
#include "network/net_hal_adapter.h"
#include "../../src/l03_message_service/m04_message_service/net_upper_private.h"
#include <stdio.h>
#include <string.h>
/* Independent profile execution: physical Port and external Owner only.
 * Frozen literal CAN fixtures copied without their old Network test harness.
 * Fixture source SHA256: 7c87351e8730e753176a32f195f3b2ebee2b6fe1a52ba1721a833ea9558db5cc */
#define CHECK(x) do { if(!(x)) { printf("FAIL %s:%d %s\n",case_name,__LINE__,#x); return 1; } } while(0)
static const char *case_name;
/* The capacity cases fill the profile's own queues (net_upper.h), whatever their size. */
#define PROFILE_TX NET_UPPER_TX_CAPACITY
#define PROFILE_RX NET_UPPER_RX_CAPACITY
#define PROFILE_PORT_SLOTS 32u
typedef char profile_port_holds_full_tx_queue[(PROFILE_PORT_SLOTS >= PROFILE_TX + 1u) ? 1 : -1];
typedef struct { uint64_t now; size_t count; const NetUpperPacket *held[PROFILE_PORT_SLOTS]; NetTxKey keys[PROFILE_PORT_SLOTS]; } ProfilePort;
typedef struct { unsigned calls; NetUpperCanMessage can; NetUpperRear rear; NetUpperUartDelivery uart; NetUpperDelivery metadata; } ProfileOwner;
static NetUpper service;
static unsigned char before[sizeof(NetUpper)];
static ProfilePort physical;
static ProfileOwner owner;
static NetUpperConfig input_config;
static NetStatus port_now(void *p,uint64_t *out) { *out=((ProfilePort *)p)->now;return NET_OK; }
static NetStatus port_submit(void *p,const NetUpperPacket *packet,NetTxKey key)
{ ProfilePort *f=p;if(f->count>=PROFILE_PORT_SLOTS)return NET_NO_CAPACITY;f->held[f->count]=packet;f->keys[f->count++]=key;return NET_OK; }
static NetStatus port_poll(void *p,NetUpperEvent *out) { (void)p;(void)out;return NET_NO_WORK; }
static const NetUpperPortOps ops={port_now,port_submit,port_poll};
static NetStatus can_owner(void *p,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{ ProfileOwner *o=p;++o->calls;o->can=*m;o->metadata=*d;return NET_OK; }
static NetStatus rear_owner(void *p,const NetUpperRear *m,const NetUpperDelivery *d)
{ ProfileOwner *o=p;++o->calls;o->rear=*m;o->metadata=*d;return NET_OK; }
static NetStatus uart_owner(void *p,const NetUpperUartDelivery *m,const NetUpperDelivery *d)
{ ProfileOwner *o=p;++o->calls;o->uart=*m;o->metadata=*d;return NET_OK; }
static uint32_t node(void) { return NET_UPPER_PROFILE_NODE==0u?NET_WIRE_NODE_DOMAIN:NET_UPPER_PROFILE_NODE; }
static bool link_enabled(NetWireLink link)
{
    if(link==NET_WIRE_CAN)return true;
    if(NETWORK_UPPER_NODE_PROFILE==NET_UPPER_PROFILE_HOST)return true;
    return (NETWORK_UPPER_NODE_PROFILE==NET_UPPER_PROFILE_CIS&&link==NET_WIRE_UART_C)||
        (NETWORK_UPPER_NODE_PROFILE==NET_UPPER_PROFILE_DOMAIN&&link==NET_WIRE_UART_M);
}
static void prepare(void)
{
    size_t i;memset(&service,0xa5,sizeof(service));memset(&physical,0,sizeof(physical));memset(&owner,0,sizeof(owner));memset(&input_config,0,sizeof(input_config));
    physical.now=100u;input_config.node=node();input_config.local_epoch=9u;input_config.profile=NET_WIRE_CANDIDATE_ONLY;
    input_config.port.ops=&ops;input_config.port.context=&physical;input_config.owner.context=&owner;
    input_config.owner.can=can_owner;input_config.owner.rear=rear_owner;input_config.owner.uart=uart_owner;
    for(i=0u;i<3u;++i)input_config.port_epoch[i]=1u;
    for(i=0u;i<48u;++i){input_config.rx_policy[i].bound=true;input_config.rx_policy[i].maximum_age_ms=1000u+i;input_config.rx_policy[i].expires_at_equality=true;}
    input_config.rx_per_field_age[20u]=true; /* Explicit partial VSS warning lane policy. */
    for(i=0u;i<2u;++i){input_config.uart_policy[i].candidate_time_policy_selected=true;input_config.uart_policy[i].assembly_limit_ms=1000u;input_config.uart_policy[i].expires_at_equality=true;}
}
static int init(void) { prepare();CHECK(net_upper_init(&service,&input_config)==NET_OK);return 0; }
static NetUpperCapture captured(void)
{ NetUpperCapture c={0};c.port_epoch=1u;c.captured_at_ms=physical.now;c.timestamp_known=true;return c; }
typedef struct { const char *name; uint16_t id; uint8_t dlc; size_t length; uint32_t sender,receiver; bool hold; uint16_t age; uint8_t initial[64],dispatched[64]; } Fixture;
static const Fixture fixtures[] = {
 {"domain_permission",0x080u,11u,20u,NET_WIRE_NODE_DOMAIN,NET_WIRE_NODE_BCM,false,0u,{0x33u,0x00u,0x00u,0x00u,0x01u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x07u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u},{0x33u,0x00u,0x00u,0x00u,0x01u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x07u,0x00u,0x00u,0x00u,0x03u,0x00u,0x00u}},
 {"bcm_door_command",0x100u,12u,24u,NET_WIRE_NODE_DOMAIN,NET_WIRE_NODE_BCM,false,0u,{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x00u,0x00u,0x8cu,0x7bu,0x01u,0x05u,0x00u,0x00u},{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x03u,0x00u,0x8cu,0x7bu,0x01u,0x05u,0x00u,0x00u}},
 {"bcm_climate_command",0x110u,12u,24u,NET_WIRE_NODE_DOMAIN,NET_WIRE_NODE_BCM,false,0u,{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x00u,0x00u,0x8cu,0x7bu,0x03u,0x02u,0x64u,0x00u},{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x03u,0x00u,0x8cu,0x7bu,0x03u,0x02u,0x64u,0x00u}},
 {"bcm_light_command",0x120u,13u,32u,NET_WIRE_NODE_DOMAIN,NET_WIRE_NODE_BCM,false,0u,{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x00u,0x00u,0x8cu,0x7bu,0x02u,0x25u,0x12u,0x34u,0x56u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u},{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x03u,0x00u,0x8cu,0x7bu,0x02u,0x25u,0x12u,0x34u,0x56u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u}},
 {"window_command",0x090u,12u,24u,NET_WIRE_NODE_DOMAIN,NET_WIRE_NODE_WINDOW,true,0u,{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x00u,0x00u,0x78u,0x56u,0x05u,0x00u,0x25u,0x04u},{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x03u,0x00u,0x78u,0x56u,0x05u,0x00u,0x25u,0x04u}},
 {"bcm_door_state",0x200u,11u,20u,NET_WIRE_NODE_BCM,NET_WIRE_NODE_DOMAIN,false,0u,{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xffu,0x01u,0x02u,0x03u,0x01u,0x02u,0xaau,0xbbu,0xccu,0xddu,0x00u,0x00u},{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xffu,0x01u,0x02u,0x03u,0x01u,0x02u,0xaau,0xbbu,0xccu,0xddu,0x03u,0x00u}},
 {"bcm_climate_state",0x210u,12u,24u,NET_WIRE_NODE_BCM,NET_WIRE_NODE_DOMAIN,false,0u,{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x02u,0xffu,0x01u,0x25u,0xffu,0x00u,0x03u,0x01u,0x02u,0x03u,0xaau,0xbbu,0xccu,0xddu,0x00u,0x00u},{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x02u,0xffu,0x01u,0x25u,0xffu,0x00u,0x03u,0x01u,0x02u,0x03u,0xaau,0xbbu,0xccu,0xddu,0x03u,0x00u}},
 {"bcm_light_state",0x220u,13u,32u,NET_WIRE_NODE_BCM,NET_WIRE_NODE_DOMAIN,false,0u,{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x03u,0x49u,0x12u,0x34u,0x56u,0xffu,0x00u,0x01u,0x02u,0x03u,0x03u,0xaau,0xbbu,0xccu,0xddu,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u},{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x03u,0x49u,0x12u,0x34u,0x56u,0xffu,0x00u,0x01u,0x02u,0x03u,0x03u,0xaau,0xbbu,0xccu,0xddu,0x03u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u}},
 {"bcm_result",0x140u,13u,32u,NET_WIRE_NODE_BCM,NET_WIRE_NODE_DOMAIN,false,0u,{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x01u,0x02u,0x09u,0x00u,0x03u,0xb4u,0xc3u,0xd2u,0xe1u,0x00u,0x00u,0x01u,0x01u,0x00u,0x00u,0x00u},{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x01u,0x02u,0x09u,0x00u,0x03u,0xb4u,0xc3u,0xd2u,0xe1u,0x03u,0x00u,0x01u,0x01u,0x00u,0x00u,0x00u}},
 {"bcm_event",0x141u,13u,32u,NET_WIRE_NODE_BCM,NET_WIRE_NODE_DOMAIN,false,0u,{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x02u,0x31u,0x32u,0x33u,0x34u,0x00u,0x00u,0x02u,0x71u,0x72u,0x73u,0x74u,0x81u,0x82u,0x83u,0x84u,0x02u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u},{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x02u,0x31u,0x32u,0x33u,0x34u,0x03u,0x00u,0x02u,0x71u,0x72u,0x73u,0x74u,0x81u,0x82u,0x83u,0x84u,0x02u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u}},
 {"bcm_status",0x230u,12u,24u,NET_WIRE_NODE_BCM,NET_WIRE_NODE_DOMAIN,false,0u,{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x03u,0xa5u,0x00u,0x03u,0x08u,0x00u,0x07u,0x01u,0xd4u,0xc3u,0xb2u,0xa1u,0x00u,0x00u,0x00u,0x00u},{0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x03u,0xa5u,0x00u,0x03u,0x08u,0x00u,0x07u,0x01u,0xd4u,0xc3u,0xb2u,0xa1u,0x03u,0x00u,0x00u,0x00u}},
 {"cis_environment",0x300u,14u,48u,NET_WIRE_NODE_CIS,NET_WIRE_NODE_DOMAIN,false,0u,{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x60u,0xf0u,0x55u,0x44u,0x33u,0x22u,0x00u,0x00u,0x01u,0x00u,0x85u,0x1au,0x66u,0x55u,0x44u,0x33u,0x00u,0x00u,0x01u,0x00u,0x40u,0xe2u,0x01u,0x00u,0x77u,0x66u,0x55u,0x44u,0x00u,0x00u,0x01u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u},{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x60u,0xf0u,0x55u,0x44u,0x33u,0x22u,0x03u,0x00u,0x01u,0x00u,0x85u,0x1au,0x66u,0x55u,0x44u,0x33u,0x03u,0x00u,0x01u,0x00u,0x40u,0xe2u,0x01u,0x00u,0x77u,0x66u,0x55u,0x44u,0x03u,0x00u,0x01u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u}},
 {"cis_occupant",0x310u,13u,32u,NET_WIRE_NODE_CIS,NET_WIRE_NODE_DOMAIN,false,0u,{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x00u,0x00u,0x01u,0x01u,0x00u,0x03u,0x01u,0x00u,0x02u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u},{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x03u,0x00u,0x01u,0x01u,0x00u,0x03u,0x01u,0x00u,0x02u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u}},
 {"cis_rear",0x0a0u,11u,20u,NET_WIRE_NODE_CIS,NET_WIRE_NODE_DOMAIN,false,0u,{0x29u,0x00u,0x00u,0x00u,0x01u,0x00u,0x00u,0x00u,0x4du,0x00u,0x00u,0x00u,0x00u,0x00u,0x7bu,0x00u,0x01u,0x00u,0x00u,0x00u},{0x29u,0x00u,0x00u,0x00u,0x01u,0x00u,0x00u,0x00u,0x4du,0x00u,0x00u,0x00u,0x03u,0x00u,0x7bu,0x00u,0x01u,0x00u,0x00u,0x00u}},
 {"cis_status",0x320u,14u,48u,NET_WIRE_NODE_CIS,NET_WIRE_NODE_DOMAIN,false,0u,{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x03u,0x02u,0x01u,0x04u,0x03u,0x00u,0x02u,0x01u,0xa5u,0x01u,0x09u,0x00u,0x01u,0x08u,0x00u,0x88u,0x77u,0x66u,0x55u,0x2du,0xccu,0xbbu,0xaau,0x99u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u},{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x03u,0x02u,0x01u,0x04u,0x03u,0x00u,0x02u,0x01u,0xa5u,0x01u,0x09u,0x00u,0x01u,0x08u,0x00u,0x88u,0x77u,0x66u,0x55u,0x2du,0xccu,0xbbu,0xaau,0x99u,0x03u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u}},
 {"window_state",0x0b0u,12u,24u,NET_WIRE_NODE_WINDOW,NET_WIRE_NODE_DOMAIN,true,0u,{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x02u,0x02u,0x25u,0x01u,0x00u,0x00u,0x01u,0x01u,0xd4u,0xc3u,0xb2u,0xa1u,0x00u,0x00u,0x00u,0x00u},{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x02u,0x02u,0x25u,0x01u,0x00u,0x00u,0x01u,0x01u,0xd4u,0xc3u,0xb2u,0xa1u,0x03u,0x00u,0x00u,0x00u}},
 {"window_event",0x091u,13u,32u,NET_WIRE_NODE_WINDOW,NET_WIRE_NODE_DOMAIN,true,0u,{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x03u,0x00u,0xd4u,0xc3u,0xb2u,0xa1u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x05u,0x00u,0x00u,0x02u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u},{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x03u,0x00u,0xd4u,0xc3u,0xb2u,0xa1u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x05u,0x03u,0x00u,0x02u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u}},
 {"window_result",0x092u,11u,20u,NET_WIRE_NODE_WINDOW,NET_WIRE_NODE_DOMAIN,true,0u,{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x00u,0x05u,0x19u,0x00u},{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x00u,0x05u,0x19u,0x00u}},
 {"window_fault",0x330u,12u,24u,NET_WIRE_NODE_WINDOW,NET_WIRE_NODE_DOMAIN,true,0u,{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x02u,0xffu,0x00u,0x00u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x00u,0x00u,0x00u,0x00u},{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x02u,0xffu,0x00u,0x00u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x03u,0x00u,0x00u,0x00u}},
 {"vss_event",0x160u,12u,24u,NET_WIRE_NODE_DOMAIN,NET_WIRE_NODE_VSS,false,0u,{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x05u,0x88u,0x77u,0x66u,0x55u,0x00u,0x00u,0x78u,0x56u,0x01u,0x01u,0xccu,0xbbu,0xaau,0x99u,0x00u},{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x05u,0x88u,0x77u,0x66u,0x55u,0x03u,0x00u,0x78u,0x56u,0x01u,0x01u,0xccu,0xbbu,0xaau,0x99u,0x00u}},
 {"vss_warning_state",0x0c0u,15u,64u,NET_WIRE_NODE_DOMAIN,NET_WIRE_NODE_VSS,false,65535u,{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0xffu,0xffu,0x00u,0x03u,0x06u,0x02u,0x55u,0x44u,0x33u,0x22u,0x66u,0x55u,0x44u,0x33u,0x00u,0x00u,0x01u,0x00u,0x00u,0x01u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x00u,0x00u,0x02u,0x00u,0x00u,0x00u,0x01u,0x01u,0x00u,0x00u,0xaau,0x99u,0x88u,0x77u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u},{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0xffu,0xffu,0x00u,0x03u,0x06u,0x02u,0x55u,0x44u,0x33u,0x22u,0x66u,0x55u,0x44u,0x33u,0x03u,0x00u,0x01u,0x00u,0x00u,0x01u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x03u,0x00u,0x02u,0x00u,0x00u,0x00u,0x01u,0x01u,0x00u,0x00u,0xaau,0x99u,0x88u,0x77u,0x03u,0x00u,0x00u,0x00u,0x00u,0x00u}},
 {"vss_status",0x340u,13u,32u,NET_WIRE_NODE_VSS,NET_WIRE_NODE_DOMAIN,false,0u,{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x02u,0x01u,0x01u,0x00u,0x00u,0x03u,0x00u,0x88u,0x77u,0x66u,0x55u,0x00u,0x00u,0x00u,0xccu,0xbbu,0xaau,0x99u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u},{0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x02u,0x01u,0x01u,0x00u,0x00u,0x03u,0x00u,0x88u,0x77u,0x66u,0x55u,0x03u,0x00u,0x00u,0xccu,0xbbu,0xaau,0x99u,0x03u,0x00u,0x00u,0x00u,0x00u,0x00u}},
};
static NetWireCanEnvelope envelope(size_t i)
{
    NetWireCanEnvelope f={0};f.standard_id=fixtures[i].id;f.dlc=fixtures[i].dlc;f.length=fixtures[i].length;f.fdf=true;f.brs=true;memcpy(f.bytes,fixtures[i].initial,f.length);return f;
}
static int message(size_t i,NetUpperCanMessage *m)
{ NetWireCanEnvelope f=envelope(i);CHECK(net_upper_can_decode(NET_WIRE_CANDIDATE_ONLY,&f,m)==NET_WIRE_OK);return 0; }
static NetStatus receive_fixture(size_t i)
{ NetUpperEvent e={0};NetStatus r;e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture=captured();e.can=envelope(i);r=net_upper_capture(&service,&e);return r==NET_OK?net_upper_step(&service,1u):r; }
static size_t outbound(void)
{ if(node()==NET_WIRE_NODE_CIS)return 13u;if(node()==NET_WIRE_NODE_BCM)return 5u;if(node()==NET_WIRE_NODE_VSS)return 21u;if(node()==NET_WIRE_NODE_WINDOW)return 15u;return 0u; }
static int enqueue(size_t i,NetTxKey *key)
{
    NetUpperCanMessage m;NetSourceStamp source={1u,100u,0u};NetAgePolicy p={true,100000u,true};CHECK(message(i,&m)==0);
    source.original_age_ms=fixtures[i].age;CHECK(net_upper_submit_can(&service,&m,&source,&p,key)==NET_OK);return 0;
}
static int complete(NetTxKey key,bool release_only)
{
    NetUpperEvent e={0};e.kind=NET_UPPER_TX_EVENT;e.link=NET_WIRE_CAN;e.capture=captured();e.tx.key=key;e.tx.release_observed=true;
    if(!release_only){e.tx.admission_observed=true;e.tx.accepted=true;e.tx.outcome=NET_TX_SUCCEEDED;}
    CHECK(net_upper_capture(&service,&e)==NET_OK&&net_upper_step(&service,1u)==NET_OK);return 0;
}
static int storage(void)
{
    size_t i,owned=0u,fanout=0u;bool visited[48]={false};CHECK(init()==0);
    CHECK(net_wire_registry_count()==48u&&PROFILE_TX>=1u&&PROFILE_RX>=1u);
    CHECK(sizeof(service.tx)/sizeof(service.tx[0])==PROFILE_TX&&sizeof(service.rx)/sizeof(service.rx[0])==PROFILE_RX);
    for(i=0u;i<48u;++i){
        const NetWireRegistryEntry *r=net_wire_registry_at(i);size_t rank=net_upper_admission_index(i);uint32_t bits=r->receivers;
        bool selected=NETWORK_UPPER_NODE_PROFILE==NET_UPPER_PROFILE_HOST||((r->receivers&node())!=0u&&link_enabled(r->link));
        while(bits!=0u){fanout+=(bits&1u)!=0u;bits>>=1u;}
        if(selected){
            CHECK(rank<NET_UPPER_ADMISSION_CAPACITY&&!visited[rank]);visited[rank]=true;++owned;
            CHECK(net_upper_admission_at(&service,i)==&service.admission[rank]);
            CHECK(service.config.rx_policy[rank].maximum_age_ms==1000u+i);
            CHECK(service.config.rx_policy[rank].bound&&service.config.rx_policy[rank].expires_at_equality);
            CHECK(service.config.rx_per_field_age[rank]==input_config.rx_per_field_age[i]);
        }else CHECK(rank==SIZE_MAX&&net_upper_admission_at(&service,i)==NULL);
    }
    CHECK(fanout==51u&&owned==NET_UPPER_ADMISSION_CAPACITY);
    CHECK(net_upper_admission_index(48u)==SIZE_MAX&&net_upper_admission_at(&service,SIZE_MAX)==NULL);
    if(NETWORK_UPPER_NODE_PROFILE==NET_UPPER_PROFILE_CIS){CHECK(net_upper_receiver(&service,0u)==NULL&&net_upper_receiver(&service,1u)!=NULL);}
    if(NETWORK_UPPER_NODE_PROFILE==NET_UPPER_PROFILE_DOMAIN){CHECK(net_upper_receiver(&service,0u)!=NULL&&net_upper_receiver(&service,1u)==NULL);}
    if(NET_UPPER_UART_RECEIVER_CAPACITY==0u)CHECK(net_upper_receiver(&service,0u)==NULL&&net_upper_receiver(&service,1u)==NULL);
    CHECK(net_upper_receiver(&service,2u)==NULL);return 0;
}
static int wrong_node(void)
{
    uint32_t candidate;for(candidate=NET_WIRE_NODE_DOMAIN;candidate<=NET_WIRE_NODE_PI;candidate<<=1u){
        prepare();input_config.node=candidate;memcpy(before,&service,sizeof(service));
        if(NETWORK_UPPER_NODE_PROFILE==NET_UPPER_PROFILE_HOST||candidate==node())CHECK(net_upper_init(&service,&input_config)==NET_OK);
        else {CHECK(net_upper_init(&service,&input_config)==NET_NOT_CONFIGURED);CHECK(memcmp(before,&service,sizeof(service))==0);}
    }return 0;
}
static int nonowned_receipts(void)
{
    size_t i;NetUpperCanMessage m,old_m;NetUpperDelivery d,old_d;NetSourceStamp s,old_s;NetUpperUartReceipt u,old_u;CHECK(init()==0);
    memset(&m,0x5a,sizeof(m));old_m=m;memset(&d,0x5a,sizeof(d));old_d=d;memset(&s,0x5a,sizeof(s));old_s=s;memset(&u,0x5a,sizeof(u));old_u=u;
    for(i=0u;i<48u;++i){const NetWireRegistryEntry *r=net_wire_registry_at(i);
        if(r->link==NET_WIRE_CAN){CHECK(net_upper_received_can(&service,r->id_type,&m,&d,&s)==NET_NO_WORK);CHECK(memcmp(&m,&old_m,sizeof(m))==0&&memcmp(&d,&old_d,sizeof(d))==0&&memcmp(&s,&old_s,sizeof(s))==0);}
        else {CHECK(net_upper_received_uart(&service,r->link,(uint8_t)r->id_type,&u)==NET_NO_WORK);CHECK(memcmp(&u,&old_u,sizeof(u))==0);}
    }return 0;
}
static int can_routes(void)
{
    size_t i,active=0u,held=0u;NetTxKey key={99u,88u};NetUpperCanMessage m;NetSourceStamp source={1u,100u,0u};NetAgePolicy p={true,100000u,true};
    for(i=0u;i<22u;++i){const NetWireRegistryEntry *r=net_wire_registry_find(NET_WIRE_CAN,fixtures[i].id);
        if((r->receivers&node())==0u)continue;
        CHECK(init()==0);
        if(node()==NET_WIRE_NODE_WINDOW||fixtures[i].hold){CHECK(receive_fixture(i)==NET_HOLD&&owner.calls==0u);++held;}
        else {
            NetUpperCanMessage actual;NetUpperDelivery d;NetSourceStamp observed;NetWireCanEnvelope encoded;
            CHECK(receive_fixture(i)==NET_OK&&owner.calls==1u);CHECK(net_upper_received_can(&service,fixtures[i].id,&actual,&d,&observed)==NET_OK);
            CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY,&actual,&encoded)==NET_WIRE_OK);
            CHECK(encoded.length==fixtures[i].length&&memcmp(encoded.bytes,fixtures[i].initial,encoded.length)==0);
            CHECK(observed.observed_at_ms==100u&&observed.original_age_ms==fixtures[i].age&&observed.observation_id!=0u);
            CHECK(receive_fixture(i)==NET_DUPLICATE&&owner.calls==1u);++active;
        }
    }
    CHECK(init()==0);i=outbound();CHECK(message(i,&m)==0);source.original_age_ms=fixtures[i].age;
    if(node()==NET_WIRE_NODE_WINDOW){CHECK(net_upper_submit_can(&service,&m,&source,&p,&key)==NET_HOLD&&key.epoch==99u&&key.token==88u&&physical.count==0u);}
    else {
        CHECK(net_upper_submit_can(&service,&m,&source,&p,&key)==NET_OK);CHECK(net_upper_step(&service,0u)==NET_OK&&physical.count==1u);
        CHECK(physical.held[0]->can.length==fixtures[i].length&&memcmp(physical.held[0]->can.bytes,fixtures[i].initial,fixtures[i].length)==0);
        physical.held[0]=NULL;CHECK(complete(key,false)==0&&net_upper_retire(&service,key)==NET_OK);
    }
    printf("PROFILE_CAN node=%u active_rx=%lu held_rx=%lu actual_product=1\n",node(),(unsigned long)active,(unsigned long)held);return 0;
}
static int tx_capacity(void)
{
    NetTxKey keys[PROFILE_TX],sentinel={99u,88u};NetUpperCanMessage m;NetSourceStamp source={1u,100u,0u};NetAgePolicy p={true,100000u,true};size_t i;NetWireCanEnvelope stable[PROFILE_TX];
    CHECK(init()==0);CHECK(message(outbound(),&m)==0);source.original_age_ms=fixtures[outbound()].age;
    if(node()==NET_WIRE_NODE_WINDOW){CHECK(net_upper_submit_can(&service,&m,&source,&p,&sentinel)==NET_HOLD&&physical.count==0u);CHECK(sentinel.epoch==99u&&sentinel.token==88u);return 0;}
    for(i=0u;i<PROFILE_TX;++i)CHECK(enqueue(outbound(),&keys[i])==0);
    CHECK(net_upper_submit_can(&service,&m,&source,&p,&sentinel)==NET_NO_CAPACITY&&sentinel.epoch==99u&&sentinel.token==88u);
    CHECK(net_upper_step(&service,0u)==NET_OK&&physical.count==PROFILE_TX);
    for(i=0u;i<PROFILE_TX;++i)stable[i]=physical.held[i]->can;
    physical.held[0]=NULL;CHECK(complete(keys[0],true)==0);CHECK(net_upper_retire(&service,keys[0])==NET_HOLD);
    CHECK(net_upper_submit_can(&service,&m,&source,&p,&sentinel)==NET_NO_CAPACITY);
    /* Slot 0 is released; every other held packet is unchanged. Each loop walks
     * the whole queue and skips slot 0, so a one-slot profile never indexes past it. */
    for(i=0u;i<PROFILE_TX;++i)if(i!=0u)CHECK(physical.held[i]->can.length==stable[i].length&&memcmp(physical.held[i]->can.bytes,stable[i].bytes,stable[i].length)==0);
    CHECK(complete(keys[0],false)==0&&net_upper_retire(&service,keys[0])==NET_OK);CHECK(enqueue(outbound(),&sentinel)==0&&sentinel.token>keys[PROFILE_TX-1u].token);
    /* The freed slot takes the next submission, and the next pass sends it. */
    CHECK(net_upper_step(&service,0u)==NET_OK&&physical.count==PROFILE_TX+1u);
    for(i=0u;i<PROFILE_TX;++i)if(i!=0u){physical.held[i]=NULL;CHECK(complete(keys[i],false)==0&&net_upper_retire(&service,keys[i])==NET_OK);}
    CHECK(physical.count==PROFILE_TX+1u);physical.held[PROFILE_TX]=NULL;CHECK(complete(sentinel,false)==0&&net_upper_retire(&service,sentinel)==NET_OK);return 0;
}
static int rx_capacity(void)
{
    NetUpperEvent e={0};size_t i,index=node()==NET_WIRE_NODE_DOMAIN?13u:0u;NetUpperEvent retained[PROFILE_RX];
    CHECK(init()==0);e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture=captured();e.can=envelope(index);
    for(i=0u;i<PROFILE_RX;++i)CHECK(net_upper_capture(&service,&e)==NET_OK);
    CHECK(sizeof(retained)==sizeof(service.rx));memcpy(retained,service.rx,sizeof(service.rx));CHECK(net_upper_capture(&service,&e)==NET_NO_CAPACITY&&service.rx_count==PROFILE_RX);
    CHECK(memcmp(retained,service.rx,sizeof(service.rx))==0&&service.observation.rx_dropped==1u);
    e.can.bytes[0]^=0xffu;
    CHECK(net_upper_step(&service,1u)==(node()==NET_WIRE_NODE_WINDOW?NET_HOLD:NET_OK));
    CHECK(service.rx_count==PROFILE_RX-1u&&owner.calls==(node()==NET_WIRE_NODE_WINDOW?0u:1u));
    for(i=0u;i<PROFILE_RX-1u;++i)CHECK(net_upper_step(&service,1u)==(node()==NET_WIRE_NODE_WINDOW?NET_HOLD:NET_DUPLICATE));
    CHECK(service.rx_count==0u);return 0;
}
static int inject_uart(const NetUpperUartMessage *m)
{
    NetWireUartFrame f;NetUpperEvent e={0};size_t i;CHECK(net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY,m,&f)==NET_WIRE_OK);
    e.kind=NET_UPPER_UART_BYTE;e.link=m->link;e.capture=captured();
    for(i=0u;i<f.length;++i){e.byte=f.bytes[i];CHECK(net_upper_capture(&service,&e)==NET_OK);CHECK(net_upper_step(&service,1u)==(i+1u==f.length?NET_OK:NET_NO_WORK));}return 0;
}
static int uart_boundary(void)
{
    NetUpperUartMessage m={0};NetUpperEvent e={0};NetTxKey key;NetAgePolicy p={true,10000u,true};CHECK(init()==0);
    if(node()==NET_WIRE_NODE_CIS&&NETWORK_UPPER_NODE_PROFILE==NET_UPPER_PROFILE_CIS){
        NetUpperCanMessage actual;m.link=NET_WIRE_UART_C;m.type=0x60u;m.sequence=7u;
        m.data.p_vision_result.pi_boot_id=41u;m.data.p_vision_result.vision_sequence=77u;m.data.p_vision_result.vision_age_ms=10u;
        m.data.p_vision_result.occupant_presence=1u;m.data.p_vision_result.presence_validity=1u;m.data.p_vision_result.occupant_count=3u;m.data.p_vision_result.count_validity=1u;m.data.p_vision_result.vision_status=2u;
        CHECK(inject_uart(&m)==0&&owner.calls==1u);CHECK(net_upper_cis_publish_occupant(&service,9u,10u,&p,&key)==NET_OK);
        CHECK(net_upper_step(&service,0u)==NET_OK&&physical.count==1u);CHECK(net_upper_can_decode(NET_WIRE_CANDIDATE_ONLY,&physical.held[0]->can,&actual)==NET_WIRE_OK);
        CHECK(actual.id==0x310u&&actual.data.cis_occupant.pi_boot_id==41u&&actual.data.cis_occupant.vision_sequence==77u&&actual.data.cis_occupant.vision_age_ms==10u);
    }else if(node()==NET_WIRE_NODE_DOMAIN){
        NetUpperCanMessage raw;NetUpperDelivery d;NetSourceStamp original;m.link=NET_WIRE_UART_M;m.type=0x01u;m.sequence=4u;
        m.data.m_context.device_context_id=12u;m.data.m_context.session_id=34u;m.data.m_context.device_registration_state=1u;m.data.m_context.bt_connection_state=1u;
        m.data.m_context.app_active_state=1u;m.data.m_context.app_update_sequence=3u;m.data.m_context.proximity_state=1u;m.data.m_context.proximity_validity=1u;
        m.data.m_context.proximity_update_sequence=4u;m.data.m_context.registration_connection_update_sequence=5u;
        CHECK(inject_uart(&m)==0&&service.context_seen);
        memset(&m,0,sizeof(m));m.link=NET_WIRE_UART_M;m.type=0x10u;m.sequence=5u;m.data.m_request.device_context_id=12u;m.data.m_request.session_id=34u;m.data.m_request.request_id=50u;m.data.m_request.kind=1u;m.data.m_request.operation=1u;
        CHECK(inject_uart(&m)==0&&owner.uart.message.data.m_request.request_id==50u);
        CHECK(receive_fixture(13u)==NET_OK);CHECK(net_upper_received_can(&service,0x0a0u,&raw,&d,&original)==NET_OK);
        CHECK(net_upper_uart_project_can(NET_WIRE_CANDIDATE_ONLY,&raw,12u,34u,51u,0u,9u,&m)==NET_WIRE_OK);
        CHECK(net_upper_submit_uart(&service,&m,&original,&p,&key)==NET_OK&&net_upper_step(&service,0u)==NET_OK&&physical.count==1u);
        CHECK(physical.held[0]->link==NET_WIRE_UART_M&&owner.rear.rear_distance_mm==123u);
    }else {
        e.kind=NET_UPPER_UART_BYTE;e.link=NET_WIRE_UART_C;e.capture=captured();e.byte=0xa5u;
        memcpy(before,&service,sizeof(service));CHECK(net_upper_capture(&service,&e)==NET_UNSUPPORTED&&memcmp(before,&service,sizeof(service))==0);
        CHECK(net_upper_receiver(&service,0u)==NULL&&net_upper_receiver(&service,1u)==NULL&&owner.calls==0u);
        if(node()==NET_WIRE_NODE_CIS)CHECK(net_upper_cis_publish_occupant(&service,9u,10u,&p,&key)==NET_HOLD);
    }return 0;
}
static int footprint(void)
{
    printf("{\"profile\":%d,\"node\":%u,\"NetUpper\":%lu,\"stored_config\":%lu,\"admission_bytes\":%lu,\"admission_count\":%u,\"tx_bytes\":%lu,\"tx_capacity\":%u,\"rx_bytes\":%lu,\"rx_capacity\":%u,\"uart_parser_count\":%u,\"uart_parser_bytes\":%lu,\"adapter_bytes\":%lu,\"query_if_domain_bytes\":%lu,\"target_ram_fit\":\"NOT_VERIFIED\"}\n",
        NETWORK_UPPER_NODE_PROFILE,node(),(unsigned long)sizeof(NetUpper),(unsigned long)sizeof(NetUpperStoredConfig),(unsigned long)sizeof(service.admission),NET_UPPER_ADMISSION_CAPACITY,(unsigned long)sizeof(service.tx),(unsigned)PROFILE_TX,(unsigned long)sizeof(service.rx),(unsigned)PROFILE_RX,NET_UPPER_UART_RECEIVER_CAPACITY,(unsigned long)(NET_UPPER_UART_RECEIVER_CAPACITY*sizeof(NetUpperUartReceiver)),(unsigned long)sizeof(NetHalAdapter),(unsigned long)(node()==NET_WIRE_NODE_DOMAIN?sizeof(NetUpperQuery):0u));
    CHECK(sizeof(service.tx)==PROFILE_TX*sizeof(NetUpperTx)&&sizeof(service.rx)==PROFILE_RX*sizeof(NetUpperEvent));return 0;
}
int main(int argc,char **argv)
{
    int result;if(argc!=2)return 2;case_name=argv[1];
    if(strcmp(case_name,"storage")==0)result=storage();
    else if(strcmp(case_name,"wrong_node")==0)result=wrong_node();
    else if(strcmp(case_name,"nonowned_receipts")==0)result=nonowned_receipts();
    else if(strcmp(case_name,"can_routes")==0)result=can_routes();
    else if(strcmp(case_name,"tx_capacity")==0)result=tx_capacity();
    else if(strcmp(case_name,"rx_capacity")==0)result=rx_capacity();
    else if(strcmp(case_name,"uart_boundary")==0)result=uart_boundary();
    else if(strcmp(case_name,"footprint")==0)result=footprint();
    else return 2;
    if(result==0)printf("PASS node_profile.%s profile=%d\n",case_name,NETWORK_UPPER_NODE_PROFILE);
    return result;
}
