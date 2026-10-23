[CmdletBinding()]
param([Parameter(Mandatory)]$Receipt, [Parameter(Mandatory)]$Run,
    [Parameter(Mandatory)][string]$Head, [Parameter(Mandatory)][string]$CommandSha256)
$ErrorActionPreference = 'Stop'
if ($Receipt.schemaVersion -ne 1 -or $Receipt.result -ne 'PASS' -or $Receipt.suite -ne 'clipboard-ole' -or
    $Receipt.head -ne $Head -or $Receipt.commandSha256 -ne $CommandSha256 -or
    $Receipt.repository -ne 'sklart/fictionbook-editor-next' -or $Receipt.runId -notmatch '^\d+$' -or
    $Receipt.executableSha256 -notmatch '^[0-9A-Fa-f]{64}$') { throw 'External OLE receipt is stale or invalid.' }
if ([string]$Run.id -ne [string]$Receipt.runId -or $Run.head_sha -ne $Head -or
    $Run.status -ne 'completed' -or $Run.conclusion -ne 'success' -or $Run.path -ne '.github/workflows/build.yml') {
    throw 'External OLE receipt does not belong to a successful Build of this SHA.'
}
