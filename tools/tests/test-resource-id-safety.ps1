$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$resource = Get-Content -LiteralPath (Join-Path $repoRoot 'src\fbe\resource.h')
$values = @{}
foreach($line in $resource) {
    if($line -match '^#define\s+((?:ID_|IDC_)[A-Z0-9_]+)\s+(\d+)$') {
        $name = $Matches[1]; $value = [int]$Matches[2]
        if($name -like 'ID_*' -and $values.ContainsKey($value)) { throw "Duplicate command ID ${value}: $($values[$value]), $name" }
        if($name -like 'ID_*') { $values[$value] = $name }
    }
}
function Get-MacroValue([string]$name) { foreach($line in $resource) { if($line -match ('^#define\s+' + [regex]::Escape($name) + '\s+(\d+)$')) { return [int]$Matches[1] } }; throw "Missing $name" }
$maxCommand = ($values.GetEnumerator() | Where-Object { $_.Value -like 'ID_*' -and $_.Value -notlike 'IDC_*' } | Measure-Object -Property Key -Maximum).Maximum
$controlValues = foreach($line in $resource) { if($line -match '^#define\s+IDC_[A-Z0-9_]+\s+(\d+)$') { [int]$Matches[1] } }; $maxControl = ($controlValues | Measure-Object -Maximum).Maximum
if((Get-MacroValue '_APS_NEXT_COMMAND_VALUE') -le $maxCommand) { throw '_APS_NEXT_COMMAND_VALUE collides with an existing command.' }
if((Get-MacroValue '_APS_NEXT_CONTROL_VALUE') -le $maxControl) { throw '_APS_NEXT_CONTROL_VALUE collides with an existing control.' }
Write-Host 'Resource ID safety contract passed.'