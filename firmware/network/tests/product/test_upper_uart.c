#include "network/net_upper_uart.h"
#include "network/net_upper.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
#define PROFILE NET_WIRE_CANDIDATE_ONLY
/* Frozen independent prior Golden literals, not expected output from this router. */
static const uint8_t golden_m_context[] = {0xa5u,0x5au,0x02u,0x01u,0x26u,0x00u,0xa5u,0x5au,0x78u,0x56u,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x01u,0x01u,0x00u,0x00u,0x40u,0x30u,0x20u,0x10u,0x34u,0x12u,0x01u,0x01u,0x00u,0x80u,0x70u,0x60u,0x50u,0x78u,0x56u,0x00u,0xefu,0xcdu,0xabu,0x90u,0xbcu,0x9au,0x3du,0xfdu};
static const uint8_t golden_m_request[] = {0xa5u,0x5au,0x02u,0x10u,0x18u,0x00u,0xa5u,0x5au,0x78u,0x56u,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0xefu,0xcdu,0xabu,0x90u,0x02u,0x01u,0x2eu,0xfbu,0x00u,0x00u,0x00u,0x00u,0x81u,0x68u};
static const uint8_t golden_m_query[] = {0xa5u,0x5au,0x02u,0x11u,0x20u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x80u,0x70u,0x60u,0x50u,0x01u,0x00u,0x22u,0x33u,0x44u,0x55u,0x66u,0x77u,0x88u,0x99u,0xefu,0xcdu,0xabu,0x90u,0x00u,0x00u,0x9du,0x09u};
static const uint8_t golden_m_warning_ack[] = {0xa5u,0x5au,0x02u,0x12u,0x15u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x08u,0xefu,0xcdu,0xabu,0x90u,0x40u,0x30u,0x20u,0x10u,0x85u,0x0bu};
static const uint8_t golden_m_domain_alive[] = {0xa5u,0x5au,0x02u,0x02u,0x0cu,0x00u,0xa5u,0x5au,0x40u,0x30u,0x20u,0x10u,0xefu,0xcdu,0xabu,0x90u,0xffu,0xffu,0x02u,0x01u,0x65u,0x0fu};
static const uint8_t golden_m_result[] = {0xa5u,0x5au,0x02u,0x20u,0x2cu,0x00u,0xa5u,0x5au,0x78u,0x56u,0x34u,0x12u,0x0cu,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x07u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0xefu,0xcdu,0xabu,0x90u,0x02u,0x09u,0x00u,0x01u,0x01u,0x44u,0x33u,0x22u,0x11u,0x01u,0x34u,0x12u,0xa2u,0x5du};
static const uint8_t golden_m_warning[] = {0xa5u,0x5au,0x02u,0x21u,0x28u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x08u,0xefu,0xcdu,0xabu,0x90u,0x02u,0x01u,0x00u,0x01u,0x05u,0x87u,0x65u,0x43u,0x21u,0x98u,0xbau,0xdcu,0xfeu,0x34u,0x12u,0xb3u,0xcdu};
static const uint8_t golden_m_availability[] = {0xa5u,0x5au,0x02u,0x22u,0x3eu,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x01u,0x00u,0x00u,0x00u,0x02u,0x01u,0x11u,0x00u,0x03u,0x02u,0x0eu,0x00u,0x04u,0x00u,0x00u,0x00u,0x05u,0x01u,0x11u,0x00u,0x06u,0x02u,0x0eu,0x00u,0x07u,0x00u,0x00u,0x00u,0x08u,0x01u,0x11u,0x00u,0x09u,0x02u,0x0eu,0x00u,0xefu,0xcdu,0xabu,0x90u,0x34u,0x12u,0xe9u,0xcdu};
static const uint8_t golden_m_digital_status[] = {0xa5u,0x5au,0x02u,0x23u,0x1eu,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x00u,0x01u,0x11u,0x00u,0xefu,0xcdu,0xabu,0x90u,0x34u,0x12u,0x88u,0xa4u};
static const uint8_t golden_m_digital_result[] = {0xa5u,0x5au,0x02u,0x24u,0x23u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0xefu,0xcdu,0xabu,0x90u,0x87u,0x65u,0x43u,0x21u,0x02u,0x02u,0x09u,0x00u,0x01u,0x34u,0x12u,0xc6u,0x9fu};
static const uint8_t golden_m_user_settings[] = {0xa5u,0x5au,0x02u,0x25u,0x24u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x2eu,0xfbu,0x00u,0x03u,0x01u,0x64u,0xa5u,0x5au,0xffu,0x00u,0xefu,0xcdu,0xabu,0x90u,0x34u,0x12u,0x41u,0x54u};
static const uint8_t golden_m_query_end[] = {0xa5u,0x5au,0x02u,0x40u,0x18u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,0x67u,0x45u,0x23u,0x01u,0x40u,0x30u,0x20u,0x10u,0x80u,0x70u,0x60u,0x50u,0x03u,0x02u,0x08u,0x00u,0xf7u,0x85u};
static const uint8_t golden_m_bcm_door_state[] = {0xa5u,0x5au,0x02u,0x30u,0x28u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x01u,0x00u,0x00u,0x00u,0x01u,0x02u,0x55u,0x66u,0x77u,0x88u,0xffu,0xffu,0x5au,0xd9u};
static const uint8_t golden_m_bcm_climate_state[] = {0xa5u,0x5au,0x02u,0x31u,0x2cu,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x03u,0xffu,0x01u,0x64u,0xffu,0x00u,0x03u,0x00u,0x01u,0x03u,0x78u,0x56u,0x34u,0x12u,0x34u,0x12u,0x49u,0x88u};
static const uint8_t golden_m_bcm_light_state[] = {0xa5u,0x5au,0x02u,0x32u,0x2du,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x02u,0x2au,0x11u,0x22u,0xffu,0x01u,0x00u,0x00u,0x01u,0x02u,0x03u,0x66u,0x77u,0x88u,0x99u,0xffu,0xffu,0xa8u,0x29u};
static const uint8_t golden_m_bcm_status[] = {0xa5u,0x5au,0x02u,0x33u,0x2au,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x02u,0x80u,0x00u,0x03u,0x08u,0x00u,0x07u,0x01u,0x34u,0x12u,0xcdu,0xabu,0xffu,0xffu,0x8cu,0x84u};
static const uint8_t golden_m_cis_environment[] = {0xa5u,0x5au,0x02u,0x34u,0x3cu,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x2eu,0xfbu,0x04u,0x03u,0x02u,0x01u,0x00u,0x00u,0x01u,0x00u,0x10u,0x27u,0x44u,0x33u,0x22u,0x11u,0xffu,0xffu,0x01u,0x00u,0x40u,0xe2u,0x01u,0x00u,0xd4u,0xc3u,0xb2u,0xa1u,0x7bu,0x00u,0x01u,0x00u,0x23u,0xabu};
static const uint8_t golden_m_cis_occupant[] = {0xa5u,0x5au,0x02u,0x35u,0x2du,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x88u,0x77u,0x66u,0x55u,0x44u,0x33u,0x22u,0x11u,0xffu,0xffu,0x01u,0x01u,0x00u,0x03u,0x01u,0x00u,0x02u,0x88u,0x2du};
static const uint8_t golden_m_cis_rear[] = {0xa5u,0x5au,0x02u,0x36u,0x27u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0xefu,0xbeu,0xadu,0xdeu,0xffu,0xffu,0x7bu,0x00u,0x01u,0x00u,0x00u,0xebu,0x32u};
static const uint8_t golden_m_cis_status[] = {0xa5u,0x5au,0x02u,0x37u,0x36u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x02u,0x02u,0x02u,0x01u,0x00u,0x04u,0x01u,0x02u,0x00u,0x01u,0x09u,0x00u,0x01u,0x07u,0x00u,0x21u,0x43u,0x65u,0x87u,0x3fu,0xffu,0xefu,0xcdu,0xabu,0xffu,0xffu,0x17u,0xc2u};
static const uint8_t golden_m_vss_status[] = {0xa5u,0x5au,0x02u,0x3au,0x30u,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x01u,0x01u,0x01u,0x00u,0x00u,0x03u,0x00u,0x32u,0x54u,0x76u,0x98u,0xffu,0xffu,0x00u,0x40u,0x30u,0x20u,0x10u,0xd2u,0x04u,0x72u,0x8au};
static const uint8_t golden_m_window_state[] = {0xa5u,0x5au,0x02u,0x38u,0x2au,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x02u,0x01u,0x25u,0x01u,0x00u,0x00u,0x01u,0x01u,0xcdu,0xabu,0x34u,0x12u,0xffu,0xffu,0xe4u,0x64u};
static const uint8_t golden_m_window_fault[] = {0xa5u,0x5au,0x02u,0x39u,0x2au,0x00u,0xa5u,0x5au,0xa5u,0x5au,0x22u,0x11u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,0x08u,0x44u,0x33u,0x22u,0x11u,0x99u,0x88u,0x77u,0x66u,0xd4u,0xc3u,0xb2u,0xa1u,0x40u,0x30u,0x20u,0x10u,0x03u,0x05u,0x00u,0x02u,0x40u,0x30u,0x20u,0x10u,0xd4u,0xc3u,0xb2u,0xa1u,0xffu,0xffu,0x11u,0xeeu};
static const uint8_t golden_p_vision_result[] = {0xa5u,0x5au,0x01u,0x60u,0x13u,0x00u,0xa5u,0x5au,0x78u,0x56u,0x34u,0x12u,0xefu,0xcdu,0xabu,0x90u,0x34u,0x12u,0x01u,0x01u,0x00u,0x03u,0x01u,0x00u,0x02u,0x00u,0x00u,0xa7u,0x60u};
static const uint8_t golden_p_vision_status[] = {0xa5u,0x5au,0x01u,0x61u,0x10u,0x00u,0xa5u,0x5au,0x78u,0xa5u,0x5au,0x12u,0xefu,0xcdu,0xabu,0x90u,0x01u,0x00u,0x00u,0x04u,0x00u,0x34u,0x12u,0x00u,0xe6u,0x74u};
static const uint8_t golden_p_permission[] = {0xa5u,0x5au,0x01u,0x62u,0x0fu,0x00u,0xa5u,0x5au,0x78u,0x56u,0x34u,0x12u,0xefu,0xcdu,0xabu,0x90u,0x01u,0x00u,0x00u,0xdcu,0x05u,0xf3u,0x01u,0xcbu,0xd2u};
typedef struct { const char *name; NetWireLink link; NetWireDirection direction;
    uint8_t type; const uint8_t *bytes; size_t length; } Vector;
