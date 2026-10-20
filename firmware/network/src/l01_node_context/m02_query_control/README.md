# M02 Query control: executable product implementation

`net_upper_query.c` implements F04/F05 with the private `NetUpperQuery` API.
It is linked into `network_product`; this module is no longer DESIGN_ONLY.

`net_upper_query_receive` / `net_upper_query_begin` validate actual UART request
and context provenance. `net_upper_query_step` invokes the selected Owner getter
slots, preserves original CAN receipts in projected replies, tracks independent
local TX facts, counts only successful final TX, emits Query End and cancels on
context/permission/link changes. Capacity and callback reentry are bounded.

`NetUpperQueryOwner` supplies explicit context validation, plan selection and 18
getter slots producing 17 wire schemas. `net_domain_c2_query_owner` binds actual
pinned C2 signatures where bodies exist. Missing Owner functions or unresolved
plan/commit policy HOLD only the affected request. A returned Owner getter value
is not proof that Network's latest RX was committed to Core; projection requires
matching value/identity and explicit commit correlation.

The `product.query_*` regressions cover every getter slot, result variants,
capacity/overflow, context cancellation, final TX and End. The C2 and virtual ECU
suites exercise the product bindings. Exact run status is in the result package.
Ordinary WINDOW queries remain HOLD; no unsupported actuator/state API is invented.
The local final TX count never means BLE/App receipt or execution.
