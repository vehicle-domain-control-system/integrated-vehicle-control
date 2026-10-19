#include "vss_test_input.h"

#include <string.h>

#include "../driver/input/sw2_button.h"
#include "../service/input/vss_input_net.h"
#include "../infra/vss_config.h"
#include "../infra/vss_time.h"

#define TEST_INPUT_BOOT_ID       1u
#define TEST_INPUT_USE_LIMIT_MS  500u

/* Debugger view. */
static VssTestInputObservation g_vssTestInput;

/* Warning values the frames carry. */
static NetRearWarningState s_rear;
static NetValueQuality s_rearQuality;
static NetFlag s_occupant;
static NetFlag s_antipinch;

static uint32_t s_eventTx;
static uint32_t s_warningTx;
static uint32_t s_laneSequence;
static uint32_t s_occurrence;
static uint32_t s_lastEventRxMs;
static bool s_warningSent;
static uint64_t s_lastWarningMs;

/* Step 1, kept for its retransmission in step 2. */
static NetVssEvent s_firstEvent;
static uint32_t s_firstRxMs;

/* Scratch. */
static NetVssWarningState s_frame;
static VssInputNetObservation s_adapter;

static void count_up(uint32_t *counter)
{
    if (*counter != UINT32_MAX)
    {
        ++*counter;
    }
}

static uint32_t dropped(void)
{
    memset(&s_adapter, 0, sizeof s_adapter);
    (void)VssInputNet_Read(VSS_TEST, &s_adapter);
    return s_adapter.eventsDiscarded + s_adapter.warningFramesUnverified + s_adapter.inputsWithoutClock;
}

static NetCanRxMeta meta_now(uint32_t tx)
{
    NetCanRxMeta m;

    memset(&m, 0, sizeof m);
    m.source.boot_id = TEST_INPUT_BOOT_ID;
    m.source.tx_sequence = tx;
    m.received_at_ms = VssTime_NowMs32();
    return m;
}

static void send_event(const NetVssEvent *e)
{
    uint32_t before = dropped();
    NetCanRxMeta m;

    s_eventTx++;
    m = meta_now(s_eventTx);
    s_lastEventRxMs = m.received_at_ms;
    VssInputNet_OnEvent(VSS_TEST, e, &m);
    if (dropped() != before)
    {
        g_vssTestInput.fault = true;
        return;
    }
    switch (s_adapter.lastEvent.validation)
    {
    case VSS_VALIDATION_APPLICABLE:
        count_up(&g_vssTestInput.accepted);
        break;
    case VSS_VALIDATION_NO_CHANGE:
        count_up(&g_vssTestInput.duplicates);
        break;
    default:
        count_up(&g_vssTestInput.rejected);
        break;
    }
}

static void new_event(NetVssEventKind kind)
{
    NetVssEvent e;

    memset(&e, 0, sizeof e);
    s_occurrence++;
    e.event_kind = kind;
    e.occurrence_id = s_occurrence;
    e.original_age_ms = 0u;
    e.use_limit_ms = TEST_INPUT_USE_LIMIT_MS;
    e.value_quality = NET_QUALITY_OK;
    e.input_availability = NET_AVAILABILITY_AVAILABLE;
    e.related_command_id = 0u;
    send_event(&e);
    if (s_occurrence == 1u)
    {
        s_firstEvent = e;
        s_firstRxMs = s_lastEventRxMs;
    }
}

/* The sender's retransmission of step 1: same occurrence, the next TX, a new
 * receive time and the age grown by the time since the first delivery. */
static void retransmit_first_event(void)
{
    NetVssEvent e = s_firstEvent;
    uint32_t age = VssTime_NowMs32() - s_firstRxMs;

    e.original_age_ms = (uint16_t)((age < (uint32_t)NET_AGE_SATURATED_MS) ? age : NET_AGE_SATURATED_MS);
    send_event(&e);
}