static const Vector vectors[] = {
    {"M_CONTEXT", NET_WIRE_UART_M, NET_WIRE_UPLINK, 0x01u, golden_m_context, sizeof(golden_m_context)},
    {"M_REQUEST", NET_WIRE_UART_M, NET_WIRE_UPLINK, 0x10u, golden_m_request, sizeof(golden_m_request)},
    {"M_QUERY", NET_WIRE_UART_M, NET_WIRE_UPLINK, 0x11u, golden_m_query, sizeof(golden_m_query)},
    {"M_WARNING_ACK", NET_WIRE_UART_M, NET_WIRE_UPLINK, 0x12u, golden_m_warning_ack, sizeof(golden_m_warning_ack)},
    {"M_DOMAIN_ALIVE", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x02u, golden_m_domain_alive, sizeof(golden_m_domain_alive)},
    {"M_RESULT", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x20u, golden_m_result, sizeof(golden_m_result)},
    {"M_WARNING", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x21u, golden_m_warning, sizeof(golden_m_warning)},
    {"M_AVAILABILITY", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x22u, golden_m_availability, sizeof(golden_m_availability)},
    {"M_DIGITAL_STATUS", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x23u, golden_m_digital_status, sizeof(golden_m_digital_status)},
    {"M_DIGITAL_RESULT", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x24u, golden_m_digital_result, sizeof(golden_m_digital_result)},
    {"M_USER_SETTINGS", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x25u, golden_m_user_settings, sizeof(golden_m_user_settings)},
    {"M_QUERY_END", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x40u, golden_m_query_end, sizeof(golden_m_query_end)},
    {"M_BCM_DOOR_STATE", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x30u, golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state)},
    {"M_BCM_CLIMATE_STATE", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x31u, golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state)},
    {"M_BCM_LIGHT_STATE", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x32u, golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state)},
    {"M_BCM_STATUS", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x33u, golden_m_bcm_status, sizeof(golden_m_bcm_status)},
    {"M_CIS_ENVIRONMENT", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x34u, golden_m_cis_environment, sizeof(golden_m_cis_environment)},
    {"M_CIS_OCCUPANT", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x35u, golden_m_cis_occupant, sizeof(golden_m_cis_occupant)},
    {"M_CIS_REAR", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x36u, golden_m_cis_rear, sizeof(golden_m_cis_rear)},
    {"M_CIS_STATUS", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x37u, golden_m_cis_status, sizeof(golden_m_cis_status)},
    {"M_VSS_STATUS", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x3Au, golden_m_vss_status, sizeof(golden_m_vss_status)},
    {"M_WINDOW_STATE", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x38u, golden_m_window_state, sizeof(golden_m_window_state)},
    {"M_WINDOW_FAULT", NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x39u, golden_m_window_fault, sizeof(golden_m_window_fault)},
    {"P_VISION_RESULT", NET_WIRE_UART_C, NET_WIRE_UPLINK, 0x60u, golden_p_vision_result, sizeof(golden_p_vision_result)},
    {"P_VISION_STATUS", NET_WIRE_UART_C, NET_WIRE_UPLINK, 0x61u, golden_p_vision_status, sizeof(golden_p_vision_status)},
    {"P_PERMISSION", NET_WIRE_UART_C, NET_WIRE_DOWNLINK, 0x62u, golden_p_permission, sizeof(golden_p_permission)},
};
static NetWireUartFrame frame_at(size_t n)
{
    NetWireUartFrame f = {0};
    f.length = vectors[n].length; memcpy(f.bytes, vectors[n].bytes, f.length);
    f.captured_at_ms = 1000u;
    return f;
}
static int dispatch_all(void)
{
    size_t i;
    for (i = 0u; i < sizeof(vectors)/sizeof(vectors[0]); ++i) {
        NetWireUartFrame f = frame_at(i), encoded, bad;
        NetUpperUartMessage m, unchanged, invalid;
        memset(&m, 0xa6, sizeof(m)); unchanged = m;
        CHECK(net_upper_uart_decode(PROFILE, vectors[i].link, vectors[i].direction, &f, &m) == NET_WIRE_OK);
        CHECK(m.type == vectors[i].type && m.link == vectors[i].link && m.sequence == 0x5aa5u);
        CHECK(net_upper_uart_encode(PROFILE, &m, &encoded) == NET_WIRE_OK);
        CHECK(encoded.length == f.length && memcmp(encoded.bytes, f.bytes, f.length) == 0);
        CHECK(net_upper_uart_add_age(&m, 0u) == NET_WIRE_OK);
        CHECK(net_upper_uart_encode(PROFILE, &m, &encoded) == NET_WIRE_OK);
        CHECK(memcmp(encoded.bytes, f.bytes, f.length) == 0);
        bad = f; bad.bytes[bad.length - 1u] ^= 0x01u; m = unchanged;
        CHECK(net_upper_uart_decode(PROFILE, vectors[i].link, vectors[i].direction, &bad, &m) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&m, &unchanged, sizeof(m)) == 0);
        CHECK(net_upper_uart_decode(PROFILE, vectors[i].link,
            vectors[i].direction == NET_WIRE_UPLINK ? NET_WIRE_DOWNLINK : NET_WIRE_UPLINK, &f, &m) != NET_WIRE_OK);
        CHECK(memcmp(&m, &unchanged, sizeof(m)) == 0);
        CHECK(net_upper_uart_decode(NET_WIRE_NOT_BOUND, vectors[i].link, vectors[i].direction, &f, &m) == NET_WIRE_NOT_BOUND_STATUS);
        CHECK(memcmp(&m, &unchanged, sizeof(m)) == 0);
        invalid = unchanged; invalid.link = NET_WIRE_UART_M; invalid.type = 0xffu; m = invalid;
        CHECK(net_upper_uart_add_age(&m, 100u) == NET_WIRE_UNSUPPORTED && memcmp(&m,&invalid,sizeof(m)) == 0);
        printf("{\"case\":\"uart26_dispatch\",\"wire\":\"%s\",\"bytes\":%lu,\"actual_codec_roundtrip\":true}\n",
            vectors[i].name,(unsigned long)f.length);
    }
    return 0;
}
static int age_fields(void)
{
    NetUpperUartMessage m = {0}, before;
    size_t i;
    for (i = 0u; i < sizeof(vectors)/sizeof(vectors[0]); ++i) {
        NetWireUartFrame f = frame_at(i);
        CHECK(net_upper_uart_decode(PROFILE,vectors[i].link,vectors[i].direction,&f,&m) == NET_WIRE_OK);
        before = m;
        CHECK(net_upper_uart_add_age(&m, 17u) == NET_WIRE_OK);
        CHECK(m.sequence == before.sequence && m.type == before.type && m.link == before.link);
        switch (m.type) {
        case 0x01u:
            CHECK(m.data.m_context.app_age_ms == (before.data.m_context.app_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_context.app_age_ms+17u));
            CHECK(m.data.m_context.proximity_age_ms == (before.data.m_context.proximity_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_context.proximity_age_ms+17u));
            CHECK(m.data.m_context.registration_connection_age_ms == (before.data.m_context.registration_connection_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_context.registration_connection_age_ms+17u));
            break;
        case 0x10u:
            break;
        case 0x11u:
            break;
        case 0x12u:
            break;
        case 0x02u:
            CHECK(m.data.m_domain_alive.source_age_ms == (before.data.m_domain_alive.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_domain_alive.source_age_ms+17u));
            break;
        case 0x20u:
            CHECK(m.data.m_result.result_age_ms == (before.data.m_result.result_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_result.result_age_ms+17u));
            break;
        case 0x21u:
            CHECK(m.data.m_warning.source_age_ms == (before.data.m_warning.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_warning.source_age_ms+17u));
            break;
        case 0x22u:
            CHECK(m.data.m_availability.source_age_ms == (before.data.m_availability.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_availability.source_age_ms+17u));
            break;
        case 0x23u:
            CHECK(m.data.m_digital_status.source_age_ms == (before.data.m_digital_status.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_digital_status.source_age_ms+17u));
            break;
        case 0x24u:
            CHECK(m.data.m_digital_result.result_age_ms == (before.data.m_digital_result.result_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_digital_result.result_age_ms+17u));
            break;
        case 0x25u:
            CHECK(m.data.m_user_settings.source_age_ms == (before.data.m_user_settings.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_user_settings.source_age_ms+17u));
            break;
        case 0x40u:
            break;
        case 0x30u:
            CHECK(m.data.m_bcm_door_state.source_age_ms == (before.data.m_bcm_door_state.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_bcm_door_state.source_age_ms+17u));
            break;
        case 0x31u:
            CHECK(m.data.m_bcm_climate_state.source_age_ms == (before.data.m_bcm_climate_state.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_bcm_climate_state.source_age_ms+17u));
            break;
        case 0x32u:
            CHECK(m.data.m_bcm_light_state.source_age_ms == (before.data.m_bcm_light_state.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_bcm_light_state.source_age_ms+17u));
            break;
        case 0x33u:
            CHECK(m.data.m_bcm_status.source_age_ms == (before.data.m_bcm_status.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_bcm_status.source_age_ms+17u));
            break;
        case 0x34u:
            CHECK(m.data.m_cis_environment.temperature_age_ms == (before.data.m_cis_environment.temperature_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_cis_environment.temperature_age_ms+17u));
            CHECK(m.data.m_cis_environment.humidity_age_ms == (before.data.m_cis_environment.humidity_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_cis_environment.humidity_age_ms+17u));
            CHECK(m.data.m_cis_environment.illuminance_age_ms == (before.data.m_cis_environment.illuminance_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_cis_environment.illuminance_age_ms+17u));
            break;
        case 0x35u:
            CHECK(m.data.m_cis_occupant.vision_age_ms == (before.data.m_cis_occupant.vision_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_cis_occupant.vision_age_ms+17u));
            break;
        case 0x36u:
            CHECK(m.data.m_cis_rear.source_age_ms == (before.data.m_cis_rear.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_cis_rear.source_age_ms+17u));
            break;
        case 0x37u:
            CHECK(m.data.m_cis_status.source_age_ms == (before.data.m_cis_status.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_cis_status.source_age_ms+17u));
            break;
        case 0x3Au:
            CHECK(m.data.m_vss_status.last_fault_age_ms == (before.data.m_vss_status.last_fault_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_vss_status.last_fault_age_ms+17u));
            CHECK(m.data.m_vss_status.source_age_ms == (before.data.m_vss_status.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_vss_status.source_age_ms+17u));
            break;
        case 0x38u:
            CHECK(m.data.m_window_state.source_age_ms == (before.data.m_window_state.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_window_state.source_age_ms+17u));
            break;
        case 0x39u:
            CHECK(m.data.m_window_fault.source_age_ms == (before.data.m_window_fault.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.m_window_fault.source_age_ms+17u));
            break;
        case 0x60u:
            CHECK(m.data.p_vision_result.vision_age_ms == (before.data.p_vision_result.vision_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.p_vision_result.vision_age_ms+17u));
            break;
        case 0x61u:
            CHECK(m.data.p_vision_status.status_age_ms == (before.data.p_vision_status.status_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.p_vision_status.status_age_ms+17u));
            break;
        case 0x62u:
            CHECK(m.data.p_permission.source_age_ms == (before.data.p_permission.source_age_ms > UINT16_MAX-17u ? UINT16_MAX : before.data.p_permission.source_age_ms+17u));
            break;
        default: CHECK(0);
        }
        CHECK(net_upper_uart_add_age(&m, UINT64_MAX) == NET_WIRE_OK);
        switch (m.type) {
        case 0x01u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_context.app_age_ms == UINT16_MAX);
            CHECK(m.data.m_context.proximity_age_ms == UINT16_MAX);
            CHECK(m.data.m_context.registration_connection_age_ms == UINT16_MAX);
            break;
        case 0x10u:
            CHECK(net_upper_uart_max_age(&m) == 0u);
            break;
        case 0x11u:
            CHECK(net_upper_uart_max_age(&m) == 0u);
            break;
        case 0x12u:
            CHECK(net_upper_uart_max_age(&m) == 0u);
            break;
        case 0x02u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_domain_alive.source_age_ms == UINT16_MAX);
            break;
        case 0x20u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_result.result_age_ms == UINT16_MAX);
            break;
        case 0x21u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_warning.source_age_ms == UINT16_MAX);
            break;
        case 0x22u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_availability.source_age_ms == UINT16_MAX);
            break;
        case 0x23u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_digital_status.source_age_ms == UINT16_MAX);
            break;
        case 0x24u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_digital_result.result_age_ms == UINT16_MAX);
            break;
        case 0x25u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_user_settings.source_age_ms == UINT16_MAX);
            break;
        case 0x40u:
            CHECK(net_upper_uart_max_age(&m) == 0u);
            break;
        case 0x30u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_bcm_door_state.source_age_ms == UINT16_MAX);
            break;
        case 0x31u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_bcm_climate_state.source_age_ms == UINT16_MAX);
            break;
        case 0x32u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_bcm_light_state.source_age_ms == UINT16_MAX);
            break;
        case 0x33u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_bcm_status.source_age_ms == UINT16_MAX);
            break;
        case 0x34u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_cis_environment.temperature_age_ms == UINT16_MAX);
            CHECK(m.data.m_cis_environment.humidity_age_ms == UINT16_MAX);
            CHECK(m.data.m_cis_environment.illuminance_age_ms == UINT16_MAX);
            break;
        case 0x35u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_cis_occupant.vision_age_ms == UINT16_MAX);
            break;
        case 0x36u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_cis_rear.source_age_ms == UINT16_MAX);
            break;
        case 0x37u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_cis_status.source_age_ms == UINT16_MAX);
            break;
        case 0x3Au:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_vss_status.last_fault_age_ms == UINT16_MAX);
            CHECK(m.data.m_vss_status.source_age_ms == UINT16_MAX);
            break;
        case 0x38u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_window_state.source_age_ms == UINT16_MAX);
            break;
        case 0x39u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.m_window_fault.source_age_ms == UINT16_MAX);
            break;
        case 0x60u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.p_vision_result.vision_age_ms == UINT16_MAX);
            break;
        case 0x61u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.p_vision_status.status_age_ms == UINT16_MAX);
            break;
        case 0x62u:
            CHECK(net_upper_uart_max_age(&m) == UINT16_MAX);
            CHECK(m.data.p_permission.source_age_ms == UINT16_MAX);
            break;
        default: CHECK(0);
        }
    }
    m.link=NET_WIRE_UART_C; m.type=0x61u;
    memset(&m.data,0,sizeof(m.data)); m.data.p_vision_status.vision_status=1u;
    m.data.p_vision_status.status_sequence=7u; m.data.p_vision_status.status_age_ms=12u;
    CHECK(net_upper_uart_add_age(&m,5u)==NET_WIRE_OK);
    CHECK(m.data.p_vision_status.status_sequence==7u && m.data.p_vision_status.status_age_ms==17u);
    {
        NetWireUartFrame f=frame_at(20u),encoded;
        CHECK(vectors[20].type==0x3au);
        CHECK(net_upper_uart_decode(PROFILE,NET_WIRE_UART_M,NET_WIRE_DOWNLINK,&f,&m)==NET_WIRE_OK);
        m.data.m_vss_status.last_fault_quality=3u;m.data.m_vss_status.vss_last_fault=0u;
        m.data.m_vss_status.last_fault_occurrence=0u;m.data.m_vss_status.last_fault_age_ms=0u;
        m.data.m_vss_status.source_age_ms=17u;
        CHECK(net_upper_uart_encode(PROFILE,&m,&encoded)==NET_WIRE_OK);
        CHECK(net_upper_uart_add_age(&m,90u)==NET_WIRE_OK);
        CHECK(m.data.m_vss_status.last_fault_age_ms==0u && m.data.m_vss_status.source_age_ms==107u);
        CHECK(net_upper_uart_encode(PROFILE,&m,&encoded)==NET_WIRE_OK);
    }
    return 0;
}
static int query_context(void)
{
    size_t i, changed = 0u;
    for (i = 0u; i < sizeof(vectors)/sizeof(vectors[0]); ++i) {
        NetWireUartFrame f=frame_at(i), actual;
        NetUpperUartMessage m, before;
        NetWireStatus s;
        CHECK(net_upper_uart_decode(PROFILE,vectors[i].link,vectors[i].direction,&f,&m)==NET_WIRE_OK);
        before=m;
        s=net_upper_uart_set_query_context(&m,7u,8u,9u,10u);
        if (vectors[i].link==NET_WIRE_UART_M && vectors[i].direction==NET_WIRE_DOWNLINK &&
            m.type!=0x02u && m.type!=0x40u) {
            ++changed; CHECK(s==NET_WIRE_OK);
            CHECK(net_upper_uart_encode(PROFILE,&m,&actual)==NET_WIRE_OK);
            /* Only payload current context 20 bytes changed; original identity/ages unchanged. */
            CHECK(memcmp(actual.bytes+28u,f.bytes+28u,f.length-30u)==0);
            CHECK(actual.bytes[8]==7u && actual.bytes[12]==8u && actual.bytes[20]==9u && actual.bytes[24]==10u);
        } else {
            CHECK(s!=NET_WIRE_OK && memcmp(&m,&before,sizeof(m))==0);
        }
    }
    CHECK(changed==17u);
    return 0;
}
static int receive_all(void)
{
    size_t i,j;
    NetWireStreamPolicy policy={true,1000u,false};
    for(i=0u;i<sizeof(vectors)/sizeof(vectors[0]);++i) {
        NetUpperUartReceiver r;
        NetUpperUartDelivery d, sentinel;
        CHECK(net_upper_uart_receiver_init(&r,PROFILE,vectors[i].link,vectors[i].direction,&policy,3u)==NET_WIRE_OK);
        memset(&d,0xa6,sizeof(d)); sentinel=d;
        for(j=0u;j<vectors[i].length;++j) {
            bool consumed=false;
            NetWireStatus s=net_upper_uart_receiver_feed(&r,vectors[i].bytes[j],100u+j,10000u,true,3u,&consumed,&d);
            CHECK(consumed);
            CHECK(s==(j+1u==vectors[i].length?NET_WIRE_OK:NET_WIRE_NEED_MORE));
            if(s!=NET_WIRE_OK) CHECK(memcmp(&d,&sentinel,sizeof(d))==0);
        }
        CHECK(d.message.type==vectors[i].type && d.port_epoch==3u && d.frame.length==vectors[i].length);
        for(j=0u;j<d.frame.length;++j) CHECK(d.byte_at_ms[j]==100u+j);
        CHECK(d.frame.captured_at_ms==100u+d.frame.length-1u);
        printf("{\"case\":\"uart26_product_receiver\",\"wire\":\"%s\",\"first_byte_at\":100,\"last_byte_at\":%llu,\"worker_now\":10000,\"exact_times\":true}\n",
            vectors[i].name,(unsigned long long)d.frame.captured_at_ms);
    }
    return 0;
}
static void independent_crc(uint8_t *bytes,size_t length)
{
    unsigned crc=0xffffu;size_t i;unsigned bit;
    for(i=2u;i<length-2u;++i) {
        crc^=(unsigned)bytes[i]<<8u;
        for(bit=0u;bit<8u;++bit)
            crc=(crc&0x8000u)?((crc<<1u)^0x1021u)&0xffffu:(crc<<1u)&0xffffu;
    }
    bytes[length-2u]=(uint8_t)crc;bytes[length-1u]=(uint8_t)(crc>>8u);
}

