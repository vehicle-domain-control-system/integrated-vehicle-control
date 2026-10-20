#include "network/net_upper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)
#define PROFILE NET_WIRE_CANDIDATE_ONLY
/* External owner sample inputs originate in independent frozen Golden vectors.
 * Product service/queue/router/codec/query are never replaced by a fake. */
static const uint8_t c_domain_permission[]={0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x01u,0x00u,0xffu,0x00u,0x03u,0xefu,0xcdu,0xabu,0x89u,0xdcu,0xfeu,0x00u};
static const uint8_t c_bcm_door_command[]={0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x6au,0x59u,0x8cu,0x7bu,0x01u,0x05u,0x00u,0x00u};
static const uint8_t c_bcm_climate_command[]={0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x6au,0x59u,0x8cu,0x7bu,0x03u,0x02u,0x64u,0x00u};
static const uint8_t c_bcm_light_command[]={0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x6au,0x59u,0x8cu,0x7bu,0x02u,0x25u,0x12u,0x34u,0x56u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u};
static const uint8_t c_window_command[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x34u,0x12u,0x78u,0x56u,0x05u,0x00u,0x25u,0x04u};
static const uint8_t c_bcm_door_state[]={0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xffu,0x01u,0x02u,0x03u,0x01u,0x02u,0xaau,0xbbu,0xccu,0xddu,0x9au,0xbcu};
static const uint8_t c_bcm_climate_state[]={0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x02u,0xffu,0x01u,0x25u,0xffu,0x00u,0x03u,0x01u,0x02u,0x03u,0xaau,0xbbu,0xccu,0xddu,0x9au,0xbcu};
static const uint8_t c_bcm_light_state[]={0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x03u,0x49u,0x12u,0x34u,0x56u,0xffu,0x00u,0x01u,0x02u,0x03u,0x03u,0xaau,0xbbu,0xccu,0xddu,0x9au,0xbcu,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u};
static const uint8_t c_bcm_result[]={0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xc4u,0xb3u,0xa2u,0x91u,0x45u,0x36u,0x27u,0x18u,0x01u,0x02u,0x09u,0x00u,0x03u,0xb4u,0xc3u,0xd2u,0xe1u,0x9au,0x78u,0x01u,0x01u,0x00u,0x00u,0x00u};
static const uint8_t c_bcm_event[]={0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x02u,0x31u,0x32u,0x33u,0x34u,0x61u,0x62u,0x02u,0x71u,0x72u,0x73u,0x74u,0x81u,0x82u,0x83u,0x84u,0x02u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u};
static const uint8_t c_bcm_status[]={0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x03u,0xa5u,0x00u,0x03u,0x08u,0x00u,0x07u,0x01u,0xd4u,0xc3u,0xb2u,0xa1u,0xefu,0xcdu,0x00u,0x00u};
static const uint8_t c_cis_environment[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x60u,0xf0u,0x55u,0x44u,0x33u,0x22u,0x34u,0x12u,0x01u,0x00u,0x85u,0x1au,0x66u,0x55u,0x44u,0x33u,0x56u,0x34u,0x01u,0x00u,0x40u,0xe2u,0x01u,0x00u,0x77u,0x66u,0x55u,0x44u,0x78u,0x56u,0x01u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u};
static const uint8_t c_cis_occupant[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x34u,0x12u,0x01u,0x01u,0x00u,0x03u,0x01u,0x00u,0x02u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u};
static const uint8_t c_cis_rear[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0xd4u,0xc3u,0xb2u,0xa1u,0x34u,0x12u,0x7bu,0x00u,0x01u,0x00u,0x00u,0x00u};
static const uint8_t c_cis_status[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x03u,0x02u,0x01u,0x04u,0x03u,0x00u,0x02u,0x01u,0xa5u,0x01u,0x09u,0x00u,0x01u,0x08u,0x00u,0x88u,0x77u,0x66u,0x55u,0x2du,0xccu,0xbbu,0xaau,0x99u,0xdcu,0xfeu,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u};
static const uint8_t c_window_state[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x02u,0x02u,0x25u,0x01u,0x00u,0x00u,0x01u,0x01u,0xd4u,0xc3u,0xb2u,0xa1u,0xdcu,0xfeu,0x00u,0x00u};
static const uint8_t c_window_event[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x03u,0x00u,0xd4u,0xc3u,0xb2u,0xa1u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x05u,0x34u,0x12u,0x02u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u};
static const uint8_t c_window_result[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x00u,0x05u,0x19u,0x00u};
static const uint8_t c_window_fault[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x02u,0xffu,0x00u,0x00u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0xffu,0xffu,0x00u,0x00u};
static const uint8_t c_vss_event[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x05u,0x88u,0x77u,0x66u,0x55u,0x34u,0x12u,0x78u,0x56u,0x01u,0x01u,0xccu,0xbbu,0xaau,0x99u,0x00u};
static const uint8_t c_vss_warning_state[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0xffu,0xffu,0x00u,0x03u,0x06u,0x02u,0x55u,0x44u,0x33u,0x22u,0x66u,0x55u,0x44u,0x33u,0x34u,0x12u,0x01u,0x00u,0x00u,0x01u,0x88u,0x77u,0x66u,0x55u,0xccu,0xbbu,0xaau,0x99u,0x45u,0x23u,0x02u,0x00u,0x00u,0x00u,0x01u,0x01u,0x00u,0x00u,0xaau,0x99u,0x88u,0x77u,0x56u,0x34u,0x00u,0x00u,0x00u,0x00u};
static const uint8_t c_vss_status[]={0x04u,0x03u,0x02u,0x01u,0x44u,0x33u,0x22u,0x11u,0x02u,0x01u,0x01u,0x00u,0x00u,0x03u,0x00u,0x88u,0x77u,0x66u,0x55u,0xffu,0xffu,0x00u,0xccu,0xbbu,0xaau,0x99u,0x34u,0x12u,0x00u,0x00u,0x00u,0x00u};
static const uint8_t u_m_context[]={0xa5u,0x5au,0x02u,0x01u,0x26u,0x00u,0xa5u,0x5au,0x78u,0x56u,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x01u,0x01u,0x00u,0x00u,0x40u,0x30u,0x20u,0x10u,0x34u,0x12u,0x01u,0x01u,0x00u,0x80u,0x70u,0x60u,0x50u,0x78u,0x56u,0x00u,0xefu,0xcdu,0xabu,0x90u,0xbcu,0x9au,0x3du,0xfdu};
static const uint8_t u_m_request[]={0xa5u,0x5au,0x02u,0x10u,0x18u,0x00u,0xa5u,0x5au,0x78u,0x56u,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0xefu,0xcdu,0xabu,0x90u,0x02u,0x01u,0x2eu,0xfbu,0x00u,0x00u,0x00u,0x00u,0x81u,0x68u};
static const uint8_t u_m_query[]={0xa5u,0x5au,0x02u,0x11u,0x20u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x80u,0x70u,0x60u,0x50u,0x01u,0x00u,0x22u,0x33u,0x44u,0x55u,0x66u,0x77u,0x88u,0x99u,0xefu,0xcdu,0xabu,0x90u,0x00u,0x00u,0x9du,0x09u};
static const uint8_t u_m_warning_ack[]={0xa5u,0x5au,0x02u,0x12u,0x15u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x08u,0xefu,0xcdu,0xabu,0x90u,0x40u,0x30u,0x20u,0x10u,0x85u,0x0bu};
static const uint8_t u_m_domain_alive[]={0xa5u,0x5au,0x02u,0x02u,0x0cu,0x00u,0xa5u,0x5au,0x40u,0x30u,0x20u,0x10u,0xefu,0xcdu,0xabu,0x90u,0xffu,0xffu,0x02u,0x01u,0x65u,0x0fu};
static const uint8_t u_m_result[]={0xa5u,0x5au,0x02u,0x20u,0x2cu,0x00u,0xa5u,0x5au,0x78u,0x56u,0x34u,0x12u,0x0cu,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x07u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0xefu,0xcdu,0xabu,0x90u,0x02u,0x09u,0x00u,0x01u,0x01u,0x44u,0x33u,0x22u,0x11u,0x01u,0x34u,0x12u,0xa2u,0x5du};
static const uint8_t u_m_warning[]={0xa5u,0x5au,0x02u,0x21u,0x28u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x08u,0xefu,0xcdu,0xabu,0x90u,0x02u,0x01u,0x00u,0x01u,0x05u,0x87u,0x65u,0x43u,0x21u,0x98u,0xbau,0xdcu,0xfeu,0x34u,0x12u,0xb3u,0xcdu};
static const uint8_t u_m_availability[]={0xa5u,0x5au,0x02u,0x22u,0x3eu,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x01u,0x00u,0x00u,0x00u,0x02u,0x01u,0x11u,0x00u,0x03u,0x02u,0x0eu,0x00u,0x04u,0x00u,0x00u,0x00u,0x05u,0x01u,0x11u,0x00u,0x06u,0x02u,0x0eu,0x00u,0x07u,0x00u,0x00u,0x00u,0x08u,0x01u,0x11u,0x00u,0x09u,0x02u,0x0eu,0x00u,0xefu,0xcdu,0xabu,0x90u,0x34u,0x12u,0xe9u,0xcdu};
static const uint8_t u_m_digital_status[]={0xa5u,0x5au,0x02u,0x23u,0x1eu,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x00u,0x01u,0x11u,0x00u,0xefu,0xcdu,0xabu,0x90u,0x34u,0x12u,0x88u,0xa4u};
static const uint8_t u_m_digital_result[]={0xa5u,0x5au,0x02u,0x24u,0x23u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xefu,0xcdu,0xabu,0x90u,0x87u,0x65u,0x43u,0x21u,0x02u,0x02u,0x09u,0x00u,0x01u,0x34u,0x12u,0xc6u,0x9fu};
static const uint8_t u_m_user_settings[]={0xa5u,0x5au,0x02u,0x25u,0x24u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x2eu,0xfbu,0x00u,0x03u,0x01u,0x64u,0xa5u,0x5au,0xffu,0x00u,0xefu,0xcdu,0xabu,0x90u,0x34u,0x12u,0x41u,0x54u};
static const uint8_t u_m_query_end[]={0xa5u,0x5au,0x02u,0x40u,0x18u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x03u,0x02u,0x08u,0x00u,0xf7u,0x85u};
static const uint8_t u_m_bcm_door_state[]={0xa5u,0x5au,0x02u,0x30u,0x28u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x01u,0x00u,0x00u,0x00u,0x01u,0x02u,0x55u,0x66u,0x77u,0x88u,0xffu,0xffu,0x5au,0xd9u};
static const uint8_t u_m_bcm_climate_state[]={0xa5u,0x5au,0x02u,0x31u,0x2cu,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x03u,0xffu,0x01u,0x64u,0xffu,0x00u,0x03u,0x00u,0x01u,0x03u,0x78u,0x56u,0x34u,0x12u,0x34u,0x12u,0x49u,0x88u};
static const uint8_t u_m_bcm_light_state[]={0xa5u,0x5au,0x02u,0x32u,0x2du,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x02u,0x2au,0x11u,0x22u,0xffu,0x01u,0x00u,0x00u,0x01u,0x02u,0x03u,0x66u,0x77u,0x88u,0x99u,0xffu,0xffu,0xa8u,0x29u};
static const uint8_t u_m_bcm_status[]={0xa5u,0x5au,0x02u,0x33u,0x2au,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x02u,0x80u,0x00u,0x03u,0x08u,0x00u,0x07u,0x01u,0x34u,0x12u,0xcdu,0xabu,0xffu,0xffu,0x8cu,0x84u};
static const uint8_t u_m_cis_environment[]={0xa5u,0x5au,0x02u,0x34u,0x3cu,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x2eu,0xfbu,0x04u,0x03u,0x02u,0x01u,0x00u,0x00u,0x01u,0x00u,0x10u,0x27u,0x44u,0x33u,0x22u,0x11u,0xffu,0xffu,0x01u,0x00u,0x40u,0xe2u,0x01u,0x00u,0xd4u,0xc3u,0xb2u,0xa1u,0x7bu,0x00u,0x01u,0x00u,0x23u,0xabu};
static const uint8_t u_m_cis_occupant[]={0xa5u,0x5au,0x02u,0x35u,0x2du,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x88u,0x77u,0x66u,0x55u,0x44u,0x33u,0x22u,0x11u,0xffu,0xffu,0x01u,0x01u,0x00u,0x03u,0x01u,0x00u,0x02u,0x88u,0x2du};
static const uint8_t u_m_cis_rear[]={0xa5u,0x5au,0x02u,0x36u,0x27u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0xefu,0xbeu,0xadu,0xdeu,0xffu,0xffu,0x7bu,0x00u,0x01u,0x00u,0x00u,0xebu,0x32u};
static const uint8_t u_m_cis_status[]={0xa5u,0x5au,0x02u,0x37u,0x36u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x02u,0x02u,0x02u,0x01u,0x00u,0x04u,0x01u,0x02u,0x00u,0x01u,0x09u,0x00u,0x01u,0x07u,0x00u,0x21u,0x43u,0x65u,0x87u,0x3fu,0xffu,0xefu,0xcdu,0xabu,0xffu,0xffu,0x17u,0xc2u};
static const uint8_t u_m_vss_status[]={0xa5u,0x5au,0x02u,0x3au,0x30u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x01u,0x01u,0x01u,0x00u,0x00u,0x03u,0x00u,0x32u,0x54u,0x76u,0x98u,0xffu,0xffu,0x00u,0x40u,0x30u,0x20u,0x10u,0xd2u,0x04u,0x72u,0x8au};
static const uint8_t u_m_window_state[]={0xa5u,0x5au,0x02u,0x38u,0x2au,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x02u,0x01u,0x25u,0x01u,0x00u,0x00u,0x01u,0x01u,0xcdu,0xabu,0x34u,0x12u,0xffu,0xffu,0xe4u,0x64u};
static const uint8_t u_m_window_fault[]={0xa5u,0x5au,0x02u,0x39u,0x2au,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x03u,0x05u,0x00u,0x02u,0x40u,0x30u,0x20u,0x10u,0xd4u,0xc3u,0xb2u,0xa1u,0xffu,0xffu,0x11u,0xeeu};
static const uint8_t u_p_vision_result[]={0xa5u,0x5au,0x01u,0x60u,0x13u,0x00u,0xa5u,0x5au,0x78u,0x56u,0x34u,0x12u,0xefu,0xcdu,0xabu,0x90u,0x34u,0x12u,0x01u,0x01u,0x00u,0x03u,0x01u,0x00u,0x02u,0x00u,0x00u,0xa7u,0x60u};
static const uint8_t u_p_vision_status[]={0xa5u,0x5au,0x01u,0x61u,0x10u,0x00u,0xa5u,0x5au,0x78u,0xa5u,0x5au,0x12u,0xefu,0xcdu,0xabu,0x90u,0x01u,0x00u,0x00u,0x04u,0x00u,0x34u,0x12u,0x00u,0xe6u,0x74u};
static const uint8_t u_p_permission[]={0xa5u,0x5au,0x01u,0x62u,0x0fu,0x00u,0xa5u,0x5au,0x78u,0x56u,0x34u,0x12u,0xefu,0xcdu,0xabu,0x90u,0x01u,0x00u,0x00u,0xdcu,0x05u,0xf3u,0x01u,0xcbu,0xd2u};

