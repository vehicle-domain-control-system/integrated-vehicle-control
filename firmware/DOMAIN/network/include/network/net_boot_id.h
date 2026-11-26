#ifndef NETWORK_NET_BOOT_ID_H
#define NETWORK_NET_BOOT_ID_H

/* BOOT_ID of the Network sending run, computed by the board layer once at
 * start-up and handed to Net_Init (NET_BOARD_NODE_CONFIG_HOOK ->
 * NetNodeConfig.boot_id). The application reads it only (Net_GetBootId).
 *
 * Basis: docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md (identifier
 * table: BOOT_ID belongs to the Network run of the sending node, 0 is
 * reserved, an accepted change invalidates that sender's earlier context) and
 * docs/network/interface/03_COMMUNICATION/10_CAN_FD.md (common header: BOOT_ID
 * of the sending Network, 0 reserved; TX_SEQUENCE starts at 1 in every run).
 * A run that kept the BOOT_ID of the previous run would have its restarted
 * TX_SEQUENCE taken as old or duplicate by every receiver.
 *
 * Rules of net_boot_id_next():
 *   - a record is valid when its magic is NET_BOOT_ID_MAGIC, its check is
 *     the CRC-32 of (magic, boot_id) and its boot_id is not 0;
 *   - warm reset (or unknown cause) with a valid record: the previous value
 *     + 1, 0 skipped (0xFFFFFFFF -> 1);
 *   - power-on reset, or no valid record: a value mixed from the seed; 0 is
 *     replaced, and a value equal to the valid previous one gets + 1;
 *   - the record is written with the new value before the function returns.
 *
 * Portable: no static state, no SDK. Placing the record in RAM that a warm
 * reset keeps (noinit), reading the reset cause and choosing the seed source
 * (a free-running timer, an ADC noise read, a unique ID ...) belong to the
 * board; nothing here does that. */
#include "network/net_types.h"

#define NET_BOOT_ID_MAGIC 0x4E424944u /* "NBID" */

typedef struct { uint32_t magic, boot_id, check; } NetBootIdRecord; /* the board keeps it in noinit RAM */
typedef enum { NET_BOOT_RESET_UNKNOWN, NET_BOOT_RESET_POWER_ON, NET_BOOT_RESET_WARM } NetBootResetCause;

/* OK: *out is the BOOT_ID of this run (never 0) and the record holds it.
 * INVALID_ARGUMENT (record and *out untouched): NULL pointer, out inside the
 * record, or a cause outside NetBootResetCause. */
NetStatus net_boot_id_next(NetBootIdRecord *record, NetBootResetCause cause, uint32_t seed, uint32_t *out);

#endif
