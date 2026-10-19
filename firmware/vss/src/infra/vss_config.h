#ifndef VSS_CONFIG_H
#define VSS_CONFIG_H

/* Build switches and tunables. This is the only file that defines them;
 * every default can be overridden with -D on the compiler command line. */

/* 1: SW2 drives the built-in input sequence (no CAN needed).
 * 0: only the Network callbacks feed the input path. */
#ifndef VSS_CFG_TEST_INPUT
#define VSS_CFG_TEST_INPUT 1
#endif

/* 1: net_if/net_stub.c stands in for the Network module.
 * 0: link against the real Network implementation. */
#ifndef VSS_CFG_NET_STUB
#define VSS_CFG_NET_STUB 1
#endif

/* Headphone level applied when the codec is unmuted, in percent.
 * Kept low by default; the allowed range is enforced below. */
#ifndef VSS_CFG_VOLUME_PERCENT
#define VSS_CFG_VOLUME_PERCENT 20
#endif
#define VSS_CFG_VOLUME_PERCENT_MIN 10
#define VSS_CFG_VOLUME_PERCENT_MAX 40

/* Receive-path allowance added to a message's reported age, in ms. */
#ifndef VSS_CFG_RX_GAP_BUDGET_MS
#define VSS_CFG_RX_GAP_BUDGET_MS 5
#endif

/* Minimum interval between two flow evaluations when no input arrived, in ms. */
#ifndef VSS_CFG_FLOW_PERIOD_MS
#define VSS_CFG_FLOW_PERIOD_MS 10
#endif

/* Minimum interval between two status evaluations, in ms. */
#ifndef VSS_CFG_STATUS_PERIOD_MS
#define VSS_CFG_STATUS_PERIOD_MS 250
#endif

/* A terminated record is reclaimed once its original time interval lies this
 * far in the past, in ms. Must exceed the absolute use-limit ceiling (2000). */
#ifndef VSS_CFG_STORE_RECLAIM_MS
#define VSS_CFG_STORE_RECLAIM_MS 2500
#endif

/* Period of the built-in input sequence's warning frames, in ms. */
#ifndef VSS_CFG_TEST_WARNING_PERIOD_MS
#define VSS_CFG_TEST_WARNING_PERIOD_MS 50
#endif

#if (VSS_CFG_TEST_INPUT != 0) && (VSS_CFG_TEST_INPUT != 1)
#error "VSS_CFG_TEST_INPUT must be 0 or 1"
#endif
#if (VSS_CFG_NET_STUB != 0) && (VSS_CFG_NET_STUB != 1)
#error "VSS_CFG_NET_STUB must be 0 or 1"
#endif
#if (VSS_CFG_VOLUME_PERCENT < VSS_CFG_VOLUME_PERCENT_MIN) || \
    (VSS_CFG_VOLUME_PERCENT > VSS_CFG_VOLUME_PERCENT_MAX)
#error "VSS_CFG_VOLUME_PERCENT is outside the allowed range"
#endif
#if (VSS_CFG_RX_GAP_BUDGET_MS < 0) || (VSS_CFG_RX_GAP_BUDGET_MS > 100)
#error "VSS_CFG_RX_GAP_BUDGET_MS must be 0..100"
#endif
#if (VSS_CFG_FLOW_PERIOD_MS < 1) || (VSS_CFG_FLOW_PERIOD_MS > 20)
#error "VSS_CFG_FLOW_PERIOD_MS must be 1..20 (half-buffer deadline is 21.3 ms)"
#endif
#if (VSS_CFG_STATUS_PERIOD_MS < 50)
#error "VSS_CFG_STATUS_PERIOD_MS must be at least 50"
#endif
#if (VSS_CFG_STORE_RECLAIM_MS <= 2000)
#error "VSS_CFG_STORE_RECLAIM_MS must exceed the 2000 ms use-limit ceiling"
#endif
#if (VSS_CFG_TEST_WARNING_PERIOD_MS < 10)
#error "VSS_CFG_TEST_WARNING_PERIOD_MS must be at least 10"
#endif

#endif /* VSS_CONFIG_H */
