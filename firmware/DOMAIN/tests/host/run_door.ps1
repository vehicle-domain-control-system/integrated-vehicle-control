param(
    [Parameter(Mandatory=$true)][string]$Esp32Project,
    [string]$Cc = 'gcc',
    [string[]]$Cases = @()
)
$ErrorActionPreference = 'Stop'
$prod = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$stage = $PSScriptRoot
$fixture = $PSScriptRoot
$sources = @("$stage\test_door_result_retry.c",
    "$prod\src\interface\domain_interface_core.c",
    "$prod\network\board\s32k344_domain\net_domain_board.c",
    "$prod\network\src\platform\net_boot_id.c")
foreach ($file in (Get-ChildItem "$prod\src\application" -Filter *.c |
    Where-Object { $_.Name -ne 'domain_application_entry.c' })) {
    $candidate = Join-Path $stage $file.FullName.Substring($prod.Length + 1)
    if (Test-Path $candidate) { $sources += $candidate }
    else { $sources += $file.FullName }
}
foreach ($file in (Get-ChildItem "$prod\network\src\l0*" -Recurse -Filter *.c)) {
    $candidate = Join-Path $stage $file.FullName.Substring($prod.Length + 1)
    if (Test-Path $candidate) { $sources += $candidate }
    else { $sources += $file.FullName }
}
$esp = (Resolve-Path -LiteralPath $Esp32Project).Path
$sources += @("$esp\components\uart\uart_m_codec.c",
    "$esp\components\uart\uart_m_frame.c", "$esp\components\uart\uart_m_crc16.c")
$includes = @("-I$stage\src\application", "-I$stage\src\interface", "-I$prod\src\application",
    "-I$prod\src\interface", "-I$prod\network\include", "-I$fixture",
    "-I$esp\components\uart\include", "-I$prod\src\board",
    "-I$stage\network\src\l02_typed_handoff\m03_typed_handoff",
    "-I$prod\network\board\s32k344_domain")
$headerDirs = Get-ChildItem "$prod\network\src\l0*" -Recurse -Filter *.h |
    ForEach-Object { $_.DirectoryName } | Sort-Object -Unique
foreach ($dir in $headerDirs) { $includes += "-I$dir" }
& $Cc -std=c99 -Wall -Wextra -Wpedantic -Werror `
    -DNETWORK_UPPER_NODE_PROFILE=5 -DNET_BOARD_DOMAIN_C2_HOOK=NetDomainBoard_C2Config `
    @includes @sources -o "$stage\test_door_result_retry.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "$stage\test_door_result_retry.exe"
exit $LASTEXITCODE