static int stream_errors(void)
{
    NetUpperUartReceiver r, saved;
    NetUpperUartDelivery d, before;
    NetWireStreamPolicy policy={true,1000u,false};
    bool consumed=true;
    size_t i;
    CHECK(net_upper_uart_receiver_init(&r,PROFILE,NET_WIRE_UART_M,NET_WIRE_UPLINK,&policy,3u)==NET_WIRE_OK);
    memset(&d,0xa6,sizeof(d)); before=d; saved=r;
    CHECK(net_upper_uart_receiver_feed(&r,0xa5u,100u,100u,false,3u,&consumed,&d)==NET_WIRE_TIME_HOLD);
    CHECK(memcmp(&r,&saved,sizeof(r))==0 && memcmp(&d,&before,sizeof(d))==0 && consumed);
    CHECK(net_upper_uart_receiver_feed(&r,0xa5u,100u,99u,true,3u,&consumed,&d)==NET_WIRE_TIME_HOLD);
    CHECK(net_upper_uart_receiver_feed(&r,0xa5u,100u,100u,true,4u,&consumed,&d)==NET_WIRE_CONTRACT_HOLD);
    CHECK(net_upper_uart_receiver_feed(&r,0xa5u,100u,100u,true,3u,&consumed,&d)==NET_WIRE_NEED_MORE);
    saved=r;
    CHECK(net_upper_uart_receiver_feed(&r,0x5au,99u,100u,true,3u,&consumed,&d)==NET_WIRE_TIME_HOLD);
    CHECK(memcmp(&r,&saved,sizeof(r))==0);
    CHECK(net_upper_uart_receiver_signal(&r,3u,true,false)==NET_WIRE_OK);
    CHECK(r.stream.count==0u && r.stream.byte_losses==1u && r.stream.last_capture_ms==100u);
    CHECK(net_upper_uart_receiver_feed(&r,0xa5u,99u,100u,true,3u,&consumed,&d)==NET_WIRE_TIME_HOLD);
    CHECK(net_upper_uart_receiver_signal(&r,3u,false,true)==NET_WIRE_CONTRACT_HOLD);
    CHECK(net_upper_uart_receiver_signal(&r,4u,false,true)==NET_WIRE_OK);
    CHECK(net_upper_uart_receiver_feed(&r,0x00u,1u,1u,true,4u,&consumed,&d)==NET_WIRE_NEED_MORE);
    for(i=0u;i<sizeof(golden_m_request);++i) {
        CHECK(net_upper_uart_receiver_feed(&r,golden_m_request[i],10u+i,200u,true,4u,&consumed,&d)==
            (i+1u==sizeof(golden_m_request)?NET_WIRE_OK:NET_WIRE_NEED_MORE));
    }
    CHECK(d.byte_at_ms[0]==10u && r.stream.discarded_bytes>=1u);
    CHECK(net_upper_uart_receiver_poll(&r,200u,4u,&d)==NET_WIRE_NEED_MORE);
    CHECK(net_upper_uart_receiver_feed(&r,0xa5u,199u,200u,true,4u,&consumed,&d)==NET_WIRE_TIME_HOLD);
    {
        uint8_t bad[sizeof(golden_m_request)];
        memcpy(bad,golden_m_request,sizeof(bad));bad[24]=0xffu; /* unknown KIND, CRC valid */
        independent_crc(bad,sizeof(bad));before=d;
        for(i=0u;i<sizeof(bad);++i) {
            NetWireStatus status=net_upper_uart_receiver_feed(&r,bad[i],300u+i,400u,true,4u,&consumed,&d);
            CHECK(consumed && status==(i+1u==sizeof(bad)?NET_WIRE_MALFORMED:NET_WIRE_NEED_MORE));
        }
        CHECK(r.typed_rejected==1u && r.stream.count==0u && memcmp(&d,&before,sizeof(d))==0);
        /* The rejected structural boundary stays consumed; next complete frame is independent. */
        for(i=0u;i<sizeof(golden_m_request);++i) {
            NetWireStatus status=net_upper_uart_receiver_feed(&r,golden_m_request[i],500u+i,600u,true,4u,&consumed,&d);
            CHECK(consumed && status==(i+1u==sizeof(golden_m_request)?NET_WIRE_OK:NET_WIRE_NEED_MORE));
        }
        CHECK(d.byte_at_ms[0]==500u);
    }
    return 0;
}
static int buffered_suffix(void)
{
    NetUpperUartReceiver r;
    NetUpperUartDelivery d;
    NetWireStreamPolicy policy={true,1000u,false};
    uint8_t outer[sizeof(golden_m_availability)];
    bool consumed=false;
    size_t i;
    memcpy(outer,golden_m_availability,sizeof(outer));
    memcpy(outer+8u,golden_m_domain_alive,sizeof(golden_m_domain_alive));
    memcpy(outer+8u+sizeof(golden_m_domain_alive),golden_m_domain_alive,sizeof(golden_m_domain_alive));
    /* CRC of outer is deliberately invalid; two complete nested suffixes are real bytes. */
    CHECK(net_upper_uart_receiver_init(&r,PROFILE,NET_WIRE_UART_M,NET_WIRE_DOWNLINK,&policy,3u)==NET_WIRE_OK);
    for(i=0u;i<sizeof(outer);++i) {
        NetWireStatus s=net_upper_uart_receiver_feed(&r,outer[i],100u+i,500u,true,3u,&consumed,&d);
        CHECK(consumed && s==(i+1u==sizeof(outer)?NET_WIRE_OK:NET_WIRE_NEED_MORE));
    }
    CHECK(d.message.type==0x02u && d.byte_at_ms[0]==108u && d.frame.captured_at_ms==129u);
    CHECK(net_upper_uart_receiver_feed(&r,0x55u,200u,500u,true,3u,&consumed,&d)==NET_WIRE_OK);
    CHECK(!consumed && d.byte_at_ms[0]==130u && d.frame.captured_at_ms==151u);
    CHECK(net_upper_uart_receiver_feed(&r,0x55u,200u,500u,true,3u,&consumed,&d)==NET_WIRE_NEED_MORE && consumed);
    CHECK(r.stream.bad_crc==1u && r.delivered_frames==2u);
    /* Independent review counterexample: identical inner bytes and equal end
     * timestamps must retain the FIRST frame's distinct original time vector. */
    CHECK(net_upper_uart_receiver_init(&r,PROFILE,NET_WIRE_UART_M,NET_WIRE_DOWNLINK,&policy,3u)==NET_WIRE_OK);
    for(i=0u;i<sizeof(outer);++i) {
        uint64_t at = i < 8u ? 70u : (i < 29u ? 80u+i-8u : 100u);
        NetWireStatus s=net_upper_uart_receiver_feed(&r,outer[i],at,500u,true,3u,&consumed,&d);
        CHECK(consumed && s==(i+1u==sizeof(outer)?NET_WIRE_OK:NET_WIRE_NEED_MORE));
    }
    CHECK(d.byte_at_ms[0]==80u && d.byte_at_ms[20]==100u && d.byte_at_ms[21]==100u);
    CHECK(net_upper_uart_receiver_feed(&r,0x55u,200u,500u,true,3u,&consumed,&d)==NET_WIRE_OK);
    CHECK(!consumed);
    for(i=0u;i<d.frame.length;++i) CHECK(d.byte_at_ms[i]==100u);
    return 0;
}
static uint64_t read_le_test(const uint8_t *p,size_t count)
{
    uint64_t value=0u; size_t i;
    for(i=0u;i<count;++i) value|=(uint64_t)p[i]<<(8u*i);
    return value;
}
static uint16_t projection_can_id(uint8_t type)
{
    switch(type) {
    case 0x30u:return 0x200u; case 0x31u:return 0x210u;
    case 0x32u:return 0x220u; case 0x33u:return 0x230u;
    case 0x34u:return 0x300u; case 0x35u:return 0x310u;
    case 0x36u:return 0x0a0u; case 0x37u:return 0x320u;
    case 0x3au:return 0x340u; case 0x38u:return 0x0b0u;
    case 0x39u:return 0x330u; default:return 0u;
    }
}
static int projection11(void)
{
    size_t i,count=0u;
    for(i=0u;i<sizeof(vectors)/sizeof(vectors[0]);++i) {
        uint16_t id=projection_can_id(vectors[i].type);
        const NetWireRegistryEntry *entry;
        NetWireCanEnvelope can={0};
        NetWireUartFrame encoded;
        NetUpperCanMessage typed;
        NetUpperUartMessage projected;
        const uint8_t *wire=vectors[i].bytes;
        if(id==0u) continue;
        ++count; entry=net_wire_registry_find(NET_WIRE_CAN,id); CHECK(entry!=NULL);
        can.standard_id=id; can.fdf=true; can.brs=true;
        can.length=entry->frame_length; can.dlc=entry->dlc;
        memcpy(can.bytes,wire+28u,entry->payload_length);
        CHECK(net_upper_can_decode(PROFILE,&can,&typed)==NET_WIRE_OK);
        CHECK(net_upper_uart_project_can(PROFILE,&typed,(uint32_t)read_le_test(wire+8u,4u),
            read_le_test(wire+12u,8u),(uint32_t)read_le_test(wire+20u,4u),
            (uint32_t)read_le_test(wire+24u,4u),0x5aa5u,&projected)==NET_WIRE_OK);
        CHECK(net_upper_uart_encode(PROFILE,&projected,&encoded)==NET_WIRE_OK);
        CHECK(encoded.length==vectors[i].length && memcmp(encoded.bytes,wire,encoded.length)==0);
        printf("{\"case\":\"same_can_projection11\",\"wire\":\"%s\",\"same_origin_bytes\":true,\"golden_equal\":true}\n",vectors[i].name);
    }
    CHECK(count==11u);
    return 0;
}

