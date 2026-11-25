param(
    [Parameter(Mandatory=$true)][string]$Esp32Project,
    [string]$Cc = 'gcc',
    [string[]]$Cases = @()
)
$ErrorActionPreference = 'Stop'
$prod = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$stage = $PSScriptRoot
$testOutput = Join-Path $stage 'test_mobile_state_commit.exe'
$sources = @((Join-Path $stage 'test_mobile_state_commit.c'))
$files = @(Get-ChildItem "$prod\src\application" -Filter *.c |
    Where-Object { $_.Name -ne 'domain_application_entry.c' })
$files += @(Get-Item "$prod\src\interface\domain_interface_core.c")
$files += @(Get-ChildItem "$prod\network\src\l0*" -Recurse -Filter *.c)
foreach ($file in $files) {
    $candidate = Join-Path $stage $file.FullName.Substring($prod.Length + 1)
    if (Test-Path $candidate) { $sources += $candidate }
    else { $sources += $file.FullName }
}
$esp = (Resolve-Path -LiteralPath $Esp32Project).Path
$sources += @("$esp\components\uart\uart_m_codec.c",
    "$esp\components\uart\uart_m_frame.c", "$esp\components\uart\uart_m_crc16.c",
    "$esp\components\gateway\gateway_mapper.c", "$esp\components\ble\ble_gatt_protocol.c")
$includes = @("-I$stage\src\application", "-I$stage\src\interface", "-I$prod\src\application",
    "-I$prod\src\interface", "-I$prod\src\board", "-I$prod\network\include",
    "-I$stage\network\src\l02_typed_handoff\m03_typed_handoff",
    "-I$esp\components\uart\include", "-I$esp\components\gateway\include",
    "-I$esp\components\ble\include")
$headerDirs = Get-ChildItem "$prod\network\src\l0*" -Recurse -Filter *.h |
    ForEach-Object { $_.DirectoryName } | Sort-Object -Unique
foreach ($dir in $headerDirs) { $includes += "-I$dir" }
& $Cc -std=c99 -Wall -Wextra -Wpedantic -Werror `
    -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
    -DNETWORK_UPPER_NODE_PROFILE=5 -DNET_BOARD_DOMAIN_C2_HOOK=NetDomainBoard_C2Config `
    @includes @sources -o $testOutput
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($Cases.Count -eq 0) { $Cases = @('environment', 'rear', 'occupant', 'occupant_timeout', 'occupant_disabled', 'no_commit', 'queued_revoke') }
foreach ($case in $Cases) {
    & $testOutput $case
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
