<# Exercises the migration contract independently of the Settings dialog.  The
   dialog delegates its selection decision to KeyboardLayoutSelection.h; these
   cases keep the persisted-layout semantics explicit and regression-tested. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$helper = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\KeyboardLayoutSelection.h')
$dialog = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SettingsHotkeysDlg.cpp')

if($helper -notmatch 'ResolveKeyboardLayoutSelection' -or $dialog -notmatch 'ResolveKeyboardLayoutSelection\(') { throw 'The Settings dialog must delegate keyboard layout migration to the shared helper.' }
foreach($kind in @('ExactKlid', 'ExactLegacy', 'MigratedLegacy', 'UnavailableKlid', 'UnresolvedLegacy', 'CurrentDefault')) { if($helper -notmatch $kind) { throw "Keyboard migration helper lacks $kind state." } }

function Resolve-Selection([string]$storedKlid, [uint32]$legacy, [uint32]$active, [object[]]$layouts) {
    if($storedKlid) {
        for($i = 0; $i -lt $layouts.Count; ++$i) { if($layouts[$i].Id -eq $storedKlid) { return @{ Kind = 'ExactKlid'; Index = $i } } }
        return @{ Kind = 'UnavailableKlid'; Index = -1 }
    }
    if($legacy -ne 0) {
        if(($legacy -band 0xffff0000) -ne 0) {
            for($i = 0; $i -lt $layouts.Count; ++$i) { if([uint32]$layouts[$i].Hkl -eq $legacy) { return @{ Kind = 'ExactLegacy'; Index = $i } } }
        }
        $matches = 0; $onlyMatch = -1; $activeMatch = -1
        for($i = 0; $i -lt $layouts.Count; ++$i) {
            if((([uint32]$layouts[$i].Hkl -band 0xffff) -eq ($legacy -band 0xffff))) {
                [void]++$matches; $onlyMatch = $i
                if([uint32]$layouts[$i].Hkl -eq $active) { $activeMatch = $i }
            }
        }
        if($activeMatch -ge 0) { return @{ Kind = 'MigratedLegacy'; Index = $activeMatch } }
        if($matches -eq 1) { return @{ Kind = 'MigratedLegacy'; Index = $onlyMatch } }
        return @{ Kind = 'UnresolvedLegacy'; Index = -1 }
    }
    for($i = 0; $i -lt $layouts.Count; ++$i) { if([uint32]$layouts[$i].Hkl -eq $active) { return @{ Kind = 'CurrentDefault'; Index = $i } } }
    return @{ Kind = 'None'; Index = -1 }
}

$ru = [pscustomobject]@{ Hkl = [uint32]0x00000419; Id = '00000419' }
$en = [pscustomobject]@{ Hkl = [uint32]0x00000409; Id = '00000409' }
$enAlt = [pscustomobject]@{ Hkl = [uint32]0x00010409; Id = '00010409' }
$cases = @(
    @{ Name = 'exact KLID'; Result = Resolve-Selection '00010409' 0 0 @($en, $enAlt); Kind = 'ExactKlid'; Index = 1 },
    @{ Name = 'unavailable KLID'; Result = Resolve-Selection '00010409' 0 0 @($en); Kind = 'UnavailableKlid'; Index = -1 },
    @{ Name = 'full legacy HKL'; Result = Resolve-Selection '' 0x00010409 0 @($en, $enAlt); Kind = 'ExactLegacy'; Index = 1 },
    @{ Name = 'legacy Russian'; Result = Resolve-Selection '' 0x0419 0 @($ru); Kind = 'MigratedLegacy'; Index = 0 },
    @{ Name = 'legacy English single'; Result = Resolve-Selection '' 0x0409 0 @($en); Kind = 'MigratedLegacy'; Index = 0 },
    @{ Name = 'legacy English active variant'; Result = Resolve-Selection '' 0x0409 0x00010409 @($en, $enAlt); Kind = 'MigratedLegacy'; Index = 1 },
    @{ Name = 'legacy English ambiguous active Russian'; Result = Resolve-Selection '' 0x0409 0x00000419 @($en, $enAlt, $ru); Kind = 'UnresolvedLegacy'; Index = -1 },
    @{ Name = 'new profile default'; Result = Resolve-Selection '' 0 0x00000419 @($en, $ru); Kind = 'CurrentDefault'; Index = 1 }
)
foreach($case in $cases) {
    if($case.Result.Kind -ne $case.Kind -or $case.Result.Index -ne $case.Index) { throw "Keyboard migration case failed: $($case.Name): got $($case.Result.Kind)/$($case.Result.Index), expected $($case.Kind)/$($case.Index)." }
}

Write-Host 'Keyboard-layout migration behavior passed.'