typedef struct {
    uint64_t now;
    const NetUpperPacket *borrowed[16];
    NetUpperPacket sent[16]; NetTxKey keys[16];
    size_t sent_count, uart_count, can_count, rear_count;
    NetUpperUartDelivery received;
    NetUpperDelivery metadata;
    NetStatus owner_status;
} ProductPeer;
static NetStatus peer_now(void *context,uint64_t *out)
{
    ProductPeer *p=(ProductPeer *)context;
    if(out==NULL) return NET_INVALID_ARGUMENT;
    *out=p->now; return NET_OK;
}
static NetStatus peer_submit(void *context,const NetUpperPacket *packet,NetTxKey key)
{
    ProductPeer *p=(ProductPeer *)context;
    if(p->sent_count>=16u) return NET_NO_CAPACITY;
    p->borrowed[p->sent_count]=packet; p->sent[p->sent_count]=*packet;
    p->keys[p->sent_count++]=key; return NET_OK;
}
static NetStatus peer_poll(void *context,NetUpperEvent *out)
{
    (void)context; (void)out; return NET_NO_WORK;
}
static NetStatus peer_uart(void *context,const NetUpperUartDelivery *message,const NetUpperDelivery *meta)
{
    ProductPeer *p=(ProductPeer *)context;
    ++p->uart_count; p->received=*message; p->metadata=*meta;
    return p->owner_status;
}
static NetStatus peer_can(void *context,const NetUpperCanMessage *message,const NetUpperDelivery *meta)
{
    ProductPeer *p=(ProductPeer *)context;
    (void)message; ++p->can_count; p->metadata=*meta; return p->owner_status;
}
static NetStatus peer_rear(void *context,const NetUpperRear *message,const NetUpperDelivery *meta)
{
    ProductPeer *p=(ProductPeer *)context;
    (void)message; ++p->rear_count; p->metadata=*meta; return p->owner_status;
}
static const NetUpperPortOps peer_ops={peer_now,peer_submit,peer_poll};
static int product_init(NetUpper *service,ProductPeer *peer,uint32_t node)
{
    NetUpperConfig config={0}; size_t i;
    memset(service,0,sizeof(*service)); /* Each scenario supplies fresh service storage. */
    memset(peer,0,sizeof(*peer)); peer->owner_status=NET_OK;
    config.node=node; config.local_epoch=7u;
    config.port_epoch[0]=3u;config.port_epoch[1]=3u;config.port_epoch[2]=3u;
    config.profile=PROFILE; config.port.ops=&peer_ops; config.port.context=peer;
    config.owner.context=peer;config.owner.uart=peer_uart;config.owner.can=peer_can;config.owner.rear=peer_rear;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i) {
        config.rx_policy[i].bound=true;config.rx_policy[i].maximum_age_ms=10000u;
        config.rx_policy[i].expires_at_equality=true;
    }
    for(i=0u;i<2u;++i) {
        config.uart_policy[i].candidate_time_policy_selected=true;
        config.uart_policy[i].assembly_limit_ms=1000u;
    }
    CHECK(net_upper_init(service,&config)==NET_OK);
    return 0;
}
static int inject_uart(NetUpper *service,ProductPeer *peer,const NetUpperUartMessage *message,uint64_t start)
{
    NetWireUartFrame frame;size_t j;
    CHECK(net_upper_uart_encode(PROFILE,message,&frame)==NET_WIRE_OK);
    for(j=0u;j<frame.length;++j) {
        NetUpperEvent e={0};
        e.kind=NET_UPPER_UART_BYTE;e.link=message->link;e.byte=frame.bytes[j];
        e.capture.port_epoch=3u;e.capture.timestamp_known=true;e.capture.captured_at_ms=start+j;
        peer->now=start+j;
        CHECK(net_upper_capture(service,&e)==NET_OK);
        /* Partial byte assembly is real work without a complete typed delivery. */
        CHECK(net_upper_step(service,NET_UPPER_RX_CAPACITY)==
            (j+1u==frame.length?NET_OK:NET_NO_WORK));
    }
    return 0;
}
static void clear_test_ages(NetUpperUartMessage *m)
{
    switch(m->type) {
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
    default: break;
    }
}
static int inject_context(NetUpper *service,ProductPeer *peer,uint32_t device,uint64_t session)
{
    NetUpperUartMessage c;NetWireUartFrame f=frame_at(0u);
    CHECK(net_upper_uart_decode(PROFILE,NET_WIRE_UART_M,NET_WIRE_UPLINK,&f,&c)==NET_WIRE_OK);
    clear_test_ages(&c);c.data.m_context.device_context_id=device;c.data.m_context.session_id=session;c.sequence=1u;
    CHECK(inject_uart(service,peer,&c,100u)==0);
    CHECK(peer->uart_count==1u && peer->received.message.type==0x01u);
    return 0;
}
static int bootstrap_esp_context(NetUpper *service,ProductPeer *peer,uint32_t device,uint64_t session)
{
    NetUpperUartMessage c;NetWireUartFrame f=frame_at(0u);
    NetSourceStamp source={1u,100u,0u};NetAgePolicy policy={true,10000u,true};NetTxKey key;
    NetUpperEvent event={0};
    CHECK(net_upper_uart_decode(PROFILE,NET_WIRE_UART_M,NET_WIRE_UPLINK,&f,&c)==NET_WIRE_OK);
    clear_test_ages(&c);c.data.m_context.device_context_id=device;c.data.m_context.session_id=session;c.sequence=1u;
    peer->now=100u;
    CHECK(net_upper_submit_uart(service,&c,&source,&policy,&key)==NET_OK);
    CHECK(net_upper_step(service,NET_UPPER_RX_CAPACITY)==NET_OK && peer->sent_count==1u);
    event.kind=NET_UPPER_TX_EVENT;event.link=NET_WIRE_UART_M;event.tx.key=key;
    event.tx.admission_observed=true;event.tx.accepted=true;event.tx.outcome=NET_TX_SUCCEEDED;
    event.tx.release_observed=true;peer->borrowed[0]=NULL;
    CHECK(net_upper_capture(service,&event)==NET_OK && net_upper_step(service,NET_UPPER_RX_CAPACITY)==NET_OK);
    CHECK(net_upper_retire(service,key)==NET_OK);
    peer->sent_count=0u; /* Fully retired prior bootstrap; next capture starts a new packet log. */
    return 0;
}

