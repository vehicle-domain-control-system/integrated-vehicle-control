# Product adapters — BINDING_OPEN

DOMAIN/CIS/BCM product API adapters are intentionally absent. Existing DOMAIN
v0.5 / Vehicle Types v0.2 headers and later candidate headers are evidence, not
approval of Network binding or ABI changes. M03 exposes only a local borrowed
frame spy callback enabled explicitly by the Host fixture. No `DomainIf_*`,
`NetRear_*`, `CisApp_*`, or other product function is declared or defined here.
WINDOW execution/monitoring and ESP32 implementation remain outside this scope.
