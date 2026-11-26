#ifndef NETWORK_NET_UPPER_QUERY_H
#define NETWORK_NET_UPPER_QUERY_H

#include "network/net_upper.h"

/* Private Network M02 boundary. This is not DomainIf/C2 ABI. Owner adapters
 * supply their real passive check, selected scope plan and successful typed
 * snapshots. No DomainIf public function is implemented by this library. */
#define NET_UPPER_QUERY_GETTERS 18u
#define NET_UPPER_QUERY_PLAN_CAPACITY 25u
#define NET_UPPER_QUERY_RECENT_CAPACITY 5u
#define NET_UPPER_QUERY_RESPONSE_CAPACITY 32u

typedef enum {
    NET_QUERY_BCM_DOOR, NET_QUERY_BCM_CLIMATE, NET_QUERY_BCM_LIGHT,
    NET_QUERY_BCM_STATUS, NET_QUERY_CIS_ENVIRONMENT, NET_QUERY_CIS_OCCUPANT,
    NET_QUERY_CIS_REAR, NET_QUERY_CIS_STATUS, NET_QUERY_VSS_STATUS,
    NET_QUERY_WINDOW_STATE, NET_QUERY_WINDOW_FAULT, NET_QUERY_USER_SETTINGS,
    NET_QUERY_AVAILABILITY, NET_QUERY_DIGITAL_STATE, NET_QUERY_WARNING,
    NET_QUERY_REQUEST_RESULT, NET_QUERY_RECENT_RESULTS, NET_QUERY_DIGITAL_RESULT
} NetUpperQueryGetter;

typedef struct {
    NetUpperQueryGetter getter;
    uint8_t selector; /* WARNING: original kind1..8; otherwise zero. */
    uint8_t capacity; /* RECENT: actual storage bound1..5; otherwise1. */
} NetUpperQuerySelection;
typedef struct {
    size_t count;
    NetUpperQuerySelection selected[NET_UPPER_QUERY_PLAN_CAPACITY];
} NetUpperQueryPlan;
typedef struct {
    NetUpperUartMessage message;
    NetSourceStamp source;
    NetAgePolicy age_policy;
} NetUpperQueryOutput;

/* Getter return !=NET_OK: outputs AND count are indeterminate and ignored.
 * Success count0 means confirmed absence/empty selection, never failed OUT.
 * Recent retention8 and latest-first selection belong to the external Owner.
 * The product always passes actual allocated capacity, at most5. */
typedef NetStatus (*NetUpperQueryGet)(void *context,
    const NetWireMQueryRaw *request, uint8_t selector, uint64_t now_ms,
    NetUpperQueryOutput *outputs, size_t capacity, size_t *count);
typedef struct {
    void *context;
    NetStatus (*validate_passive)(void *context, uint32_t device, uint64_t session);
    /* Explicit Owner plan is required: Network does not invent IF-O25 access
     * rights, record filters or a default scope list. Empty successful plan is
     * an Owner-confirmed empty selected set. */
    NetStatus (*select_plan)(void *context, const NetWireMQueryRaw *request,
                            NetUpperQueryPlan *out);
    NetUpperQueryGet getters[NET_UPPER_QUERY_GETTERS];
} NetUpperQueryOwner;

typedef struct {
    NetTxKey key;
    uint8_t type;
    bool terminal, succeeded, retired;
    NetStatus local_status;
} NetUpperQueryResponse;
typedef struct {
    bool active, planned, producing_done, cancelled, held, end_submitted;
    bool end_terminal, end_succeeded, end_retired, finished;
    bool partial, specific_not_found;
    NetWireMQueryRaw request;
    uint64_t port_epoch, received_at_ms, context_generation;
    uint16_t link_sequence;
    size_t received_length;
    uint64_t byte_at_ms[NET_WIRE_UART_FRAME_MAX];
    uint32_t domain_boot;
    NetUpperQueryPlan plan;
    size_t plan_index, output_count, output_index;
    NetUpperQueryOutput outputs[NET_UPPER_QUERY_RECENT_CAPACITY];
    NetStatus getter_status[NET_UPPER_QUERY_PLAN_CAPACITY];
    size_t getter_count[NET_UPPER_QUERY_PLAN_CAPACITY];
    NetUpperQueryResponse responses[NET_UPPER_QUERY_RESPONSE_CAPACITY];
    size_t response_count, selected_response_count, final_success_count;
    size_t failed_response_count, failed_getter_count;
    NetTxKey end_key;
    NetWireMQueryEndRaw end;
    NetStatus hold_reason;
} NetUpperQueryRun;
typedef struct {
    NetUpper *service;
    NetUpperQueryOwner owner;
    uint32_t domain_boot;
    NetWireMContext context;
    uint64_t port_epoch, context_received_at_ms, context_generation;
    bool initialized, context_known, in_call;
    NetUpperQueryRun run;
} NetUpperQuery;

NetStatus net_upper_query_init(NetUpperQuery *query, NetUpper *service,
    uint32_t confirmed_local_domain_boot, const NetUpperQueryOwner *owner);
/* Feed the actual decoded M_CONTEXT/QUERY delivery. Do not invent timestamps
 * or rebind a prior Query to a new Session. Both functions only copy/validate;
 * query_step runs getters/submits outside the service poll callback. */
NetStatus net_upper_query_on_context(NetUpperQuery *query,
    const NetUpperUartDelivery *delivery);
NetStatus net_upper_query_begin(NetUpperQuery *query,
    const NetUpperUartDelivery *delivery);
/* Install directly as the product UART Owner-dispatch endpoint for a Query
 * service. It routes decoded Context/Query to M02; it does not fake getters. */
NetStatus net_upper_query_receive(void *context,
    const NetUpperUartDelivery *delivery, const NetUpperDelivery *metadata);
NetStatus net_upper_query_step(NetUpperQuery *query);
NetStatus net_upper_query_invalidate(NetUpperQuery *query);
NetStatus net_upper_query_snapshot(const NetUpperQuery *query,
    NetUpperQueryRun *out);
uint8_t net_upper_query_schema(NetUpperQueryGetter getter);

#endif