static int inject_can(NetUpper *service,ProductPeer *peer,const NetUpperCanMessage *message,
    NetUpperCanMessage *accepted,NetSourceStamp *source)
{
    NetUpperEvent e={0};NetUpperDelivery d;
    e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;
    e.capture.port_epoch=3u;e.capture.timestamp_known=true;e.capture.captured_at_ms=1000u;
    CHECK(net_upper_can_encode(PROFILE,message,&e.can)==NET_WIRE_OK);
    peer->now=1000u;CHECK(net_upper_capture(service,&e)==NET_OK);
    CHECK(net_upper_step(service,NET_UPPER_RX_CAPACITY)==NET_OK);
    CHECK(net_upper_received_can(service,message->id,accepted,&d,source)==NET_OK);
    return 0;
}
static int from_projection(const NetUpperUartMessage *message,NetUpperCanMessage *can)
{
    NetWireUartFrame f;NetWireCanEnvelope c={0};
    uint16_t id=projection_can_id(message->type);
    const NetWireRegistryEntry *entry=net_wire_registry_find(NET_WIRE_CAN,id);
    CHECK(entry!=NULL);
    CHECK(net_upper_uart_encode(PROFILE,message,&f)==NET_WIRE_OK);
    c.standard_id=id;c.fdf=true;c.brs=true;c.dlc=entry->dlc;c.length=entry->frame_length;
    memcpy(c.bytes,f.bytes+28u,entry->payload_length);
    CHECK(net_upper_can_decode(PROFILE,&c,can)==NET_WIRE_OK);
    return 0;
}
static int product_routes(void)
{
    size_t i,passed=0u,disabled=0u;
    for(i=0u;i<sizeof(vectors)/sizeof(vectors[0]);++i) {
        static NetUpper sender,receiver;
        ProductPeer sp,rp;
        NetUpperUartMessage m,owned;
        NetWireUartFrame f=frame_at(i),expected;
        const NetWireRegistryEntry *entry=net_wire_registry_find(vectors[i].link,vectors[i].type);
        NetSourceStamp source={77u,1000u,0u};NetAgePolicy policy={true,10000u,true};NetTxKey key;
        NetStatus submit;
        uint32_t device;uint64_t session;
        CHECK(entry!=NULL);
        CHECK(product_init(&sender,&sp,entry->sender)==0);
        CHECK(product_init(&receiver,&rp,entry->receivers)==0);
        CHECK(net_upper_uart_decode(PROFILE,vectors[i].link,vectors[i].direction,&f,&m)==NET_WIRE_OK);
        clear_test_ages(&m);m.sequence=100u;
        CHECK(net_upper_uart_encode(PROFILE,&m,&f)==NET_WIRE_OK);
        device=(uint32_t)read_le_test(f.bytes+8u,4u);session=read_le_test(f.bytes+12u,8u);
        if(m.link==NET_WIRE_UART_M && m.type!=0x01u && m.type!=0x02u) {
            if(entry->sender==NET_WIRE_NODE_DOMAIN) CHECK(inject_context(&sender,&sp,device,session)==0);
            if(entry->receivers==NET_WIRE_NODE_DOMAIN) CHECK(inject_context(&receiver,&rp,device,session)==0);
            if(entry->sender==NET_WIRE_NODE_ESP32) CHECK(bootstrap_esp_context(&sender,&sp,device,session)==0);
            if(entry->receivers==NET_WIRE_NODE_ESP32) CHECK(bootstrap_esp_context(&receiver,&rp,device,session)==0);
        }
        if(projection_can_id(m.type)!=0u && m.type!=0x38u && m.type!=0x39u) {
            NetUpperCanMessage can,accepted;
            CHECK(from_projection(&m,&can)==0);
            CHECK(inject_can(&sender,&sp,&can,&accepted,&source)==0);
            CHECK(net_upper_uart_project_can(PROFILE,&accepted,device,session,
                (uint32_t)read_le_test(f.bytes+20u,4u),(uint32_t)read_le_test(f.bytes+24u,4u),100u,&m)==NET_WIRE_OK);
        }
        if(m.type==0x62u) {
            NetUpperCanMessage can={0},accepted;
            can.id=0x080u;can.data.domain_permission.boot_id=10u;can.data.domain_permission.tx_sequence=20u;
            can.data.domain_permission.update_sequence=m.data.p_permission.update_sequence;
            can.data.domain_permission.vehicle_power_permission=0u;
            can.data.domain_permission.power_permission_quality=0u;
            can.data.domain_permission.operation_quality=0u;
            CHECK(inject_can(&sender,&sp,&can,&accepted,&source)==0);
            m.data.p_permission.vehicle_power_permission=0u;m.data.p_permission.value_quality=0u;
            m.data.p_permission.quality_reason=0u;m.data.p_permission.source_age_ms=0u;
        }
        sp.now=1000u;owned=m;
        CHECK(net_upper_uart_encode(PROFILE,&owned,&expected)==NET_WIRE_OK);
        submit=net_upper_submit_uart(&sender,&m,&source,&policy,&key);
        if(m.type==0x38u || m.type==0x39u) {
            CHECK(submit==NET_HOLD);++disabled;
            printf("{\"case\":\"uart_product_routes\",\"wire\":\"%s\",\"status\":\"WINDOW_DISABLED_HOLD\"}\n",vectors[i].name);
            continue;
        }
        CHECK(submit==NET_OK);
        memset(&m,0xa6,sizeof(m));
        CHECK(net_upper_step(&sender,NET_UPPER_RX_CAPACITY)==NET_OK && sp.sent_count==1u);
        CHECK(sp.sent[0].uart.length==expected.length && memcmp(sp.sent[0].uart.bytes,expected.bytes,expected.length)==0);
        CHECK(memcmp(sp.borrowed[0]->uart.bytes,expected.bytes,expected.length)==0);
        {
            NetUpperTxSnapshot state;
            CHECK(net_upper_tx_snapshot(&sender,key,&state)==NET_OK);
            CHECK(state.submitted && state.outcome==NET_TX_UNKNOWN && !state.reference_released);
        }
        CHECK(inject_uart(&receiver,&rp,&owned,2000u)==0);
        CHECK(rp.received.message.type==owned.type);
        CHECK(rp.received.frame.length==expected.length);
        CHECK(memcmp(rp.received.frame.bytes,expected.bytes,expected.length)==0);
        {
            NetUpperEvent event={0};NetUpperTxSnapshot state;
            event.kind=NET_UPPER_TX_EVENT;event.link=owned.link;event.tx.key=key;
            event.tx.admission_observed=true;event.tx.accepted=true;
            CHECK(net_upper_capture(&sender,&event)==NET_OK && net_upper_step(&sender,NET_UPPER_RX_CAPACITY)==NET_OK);
            event.tx.admission_observed=false;event.tx.release_observed=true;
            sp.borrowed[0]=NULL; /* Actual FakePort borrow ends before release event. */
            CHECK(net_upper_capture(&sender,&event)==NET_OK && net_upper_step(&sender,NET_UPPER_RX_CAPACITY)==NET_OK);
            CHECK(net_upper_tx_snapshot(&sender,key,&state)==NET_OK);
            CHECK(state.reference_released && state.outcome==NET_TX_UNKNOWN);
            CHECK(net_upper_retire(&sender,key)==NET_HOLD);
            event.tx.release_observed=false;event.tx.outcome=NET_TX_SUCCEEDED;
            CHECK(net_upper_capture(&sender,&event)==NET_OK && net_upper_step(&sender,NET_UPPER_RX_CAPACITY)==NET_OK);
            CHECK(net_upper_tx_snapshot(&sender,key,&state)==NET_OK && state.outcome==NET_TX_SUCCEEDED);
            CHECK(net_upper_retire(&sender,key)==NET_OK);
        }
        ++passed;
        printf("{\"case\":\"uart_product_routes\",\"wire\":\"%s\",\"submit\":\"net_upper_submit_uart\",\"queued_copy_immutable\":true,\"owner_type\":%u,\"owner_calls\":%lu,\"release_before_final\":true,\"port_final\":\"SUCCEEDED_OBSERVED\",\"status\":\"PRODUCT_HOST_PASS\"}\n",
            vectors[i].name,(unsigned)rp.received.message.type,(unsigned long)rp.uart_count);
    }
    CHECK(passed==24u && disabled==2u);
    return 0;
}