static int sample_can(uint16_t id,NetUpperCanMessage *m)
{
    NetWireCanEnvelope f={0};const uint8_t *bytes=NULL;
    const NetWireRegistryEntry *entry=net_wire_registry_find(NET_WIRE_CAN,id);
    CHECK(entry!=NULL);f.standard_id=id;f.fdf=true;f.brs=true;f.dlc=entry->dlc;f.length=entry->frame_length;
    switch(id) {
    case 0x080u:bytes=c_domain_permission;break;
    case 0x100u:bytes=c_bcm_door_command;break;
    case 0x110u:bytes=c_bcm_climate_command;break;
    case 0x120u:bytes=c_bcm_light_command;break;
    case 0x090u:bytes=c_window_command;break;
    case 0x200u:bytes=c_bcm_door_state;break;
    case 0x210u:bytes=c_bcm_climate_state;break;
    case 0x220u:bytes=c_bcm_light_state;break;
    case 0x140u:bytes=c_bcm_result;break;
    case 0x141u:bytes=c_bcm_event;break;
    case 0x230u:bytes=c_bcm_status;break;
    case 0x300u:bytes=c_cis_environment;break;
    case 0x310u:bytes=c_cis_occupant;break;
    case 0x0A0u:bytes=c_cis_rear;break;
    case 0x320u:bytes=c_cis_status;break;
    case 0x0B0u:bytes=c_window_state;break;
    case 0x091u:bytes=c_window_event;break;
    case 0x092u:bytes=c_window_result;break;
    case 0x330u:bytes=c_window_fault;break;
    case 0x160u:bytes=c_vss_event;break;
    case 0x0C0u:bytes=c_vss_warning_state;break;
    case 0x340u:bytes=c_vss_status;break;
    default:CHECK(0);
    }
    memcpy(f.bytes,bytes,f.length);CHECK(net_upper_can_decode(PROFILE,&f,m)==NET_WIRE_OK);
    switch(id) {
    case 0x080u:
        m->data.domain_permission.boot_id=entry->sender*100u+1u;m->data.domain_permission.tx_sequence=1u;
        m->data.domain_permission.source_age_ms=0u;
        break;
    case 0x100u:
        m->data.bcm_door_command.boot_id=entry->sender*100u+1u;m->data.bcm_door_command.tx_sequence=1u;
        m->data.bcm_door_command.command_age_ms=0u;
        break;
    case 0x110u:
        m->data.bcm_climate_command.boot_id=entry->sender*100u+1u;m->data.bcm_climate_command.tx_sequence=1u;
        m->data.bcm_climate_command.command_age_ms=0u;
        break;
    case 0x120u:
        m->data.bcm_light_command.boot_id=entry->sender*100u+1u;m->data.bcm_light_command.tx_sequence=1u;
        m->data.bcm_light_command.command_age_ms=0u;
        break;
    case 0x090u:
        m->data.window_command.boot_id=entry->sender*100u+1u;m->data.window_command.tx_sequence=1u;
        m->data.window_command.command_age_ms=0u;
        break;
    case 0x200u:
        m->data.bcm_door_state.boot_id=entry->sender*100u+1u;m->data.bcm_door_state.tx_sequence=1u;
        m->data.bcm_door_state.source_age_ms=0u;
        break;
    case 0x210u:
        m->data.bcm_climate_state.boot_id=entry->sender*100u+1u;m->data.bcm_climate_state.tx_sequence=1u;
        m->data.bcm_climate_state.source_age_ms=0u;
        break;
    case 0x220u:
        m->data.bcm_light_state.boot_id=entry->sender*100u+1u;m->data.bcm_light_state.tx_sequence=1u;
        m->data.bcm_light_state.source_age_ms=0u;
        break;
    case 0x140u:
        m->data.bcm_result.boot_id=entry->sender*100u+1u;m->data.bcm_result.tx_sequence=1u;
        m->data.bcm_result.event_age_ms=0u;
        break;
    case 0x141u:
        m->data.bcm_event.boot_id=entry->sender*100u+1u;m->data.bcm_event.tx_sequence=1u;
        m->data.bcm_event.event_age_ms=0u;
        break;
    case 0x230u:
        m->data.bcm_status.boot_id=entry->sender*100u+1u;m->data.bcm_status.tx_sequence=1u;
        m->data.bcm_status.source_age_ms=0u;
        break;
    case 0x300u:
        m->data.cis_environment.boot_id=entry->sender*100u+1u;m->data.cis_environment.tx_sequence=1u;
        m->data.cis_environment.temperature_age_ms=0u;
        m->data.cis_environment.humidity_age_ms=0u;
        m->data.cis_environment.illuminance_age_ms=0u;
        break;
    case 0x310u:
        m->data.cis_occupant.boot_id=entry->sender*100u+1u;m->data.cis_occupant.tx_sequence=1u;
        m->data.cis_occupant.vision_age_ms=0u;
        break;
    case 0x0A0u:
        m->data.cis_rear.boot_id=entry->sender*100u+1u;m->data.cis_rear.tx_sequence=1u;
        m->data.cis_rear.source_age_ms=0u;
        break;
    case 0x320u:
        m->data.cis_status.boot_id=entry->sender*100u+1u;m->data.cis_status.tx_sequence=1u;
        m->data.cis_status.source_age_ms=0u;
        break;
    case 0x0B0u:
        m->data.window_state.boot_id=entry->sender*100u+1u;m->data.window_state.tx_sequence=1u;
        m->data.window_state.source_age_ms=0u;
        break;
    case 0x091u:
        m->data.window_event.boot_id=entry->sender*100u+1u;m->data.window_event.tx_sequence=1u;
        m->data.window_event.event_age_ms=0u;
        break;
    case 0x092u:
        m->data.window_result.boot_id=entry->sender*100u+1u;m->data.window_result.tx_sequence=1u;
        break;
    case 0x330u:
        m->data.window_fault.boot_id=entry->sender*100u+1u;m->data.window_fault.tx_sequence=1u;
        m->data.window_fault.source_age_ms=0u;
        break;
    case 0x160u:
        m->data.vss_event.boot_id=entry->sender*100u+1u;m->data.vss_event.tx_sequence=1u;
        m->data.vss_event.original_age_ms=0u;
        break;
    case 0x0C0u:
        m->data.vss_warning_state.boot_id=entry->sender*100u+1u;m->data.vss_warning_state.tx_sequence=1u;
        if(m->data.vss_warning_state.antipinch_source_boot!=0u)m->data.vss_warning_state.antipinch_original_age_ms=0u;
        if(m->data.vss_warning_state.occupant_hazard_source_boot!=0u)m->data.vss_warning_state.occupant_hazard_original_age_ms=0u;
        if(m->data.vss_warning_state.rear_obstacle_source_boot!=0u)m->data.vss_warning_state.rear_obstacle_original_age_ms=0u;
        m->data.vss_warning_state.rear_activation_age_ms=0u;
        break;
    case 0x340u:
        m->data.vss_status.boot_id=entry->sender*100u+1u;m->data.vss_status.tx_sequence=1u;
        m->data.vss_status.last_fault_age_ms=0u;
        m->data.vss_status.source_age_ms=0u;
        break;
    default:CHECK(0);
    }
    return 0;
}
static int sample_uart(uint8_t type,NetUpperUartMessage *m)
{
    NetWireUartFrame f={0};const uint8_t *bytes=NULL;
    NetWireLink link=type>=0x60u?NET_WIRE_UART_C:NET_WIRE_UART_M;
    const NetWireRegistryEntry *entry=net_wire_registry_find(link,type);
    CHECK(entry!=NULL);f.length=entry->frame_length;
    switch(type) {
    case 0x01u:bytes=u_m_context;break;
    case 0x10u:bytes=u_m_request;break;
    case 0x11u:bytes=u_m_query;break;
    case 0x12u:bytes=u_m_warning_ack;break;
    case 0x02u:bytes=u_m_domain_alive;break;
    case 0x20u:bytes=u_m_result;break;
    case 0x21u:bytes=u_m_warning;break;
    case 0x22u:bytes=u_m_availability;break;
    case 0x23u:bytes=u_m_digital_status;break;
    case 0x24u:bytes=u_m_digital_result;break;
    case 0x25u:bytes=u_m_user_settings;break;
    case 0x40u:bytes=u_m_query_end;break;
    case 0x30u:bytes=u_m_bcm_door_state;break;
    case 0x31u:bytes=u_m_bcm_climate_state;break;
    case 0x32u:bytes=u_m_bcm_light_state;break;
    case 0x33u:bytes=u_m_bcm_status;break;
    case 0x34u:bytes=u_m_cis_environment;break;
    case 0x35u:bytes=u_m_cis_occupant;break;
    case 0x36u:bytes=u_m_cis_rear;break;
    case 0x37u:bytes=u_m_cis_status;break;
    case 0x3Au:bytes=u_m_vss_status;break;
    case 0x38u:bytes=u_m_window_state;break;
    case 0x39u:bytes=u_m_window_fault;break;
    case 0x60u:bytes=u_p_vision_result;break;
    case 0x61u:bytes=u_p_vision_status;break;
    case 0x62u:bytes=u_p_permission;break;
    default:CHECK(0);
    }
    memcpy(f.bytes,bytes,f.length);
    CHECK(net_upper_uart_decode(PROFILE,link,entry->direction,&f,m)==NET_WIRE_OK);m->sequence=1u;
    switch(type) {
    case 0x01u:
        m->data.m_context.app_age_ms=0u;
        m->data.m_context.proximity_age_ms=0u;
        m->data.m_context.registration_connection_age_ms=0u;
        break;
    case 0x10u:
        break;
    case 0x11u:
        break;
    case 0x12u:
        break;
    case 0x02u:
        m->data.m_domain_alive.source_age_ms=0u;
        break;
    case 0x20u:
        m->data.m_result.result_age_ms=0u;
        break;
    case 0x21u:
        m->data.m_warning.source_age_ms=0u;
        break;
    case 0x22u:
        m->data.m_availability.source_age_ms=0u;
        break;
    case 0x23u:
        m->data.m_digital_status.source_age_ms=0u;
        break;
    case 0x24u:
        m->data.m_digital_result.result_age_ms=0u;
        break;
    case 0x25u:
        m->data.m_user_settings.source_age_ms=0u;
        break;
    case 0x40u:
        break;
    case 0x30u:
        m->data.m_bcm_door_state.source_age_ms=0u;
        break;
    case 0x31u:
        m->data.m_bcm_climate_state.source_age_ms=0u;
        break;
    case 0x32u:
        m->data.m_bcm_light_state.source_age_ms=0u;
        break;
    case 0x33u:
        m->data.m_bcm_status.source_age_ms=0u;
        break;
    case 0x34u:
        m->data.m_cis_environment.temperature_age_ms=0u;
        m->data.m_cis_environment.humidity_age_ms=0u;
        m->data.m_cis_environment.illuminance_age_ms=0u;
        break;
    case 0x35u:
        m->data.m_cis_occupant.vision_age_ms=0u;
        break;
    case 0x36u:
        m->data.m_cis_rear.source_age_ms=0u;
        break;
    case 0x37u:
        m->data.m_cis_status.source_age_ms=0u;
        break;
    case 0x3Au:
        m->data.m_vss_status.last_fault_age_ms=0u;
        m->data.m_vss_status.source_age_ms=0u;
        break;
    case 0x38u:
        m->data.m_window_state.source_age_ms=0u;
        break;
    case 0x39u:
        m->data.m_window_fault.source_age_ms=0u;
        break;
    case 0x60u:
        m->data.p_vision_result.vision_age_ms=0u;
        break;
    case 0x61u:
        m->data.p_vision_status.status_age_ms=0u;
        break;
    case 0x62u:
        m->data.p_permission.source_age_ms=0u;
        break;
    default:CHECK(0);
    }
    if(link==NET_WIRE_UART_M) {
        if(type==0x01u){m->data.m_context.device_context_id=7u;m->data.m_context.session_id=11u;}
        else if(type==0x10u){m->data.m_request.device_context_id=7u;m->data.m_request.session_id=11u;}
        else if(type==0x11u){m->data.m_query.device_context_id=7u;m->data.m_query.session_id=11u;}
        else if(type==0x12u){m->data.m_warning_ack.device_context_id=7u;m->data.m_warning_ack.session_id=11u;}
        else if(type==0x02u)m->data.m_domain_alive.domain_boot_id=101u;
        else if(type==0x40u){m->data.m_query_end.device_context_id=7u;m->data.m_query_end.session_id=11u;m->data.m_query_end.domain_boot_id=101u;}
        else CHECK(net_upper_uart_set_query_context(m,7u,11u,101u,0u)==NET_WIRE_OK);
    }
    return 0;
}
#define NODES 7u
static const uint32_t node_bits[NODES]={NET_WIRE_NODE_DOMAIN,NET_WIRE_NODE_BCM,NET_WIRE_NODE_CIS,NET_WIRE_NODE_VSS,NET_WIRE_NODE_WINDOW,NET_WIRE_NODE_ESP32,NET_WIRE_NODE_PI};
enum {DOMAIN,BCM,CIS,VSS,WINDOW,ESP,PI};
typedef struct {
    NetUpper service;
    uint64_t now;
    const NetUpperPacket *borrowed;
    NetUpperPacket packet;
    NetTxKey key;
    size_t submitted,can_calls,uart_calls,rear_calls;
    NetUpperCanMessage can_cache[NET_UPPER_ROUTE_COUNT];
    NetUpperUartMessage uart_cache[NET_UPPER_ROUTE_COUNT];
    NetUpperDelivery metadata[NET_UPPER_ROUTE_COUNT];
    NetUpperRear rear;
    NetUpperUartDelivery last_uart;
} Endpoint;
static Endpoint nodes[NODES];
static uint64_t tick=1000u;
static const char *scenario;
static size_t index_of(NetWireLink link,uint16_t id)
{
    size_t i;for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){const NetWireRegistryEntry *e=net_wire_registry_at(i);if(e->link==link && e->id_type==id)return i;}
    return NET_UPPER_ROUTE_COUNT;
}
static NetStatus clock_now(void *ctx,uint64_t *out){*out=((Endpoint *)ctx)->now;return NET_OK;}
static NetStatus port_submit(void *ctx,const NetUpperPacket *packet,NetTxKey key)
{
    Endpoint *e=ctx;if(e->borrowed!=NULL)return NET_BUSY;e->borrowed=packet;e->packet=*packet;e->key=key;++e->submitted;return NET_OK;
}
static NetStatus port_poll(void *ctx,NetUpperEvent *out){(void)ctx;(void)out;return NET_NO_WORK;}
static NetStatus owner_can(void *ctx,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{
    Endpoint *e=ctx;size_t i=index_of(NET_WIRE_CAN,m->id);
    if(i>=NET_UPPER_ROUTE_COUNT)return NET_REJECTED;
    e->can_cache[i]=*m;e->metadata[i]=*d;++e->can_calls;return NET_OK;
}
static NetStatus owner_uart(void *ctx,const NetUpperUartDelivery *m,const NetUpperDelivery *d)
{
    Endpoint *e=ctx;size_t i=index_of(m->message.link,m->message.type);
    if(i>=NET_UPPER_ROUTE_COUNT)return NET_REJECTED;
    e->uart_cache[i]=m->message;e->metadata[i]=*d;e->last_uart=*m;++e->uart_calls;return NET_OK;
}
static NetStatus owner_rear(void *ctx,const NetUpperRear *rear,const NetUpperDelivery *d)
{
    Endpoint *e=ctx;e->rear=*rear;e->metadata[index_of(NET_WIRE_CAN,0x0a0u)]=*d;++e->rear_calls;return NET_OK;
}
static const NetUpperPortOps ops={clock_now,port_submit,port_poll};
static int init_all(const char *name)
{
    size_t n,i;scenario=name;tick=1000u;memset(nodes,0,sizeof(nodes));
    for(n=0u;n<NODES;++n){NetUpperConfig c={0};c.node=node_bits[n];c.local_epoch=10u+n;c.profile=PROFILE;
        c.port.ops=&ops;c.port.context=&nodes[n];c.owner.context=&nodes[n];c.owner.can=owner_can;c.owner.uart=owner_uart;c.owner.rear=owner_rear;
        for(i=0u;i<3u;++i)c.port_epoch[i]=3u;
        for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){c.rx_policy[i].bound=true;c.rx_policy[i].maximum_age_ms=10000u;c.rx_policy[i].expires_at_equality=true;}
        /* External VSS Owner evaluates each lane, including canonical absent=65535.
         * The product delivers a composite without granting freshness to all lanes. */
        c.rx_per_field_age[index_of(NET_WIRE_CAN,0x0c0u)]=true;
        for(i=0u;i<2u;++i){c.uart_policy[i].candidate_time_policy_selected=true;c.uart_policy[i].assembly_limit_ms=1000u;}
        CHECK(net_upper_init(&nodes[n].service,&c)==NET_OK);nodes[n].now=tick;
    }
    return 0;
}
static int finish_tx(Endpoint *from,NetTxOutcome outcome)
{
    NetUpperEvent e={0};NetUpperTxSnapshot snapshot;
    e.kind=NET_UPPER_TX_EVENT;e.link=from->packet.link;e.tx.key=from->key;e.tx.admission_observed=true;e.tx.accepted=true;
    from->now=tick;
    CHECK(net_upper_capture(&from->service,&e)==NET_OK && net_upper_step(&from->service,NET_UPPER_RX_CAPACITY)==NET_OK);
    CHECK(net_upper_tx_snapshot(&from->service,from->key,&snapshot)==NET_OK && snapshot.outcome==NET_TX_UNKNOWN);
    e.tx.admission_observed=false;e.tx.release_observed=true;from->borrowed=NULL;
    CHECK(net_upper_capture(&from->service,&e)==NET_OK && net_upper_step(&from->service,NET_UPPER_RX_CAPACITY)==NET_OK);
    CHECK(net_upper_retire(&from->service,from->key)==NET_HOLD);
    e.tx.release_observed=false;e.tx.outcome=outcome;
    CHECK(net_upper_capture(&from->service,&e)==NET_OK && net_upper_step(&from->service,NET_UPPER_RX_CAPACITY)==NET_OK);
    CHECK(net_upper_retire(&from->service,from->key)==NET_OK);return 0;
}
static NetStatus deliver_packet(Endpoint *to,const NetUpperPacket *packet,uint64_t worker_delay)
{
    NetUpperEvent e={0};NetStatus status=NET_NO_WORK;size_t i;
    e.link=packet->link;e.capture.port_epoch=3u;e.capture.timestamp_known=true;
    if(packet->link==NET_WIRE_CAN){e.kind=NET_UPPER_CAN_RX;e.can=packet->can;e.capture.captured_at_ms=++tick;to->now=tick+worker_delay;
        status=net_upper_capture(&to->service,&e);if(status==NET_OK)status=net_upper_step(&to->service,NET_UPPER_RX_CAPACITY);
        tick+=worker_delay;return status;
    }
    e.kind=NET_UPPER_UART_BYTE;
    for(i=0u;i<packet->uart.length;++i){e.byte=packet->uart.bytes[i];e.capture.captured_at_ms=++tick;
        to->now=tick+(i+1u==packet->uart.length?worker_delay:0u);
        status=net_upper_capture(&to->service,&e);if(status!=NET_OK)return status;
        status=net_upper_step(&to->service,NET_UPPER_RX_CAPACITY);
        if(i+1u<packet->uart.length && status!=NET_NO_WORK)return status;
    }
    tick+=worker_delay;return status;
}
static int send_can(size_t a,size_t b,const NetUpperCanMessage *m)
{
    Endpoint *from=&nodes[a];NetSourceStamp source={0u,0u,0u};NetAgePolicy policy={true,10000u,true};NetTxKey key;
    const NetWireRegistryEntry *entry=net_wire_registry_find(NET_WIRE_CAN,m->id);
    /* Transport this composite even when one explicitly absent lane is saturated.
     * This is an external Owner test policy, never an all-lanes freshness claim. */
    if(m->id==0x0c0u)policy.maximum_age_ms=66535u;
    source.observation_id=++tick;source.observed_at_ms=tick;source.original_age_ms=net_upper_can_max_age(m);from->now=tick;
    CHECK(net_upper_submit_can(&from->service,m,&source,&policy,&key)==NET_OK);
    CHECK(net_upper_step(&from->service,NET_UPPER_RX_CAPACITY)==NET_OK && from->borrowed!=NULL);
    CHECK(deliver_packet(&nodes[b],&from->packet,0u)==NET_OK);
    CHECK(finish_tx(from,NET_TX_SUCCEEDED)==0);
    printf("{\"scenario\":\"%s\",\"wire\":\"%s\",\"tx_entry\":\"net_upper_submit_can\",\"rx_entry\":\"net_upper_capture/step/typed_owner\",\"from\":%u,\"to\":%u,\"host_owner_only\":true}\n",scenario,entry->name,node_bits[a],node_bits[b]);return 0;
}
static int send_uart_source(size_t a,size_t b,const NetUpperUartMessage *m,const NetSourceStamp *provided,uint64_t delay)
{
    Endpoint *from=&nodes[a];NetSourceStamp source={0u,0u,0u};NetAgePolicy policy={true,10000u,true};NetTxKey key;
    const NetWireRegistryEntry *entry=net_wire_registry_find(m->link,m->type);
    source.observation_id=++tick;source.observed_at_ms=tick;source.original_age_ms=net_upper_uart_max_age(m);if(provided!=NULL)source=*provided;from->now=tick;
    CHECK(net_upper_submit_uart(&from->service,m,&source,&policy,&key)==NET_OK);
    CHECK(net_upper_step(&from->service,NET_UPPER_RX_CAPACITY)==NET_OK && from->borrowed!=NULL);
    CHECK(deliver_packet(&nodes[b],&from->packet,delay)==NET_OK);
    CHECK(finish_tx(from,NET_TX_SUCCEEDED)==0);
    printf("{\"scenario\":\"%s\",\"wire\":\"%s\",\"tx_entry\":\"net_upper_submit_uart\",\"rx_entry\":\"net_upper_capture/step/typed_owner\",\"from\":%u,\"to\":%u,\"host_owner_only\":true}\n",scenario,entry->name,node_bits[a],node_bits[b]);return 0;
}
static int send_uart(size_t a,size_t b,const NetUpperUartMessage *m){return send_uart_source(a,b,m,NULL,0u);}
static int context(void){NetUpperUartMessage m;CHECK(sample_uart(0x01u,&m)==0);CHECK(send_uart(ESP,DOMAIN,&m)==0);return 0;}
static int project(uint16_t id,uint16_t sequence)
{
    NetUpperCanMessage can;NetUpperDelivery d;NetSourceStamp source;NetUpperUartMessage m;
    CHECK(net_upper_received_can(&nodes[DOMAIN].service,id,&can,&d,&source)==NET_OK);
    CHECK(net_upper_uart_project_can(PROFILE,&can,7u,11u,101u,0u,sequence,&m)==NET_WIRE_OK);
    CHECK(send_uart_source(DOMAIN,ESP,&m,&source,0u)==0);return 0;
}
static int relay_occupant(NetUpperCanMessage *observed)
{
    Endpoint *cis=&nodes[CIS];NetTxKey key;NetAgePolicy policy={true,10000u,true};
    NetUpperDelivery *origin=&cis->metadata[index_of(NET_WIRE_UART_C,0x60u)];
    const NetWirePVisionResult *raw=&cis->uart_cache[index_of(NET_WIRE_UART_C,0x60u)].data.p_vision_result;
    tick+=7u;cis->now=tick;
    CHECK(net_upper_cis_publish_occupant(&cis->service,401u,1u,&policy,&key)==NET_OK);
    CHECK(net_upper_step(&cis->service,NET_UPPER_RX_CAPACITY)==NET_OK && cis->borrowed!=NULL);
    CHECK(net_upper_can_decode(PROFILE,&cis->packet.can,observed)==NET_WIRE_OK);
    CHECK(observed->data.cis_occupant.pi_boot_id==raw->pi_boot_id && observed->data.cis_occupant.vision_sequence==raw->vision_sequence);
    CHECK(observed->data.cis_occupant.vision_age_ms==raw->vision_age_ms+tick-origin->capture.captured_at_ms);
    CHECK(observed->data.cis_occupant.presence_validity==raw->presence_validity && observed->data.cis_occupant.count_validity==raw->count_validity);
    CHECK(deliver_packet(&nodes[DOMAIN],&cis->packet,0u)==NET_OK);
    CHECK(finish_tx(cis,NET_TX_SUCCEEDED)==0);
    printf("{\"scenario\":\"%s\",\"wire\":\"CIS_OCCUPANT\",\"tx_entry\":\"net_upper_cis_publish_occupant/actual_Pi_receipt/queue/codec\",\"rx_entry\":\"capture/step/typed_owner\",\"pi_boot\":%u,\"vision_sequence\":%u,\"wire_age\":%u,\"new_owner_timestamp\":false}\n",scenario,raw->pi_boot_id,raw->vision_sequence,observed->data.cis_occupant.vision_age_ms);
    return 0;
}

