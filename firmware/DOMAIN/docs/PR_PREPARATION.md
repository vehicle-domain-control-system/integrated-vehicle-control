# Team develop integration review

Intended base: `vehicle-domain-control-system/integrated-vehicle-control:develop`
at fetched `abf12cd29229f98abe6a987dd73e568072bd35b3`.
Prepared branch: `feature/domain-monitoring-team-integration`, created directly
from that base without personal repository history.

All changes are under `firmware/DOMAIN/`: Domain product modules, the validated
DOMAIN-only Network bundle, recovered tests and integration evidence. No canonical
`firmware/network`, ESP32, Android, CIS or VSS firmware is overwritten. The bundle
retains the 0cc4ae0 common baseline plus 14 documented local integration files;
six shared-origin file deltas require Network review. A canonical patch is included
as review material, not applied to shared sources or claimed to be a complete
upstream CMake/source-list integration.

The NXP-template main, generated configuration, Startup/Linker and proprietary SDK
are excluded from this team PR scope. The full standalone preservation branch is
`kyu-choi/S32K344-Domain-controller:feature/domain-monitoring-integration`.

Validation: exact copied product hashes, existing normal Monitoring Host case PASS,
native Runtime target compile/link PASS after restoring VSS target dependencies.
Recorded Production ARM ELF is CC66C2D7EFE0F7ABF1B94ACC0EA42C5DAEA722048AF6B859B353B9D71CA23888.
No new team-layout ARM build or board test is claimed. No feature behavior changed
during Git packaging. Existing generated/upstream formatting is preserved.

Supported CIS/VSS software is SW_INTEGRATED; hardware remains BOARD_NOT_TESTED.
Monitoring/control are separate. Hardware Gate, Request Coordinator and Auto
Unlock remain disabled in the validated standalone bootstrap. BCM execution/result
evidence, Door/Status SOURCE_AGE TTLs and missing Providers remain open.

PR has not been created. The existing personal Domain repository is not a fork of
the team repository, and no personal team fork was found. GitHub needs an eligible
fork or an authorized team feature branch before this head can target team develop.
Do not push/merge develop, create a fork, or choose another publication route as a
hidden workaround. Report the final file list/base before PR creation.