static int product_holds(void)
{
    static NetUpper service;
    ProductPeer peer;
    NetUpperUartMessage m;
    NetWireUartFrame f=frame_at(5u);
    NetSourceStamp source={77u,1000u,0u};NetAgePolicy policy={true,10000u,true};NetTxKey key={99u,99u};
    CHECK(vectors[5].type==0x20u);
    CHECK(product_init(&service,&peer,NET_WIRE_NODE_DOMAIN)==0);
    CHECK(net_upper_uart_decode(PROFILE,NET_WIRE_UART_M,NET_WIRE_DOWNLINK,&f,&m)==NET_WIRE_OK);
    clear_test_ages(&m);peer.now=1000u;
    CHECK(net_upper_submit_uart(&service,&m,&source,&policy,&key)==NET_HOLD);
    CHECK(key.epoch==99u && key.token==99u && service.next_token==1u);
    CHECK(inject_context(&service,&peer,m.data.m_result.device_context_id,m.data.m_result.session_id)==0);
    peer.now=1000u;
    CHECK(net_upper_submit_uart(&service,&m,&source,&policy,&key)==NET_OK);
    CHECK(net_upper_cancel(&service,key)==NET_OK && net_upper_retire(&service,key)==NET_OK);
    {
        NetUpperCanMessage can={0},accepted;
        NetUpperUartMessage projection;
        NetSourceStamp actual;
        can.id=0x0a0u;can.data.cis_rear.boot_id=10u;can.data.cis_rear.tx_sequence=20u;
        can.data.cis_rear.update_sequence=30u;can.data.cis_rear.rear_distance_deci_cm=123u;
        can.data.cis_rear.validity=1u;
        CHECK(net_upper_uart_project_can(PROFILE,&can,m.data.m_result.device_context_id,
            m.data.m_result.session_id,11u,0u,20u,&projection)==NET_WIRE_OK);
        CHECK(net_upper_submit_uart(&service,&projection,&source,&policy,&key)==NET_HOLD);
        CHECK(inject_can(&service,&peer,&can,&accepted,&actual)==0);
        CHECK(net_upper_uart_project_can(PROFILE,&accepted,m.data.m_result.device_context_id,
            m.data.m_result.session_id,11u,0u,20u,&projection)==NET_WIRE_OK);
        source=actual;++source.observation_id;
        CHECK(net_upper_submit_uart(&service,&projection,&source,&policy,&key)==NET_HOLD);
        CHECK(net_upper_submit_uart(&service,&projection,&actual,&policy,&key)==NET_OK);
        CHECK(net_upper_cancel(&service,key)==NET_OK && net_upper_retire(&service,key)==NET_OK);
    }
    CHECK(product_init(&service,&peer,NET_WIRE_NODE_CIS)==0);
    {
        NetUpperCanMessage can={0},accepted;
        NetUpperUartMessage permission={0};NetSourceStamp actual;
        can.id=0x080u;can.data.domain_permission.boot_id=10u;can.data.domain_permission.tx_sequence=20u;
        can.data.domain_permission.update_sequence=30u;can.data.domain_permission.power_permission_quality=3u;
        can.data.domain_permission.operation_quality=3u;
        CHECK(inject_can(&service,&peer,&can,&accepted,&actual)==0);
        permission.link=NET_WIRE_UART_C;permission.type=0x62u;permission.sequence=1u;
        permission.data.p_permission.cis_boot_id=40u;permission.data.p_permission.update_sequence=30u;
        permission.data.p_permission.value_quality=3u;permission.data.p_permission.quality_reason=6u;
        permission.data.p_permission.use_limit_ms=1500u;
        CHECK(net_upper_submit_uart(&service,&permission,&actual,&policy,&key)==NET_HOLD);
        /* CAN has no quality_reason: an apparently reasonable NO_DATA reason is not a proved mapping. */
        can.data.domain_permission.tx_sequence=21u;can.data.domain_permission.power_permission_quality=0u;
        can.data.domain_permission.update_sequence=31u; /* New quality evaluation, not transport refresh. */
        can.data.domain_permission.operation_quality=0u;can.data.domain_permission.source_age_ms=499u;
        CHECK(inject_can(&service,&peer,&can,&accepted,&actual)==0);
        permission.data.p_permission.value_quality=0u;permission.data.p_permission.quality_reason=0u;
        permission.data.p_permission.update_sequence=31u; /* Exact accepted CAN revision for this relay. */
        permission.data.p_permission.source_age_ms=499u;
        CHECK(net_upper_submit_uart(&service,&permission,&actual,&policy,&key)==NET_OK);
        peer.now=1001u;
        CHECK(net_upper_step(&service,NET_UPPER_RX_CAPACITY)==NET_EXPIRED && peer.sent_count==0u);
        {
            NetUpperTxSnapshot state;
            CHECK(net_upper_tx_snapshot(&service,key,&state)==NET_OK && state.local_status==NET_EXPIRED);
            CHECK(!state.submitted && state.outcome==NET_TX_UNKNOWN);
        }
        CHECK(net_upper_retire(&service,key)==NET_OK);
    }
    printf("{\"case\":\"uart_product_holds\",\"missing_context\":\"HOLD\",\"forged_can_receipt\":\"HOLD\",\"unproven_no_data_reason\":\"HOLD\",\"relay_499_plus_1\":\"EXPIRED_NO_PORT_BORROW\"}\n");
    return 0;
}

int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    if(strcmp(argv[1],"product_holds")==0) return product_holds();
    if(strcmp(argv[1],"product_routes")==0) return product_routes();
    if(strcmp(argv[1],"projection11")==0) return projection11();
    if(strcmp(argv[1],"dispatch26")==0) return dispatch_all();
    if(strcmp(argv[1],"age26")==0) return age_fields();
    if(strcmp(argv[1],"query_context17")==0) return query_context();
    if(strcmp(argv[1],"receiver26")==0) return receive_all();
    if(strcmp(argv[1],"stream_errors")==0) return stream_errors();
    if(strcmp(argv[1],"buffered_suffix")==0) return buffered_suffix();
    return 2;
}
