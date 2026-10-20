#ifndef VSS_INTEG_BUS_H
#define VSS_INTEG_BUS_H

/* Bridge between the two halves of the VSS integration harness.
 *
 * firmware/vss src/net_if/vss_net_api.h and network/app/net_if_*.h declare the
 * same application types (NetVssEvent, NetCanRxMeta, ...), so no translation
 * unit includes both:
 *   vss_integ_bus.c   Network side: DOMAIN frames built with the repository
 *                     codec (net_wire_can_*), the bus (mock RTD: inject from the
 *                     "ISR", take the frames the node sent), VSS_STATUS decode,
 *                     Network diagnostics, the board hooks of time base A.
 *   test_vss_integ.c  VSS side: VssMain_Boot / VssMain_LoopOnce on the VSS HAL
 *                     stand-in, the DOMAIN stand-in, VSS observations, the
 *                     cases.
 * Everything here uses plain C types only. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* BOOT_ID the node configuration hook gives the Network (VSS node, 0x340). */
#define INTEG_NODE_BOOT 0x56535331u

/* One lane of VSS_WARNING_STATE (anti-pinch, occupant hazard, rear obstacle). */
typedef struct {
    uint32_t source_boot, source_sequence;
    uint16_t original_age_ms;
    uint8_t state, quality, reason, availability;
} IntegLane;

typedef struct {
    uint32_t boot_id, tx_sequence;
    uint8_t event_kind;
    uint32_t occurrence_id;
    uint16_t original_age_ms, use_limit_ms;
    uint8_t value_quality, input_availability;
    uint32_t related_command_id;
} IntegEvent;

typedef struct {
    uint32_t boot_id, tx_sequence;
    IntegLane antipinch, occupant, rear;
    uint8_t rear_detection_activation, activation_meta_format;
    uint8_t rear_activation_quality, rear_activation_reason;
    uint32_t rear_activation_sequence;
    uint16_t rear_activation_age_ms;
} IntegWarning;

/* Structure errors the Network must catch before VSS (VSS contract C5). Each
 * is a valid encoding with one property broken afterwards. */
typedef enum {
    INTEG_DEFECT_LONGER = 0,      /* one DLC step above the registered length */
    INTEG_DEFECT_SHORTER,         /* one DLC step below the registered length */
    INTEG_DEFECT_CLASSIC,         /* Classic CAN (no FDF), first 8 bytes */
    INTEG_DEFECT_EVENT_KIND_0,    /* VSS_EVENT kind 0 (reserved) */
    INTEG_DEFECT_EVENT_KIND_6,    /* VSS_EVENT kind past LOCK_ERROR */
    INTEG_DEFECT_META_FORMAT_2,   /* VSS_WARNING_STATE activation_meta_format 2 */
    INTEG_DEFECT_REAR_STATE_3,    /* VSS_WARNING_STATE rear obstacle state past EMERGENCY */
    INTEG_DEFECT_BOOT_ZERO        /* BOOT_ID 0 (reserved) */
} IntegDefect;

/* Bus -> node, from the CAN "ISR" (the mock calls the FlexCAN callback, which
 * reads HalTimer_GetTicks()). False when the codec refused the content or no
 * RX MB was armed. */
bool IntegBus_SendEvent(const IntegEvent *e);
bool IntegBus_SendWarning(const IntegWarning *w);
bool IntegBus_SendPermission(uint32_t boot_id, uint32_t tx_sequence, uint32_t update_sequence);
bool IntegBus_SendEventDefect(const IntegEvent *valid, IntegDefect defect);
bool IntegBus_SendWarningDefect(const IntegWarning *valid, IntegDefect defect);

/* Error "ISR": the controller reports bus-off. */
void IntegBus_RaiseBusOff(void);

/* True while one of the calls above runs (CAN interrupt context). */
bool IntegBus_InIsr(void);

/* A frame the node sent, as the bus sees it, and its VSS_STATUS content. */
typedef struct {
    uint32_t id;
    bool fd, brs;
    uint8_t dlc, length;
    bool decoded;                 /* a valid VSS_STATUS (repository codec) */
    uint32_t boot_id, tx_sequence;
    uint8_t vss_state, vss_availability, vss_accepting_events, vss_fault_active, fault_mask;
    uint16_t vss_last_fault;
    uint32_t last_fault_occurrence;
    uint16_t last_fault_age_ms;
    uint8_t last_fault_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} IntegFrame;

/* Node -> bus: the bus takes the next pending frame (TX_COMPLETE from the
 * "ISR"). True when there was one. */
bool IntegBus_TakeFrame(IntegFrame *out);

/* Power-on state of the mock controller (no OsIf system timer, as on the board). */
void IntegBus_Reset(void);

/* Network diagnostics the cases judge by. */
typedef struct {
    bool ready;                   /* Net_Init succeeded, node stage READY */
    uint32_t init_calls, main_calls;
    uint32_t node_boot_id;        /* BOOT_ID of the VSS_STATUS slot */
    bool app_tick_time;           /* time base A (application tick hooks) */
    uint32_t rx_frames;           /* frames the RTD backend received */
    uint32_t rx_delivered, rx_invalid, rx_held, stale, duplicates, rx_conflicts;
    uint32_t repeat_delivered;    /* repetitions of one original observation passed on */
    uint32_t repeat_age_raised;   /* of them: AGE raised to the bound within the slack */
    uint32_t generation_changes;  /* receive keys moved to a new DOMAIN BOOT */
    uint32_t old_generation;      /* frames of an ended DOMAIN BOOT refused */
    uint32_t event_delivered, warning_delivered, permission_received;
    uint32_t status_not_accepted;
    uint8_t last_status_result;   /* NetSubmitResult */
    uint32_t drain_budget, ring_capacity, ring_overflow, ring_max_depth, main_drained_max;
    uint32_t rx_loss_events;      /* CAN frames stored right after a ring / MB loss */
    uint32_t tick_future_clamped;
    uint32_t busoff_events, tx_complete;
    uint32_t main_max_us;
} IntegNetDiag;

bool IntegBus_ReadDiag(IntegNetDiag *out);

/* Implemented by the VSS half. Called by the node configuration hook, i.e. from
 * inside Net_Init(), to record where in the VSS boot sequence it runs. */
void IntegVss_OnNodeConfig(void);

/* VSS_INTEG_API_LAYOUT (vss_integ_layout.h) expanded with the Network headers:
 * up to cap values in list order. Returns the number of values. */
size_t IntegBus_ApiLayout(uint32_t *out, size_t cap);

#endif