static void send_warning(void)
{
    uint32_t before = dropped();
    NetCanRxMeta m;

    s_warningTx++;
    s_laneSequence++;
    memset(&s_frame, 0, sizeof s_frame);
    s_frame.antipinch_source_boot = TEST_INPUT_BOOT_ID;
    s_frame.antipinch_source_sequence = s_laneSequence;
    s_frame.antipinch_state = s_antipinch;
    s_frame.antipinch_quality = NET_QUALITY_OK;
    s_frame.antipinch_reason = NET_QUALITY_REASON_NONE;
    s_frame.antipinch_availability = NET_AVAILABILITY_AVAILABLE;
    s_frame.occupant_hazard_source_boot = TEST_INPUT_BOOT_ID;
    s_frame.occupant_hazard_source_sequence = s_laneSequence;
    s_frame.occupant_hazard_state = s_occupant;
    s_frame.occupant_hazard_quality = NET_QUALITY_OK;
    s_frame.occupant_hazard_reason = NET_QUALITY_REASON_NONE;
    s_frame.occupant_hazard_availability = NET_AVAILABILITY_AVAILABLE;
    s_frame.rear_obstacle_source_boot = TEST_INPUT_BOOT_ID;
    s_frame.rear_obstacle_source_sequence = s_laneSequence;
    s_frame.rear_obstacle_state = s_rear;
    s_frame.rear_obstacle_quality = s_rearQuality;
    s_frame.rear_obstacle_reason = (s_rearQuality == NET_QUALITY_OK)
                                   ? NET_QUALITY_REASON_NONE : NET_QUALITY_REASON_DATA_INVALID;
    s_frame.rear_obstacle_availability = NET_AVAILABILITY_AVAILABLE;
    s_frame.rear_detection_activation = NET_FLAG_YES;
    s_frame.activation_meta_format = 1u;
    s_frame.rear_activation_quality = NET_QUALITY_OK;
    s_frame.rear_activation_reason = NET_QUALITY_REASON_NONE;
    s_frame.rear_activation_sequence = s_laneSequence;
    s_frame.rear_activation_age_ms = 0u;
    m = meta_now(s_warningTx);
    VssInputNet_OnWarningState(VSS_TEST, &s_frame, &m);
    count_up(&g_vssTestInput.warningFrames);
    if (dropped() != before)
    {
        g_vssTestInput.fault = true;
    }
    s_warningSent = true;
    s_lastWarningMs = VssTime_NowMs();
}

/* Applies the step just entered. Returns true when it changed a warning,
 * which is sent at once. */
static bool apply_step(uint8_t step)
{
    switch (step)
    {
    case 1u:  new_event(NET_VSS_EVENT_WELCOME);         return false;
    case 2u:  retransmit_first_event();                 return false;
    case 3u:  new_event(NET_VSS_EVENT_LOCK_COMPLETE);   return false;
    case 4u:  new_event(NET_VSS_EVENT_UNLOCK_COMPLETE); return false;
    case 5u:  new_event(NET_VSS_EVENT_LOCK_ERROR);      return false;
    case 6u:  new_event(NET_VSS_EVENT_GOODBYE);         return false;
    case 7u:  s_rear = NET_REAR_WARNING_CAUTION;        return true;
    case 8u:  s_rear = NET_REAR_WARNING_EMERGENCY;      return true;
    case 9u:  s_antipinch = NET_FLAG_YES;               return true;
    case 10u: s_antipinch = NET_FLAG_NO;                return true;
    case 11u: s_occupant = NET_FLAG_YES;                return true;
    case 12u: s_rear = NET_REAR_WARNING_CLEAR;          return true;
    case 13u: s_occupant = NET_FLAG_NO;                 return true;
    case 14u: s_rearQuality = NET_QUALITY_INVALID;      return true;
    case 15u:
        s_rearQuality = NET_QUALITY_OK;
        s_rear = NET_REAR_WARNING_EMERGENCY;
        return true;
    case 16u: s_rear = NET_REAR_WARNING_CLEAR;          return true;
    case 17u: new_event(NET_VSS_EVENT_WELCOME);         return false;
    default:  return false;                             /* 18: end of input */
    }
}

void VssTestInput_Init(void)
{
    memset(&g_vssTestInput, 0, sizeof g_vssTestInput);
    s_rear = NET_REAR_WARNING_CLEAR;
    s_rearQuality = NET_QUALITY_OK;
    s_occupant = NET_FLAG_NO;
    s_antipinch = NET_FLAG_NO;
    s_eventTx = 0u;
    s_warningTx = 0u;
    s_laneSequence = 0u;
    s_occurrence = 0u;
    s_lastEventRxMs = 0u;
    s_warningSent = false;
    s_lastWarningMs = 0u;
    memset(&s_firstEvent, 0, sizeof s_firstEvent);
    s_firstRxMs = 0u;
}

void VssTestInput_Service(uint32_t nowTick)
{
    bool sendNow = false;

    Sw2Button_Process(nowTick);
    if (Sw2Button_TakePressEvent())
    {
        count_up(&g_vssTestInput.presses);
        if (g_vssTestInput.step < VSS_TEST_INPUT_LAST_STEP)
        {
            g_vssTestInput.step++;
            sendNow = apply_step(g_vssTestInput.step);
        }
    }
    if (g_vssTestInput.step >= VSS_TEST_INPUT_LAST_STEP)
    {
        return;
    }
    if (sendNow || !s_warningSent ||
        ((VssTime_NowMs() - s_lastWarningMs) >= (uint64_t)VSS_CFG_TEST_WARNING_PERIOD_MS))
    {
        send_warning();
    }
}

bool VssTestInput_Read(VssTestInputObservation *out)
{
    if (out == NULL)
    {
        return false;
    }
    *out = g_vssTestInput;
    return true;
}