static int bcm_commands(void)
{
    unsigned kind;
    for(kind=1u;kind<=3u;++kind) {
        NetUpperUartMessage request,result,settings,digital;
        NetUpperCanMessage command,reported,state,event;
        uint16_t id=kind==1u?0x100u:(kind==2u?0x110u:0x120u);
        uint16_t state_id=kind==1u?0x200u:(kind==2u?0x210u:0x220u);
        const NetWireMRequest *accepted_request;const NetWireBcmResultRaw *accepted_result;
        CHECK(init_all(kind==1u?"SC01+SC04":(kind==2u?"SC02":"SC03"))==0);CHECK(context()==0);
        CHECK(sample_uart(0x10u,&request)==0);request.data.m_request.kind=(uint8_t)kind;
        request.data.m_request.operation=1u;memset(request.data.m_request.args,0,sizeof(request.data.m_request.args));
        request.data.m_request.request_id=500u+kind;request.sequence=2u;
        if(kind==2u){request.data.m_request.args[0]=0x2eu;request.data.m_request.args[1]=0xfbu;}
        CHECK(send_uart(ESP,DOMAIN,&request)==0);
        accepted_request=&nodes[DOMAIN].uart_cache[index_of(NET_WIRE_UART_M,0x10u)].data.m_request;
        CHECK(accepted_request->request_id==500u+kind && accepted_request->session_id==11u);
        /* External DOMAIN Fake authorizes a test command from this actual input.
         * This is not a product permission policy or a vehicle execution claim. */
        CHECK(sample_can(id,&command)==0);
        if(kind==1u){command.data.bcm_door_command.command_id=700u+kind;command.data.bcm_door_command.target_boot_id=201u;}
        else if(kind==2u){command.data.bcm_climate_command.command_id=700u+kind;command.data.bcm_climate_command.target_boot_id=201u;}
        else {command.data.bcm_light_command.command_id=700u+kind;command.data.bcm_light_command.target_boot_id=201u;}
        CHECK(send_can(DOMAIN,BCM,&command)==0);
        CHECK(nodes[BCM].can_calls==1u && nodes[DOMAIN].submitted==1u);
        /* A separate BCM Fake result event, not CAN ACK or local submit OK. */
        CHECK(sample_can(0x140u,&reported)==0);reported.data.bcm_result.command_domain_boot=101u;
        reported.data.bcm_result.command_id=700u+kind;reported.data.bcm_result.command_kind=(uint8_t)kind;
        reported.data.bcm_result.result=5u;reported.data.bcm_result.result_reason=9u;
        reported.data.bcm_result.door_event=0u;reported.data.bcm_result.occurrence_id=0u;
        reported.data.bcm_result.door_target=kind==1u?1u:UINT8_MAX;
        CHECK(send_can(BCM,DOMAIN,&reported)==0);
        accepted_result=&nodes[DOMAIN].can_cache[index_of(NET_WIRE_CAN,0x140u)].data.bcm_result;
        CHECK(accepted_result->command_domain_boot==101u && accepted_result->command_id==700u+kind && accepted_result->command_kind==kind);
        CHECK(sample_uart(0x20u,&result)==0);
        result.data.m_result.request_session=accepted_request->session_id;result.data.m_result.request_id=accepted_request->request_id;
        result.data.m_result.command_id=accepted_result->command_id;result.data.m_result.result=accepted_result->result;
        result.data.m_result.result_reason=accepted_result->result_reason;result.data.m_result.result_confirmation=0u;
        CHECK(send_uart(DOMAIN,ESP,&result)==0);
        CHECK(nodes[ESP].last_uart.message.data.m_result.request_session==11u && nodes[ESP].last_uart.message.data.m_result.request_id==500u+kind);
        CHECK(nodes[ESP].last_uart.message.data.m_result.result_confirmation==0u && nodes[ESP].last_uart.message.data.m_result.result==5u);
        CHECK(sample_can(state_id,&state)==0);CHECK(send_can(BCM,DOMAIN,&state)==0);CHECK(project(state_id,3u)==0);
        if(kind==1u){CHECK(sample_can(0x160u,&event)==0);event.data.vss_event.related_command_id=701u;CHECK(send_can(DOMAIN,VSS,&event)==0);
            CHECK(sample_uart(0x23u,&digital)==0);CHECK(send_uart(DOMAIN,ESP,&digital)==0);
            CHECK(sample_uart(0x24u,&digital)==0);digital.data.m_digital_result.related_door_command_id=701u;digital.data.m_digital_result.result_confirmation=0u;CHECK(send_uart(DOMAIN,ESP,&digital)==0);
        }else {CHECK(sample_uart(0x25u,&settings)==0);CHECK(send_uart(DOMAIN,ESP,&settings)==0);}
        if(kind==2u){NetUpperCanMessage sensor;NetUpperUartMessage pi;CHECK(sample_can(0x300u,&sensor)==0);CHECK(send_can(CIS,DOMAIN,&sensor)==0);
            CHECK(sample_uart(0x60u,&pi)==0);CHECK(send_uart(PI,CIS,&pi)==0);CHECK(relay_occupant(&sensor)==0);
            CHECK(sample_can(0x141u,&event)==0);event.data.bcm_event.related_domain_boot=101u;event.data.bcm_event.related_command_id=702u;CHECK(send_can(BCM,DOMAIN,&event)==0);}
        printf("{\"scenario\":\"%s\",\"owner_request_id\":%u,\"owner_command_id\":%u,\"actual_owner_result\":5,\"confirmation\":0,\"vehicle_execution\":\"HOLD\"}\n",scenario,500u+kind,700u+kind);
    }
    return 0;
}
static int sensors_vision(void)
{
    NetUpperCanMessage environment,status,occupant,rear,warning;
    NetUpperUartMessage vision,vision_status,reported;
    const NetWirePVisionResult *accepted;uint32_t original_seq;
    CHECK(init_all("SC05")==0);CHECK(context()==0);
    CHECK(sample_can(0x300u,&environment)==0);
    environment.data.cis_environment.temperature_centi_c=-1234;environment.data.cis_environment.temperature_age_ms=7u;
    environment.data.cis_environment.humidity_age_ms=17u;environment.data.cis_environment.illuminance_age_ms=27u;
    CHECK(send_can(CIS,DOMAIN,&environment)==0);CHECK(project(0x300u,2u)==0);
    CHECK(nodes[ESP].last_uart.message.data.m_cis_environment.temperature_centi_c==-1234);
    CHECK(nodes[ESP].last_uart.message.data.m_cis_environment.humidity_age_ms-nodes[ESP].last_uart.message.data.m_cis_environment.temperature_age_ms==10u);
    CHECK(sample_can(0x320u,&status)==0);CHECK(send_can(CIS,DOMAIN,&status)==0);
    scenario="SC06+SC10";
    CHECK(sample_uart(0x60u,&vision)==0);vision.data.p_vision_result.pi_boot_id=6401u;vision.data.p_vision_result.vision_sequence=42u;
    CHECK(send_uart(PI,CIS,&vision)==0);
    accepted=&nodes[CIS].uart_cache[index_of(NET_WIRE_UART_C,0x60u)].data.p_vision_result;
    CHECK(relay_occupant(&occupant)==0);CHECK(project(0x310u,3u)==0);
    CHECK(nodes[ESP].last_uart.message.data.m_cis_occupant.pi_boot_id==6401u && nodes[ESP].last_uart.message.data.m_cis_occupant.vision_sequence==42u);
    original_seq=accepted->vision_sequence;
    CHECK(sample_uart(0x61u,&vision_status)==0);vision_status.data.p_vision_status.pi_boot_id=6401u;vision_status.data.p_vision_status.status_sequence=91u;
    vision_status.data.p_vision_status.vision_status=1u;CHECK(send_uart(PI,CIS,&vision_status)==0);
    CHECK(nodes[CIS].uart_cache[index_of(NET_WIRE_UART_C,0x60u)].data.p_vision_result.vision_sequence==original_seq);
    ++status.data.cis_status.tx_sequence;++status.data.cis_status.update_sequence;
    CHECK(send_can(CIS,DOMAIN,&status)==0);
    CHECK(project(0x320u,4u)==0);
    CHECK(sample_uart(0x22u,&reported)==0);CHECK(send_uart(DOMAIN,ESP,&reported)==0);
    scenario="SC07";CHECK(sample_can(0x0a0u,&rear)==0);rear.data.cis_rear.rear_distance_deci_cm=123u;rear.data.cis_rear.validity=1u;rear.data.cis_rear.quality_reason=0u;
    CHECK(send_can(CIS,DOMAIN,&rear)==0);CHECK(nodes[DOMAIN].rear.rear_distance_mm==123u);CHECK(project(0x0a0u,5u)==0);
    CHECK(nodes[ESP].last_uart.message.data.m_cis_rear.rear_distance_deci_cm==123u);
    /* External DOMAIN Fake supplies risk/warning output from the real accepted input. */
    CHECK(sample_can(0x0c0u,&warning)==0);warning.data.vss_warning_state.rear_obstacle_source_boot=401u;
    warning.data.vss_warning_state.rear_obstacle_source_sequence=rear.data.cis_rear.update_sequence;
    warning.data.vss_warning_state.rear_obstacle_original_age_ms=(uint16_t)(tick+1u-nodes[DOMAIN].metadata[index_of(NET_WIRE_CAN,0x0a0u)].capture.captured_at_ms);
    warning.data.vss_warning_state.occupant_hazard_source_boot=accepted->pi_boot_id;
    warning.data.vss_warning_state.occupant_hazard_source_sequence=accepted->vision_sequence;
    warning.data.vss_warning_state.occupant_hazard_original_age_ms=(uint16_t)(occupant.data.cis_occupant.vision_age_ms+tick+1u-nodes[DOMAIN].metadata[index_of(NET_WIRE_CAN,0x310u)].capture.captured_at_ms);
    warning.data.vss_warning_state.occupant_hazard_state=1u;warning.data.vss_warning_state.occupant_hazard_quality=0u;
    warning.data.vss_warning_state.occupant_hazard_reason=0u;warning.data.vss_warning_state.occupant_hazard_availability=0u;
    scenario="SC06+SC07";
    CHECK(send_can(DOMAIN,VSS,&warning)==0);
    CHECK(nodes[VSS].can_cache[index_of(NET_WIRE_CAN,0x0c0u)].data.vss_warning_state.antipinch_original_age_ms==UINT16_MAX);
    CHECK(!nodes[VSS].metadata[index_of(NET_WIRE_CAN,0x0c0u)].freshness_known);
    CHECK(sample_uart(0x21u,&reported)==0);reported.data.m_warning.warning_type=1u;reported.data.m_warning.occurrence_id=607u;
    reported.data.m_warning.source_boot=401u;reported.data.m_warning.source_sequence=rear.data.cis_rear.update_sequence;
    CHECK(send_uart(DOMAIN,ESP,&reported)==0);
    reported.sequence=2u;reported.data.m_warning.warning_type=2u;reported.data.m_warning.occurrence_id=606u;
    reported.data.m_warning.source_boot=accepted->pi_boot_id;reported.data.m_warning.source_sequence=accepted->vision_sequence;
    CHECK(send_uart(DOMAIN,ESP,&reported)==0);
    scenario="SC10";reported.sequence=3u;reported.data.m_warning.warning_type=5u;reported.data.m_warning.occurrence_id=610u;
    reported.data.m_warning.source_boot=status.data.cis_status.boot_id;reported.data.m_warning.source_sequence=status.data.cis_status.last_fault_occurrence;
    CHECK(send_uart(DOMAIN,ESP,&reported)==0);
    printf("{\"scenario\":\"SC05+SC06+SC07+SC10\",\"rear_mm\":123,\"pi_result_seq\":42,\"pi_status_seq\":91,\"ready_did_not_refresh_result\":true,\"sensor_owner_algorithms\":\"FAKE_ONLY_HOLD\"}\n");
    return 0;
}
static int permission_fanout(void)
{
    NetUpperCanMessage permission,accepted;NetUpperDelivery d;NetSourceStamp source;
    NetUpperUartMessage relay;NetStatus status;size_t n;
    CHECK(init_all("SC08")==0);CHECK(sample_can(0x080u,&permission)==0);
    permission.data.domain_permission.vehicle_power_permission=0u;permission.data.domain_permission.power_permission_quality=0u;
    permission.data.domain_permission.operation_allowed=0u;permission.data.domain_permission.operation_quality=0u;
    /* One actual transmitted immutable packet fans out, independent of local final outcome. */
    {
        NetAgePolicy p={true,500u,true};NetTxKey key;NetSourceStamp original={1u,tick,0u};
        CHECK(net_upper_submit_can(&nodes[DOMAIN].service,&permission,&original,&p,&key)==NET_OK);
        CHECK(net_upper_step(&nodes[DOMAIN].service,NET_UPPER_RX_CAPACITY)==NET_OK);
        for(n=BCM;n<=WINDOW;++n){status=deliver_packet(&nodes[n],&nodes[DOMAIN].packet,0u);
            CHECK(status==(n==WINDOW?NET_HOLD:NET_OK));
            printf("{\"scenario\":\"SC08\",\"wire\":\"DOMAIN_PERMISSION\",\"receiver\":%u,\"status\":\"%s\"}\n",node_bits[n],n==WINDOW?"WINDOW_DISABLED_HOLD":"PRODUCT_HOST_PASS");}
        CHECK(finish_tx(&nodes[DOMAIN],NET_TX_SUCCEEDED)==0);
    }
    CHECK(net_upper_received_can(&nodes[CIS].service,0x080u,&accepted,&d,&source)==NET_OK);
    CHECK(sample_uart(0x62u,&relay)==0);relay.data.p_permission.vehicle_power_permission=accepted.data.domain_permission.vehicle_power_permission;
    relay.data.p_permission.value_quality=accepted.data.domain_permission.power_permission_quality;relay.data.p_permission.quality_reason=0u;
    relay.data.p_permission.source_age_ms=accepted.data.domain_permission.source_age_ms;
    relay.data.p_permission.update_sequence=accepted.data.domain_permission.update_sequence;
    CHECK(send_uart_source(CIS,PI,&relay,&source,0u)==0);
    CHECK(nodes[PI].last_uart.message.data.p_permission.vehicle_power_permission==0u);
    CHECK(nodes[PI].last_uart.message.data.p_permission.use_limit_ms==1500u);
    printf("{\"scenario\":\"SC08\",\"wire\":\"P_PERMISSION\",\"permission\":0,\"owner_deny_preserved\":true,\"new_allow_generated\":false,\"gates\":\"G01-G08_HOLD\"}\n");
    return 0;
}
static int diagnostics_warning_ack(void)
{
    NetUpperCanMessage event,status,warning,door;NetUpperUartMessage report,availability,ack;
    uint32_t occurrence;
    CHECK(init_all("SC09")==0);CHECK(context()==0);
    CHECK(sample_can(0x141u,&event)==0);CHECK(send_can(BCM,DOMAIN,&event)==0);
    CHECK(sample_can(0x230u,&status)==0);CHECK(send_can(BCM,DOMAIN,&status)==0);CHECK(project(0x230u,2u)==0);
    CHECK(sample_uart(0x21u,&report)==0);report.data.m_warning.occurrence_id=609u;report.data.m_warning.warning_type=4u;
    report.data.m_warning.source_boot=event.data.bcm_event.boot_id;report.data.m_warning.source_sequence=event.data.bcm_event.occurrence_id;
    CHECK(send_uart(DOMAIN,ESP,&report)==0);
    CHECK(sample_uart(0x22u,&availability)==0);CHECK(send_uart(DOMAIN,ESP,&availability)==0);
    scenario="SC13+SC14";CHECK(sample_can(0x160u,&event)==0);CHECK(send_can(DOMAIN,VSS,&event)==0);
    CHECK(sample_can(0x0c0u,&warning)==0);CHECK(send_can(DOMAIN,VSS,&warning)==0);
    CHECK(sample_can(0x340u,&status)==0);CHECK(send_can(VSS,DOMAIN,&status)==0);CHECK(project(0x340u,3u)==0);
    CHECK(nodes[ESP].last_uart.message.data.m_vss_status.last_fault_occurrence==status.data.vss_status.last_fault_occurrence);
    report.sequence=2u;report.data.m_warning.occurrence_id=614u;report.data.m_warning.warning_type=7u;
    report.data.m_warning.source_boot=status.data.vss_status.boot_id;report.data.m_warning.source_sequence=status.data.vss_status.last_fault_occurrence;
    CHECK(send_uart(DOMAIN,ESP,&report)==0);
    availability.sequence=2u;++availability.data.m_availability.update_sequence;
    CHECK(send_uart(DOMAIN,ESP,&availability)==0);
    scenario="SC16";CHECK(sample_can(0x200u,&door)==0);door.data.bcm_door_state.door_open_state=1u;door.data.bcm_door_state.open_quality=0u;
    CHECK(send_can(BCM,DOMAIN,&door)==0);
    report.sequence=3u;report.data.m_warning.occurrence_id=700u;report.data.m_warning.warning_type=8u;
    report.data.m_warning.source_boot=door.data.bcm_door_state.boot_id;report.data.m_warning.source_sequence=door.data.bcm_door_state.update_sequence;
    CHECK(send_uart(DOMAIN,ESP,&report)==0);
    occurrence=nodes[ESP].uart_cache[index_of(NET_WIRE_UART_M,0x21u)].data.m_warning.occurrence_id;
    CHECK(sample_uart(0x12u,&ack)==0);ack.data.m_warning_ack.warning_domain_boot=101u;ack.data.m_warning_ack.warning_type=8u;
    ack.data.m_warning_ack.occurrence_id=occurrence;CHECK(send_uart(ESP,DOMAIN,&ack)==0);
    CHECK(nodes[DOMAIN].uart_cache[index_of(NET_WIRE_UART_M,0x12u)].data.m_warning_ack.occurrence_id==700u);
    CHECK(nodes[VSS].can_cache[index_of(NET_WIRE_CAN,0x0c0u)].data.vss_warning_state.rear_obstacle_state==warning.data.vss_warning_state.rear_obstacle_state);
    CHECK(nodes[DOMAIN].can_cache[index_of(NET_WIRE_CAN,0x200u)].data.bcm_door_state.door_open_state==1u);
    printf("{\"scenario\":\"SC09+SC13+SC14+SC16\",\"read_ack_occurrence\":700,\"warning_not_cleared\":true,\"door_not_closed\":true,\"sound_stop_inferred\":false}\n");
    return 0;
}
static int window_disabled(void)
{
    static const uint16_t ids[]={0x090u,0x0b0u,0x091u,0x092u,0x330u};size_t i;
    CHECK(init_all("SC11+SC12")==0);
    for(i=0u;i<sizeof(ids)/sizeof(ids[0]);++i){NetUpperCanMessage m;NetSourceStamp source={1u,tick,0u};NetAgePolicy p={true,10000u,true};NetTxKey key={88u,88u};
        size_t sender=ids[i]==0x090u?DOMAIN:WINDOW;CHECK(sample_can(ids[i],&m)==0);
        CHECK(net_upper_submit_can(&nodes[sender].service,&m,&source,&p,&key)==NET_HOLD);
        CHECK(key.epoch==88u && nodes[sender].submitted==0u);}
    printf("{\"scenario\":\"SC11+SC12\",\"status\":\"WINDOW_DISABLED_HOLD\",\"actual_port_submits\":0,\"vehicle_function_complete\":false}\n");return 0;
}
static int metadata_age(void)
{
    NetUpperUartMessage m;NetUpperDelivery *d;
    CHECK(init_all("UR003")==0);CHECK(context()==0);CHECK(sample_uart(0x20u,&m)==0);
    m.data.m_result.domain_boot_id=9u;m.data.m_result.result_age_ms=37u;
    CHECK(send_uart_source(DOMAIN,ESP,&m,NULL,3u)==0);
    d=&nodes[ESP].metadata[index_of(NET_WIRE_UART_M,0x20u)];
    CHECK(nodes[ESP].last_uart.message.data.m_result.result_age_ms==37u);
    CHECK(d->age_ms==40u && d->local_elapsed_ms==3u && d->source_boot==9u);
    CHECK(sample_uart(0x01u,&m)==0);m.sequence=2u;
    ++m.data.m_context.app_update_sequence;++m.data.m_context.proximity_update_sequence;
    ++m.data.m_context.registration_connection_update_sequence;
    m.data.m_context.app_age_ms=7u;m.data.m_context.proximity_age_ms=17u;m.data.m_context.registration_connection_age_ms=27u;
    CHECK(send_uart_source(ESP,DOMAIN,&m,NULL,3u)==0);
    d=&nodes[DOMAIN].metadata[index_of(NET_WIRE_UART_M,0x01u)];
    CHECK(d->age_ms==30u && d->per_field_age_required);
    CHECK(nodes[DOMAIN].last_uart.message.data.m_context.app_age_ms==7u && nodes[DOMAIN].last_uart.message.data.m_context.proximity_age_ms==17u && nodes[DOMAIN].last_uart.message.data.m_context.registration_connection_age_ms==27u);
    printf("{\"scenario\":\"UR003\",\"m_result_raw_age\":37,\"local_wait\":3,\"metadata_age\":40,\"domain_boot\":9,\"context_independent_ages\":[7,17,27],\"context_metadata_max_age\":30}\n");return 0;
}
static int recovery_failures(void)
{
    NetUpperUartMessage m;NetUpperCanMessage can;NetUpperPacket packet;NetUpperEvent event={0};NetUpperObservation before,after;
    NetSourceStamp source={1u,0u,0u};NetAgePolicy policy={true,10000u,true};NetTxKey key;size_t i;
    CHECK(init_all("SC17+SC18")==0);CHECK(context()==0);CHECK(sample_uart(0x20u,&m)==0);
    source.observed_at_ms=tick;nodes[DOMAIN].now=tick;
    CHECK(net_upper_submit_uart(&nodes[DOMAIN].service,&m,&source,&policy,&key)==NET_OK);
    CHECK(net_upper_step(&nodes[DOMAIN].service,NET_UPPER_RX_CAPACITY)==NET_OK);
    packet=nodes[DOMAIN].packet;
    event.kind=NET_UPPER_LINK_RESET;event.link=NET_WIRE_UART_M;event.capture.port_epoch=4u;
    CHECK(net_upper_capture(&nodes[DOMAIN].service,&event)==NET_OK);
    CHECK(net_upper_step(&nodes[DOMAIN].service,NET_UPPER_RX_CAPACITY)==NET_OK);
    CHECK(nodes[DOMAIN].borrowed!=NULL && memcmp(nodes[DOMAIN].borrowed->uart.bytes,packet.uart.bytes,packet.uart.length)==0);
    CHECK(net_upper_retire(&nodes[DOMAIN].service,key)==NET_HOLD);
    /* Reset never proves physical release; this instance ends with a pinned HOLD,
     * no new instance overwrites its borrowed frame. */
    CHECK(sample_can(0x300u,&can)==0);
    CHECK(net_upper_observation(&nodes[DOMAIN].service,&before)==NET_OK);
    memset(&event,0,sizeof(event));event.kind=NET_UPPER_CAN_RX;event.link=NET_WIRE_CAN;
    event.capture.port_epoch=3u;event.capture.timestamp_known=true;event.capture.captured_at_ms=++tick;nodes[DOMAIN].now=tick;
    CHECK(net_upper_can_encode(PROFILE,&can,&event.can)==NET_WIRE_OK);event.can.fdf=false;
    CHECK(net_upper_capture(&nodes[DOMAIN].service,&event)==NET_OK);
    CHECK(net_upper_step(&nodes[DOMAIN].service,NET_UPPER_RX_CAPACITY)==NET_REJECTED);
    CHECK(net_upper_observation(&nodes[DOMAIN].service,&after)==NET_OK && after.rx_delivered==before.rx_delivered);
    CHECK(send_can(CIS,DOMAIN,&can)==0); /* Independent CAN continues after UART reset. */
    {
        NetTxKey keys[NET_UPPER_TX_CAPACITY];NetUpperCanMessage state;
        CHECK(sample_can(0x200u,&state)==0);nodes[BCM].now=tick;
        for(i=0u;i<NET_UPPER_TX_CAPACITY;++i){source.observation_id=i+10u;source.observed_at_ms=tick;
            CHECK(net_upper_submit_can(&nodes[BCM].service,&state,&source,&policy,&keys[i])==NET_OK);}
        key.epoch=99u;key.token=99u;
        CHECK(net_upper_submit_can(&nodes[BCM].service,&state,&source,&policy,&key)==NET_NO_CAPACITY);
        CHECK(key.epoch==99u && key.token==99u && nodes[BCM].submitted==0u);
        for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)CHECK(net_upper_cancel(&nodes[BCM].service,keys[i])==NET_OK && net_upper_retire(&nodes[BCM].service,keys[i])==NET_OK);
    }
    {
        NetWireUartFrame partial;
        CHECK(sample_uart(0x60u,&m)==0);CHECK(net_upper_uart_encode(PROFILE,&m,&partial)==NET_WIRE_OK);
        memset(&event,0,sizeof(event));event.kind=NET_UPPER_UART_BYTE;event.link=NET_WIRE_UART_C;
        event.capture.port_epoch=3u;event.capture.timestamp_known=true;
        for(i=0u;i<5u;++i){event.byte=partial.bytes[i];event.capture.captured_at_ms=++tick;nodes[CIS].now=tick;
            CHECK(net_upper_capture(&nodes[CIS].service,&event)==NET_OK && net_upper_step(&nodes[CIS].service,NET_UPPER_RX_CAPACITY)==NET_NO_WORK);}
        event.kind=NET_UPPER_LINK_LOSS;
        CHECK(net_upper_capture(&nodes[CIS].service,&event)==NET_OK && net_upper_step(&nodes[CIS].service,NET_UPPER_RX_CAPACITY)==NET_HOLD);
        CHECK(nodes[CIS].uart_calls==0u);CHECK(send_uart(PI,CIS,&m)==0);CHECK(nodes[CIS].uart_calls==1u);
    }
    memset(&event,0,sizeof(event));event.kind=NET_UPPER_TX_EVENT;event.link=NET_WIRE_UART_M;event.tx.key=nodes[DOMAIN].key;
    event.tx.release_observed=true;nodes[DOMAIN].borrowed=NULL;nodes[DOMAIN].now=tick;
    CHECK(net_upper_capture(&nodes[DOMAIN].service,&event)==NET_OK);
    CHECK(net_upper_step(&nodes[DOMAIN].service,NET_UPPER_RX_CAPACITY)==NET_HOLD);
    CHECK(net_upper_retire(&nodes[DOMAIN].service,nodes[DOMAIN].key)==NET_HOLD);
    printf("{\"scenario\":\"SC17+SC18\",\"reset_did_not_release\":true,\"conflict_record_pinned\":true,\"independent_can_continued\":true,\"queue_full_preserved\":true,\"partial_loss_no_delivery\":true}\n");
    return 0;
}
int main(int argc,char **argv)
{
    if(argc!=2)return 2;
    if(strcmp(argv[1],"bcm_commands")==0)return bcm_commands();
    if(strcmp(argv[1],"sensors_vision")==0)return sensors_vision();
    if(strcmp(argv[1],"permission_fanout")==0)return permission_fanout();
    if(strcmp(argv[1],"diagnostics_warning_ack")==0)return diagnostics_warning_ack();
    if(strcmp(argv[1],"window_disabled")==0)return window_disabled();
    if(strcmp(argv[1],"metadata_age")==0)return metadata_age();
    if(strcmp(argv[1],"recovery_failures")==0)return recovery_failures();
    return 2;
}
