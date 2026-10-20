#include "network/net_upper.h"
#include <stdio.h>
#include <string.h>
/* Fixed literals independently prepared with historical FINAL98 Python struct.pack
 * oracles. Scalar fixtures use AGE=0 except the mandatory absent A1 sentinel.
 * This file implements only fake physical Ports and external Owner callbacks.
 * Network service, queue, codec, router, adapter and lifetime are product code. */
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",case_name,__LINE__,#x); return 1; } } while(0)
static const char *case_name;
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
static void fixture_message(size_t index,unsigned elapsed,NetUpperCanMessage *m)
{
 memset(m,0,sizeof(*m)); m->id=fixtures[index].id;
 switch(index) {
 case 0u:
  m->data.domain_permission.boot_id=51u;
  m->data.domain_permission.tx_sequence=1u;
  m->data.domain_permission.vehicle_power_permission=0u;
  m->data.domain_permission.power_permission_quality=0u;
  m->data.domain_permission.power_operation_state=0u;
  m->data.domain_permission.operation_allowed=0u;
  m->data.domain_permission.operation_quality=0u;
  m->data.domain_permission.update_sequence=7u;
  m->data.domain_permission.source_age_ms=(uint16_t)(0u+elapsed);
  break;
 case 1u:
  m->data.bcm_door_command.boot_id=270544960u;
  m->data.bcm_door_command.tx_sequence=1348497536u;
  m->data.bcm_door_command.target_boot_id=2443359172u;
  m->data.bcm_door_command.command_id=405222981u;
  m->data.bcm_door_command.command_age_ms=(uint16_t)(0u+elapsed);
  m->data.bcm_door_command.accept_limit_ms=31628u;
  m->data.bcm_door_command.door_lock_target=1u;
  m->data.bcm_door_command.origin=5u;
  break;
 case 2u:
  m->data.bcm_climate_command.boot_id=270544960u;
  m->data.bcm_climate_command.tx_sequence=1348497536u;
  m->data.bcm_climate_command.target_boot_id=2443359172u;
  m->data.bcm_climate_command.command_id=405222981u;
  m->data.bcm_climate_command.command_age_ms=(uint16_t)(0u+elapsed);
  m->data.bcm_climate_command.accept_limit_ms=31628u;
  m->data.bcm_climate_command.fan_target_level=3u;
  m->data.bcm_climate_command.thermal_direction=2u;
  m->data.bcm_climate_command.thermal_target_percent=100u;
  break;
 case 3u:
  m->data.bcm_light_command.boot_id=270544960u;
  m->data.bcm_light_command.tx_sequence=1348497536u;
  m->data.bcm_light_command.target_boot_id=2443359172u;
  m->data.bcm_light_command.command_id=405222981u;
  m->data.bcm_light_command.command_age_ms=(uint16_t)(0u+elapsed);
  m->data.bcm_light_command.accept_limit_ms=31628u;
  m->data.bcm_light_command.interior_light_type=2u;
  m->data.bcm_light_command.light_level_percent=37u;
  m->data.bcm_light_command.red=18u;
  m->data.bcm_light_command.green=52u;
  m->data.bcm_light_command.blue=86u;
  break;
 case 4u:
  m->data.window_command.boot_id=16909060u;
  m->data.window_command.tx_sequence=287454020u;
  m->data.window_command.target_boot_id=1432778632u;
  m->data.window_command.command_id=2578103244u;
  m->data.window_command.command_age_ms=(uint16_t)(0u+elapsed);
  m->data.window_command.accept_limit_ms=22136u;
  m->data.window_command.window_command=5u;
  m->data.window_command.target_channel=0u;
  m->data.window_command.target_position=37u;
  m->data.window_command.source_context=4u;
  break;
 case 5u:
  m->data.bcm_door_state.boot_id=270544960u;
  m->data.bcm_door_state.tx_sequence=1348497536u;
  m->data.bcm_door_state.door_lock_state=255u;
  m->data.bcm_door_state.door_open_state=1u;
  m->data.bcm_door_state.door_composite_state=2u;
  m->data.bcm_door_state.lock_quality=3u;
  m->data.bcm_door_state.open_quality=1u;
  m->data.bcm_door_state.composite_quality=2u;
  m->data.bcm_door_state.update_sequence=3721182122u;
  m->data.bcm_door_state.source_age_ms=(uint16_t)(0u+elapsed);
  break;
 case 6u:
  m->data.bcm_climate_state.boot_id=270544960u;
  m->data.bcm_climate_state.tx_sequence=1348497536u;
  m->data.bcm_climate_state.fan_command_level=2u;
  m->data.bcm_climate_state.fan_measured_level=255u;
  m->data.bcm_climate_state.thermal_direction_state=1u;
  m->data.bcm_climate_state.thermal_output_percent=37u;
  m->data.bcm_climate_state.heat_removal_state=255u;
  m->data.bcm_climate_state.fan_command_quality=0u;
  m->data.bcm_climate_state.fan_measured_quality=3u;
  m->data.bcm_climate_state.thermal_direction_quality=1u;
  m->data.bcm_climate_state.thermal_output_quality=2u;
  m->data.bcm_climate_state.heat_removal_quality=3u;
  m->data.bcm_climate_state.update_sequence=3721182122u;
  m->data.bcm_climate_state.source_age_ms=(uint16_t)(0u+elapsed);
  break;
 case 7u:
  m->data.bcm_light_state.boot_id=270544960u;
  m->data.bcm_light_state.tx_sequence=1348497536u;
  m->data.bcm_light_state.interior_light_type_state=3u;
  m->data.bcm_light_state.light_level_percent=73u;
  m->data.bcm_light_state.red=18u;
  m->data.bcm_light_state.green=52u;
  m->data.bcm_light_state.blue=86u;
  m->data.bcm_light_state.interior_light_apply_result=255u;
  m->data.bcm_light_state.light_physical_feedback_capability=0u;
  m->data.bcm_light_state.light_type_quality=1u;
  m->data.bcm_light_state.light_level_quality=2u;
  m->data.bcm_light_state.light_color_quality=3u;
  m->data.bcm_light_state.light_apply_quality=3u;
  m->data.bcm_light_state.update_sequence=3721182122u;
  m->data.bcm_light_state.source_age_ms=(uint16_t)(0u+elapsed);
  break;
 case 8u:
  m->data.bcm_result.boot_id=270544960u;
  m->data.bcm_result.tx_sequence=1348497536u;
  m->data.bcm_result.command_domain_boot=2443359172u;
  m->data.bcm_result.command_id=405222981u;
  m->data.bcm_result.command_kind=1u;
  m->data.bcm_result.result=2u;
  m->data.bcm_result.result_reason=9u;
  m->data.bcm_result.door_event=3u;
  m->data.bcm_result.occurrence_id=3788686260u;
  m->data.bcm_result.event_age_ms=(uint16_t)(0u+elapsed);
  m->data.bcm_result.event_quality=1u;
  m->data.bcm_result.door_target=1u;
  break;
 case 9u:
  m->data.bcm_event.boot_id=270544960u;
  m->data.bcm_event.tx_sequence=1348497536u;
  m->data.bcm_event.event_kind=2u;
  m->data.bcm_event.occurrence_id=875770417u;
  m->data.bcm_event.event_age_ms=(uint16_t)(0u+elapsed);
  m->data.bcm_event.event_quality=2u;
  m->data.bcm_event.related_domain_boot=1953722993u;
  m->data.bcm_event.related_command_id=2223211137u;
  m->data.bcm_event.affected_function=2u;
  break;
 case 10u:
  m->data.bcm_status.boot_id=270544960u;
  m->data.bcm_status.tx_sequence=1348497536u;
  m->data.bcm_status.bcm_ecu_state=3u;
  m->data.bcm_status.fault_mask=165u;
  m->data.bcm_status.active_fault_category=3u;
  m->data.bcm_status.active_fault_code=8u;
  m->data.bcm_status.affected_function=7u;
  m->data.bcm_status.recovering=1u;
  m->data.bcm_status.update_sequence=2712847316u;
  m->data.bcm_status.source_age_ms=(uint16_t)(0u+elapsed);
  break;
 case 11u:
  m->data.cis_environment.boot_id=16909060u;
  m->data.cis_environment.tx_sequence=287454020u;
  m->data.cis_environment.temperature_centi_c=-4000;
  m->data.cis_environment.temperature_sequence=573785173u;
  m->data.cis_environment.temperature_age_ms=(uint16_t)(0u+elapsed);
  m->data.cis_environment.temperature_validity=1u;
  m->data.cis_environment.temperature_reason=0u;
  m->data.cis_environment.humidity_centi_percent=6789u;
  m->data.cis_environment.humidity_sequence=860116326u;
  m->data.cis_environment.humidity_age_ms=(uint16_t)(0u+elapsed);
  m->data.cis_environment.humidity_validity=1u;
  m->data.cis_environment.humidity_reason=0u;
  m->data.cis_environment.illuminance_lux=123456u;
  m->data.cis_environment.illuminance_sequence=1146447479u;
  m->data.cis_environment.illuminance_age_ms=(uint16_t)(0u+elapsed);
  m->data.cis_environment.illuminance_validity=1u;
  m->data.cis_environment.illuminance_reason=0u;
  break;
 case 12u:
  m->data.cis_occupant.boot_id=16909060u;
  m->data.cis_occupant.tx_sequence=287454020u;
  m->data.cis_occupant.pi_boot_id=1432778632u;
  m->data.cis_occupant.vision_sequence=2578103244u;
  m->data.cis_occupant.vision_age_ms=(uint16_t)(0u+elapsed);
  m->data.cis_occupant.occupant_presence=1u;
  m->data.cis_occupant.presence_validity=1u;
  m->data.cis_occupant.presence_reason=0u;
  m->data.cis_occupant.occupant_count=3u;
  m->data.cis_occupant.count_validity=1u;
  m->data.cis_occupant.count_reason=0u;
  m->data.cis_occupant.vision_function_status=2u;
  break;
 case 13u:
  m->data.cis_rear.boot_id=41u;
  m->data.cis_rear.tx_sequence=1u;
  m->data.cis_rear.update_sequence=77u;
  m->data.cis_rear.source_age_ms=(uint16_t)(0u+elapsed);
  m->data.cis_rear.rear_distance_deci_cm=123u;
  m->data.cis_rear.validity=1u;
  m->data.cis_rear.quality_reason=0u;
  m->data.cis_rear.proximity_status=0u;
  break;
 case 14u:
  m->data.cis_status.boot_id=16909060u;
  m->data.cis_status.tx_sequence=287454020u;
  m->data.cis_status.cis_state=3u;
  m->data.cis_status.vision_status=2u;
  m->data.cis_status.temperature_status=1u;
  m->data.cis_status.humidity_status=4u;
  m->data.cis_status.illuminance_status=3u;
  m->data.cis_status.rear_status=0u;
  m->data.cis_status.pi_uart_status=2u;
  m->data.cis_status.can_interface_status=1u;
  m->data.cis_status.fault_mask=421u;
  m->data.cis_status.active_fault=9u;
  m->data.cis_status.recovering=1u;
  m->data.cis_status.last_fault=8u;
  m->data.cis_status.last_fault_occurrence=1432778632u;
  m->data.cis_status.affected_function=45u;
  m->data.cis_status.update_sequence=2578103244u;
  m->data.cis_status.source_age_ms=(uint16_t)(0u+elapsed);
  break;
 case 15u:
  m->data.window_state.boot_id=16909060u;
  m->data.window_state.tx_sequence=287454020u;
  m->data.window_state.motion_state=2u;
  m->data.window_state.window_ecu_state=2u;
  m->data.window_state.window_position=37u;
  m->data.window_state.window_value_quality=1u;
  m->data.window_state.fully_open_state=0u;
  m->data.window_state.fully_closed_state=0u;
  m->data.window_state.antipinch_protection_status=1u;
  m->data.window_state.reverse_state=1u;
  m->data.window_state.update_sequence=2712847316u;
  m->data.window_state.source_age_ms=(uint16_t)(0u+elapsed);
  break;
 case 16u:
  m->data.window_event.boot_id=16909060u;
  m->data.window_event.tx_sequence=287454020u;
  m->data.window_event.event_kind=3u;
  m->data.window_event.target_channel=0u;
  m->data.window_event.occurrence_id=2712847316u;
  m->data.window_event.related_domain_boot=1432778632u;
  m->data.window_event.related_request_sequence=2578103244u;
  m->data.window_event.origin=5u;
  m->data.window_event.event_age_ms=(uint16_t)(0u+elapsed);
  m->data.window_event.event_quality=2u;
  break;
 case 17u:
  m->data.window_result.boot_id=16909060u;
  m->data.window_result.tx_sequence=287454020u;
  m->data.window_result.request_domain_boot=1432778632u;
  m->data.window_result.request_sequence=2578103244u;
  m->data.window_result.target_channel=0u;
  m->data.window_result.result=5u;
  m->data.window_result.result_reason=25u;
  break;
 case 18u:
  m->data.window_fault.boot_id=16909060u;
  m->data.window_fault.tx_sequence=287454020u;
  m->data.window_fault.fault_category=2u;
  m->data.window_fault.fault_code=255u;
  m->data.window_fault.fault_status=0u;
  m->data.window_fault.occurrence_id=1432778632u;
  m->data.window_fault.update_sequence=2578103244u;
  m->data.window_fault.source_age_ms=(uint16_t)(0u+elapsed);
  break;
 case 19u:
  m->data.vss_event.boot_id=16909060u;
  m->data.vss_event.tx_sequence=287454020u;
  m->data.vss_event.event_kind=5u;
  m->data.vss_event.occurrence_id=1432778632u;
  m->data.vss_event.original_age_ms=(uint16_t)(0u+elapsed);
  m->data.vss_event.use_limit_ms=22136u;
  m->data.vss_event.value_quality=1u;
  m->data.vss_event.input_availability=1u;
  m->data.vss_event.related_command_id=2578103244u;
  break;
 case 20u:
  m->data.vss_warning_state.boot_id=16909060u;
  m->data.vss_warning_state.tx_sequence=287454020u;
  m->data.vss_warning_state.antipinch_source_boot=0u;
  m->data.vss_warning_state.antipinch_source_sequence=0u;
  m->data.vss_warning_state.antipinch_original_age_ms=UINT16_MAX;
  m->data.vss_warning_state.antipinch_state=0u;
  m->data.vss_warning_state.antipinch_quality=3u;
  m->data.vss_warning_state.antipinch_reason=6u;
  m->data.vss_warning_state.antipinch_availability=2u;
  m->data.vss_warning_state.occupant_hazard_source_boot=573785173u;
  m->data.vss_warning_state.occupant_hazard_source_sequence=860116326u;
  m->data.vss_warning_state.occupant_hazard_original_age_ms=(uint16_t)(0u+elapsed);
  m->data.vss_warning_state.occupant_hazard_state=1u;
  m->data.vss_warning_state.occupant_hazard_quality=0u;
  m->data.vss_warning_state.occupant_hazard_reason=0u;
  m->data.vss_warning_state.occupant_hazard_availability=1u;
  m->data.vss_warning_state.rear_obstacle_source_boot=1432778632u;
  m->data.vss_warning_state.rear_obstacle_source_sequence=2578103244u;
  m->data.vss_warning_state.rear_obstacle_original_age_ms=(uint16_t)(0u+elapsed);
  m->data.vss_warning_state.rear_obstacle_state=2u;
  m->data.vss_warning_state.rear_obstacle_quality=0u;
  m->data.vss_warning_state.rear_obstacle_reason=0u;
  m->data.vss_warning_state.rear_obstacle_availability=0u;
  m->data.vss_warning_state.rear_detection_activation=1u;
  m->data.vss_warning_state.activation_meta_format=1u;
  m->data.vss_warning_state.rear_activation_quality=0u;
  m->data.vss_warning_state.rear_activation_reason=0u;
  m->data.vss_warning_state.rear_activation_sequence=2005440938u;
  m->data.vss_warning_state.rear_activation_age_ms=(uint16_t)(0u+elapsed);
  break;
 case 21u:
  m->data.vss_status.boot_id=16909060u;
  m->data.vss_status.tx_sequence=287454020u;
  m->data.vss_status.vss_state=2u;
  m->data.vss_status.vss_availability=1u;
  m->data.vss_status.vss_accepting_events=1u;
  m->data.vss_status.vss_fault_active=0u;
  m->data.vss_status.fault_mask=0u;
  m->data.vss_status.vss_last_fault=3u;
  m->data.vss_status.last_fault_occurrence=1432778632u;
  m->data.vss_status.last_fault_age_ms=(uint16_t)(0u+elapsed);
  m->data.vss_status.last_fault_quality=0u;
  m->data.vss_status.update_sequence=2578103244u;
  m->data.vss_status.source_age_ms=(uint16_t)(0u+elapsed);
  break;
 default: break;
 }
}
static int equal_typed(size_t index,const NetUpperCanMessage *a,const NetUpperCanMessage *b)
{
 if(a->id!=b->id) return 0;
 switch(index) {
 case 0u: return a->data.domain_permission.boot_id==b->data.domain_permission.boot_id &&
   a->data.domain_permission.tx_sequence==b->data.domain_permission.tx_sequence &&
   a->data.domain_permission.vehicle_power_permission==b->data.domain_permission.vehicle_power_permission &&
   a->data.domain_permission.power_permission_quality==b->data.domain_permission.power_permission_quality &&
   a->data.domain_permission.power_operation_state==b->data.domain_permission.power_operation_state &&
   a->data.domain_permission.operation_allowed==b->data.domain_permission.operation_allowed &&
   a->data.domain_permission.operation_quality==b->data.domain_permission.operation_quality &&
   a->data.domain_permission.update_sequence==b->data.domain_permission.update_sequence &&
   a->data.domain_permission.source_age_ms==b->data.domain_permission.source_age_ms;
 case 1u: return a->data.bcm_door_command.boot_id==b->data.bcm_door_command.boot_id &&
   a->data.bcm_door_command.tx_sequence==b->data.bcm_door_command.tx_sequence &&
   a->data.bcm_door_command.target_boot_id==b->data.bcm_door_command.target_boot_id &&
   a->data.bcm_door_command.command_id==b->data.bcm_door_command.command_id &&
   a->data.bcm_door_command.command_age_ms==b->data.bcm_door_command.command_age_ms &&
   a->data.bcm_door_command.accept_limit_ms==b->data.bcm_door_command.accept_limit_ms &&
   a->data.bcm_door_command.door_lock_target==b->data.bcm_door_command.door_lock_target &&
   a->data.bcm_door_command.origin==b->data.bcm_door_command.origin;
 case 2u: return a->data.bcm_climate_command.boot_id==b->data.bcm_climate_command.boot_id &&
   a->data.bcm_climate_command.tx_sequence==b->data.bcm_climate_command.tx_sequence &&
   a->data.bcm_climate_command.target_boot_id==b->data.bcm_climate_command.target_boot_id &&
   a->data.bcm_climate_command.command_id==b->data.bcm_climate_command.command_id &&
   a->data.bcm_climate_command.command_age_ms==b->data.bcm_climate_command.command_age_ms &&
   a->data.bcm_climate_command.accept_limit_ms==b->data.bcm_climate_command.accept_limit_ms &&
   a->data.bcm_climate_command.fan_target_level==b->data.bcm_climate_command.fan_target_level &&
   a->data.bcm_climate_command.thermal_direction==b->data.bcm_climate_command.thermal_direction &&
   a->data.bcm_climate_command.thermal_target_percent==b->data.bcm_climate_command.thermal_target_percent;
 case 3u: return a->data.bcm_light_command.boot_id==b->data.bcm_light_command.boot_id &&
   a->data.bcm_light_command.tx_sequence==b->data.bcm_light_command.tx_sequence &&
   a->data.bcm_light_command.target_boot_id==b->data.bcm_light_command.target_boot_id &&
   a->data.bcm_light_command.command_id==b->data.bcm_light_command.command_id &&
   a->data.bcm_light_command.command_age_ms==b->data.bcm_light_command.command_age_ms &&
   a->data.bcm_light_command.accept_limit_ms==b->data.bcm_light_command.accept_limit_ms &&
   a->data.bcm_light_command.interior_light_type==b->data.bcm_light_command.interior_light_type &&
   a->data.bcm_light_command.light_level_percent==b->data.bcm_light_command.light_level_percent &&
   a->data.bcm_light_command.red==b->data.bcm_light_command.red &&
   a->data.bcm_light_command.green==b->data.bcm_light_command.green &&
   a->data.bcm_light_command.blue==b->data.bcm_light_command.blue;
 case 4u: return a->data.window_command.boot_id==b->data.window_command.boot_id &&
   a->data.window_command.tx_sequence==b->data.window_command.tx_sequence &&
   a->data.window_command.target_boot_id==b->data.window_command.target_boot_id &&
   a->data.window_command.command_id==b->data.window_command.command_id &&
   a->data.window_command.command_age_ms==b->data.window_command.command_age_ms &&
   a->data.window_command.accept_limit_ms==b->data.window_command.accept_limit_ms &&
   a->data.window_command.window_command==b->data.window_command.window_command &&
   a->data.window_command.target_channel==b->data.window_command.target_channel &&
   a->data.window_command.target_position==b->data.window_command.target_position &&
   a->data.window_command.source_context==b->data.window_command.source_context;
 case 5u: return a->data.bcm_door_state.boot_id==b->data.bcm_door_state.boot_id &&
   a->data.bcm_door_state.tx_sequence==b->data.bcm_door_state.tx_sequence &&
   a->data.bcm_door_state.door_lock_state==b->data.bcm_door_state.door_lock_state &&
   a->data.bcm_door_state.door_open_state==b->data.bcm_door_state.door_open_state &&
   a->data.bcm_door_state.door_composite_state==b->data.bcm_door_state.door_composite_state &&
   a->data.bcm_door_state.lock_quality==b->data.bcm_door_state.lock_quality &&
   a->data.bcm_door_state.open_quality==b->data.bcm_door_state.open_quality &&
   a->data.bcm_door_state.composite_quality==b->data.bcm_door_state.composite_quality &&
   a->data.bcm_door_state.update_sequence==b->data.bcm_door_state.update_sequence &&
   a->data.bcm_door_state.source_age_ms==b->data.bcm_door_state.source_age_ms;
 case 6u: return a->data.bcm_climate_state.boot_id==b->data.bcm_climate_state.boot_id &&
   a->data.bcm_climate_state.tx_sequence==b->data.bcm_climate_state.tx_sequence &&
   a->data.bcm_climate_state.fan_command_level==b->data.bcm_climate_state.fan_command_level &&
   a->data.bcm_climate_state.fan_measured_level==b->data.bcm_climate_state.fan_measured_level &&
   a->data.bcm_climate_state.thermal_direction_state==b->data.bcm_climate_state.thermal_direction_state &&
   a->data.bcm_climate_state.thermal_output_percent==b->data.bcm_climate_state.thermal_output_percent &&
   a->data.bcm_climate_state.heat_removal_state==b->data.bcm_climate_state.heat_removal_state &&
   a->data.bcm_climate_state.fan_command_quality==b->data.bcm_climate_state.fan_command_quality &&
   a->data.bcm_climate_state.fan_measured_quality==b->data.bcm_climate_state.fan_measured_quality &&
   a->data.bcm_climate_state.thermal_direction_quality==b->data.bcm_climate_state.thermal_direction_quality &&
   a->data.bcm_climate_state.thermal_output_quality==b->data.bcm_climate_state.thermal_output_quality &&
   a->data.bcm_climate_state.heat_removal_quality==b->data.bcm_climate_state.heat_removal_quality &&
   a->data.bcm_climate_state.update_sequence==b->data.bcm_climate_state.update_sequence &&
   a->data.bcm_climate_state.source_age_ms==b->data.bcm_climate_state.source_age_ms;
 case 7u: return a->data.bcm_light_state.boot_id==b->data.bcm_light_state.boot_id &&
   a->data.bcm_light_state.tx_sequence==b->data.bcm_light_state.tx_sequence &&
   a->data.bcm_light_state.interior_light_type_state==b->data.bcm_light_state.interior_light_type_state &&
   a->data.bcm_light_state.light_level_percent==b->data.bcm_light_state.light_level_percent &&
   a->data.bcm_light_state.red==b->data.bcm_light_state.red &&
   a->data.bcm_light_state.green==b->data.bcm_light_state.green &&
   a->data.bcm_light_state.blue==b->data.bcm_light_state.blue &&
   a->data.bcm_light_state.interior_light_apply_result==b->data.bcm_light_state.interior_light_apply_result &&
   a->data.bcm_light_state.light_physical_feedback_capability==b->data.bcm_light_state.light_physical_feedback_capability &&
   a->data.bcm_light_state.light_type_quality==b->data.bcm_light_state.light_type_quality &&
   a->data.bcm_light_state.light_level_quality==b->data.bcm_light_state.light_level_quality &&
   a->data.bcm_light_state.light_color_quality==b->data.bcm_light_state.light_color_quality &&
   a->data.bcm_light_state.light_apply_quality==b->data.bcm_light_state.light_apply_quality &&
   a->data.bcm_light_state.update_sequence==b->data.bcm_light_state.update_sequence &&
   a->data.bcm_light_state.source_age_ms==b->data.bcm_light_state.source_age_ms;
 case 8u: return a->data.bcm_result.boot_id==b->data.bcm_result.boot_id &&
   a->data.bcm_result.tx_sequence==b->data.bcm_result.tx_sequence &&
   a->data.bcm_result.command_domain_boot==b->data.bcm_result.command_domain_boot &&
   a->data.bcm_result.command_id==b->data.bcm_result.command_id &&
   a->data.bcm_result.command_kind==b->data.bcm_result.command_kind &&
   a->data.bcm_result.result==b->data.bcm_result.result &&
   a->data.bcm_result.result_reason==b->data.bcm_result.result_reason &&
   a->data.bcm_result.door_event==b->data.bcm_result.door_event &&
   a->data.bcm_result.occurrence_id==b->data.bcm_result.occurrence_id &&
   a->data.bcm_result.event_age_ms==b->data.bcm_result.event_age_ms &&
   a->data.bcm_result.event_quality==b->data.bcm_result.event_quality &&
   a->data.bcm_result.door_target==b->data.bcm_result.door_target;
 case 9u: return a->data.bcm_event.boot_id==b->data.bcm_event.boot_id &&
   a->data.bcm_event.tx_sequence==b->data.bcm_event.tx_sequence &&
   a->data.bcm_event.event_kind==b->data.bcm_event.event_kind &&
   a->data.bcm_event.occurrence_id==b->data.bcm_event.occurrence_id &&
   a->data.bcm_event.event_age_ms==b->data.bcm_event.event_age_ms &&
   a->data.bcm_event.event_quality==b->data.bcm_event.event_quality &&
   a->data.bcm_event.related_domain_boot==b->data.bcm_event.related_domain_boot &&
   a->data.bcm_event.related_command_id==b->data.bcm_event.related_command_id &&
   a->data.bcm_event.affected_function==b->data.bcm_event.affected_function;
 case 10u: return a->data.bcm_status.boot_id==b->data.bcm_status.boot_id &&
   a->data.bcm_status.tx_sequence==b->data.bcm_status.tx_sequence &&
   a->data.bcm_status.bcm_ecu_state==b->data.bcm_status.bcm_ecu_state &&
   a->data.bcm_status.fault_mask==b->data.bcm_status.fault_mask &&
   a->data.bcm_status.active_fault_category==b->data.bcm_status.active_fault_category &&
   a->data.bcm_status.active_fault_code==b->data.bcm_status.active_fault_code &&
   a->data.bcm_status.affected_function==b->data.bcm_status.affected_function &&
   a->data.bcm_status.recovering==b->data.bcm_status.recovering &&
   a->data.bcm_status.update_sequence==b->data.bcm_status.update_sequence &&
   a->data.bcm_status.source_age_ms==b->data.bcm_status.source_age_ms;
 case 11u: return a->data.cis_environment.boot_id==b->data.cis_environment.boot_id &&
   a->data.cis_environment.tx_sequence==b->data.cis_environment.tx_sequence &&
   a->data.cis_environment.temperature_centi_c==b->data.cis_environment.temperature_centi_c &&
   a->data.cis_environment.temperature_sequence==b->data.cis_environment.temperature_sequence &&
   a->data.cis_environment.temperature_age_ms==b->data.cis_environment.temperature_age_ms &&
   a->data.cis_environment.temperature_validity==b->data.cis_environment.temperature_validity &&
   a->data.cis_environment.temperature_reason==b->data.cis_environment.temperature_reason &&
   a->data.cis_environment.humidity_centi_percent==b->data.cis_environment.humidity_centi_percent &&
   a->data.cis_environment.humidity_sequence==b->data.cis_environment.humidity_sequence &&
   a->data.cis_environment.humidity_age_ms==b->data.cis_environment.humidity_age_ms &&
   a->data.cis_environment.humidity_validity==b->data.cis_environment.humidity_validity &&
   a->data.cis_environment.humidity_reason==b->data.cis_environment.humidity_reason &&
   a->data.cis_environment.illuminance_lux==b->data.cis_environment.illuminance_lux &&
   a->data.cis_environment.illuminance_sequence==b->data.cis_environment.illuminance_sequence &&
   a->data.cis_environment.illuminance_age_ms==b->data.cis_environment.illuminance_age_ms &&
   a->data.cis_environment.illuminance_validity==b->data.cis_environment.illuminance_validity &&
   a->data.cis_environment.illuminance_reason==b->data.cis_environment.illuminance_reason;
 case 12u: return a->data.cis_occupant.boot_id==b->data.cis_occupant.boot_id &&
   a->data.cis_occupant.tx_sequence==b->data.cis_occupant.tx_sequence &&
   a->data.cis_occupant.pi_boot_id==b->data.cis_occupant.pi_boot_id &&
   a->data.cis_occupant.vision_sequence==b->data.cis_occupant.vision_sequence &&
   a->data.cis_occupant.vision_age_ms==b->data.cis_occupant.vision_age_ms &&
   a->data.cis_occupant.occupant_presence==b->data.cis_occupant.occupant_presence &&
   a->data.cis_occupant.presence_validity==b->data.cis_occupant.presence_validity &&
   a->data.cis_occupant.presence_reason==b->data.cis_occupant.presence_reason &&
   a->data.cis_occupant.occupant_count==b->data.cis_occupant.occupant_count &&
   a->data.cis_occupant.count_validity==b->data.cis_occupant.count_validity &&
   a->data.cis_occupant.count_reason==b->data.cis_occupant.count_reason &&
   a->data.cis_occupant.vision_function_status==b->data.cis_occupant.vision_function_status;
 case 13u: return a->data.cis_rear.boot_id==b->data.cis_rear.boot_id &&
   a->data.cis_rear.tx_sequence==b->data.cis_rear.tx_sequence &&
   a->data.cis_rear.update_sequence==b->data.cis_rear.update_sequence &&
   a->data.cis_rear.source_age_ms==b->data.cis_rear.source_age_ms &&
   a->data.cis_rear.rear_distance_deci_cm==b->data.cis_rear.rear_distance_deci_cm &&
   a->data.cis_rear.validity==b->data.cis_rear.validity &&
   a->data.cis_rear.quality_reason==b->data.cis_rear.quality_reason &&
   a->data.cis_rear.proximity_status==b->data.cis_rear.proximity_status;
 case 14u: return a->data.cis_status.boot_id==b->data.cis_status.boot_id &&
   a->data.cis_status.tx_sequence==b->data.cis_status.tx_sequence &&
   a->data.cis_status.cis_state==b->data.cis_status.cis_state &&
   a->data.cis_status.vision_status==b->data.cis_status.vision_status &&
   a->data.cis_status.temperature_status==b->data.cis_status.temperature_status &&
   a->data.cis_status.humidity_status==b->data.cis_status.humidity_status &&
   a->data.cis_status.illuminance_status==b->data.cis_status.illuminance_status &&
   a->data.cis_status.rear_status==b->data.cis_status.rear_status &&
   a->data.cis_status.pi_uart_status==b->data.cis_status.pi_uart_status &&
   a->data.cis_status.can_interface_status==b->data.cis_status.can_interface_status &&
   a->data.cis_status.fault_mask==b->data.cis_status.fault_mask &&
   a->data.cis_status.active_fault==b->data.cis_status.active_fault &&
   a->data.cis_status.recovering==b->data.cis_status.recovering &&
   a->data.cis_status.last_fault==b->data.cis_status.last_fault &&
   a->data.cis_status.last_fault_occurrence==b->data.cis_status.last_fault_occurrence &&
   a->data.cis_status.affected_function==b->data.cis_status.affected_function &&
   a->data.cis_status.update_sequence==b->data.cis_status.update_sequence &&
   a->data.cis_status.source_age_ms==b->data.cis_status.source_age_ms;
 case 15u: return a->data.window_state.boot_id==b->data.window_state.boot_id &&
   a->data.window_state.tx_sequence==b->data.window_state.tx_sequence &&
   a->data.window_state.motion_state==b->data.window_state.motion_state &&
   a->data.window_state.window_ecu_state==b->data.window_state.window_ecu_state &&
   a->data.window_state.window_position==b->data.window_state.window_position &&
   a->data.window_state.window_value_quality==b->data.window_state.window_value_quality &&
   a->data.window_state.fully_open_state==b->data.window_state.fully_open_state &&
   a->data.window_state.fully_closed_state==b->data.window_state.fully_closed_state &&
   a->data.window_state.antipinch_protection_status==b->data.window_state.antipinch_protection_status &&
   a->data.window_state.reverse_state==b->data.window_state.reverse_state &&
   a->data.window_state.update_sequence==b->data.window_state.update_sequence &&
   a->data.window_state.source_age_ms==b->data.window_state.source_age_ms;
 case 16u: return a->data.window_event.boot_id==b->data.window_event.boot_id &&
   a->data.window_event.tx_sequence==b->data.window_event.tx_sequence &&
   a->data.window_event.event_kind==b->data.window_event.event_kind &&
   a->data.window_event.target_channel==b->data.window_event.target_channel &&
   a->data.window_event.occurrence_id==b->data.window_event.occurrence_id &&
   a->data.window_event.related_domain_boot==b->data.window_event.related_domain_boot &&
   a->data.window_event.related_request_sequence==b->data.window_event.related_request_sequence &&
   a->data.window_event.origin==b->data.window_event.origin &&
   a->data.window_event.event_age_ms==b->data.window_event.event_age_ms &&
   a->data.window_event.event_quality==b->data.window_event.event_quality;
 case 17u: return a->data.window_result.boot_id==b->data.window_result.boot_id &&
   a->data.window_result.tx_sequence==b->data.window_result.tx_sequence &&
   a->data.window_result.request_domain_boot==b->data.window_result.request_domain_boot &&
   a->data.window_result.request_sequence==b->data.window_result.request_sequence &&
   a->data.window_result.target_channel==b->data.window_result.target_channel &&
   a->data.window_result.result==b->data.window_result.result &&
   a->data.window_result.result_reason==b->data.window_result.result_reason;
 case 18u: return a->data.window_fault.boot_id==b->data.window_fault.boot_id &&
   a->data.window_fault.tx_sequence==b->data.window_fault.tx_sequence &&
   a->data.window_fault.fault_category==b->data.window_fault.fault_category &&
   a->data.window_fault.fault_code==b->data.window_fault.fault_code &&
   a->data.window_fault.fault_status==b->data.window_fault.fault_status &&
   a->data.window_fault.occurrence_id==b->data.window_fault.occurrence_id &&
   a->data.window_fault.update_sequence==b->data.window_fault.update_sequence &&
   a->data.window_fault.source_age_ms==b->data.window_fault.source_age_ms;
 case 19u: return a->data.vss_event.boot_id==b->data.vss_event.boot_id &&
   a->data.vss_event.tx_sequence==b->data.vss_event.tx_sequence &&
   a->data.vss_event.event_kind==b->data.vss_event.event_kind &&
   a->data.vss_event.occurrence_id==b->data.vss_event.occurrence_id &&
   a->data.vss_event.original_age_ms==b->data.vss_event.original_age_ms &&
   a->data.vss_event.use_limit_ms==b->data.vss_event.use_limit_ms &&
   a->data.vss_event.value_quality==b->data.vss_event.value_quality &&
   a->data.vss_event.input_availability==b->data.vss_event.input_availability &&
   a->data.vss_event.related_command_id==b->data.vss_event.related_command_id;
 case 20u: return a->data.vss_warning_state.boot_id==b->data.vss_warning_state.boot_id &&
   a->data.vss_warning_state.tx_sequence==b->data.vss_warning_state.tx_sequence &&
   a->data.vss_warning_state.antipinch_source_boot==b->data.vss_warning_state.antipinch_source_boot &&
   a->data.vss_warning_state.antipinch_source_sequence==b->data.vss_warning_state.antipinch_source_sequence &&
   a->data.vss_warning_state.antipinch_original_age_ms==b->data.vss_warning_state.antipinch_original_age_ms &&
   a->data.vss_warning_state.antipinch_state==b->data.vss_warning_state.antipinch_state &&
   a->data.vss_warning_state.antipinch_quality==b->data.vss_warning_state.antipinch_quality &&
   a->data.vss_warning_state.antipinch_reason==b->data.vss_warning_state.antipinch_reason &&
   a->data.vss_warning_state.antipinch_availability==b->data.vss_warning_state.antipinch_availability &&
   a->data.vss_warning_state.occupant_hazard_source_boot==b->data.vss_warning_state.occupant_hazard_source_boot &&
   a->data.vss_warning_state.occupant_hazard_source_sequence==b->data.vss_warning_state.occupant_hazard_source_sequence &&
   a->data.vss_warning_state.occupant_hazard_original_age_ms==b->data.vss_warning_state.occupant_hazard_original_age_ms &&
   a->data.vss_warning_state.occupant_hazard_state==b->data.vss_warning_state.occupant_hazard_state &&
   a->data.vss_warning_state.occupant_hazard_quality==b->data.vss_warning_state.occupant_hazard_quality &&
   a->data.vss_warning_state.occupant_hazard_reason==b->data.vss_warning_state.occupant_hazard_reason &&
   a->data.vss_warning_state.occupant_hazard_availability==b->data.vss_warning_state.occupant_hazard_availability &&
   a->data.vss_warning_state.rear_obstacle_source_boot==b->data.vss_warning_state.rear_obstacle_source_boot &&
   a->data.vss_warning_state.rear_obstacle_source_sequence==b->data.vss_warning_state.rear_obstacle_source_sequence &&
   a->data.vss_warning_state.rear_obstacle_original_age_ms==b->data.vss_warning_state.rear_obstacle_original_age_ms &&
   a->data.vss_warning_state.rear_obstacle_state==b->data.vss_warning_state.rear_obstacle_state &&
   a->data.vss_warning_state.rear_obstacle_quality==b->data.vss_warning_state.rear_obstacle_quality &&
   a->data.vss_warning_state.rear_obstacle_reason==b->data.vss_warning_state.rear_obstacle_reason &&
   a->data.vss_warning_state.rear_obstacle_availability==b->data.vss_warning_state.rear_obstacle_availability &&
   a->data.vss_warning_state.rear_detection_activation==b->data.vss_warning_state.rear_detection_activation &&
   a->data.vss_warning_state.activation_meta_format==b->data.vss_warning_state.activation_meta_format &&
   a->data.vss_warning_state.rear_activation_quality==b->data.vss_warning_state.rear_activation_quality &&
   a->data.vss_warning_state.rear_activation_reason==b->data.vss_warning_state.rear_activation_reason &&
   a->data.vss_warning_state.rear_activation_sequence==b->data.vss_warning_state.rear_activation_sequence &&
   a->data.vss_warning_state.rear_activation_age_ms==b->data.vss_warning_state.rear_activation_age_ms;
 case 21u: return a->data.vss_status.boot_id==b->data.vss_status.boot_id &&
   a->data.vss_status.tx_sequence==b->data.vss_status.tx_sequence &&
   a->data.vss_status.vss_state==b->data.vss_status.vss_state &&
   a->data.vss_status.vss_availability==b->data.vss_status.vss_availability &&
   a->data.vss_status.vss_accepting_events==b->data.vss_status.vss_accepting_events &&
   a->data.vss_status.vss_fault_active==b->data.vss_status.vss_fault_active &&
   a->data.vss_status.fault_mask==b->data.vss_status.fault_mask &&
   a->data.vss_status.vss_last_fault==b->data.vss_status.vss_last_fault &&
   a->data.vss_status.last_fault_occurrence==b->data.vss_status.last_fault_occurrence &&
   a->data.vss_status.last_fault_age_ms==b->data.vss_status.last_fault_age_ms &&
   a->data.vss_status.last_fault_quality==b->data.vss_status.last_fault_quality &&
   a->data.vss_status.update_sequence==b->data.vss_status.update_sequence &&
   a->data.vss_status.source_age_ms==b->data.vss_status.source_age_ms;
 default: return 0;
 }
}
typedef struct {
 uint64_t now; size_t submitted; const NetUpperPacket *borrowed; NetTxKey key;
} FakePort;
typedef struct {
 size_t can_calls,rear_calls,uart_calls; NetUpperCanMessage value; NetUpperRear rear;
 NetUpperUartDelivery uart;
 NetUpperDelivery delivery;
} FakeOwner;
static NetUpper tx,rx,pi;
static FakePort tx_port,rx_port,pi_port;
static FakeOwner owner,bridge_owner;
static NetStatus fake_now(void *context,uint64_t *out)
{ *out=((FakePort *)context)->now; return NET_OK; }
static NetStatus fake_submit(void *context,const NetUpperPacket *packet,NetTxKey key)
{
 FakePort *p=context;
 if(p->borrowed!=NULL) return NET_NO_CAPACITY;
 p->borrowed=packet; p->key=key; ++p->submitted; return NET_OK;
}
static NetStatus fake_poll(void *context,NetUpperEvent *out)
{ (void)context; (void)out; return NET_NO_WORK; }
static const NetUpperPortOps port_ops={fake_now,fake_submit,fake_poll};
static NetStatus fake_can(void *context,const NetUpperCanMessage *value,const NetUpperDelivery *delivery)
{
 FakeOwner *o=context; ++o->can_calls; o->value=*value; o->delivery=*delivery;
 return NET_OK;
}
static NetStatus fake_rear(void *context,const NetUpperRear *value,const NetUpperDelivery *delivery)
{
 FakeOwner *o=context; ++o->rear_calls; o->rear=*value; o->delivery=*delivery;
 return NET_OK;
}
static NetStatus fake_uart(void *context,const NetUpperUartDelivery *value,const NetUpperDelivery *delivery)
{
 FakeOwner *o=context; ++o->uart_calls; o->uart=*value; o->delivery=*delivery;
 return NET_OK;
}
static NetStatus initialize(NetUpper *s,FakePort *p,uint32_t node,FakeOwner *o)
{
 NetUpperConfig c={0}; size_t i;
 memset(s,0,sizeof(*s)); memset(p,0,sizeof(*p)); p->now=100u;
 c.node=node; c.local_epoch=9u; c.profile=NET_WIRE_CANDIDATE_ONLY;
 c.port.ops=&port_ops; c.port.context=p;
 for(i=0u;i<3u;++i) c.port_epoch[i]=1u;
 for(i=0u;i<2u;++i) {
  c.uart_policy[i].candidate_time_policy_selected=true;
  c.uart_policy[i].assembly_limit_ms=1000u;c.uart_policy[i].expires_at_equality=true;
 }
 /* Explicit test Owner policy. VSS WARNING requires per-field handling because
  * its mandatory disabled A1 lane carries AGE=65535. No ECU policy is adopted. */
 for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i) {
  const NetWireRegistryEntry *r=net_wire_registry_at(i);
  c.rx_policy[i].bound=true; c.rx_policy[i].maximum_age_ms=500u;
  c.rx_policy[i].expires_at_equality=true;
  c.rx_per_field_age[i]=r->link==NET_WIRE_CAN && r->id_type==0x0c0u;
 }
 if(o!=NULL) {
  memset(o,0,sizeof(*o)); c.owner.context=o;
  c.owner.can=fake_can; c.owner.rear=fake_rear;c.owner.uart=fake_uart;
 }
 return net_upper_init(s,&c);
}
static int bootstrap_vision(const NetUpperCanMessage *occupant)
{
 NetUpperUartMessage m={0};NetSourceStamp source={1u,100u,0u};
 NetAgePolicy policy={true,500u,true};NetTxKey key;NetUpperEvent e={0};size_t i;
 const NetWireCisOccupantRaw *v=&occupant->data.cis_occupant;
 CHECK(initialize(&pi,&pi_port,NET_WIRE_NODE_PI,NULL)==NET_OK);
 m.link=NET_WIRE_UART_C;m.type=0x60u;m.sequence=1u;
 m.data.p_vision_result.pi_boot_id=v->pi_boot_id;
 m.data.p_vision_result.vision_sequence=v->vision_sequence;
 m.data.p_vision_result.vision_age_ms=v->vision_age_ms;
 m.data.p_vision_result.occupant_presence=v->occupant_presence;
 m.data.p_vision_result.presence_validity=v->presence_validity;
 m.data.p_vision_result.presence_reason=v->presence_reason;
 m.data.p_vision_result.occupant_count=v->occupant_count;
 m.data.p_vision_result.count_validity=v->count_validity;
 m.data.p_vision_result.count_reason=v->count_reason;
 m.data.p_vision_result.vision_status=v->vision_function_status;
 source.original_age_ms=v->vision_age_ms;
 CHECK(net_upper_submit_uart(&pi,&m,&source,&policy,&key)==NET_OK);
 CHECK(net_upper_step(&pi,32u)==NET_OK && pi_port.borrowed!=NULL);
 e.kind=NET_UPPER_UART_BYTE;e.link=NET_WIRE_UART_C;
 e.capture.timestamp_known=true;e.capture.port_epoch=1u;e.capture.captured_at_ms=100u;
 for(i=0u;i<pi_port.borrowed->uart.length;++i) {
  e.byte=pi_port.borrowed->uart.bytes[i];CHECK(net_upper_capture(&tx,&e)==NET_OK);
 }
 CHECK(net_upper_step(&tx,32u)==NET_OK);
 CHECK(bridge_owner.uart_calls==1u && bridge_owner.uart.message.type==0x60u);
 CHECK(bridge_owner.delivery.receipt_id==1u && bridge_owner.delivery.capture.captured_at_ms==100u);
 return 0;
}
static NetUpperEvent frame_event(const NetWireCanEnvelope *frame)
{
 NetUpperEvent e={0}; e.kind=NET_UPPER_CAN_RX; e.link=NET_WIRE_CAN;
 e.can=*frame; e.capture.timestamp_known=true;
 e.capture.port_epoch=1u; e.capture.captured_at_ms=103u; return e;
}
static int check_owner(size_t index)
{
 const Fixture *f=&fixtures[index]; NetUpperCanMessage expected,stored;
 NetUpperDelivery delivery; NetSourceStamp source;
 uint64_t wire_age=f->age==UINT16_MAX?UINT16_MAX:3u;
 fixture_message(index,3u,&expected);
 if(f->id==0x0a0u) {
  CHECK(owner.rear_calls==1u && owner.can_calls==0u);
  CHECK(owner.rear.rear_distance_mm==123u && owner.rear.distance_available);
  CHECK(owner.rear.boot_id==41u && owner.rear.tx_sequence==1u && owner.rear.update_sequence==77u);
  CHECK(owner.rear.source_age_ms==5u);
 } else {
  CHECK(owner.can_calls==1u && owner.rear_calls==0u);
  CHECK(equal_typed(index,&owner.value,&expected));
 }
 CHECK(owner.delivery.capture.captured_at_ms==103u);
 CHECK(owner.delivery.delivered_at_ms==105u && owner.delivery.local_elapsed_ms==2u);
 CHECK(owner.delivery.age_ms==wire_age+2u && owner.delivery.receipt_id==1u);
 CHECK(owner.delivery.freshness_known==(f->age!=UINT16_MAX));
 CHECK(owner.delivery.per_field_age_required);
 CHECK(net_upper_received_can(&rx,f->id,&stored,&delivery,&source)==NET_OK);
 CHECK(equal_typed(index,&stored,&expected));
 CHECK(source.observation_id==1u && source.observed_at_ms==103u && source.original_age_ms==wire_age);
 return 0;
}
static int run_route(size_t index)
{
 const Fixture *f=&fixtures[index]; NetUpperCanMessage m; NetSourceStamp source={7u,100u,0u};
 NetAgePolicy age={true,70000u,true}; NetTxKey key={99u,99u};
 NetUpperTxSnapshot state; NetUpperObservation observation; NetUpperEvent e;
 NetWireCanEnvelope literal={0}; size_t part;
 CHECK(initialize(&tx,&tx_port,f->sender,f->id==0x310u?&bridge_owner:NULL)==NET_OK);
 CHECK(initialize(&rx,&rx_port,f->receiver,&owner)==NET_OK);
 fixture_message(index,0u,&m); source.original_age_ms=f->age;
 if(f->hold) {
  CHECK(net_upper_submit_can(&tx,&m,&source,&age,&key)==NET_HOLD);
  CHECK(key.epoch==99u && key.token==99u && tx_port.submitted==0u);
  literal.standard_id=f->id; literal.dlc=f->dlc; literal.length=f->length;
  literal.fdf=true; literal.brs=true; memcpy(literal.bytes,f->initial,f->length);
  e=frame_event(&literal); CHECK(net_upper_capture(&rx,&e)==NET_OK);
  rx_port.now=105u; (void)net_upper_step(&rx,32u);
  CHECK(owner.can_calls==0u && owner.rear_calls==0u);
  CHECK(net_upper_observation(&rx,&observation)==NET_OK && observation.rx_held==1u);
  printf("{\"route\":\"%s\",\"id\":%u,\"tx_hold\":true,\"rx_hold\":true,\"feature_status\":\"DESIGN_ONLY_DISABLED\",\"result\":\"PASS\"}\n",f->name,f->id);
  return 0;
 }
 if(f->id==0x310u) {
  NetUpperCanMessage forged;NetSourceStamp changed;
  CHECK(bootstrap_vision(&m)==0);source.observation_id=1u;
  forged=m;++forged.data.cis_occupant.vision_sequence;
  CHECK(net_upper_submit_can(&tx,&forged,&source,&age,&key)==NET_HOLD);
  CHECK(key.epoch==99u && key.token==99u && tx.next_token==1u);
  forged=m;forged.data.cis_occupant.vision_age_ms=1u;changed=source;changed.original_age_ms=1u;
  CHECK(net_upper_submit_can(&tx,&forged,&changed,&age,&key)==NET_HOLD);
  CHECK(key.epoch==99u && key.token==99u && tx.next_token==1u);
  CHECK(CisNet_PublishOccupant(&tx,m.data.cis_occupant.boot_id,m.data.cis_occupant.tx_sequence,&age,&key)==NET_OK);
 } else CHECK(net_upper_submit_can(&tx,&m,&source,&age,&key)==NET_OK);
 CHECK(net_upper_tx_snapshot(&tx,key,&state)==NET_OK && state.queued && !state.submitted);
 CHECK(tx_port.submitted==0u);
 memset(&m,0xa5,sizeof(m)); memset(&source,0xa5,sizeof(source));
 tx_port.now=103u; CHECK(net_upper_step(&tx,32u)==NET_OK);
 CHECK(tx_port.submitted==1u && tx_port.borrowed!=NULL);
 CHECK(tx_port.borrowed->link==NET_WIRE_CAN && tx_port.borrowed->direction==NET_WIRE_CAN_DIRECTION);
 CHECK(tx_port.borrowed->can.standard_id==f->id && tx_port.borrowed->can.dlc==f->dlc);
 CHECK(tx_port.borrowed->can.length==f->length && tx_port.borrowed->can.fdf && tx_port.borrowed->can.brs);
 CHECK(!tx_port.borrowed->can.extended && !tx_port.borrowed->can.rtr && !tx_port.borrowed->can.driver_error);
 CHECK(memcmp(tx_port.borrowed->can.bytes,f->dispatched,f->length)==0);
 e=frame_event(&tx_port.borrowed->can); CHECK(net_upper_capture(&rx,&e)==NET_OK);
 memset(&e,0xa5,sizeof(e)); rx_port.now=105u;
 CHECK(net_upper_step(&rx,32u)==NET_OK); CHECK(check_owner(index)==0);
 CHECK(net_upper_tx_snapshot(&tx,key,&state)==NET_OK && state.outcome==NET_TX_UNKNOWN);
 CHECK(!state.admission_known && !state.reference_released);
 CHECK(net_upper_retire(&tx,key)==NET_HOLD);
 for(part=0u;part<3u;++part) {
  memset(&e,0,sizeof(e)); e.kind=NET_UPPER_TX_EVENT; e.link=NET_WIRE_CAN; e.tx.key=key;
  if(part==0u) { e.tx.admission_observed=true; e.tx.accepted=true; }
  if(part==1u) e.tx.outcome=NET_TX_SUCCEEDED;
  if(part==2u) { tx_port.borrowed=NULL; e.tx.release_observed=true; }
  CHECK(net_upper_capture(&tx,&e)==NET_OK); CHECK(net_upper_step(&tx,32u)==NET_OK);
  if(part<2u) CHECK(net_upper_retire(&tx,key)==NET_HOLD);
 }
 CHECK(net_upper_tx_snapshot(&tx,key,&state)==NET_OK && state.accepted && state.outcome==NET_TX_SUCCEEDED && state.reference_released);
 CHECK(net_upper_retire(&tx,key)==NET_OK);
 printf("{\"route\":\"%s\",\"id\":%u,\"sender\":%lu,\"receiver\":%lu,\"path\":\"submit_queue_codec_port_capture_router_adapter_owner\",\"wire_literal_match\":true,\"tx_elapsed\":3,\"rx_elapsed\":2,\"all_typed_fields_equal\":true,\"remote_execution_claimed\":false,\"result\":\"PASS\"}\n",f->name,f->id,(unsigned long)f->sender,(unsigned long)f->receiver);
 return 0;
}
static int permission_fanout(void)
{
 static const uint32_t receivers[]={NET_WIRE_NODE_BCM,NET_WIRE_NODE_CIS,NET_WIRE_NODE_VSS,NET_WIRE_NODE_WINDOW};
 NetUpperCanMessage message; NetSourceStamp source={1u,100u,0u}; NetAgePolicy age={true,500u,true};
 NetTxKey key; NetUpperEvent e; NetUpperObservation observation; size_t i;
 CHECK(initialize(&tx,&tx_port,NET_WIRE_NODE_DOMAIN,NULL)==NET_OK);
 fixture_message(0u,0u,&message);
 CHECK(message.id==0x080u && message.data.domain_permission.vehicle_power_permission==0u);
 CHECK(net_upper_submit_can(&tx,&message,&source,&age,&key)==NET_OK);
 tx_port.now=103u; CHECK(net_upper_step(&tx,32u)==NET_OK);
 CHECK(tx_port.borrowed!=NULL);
 for(i=0u;i<4u;++i) {
  CHECK(initialize(&rx,&rx_port,receivers[i],&owner)==NET_OK);
  e=frame_event(&tx_port.borrowed->can); CHECK(net_upper_capture(&rx,&e)==NET_OK);
  rx_port.now=105u; (void)net_upper_step(&rx,32u);
  if(receivers[i]==NET_WIRE_NODE_WINDOW) {
   CHECK(owner.can_calls==0u && owner.rear_calls==0u);
   CHECK(net_upper_observation(&rx,&observation)==NET_OK && observation.rx_held==1u);
  } else CHECK(check_owner(0u)==0);
  printf("{\"route\":\"domain_permission\",\"receiver\":%lu,\"owner_deny_only\":true,\"status\":\"%s\",\"result\":\"PASS\"}\n",(unsigned long)receivers[i],receivers[i]==NET_WIRE_NODE_WINDOW?"HOLD":"DELIVERED");
 }
 return 0;
}
static int partial_lanes(void)
{
 size_t index,route; NetUpperCanMessage m; NetSourceStamp source={1u,100u,UINT16_MAX};
 NetAgePolicy age={true,70000u,true}; NetTxKey key; NetUpperEvent e; NetUpperObservation obs;
 for(index=0u;index<sizeof(fixtures)/sizeof(fixtures[0]);++index)
  if(fixtures[index].id==0x0c0u) break;
 CHECK(index<sizeof(fixtures)/sizeof(fixtures[0]));
 for(route=0u;route<net_wire_registry_count();++route) {
  const NetWireRegistryEntry *r=net_wire_registry_at(route);
  if(r->link==NET_WIRE_CAN && r->id_type==0x0c0u) break;
 }
 CHECK(route<NET_UPPER_ROUTE_COUNT);
 CHECK(initialize(&tx,&tx_port,NET_WIRE_NODE_DOMAIN,NULL)==NET_OK);
 fixture_message(index,0u,&m);
 CHECK(net_upper_submit_can(&tx,&m,&source,&age,&key)==NET_OK);
 tx_port.now=103u; CHECK(net_upper_step(&tx,32u)==NET_OK);
 CHECK(tx_port.borrowed!=NULL); e=frame_event(&tx_port.borrowed->can);
 CHECK(initialize(&rx,&rx_port,NET_WIRE_NODE_VSS,&owner)==NET_OK);
 rx.config.rx_per_field_age[route]=false;
 CHECK(net_upper_capture(&rx,&e)==NET_OK); rx_port.now=105u; (void)net_upper_step(&rx,32u);
 CHECK(owner.can_calls==0u); CHECK(net_upper_observation(&rx,&obs)==NET_OK && obs.rx_held==1u);
 CHECK(initialize(&rx,&rx_port,NET_WIRE_NODE_VSS,&owner)==NET_OK);
 CHECK(net_upper_capture(&rx,&e)==NET_OK); rx_port.now=105u; CHECK(net_upper_step(&rx,32u)==NET_OK);
 CHECK(check_owner(index)==0);
 CHECK(!owner.delivery.freshness_known && owner.delivery.per_field_age_required);
 CHECK(owner.value.data.vss_warning_state.antipinch_original_age_ms==UINT16_MAX);
 CHECK(owner.value.data.vss_warning_state.antipinch_quality==3u);
 CHECK(owner.value.data.vss_warning_state.occupant_hazard_original_age_ms==3u);
 CHECK(owner.value.data.vss_warning_state.rear_obstacle_original_age_ms==3u);
 CHECK(initialize(&rx,&rx_port,NET_WIRE_NODE_VSS,&owner)==NET_OK);
 CHECK(net_upper_capture(&rx,&e)==NET_OK); rx_port.now=603u; (void)net_upper_step(&rx,32u);
 CHECK(owner.can_calls==0u); CHECK(net_upper_observation(&rx,&obs)==NET_OK && obs.rx_held==1u);
 printf("{\"case\":\"partial_lanes\",\"default_aggregate\":\"HOLD\",\"explicit_owner_per_field\":\"DELIVERED_NO_FRESHNESS_CLAIM\",\"local_backlog_500\":\"HOLD\",\"a1_disabled_preserved\":true,\"result\":\"PASS\"}\n");
 return 0;
}
int main(int argc,char **argv)
{
 size_t i; if(argc!=2) return 2; case_name=argv[1];
 if(strcmp(case_name,"permission_fanout")==0) return permission_fanout();
 if(strcmp(case_name,"partial_lanes")==0) return partial_lanes();
 for(i=0u;i<sizeof(fixtures)/sizeof(fixtures[0]);++i)
  if(strcmp(case_name,fixtures[i].name)==0) return run_route(i);
 return 2;
}

