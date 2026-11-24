function Get-Rc3NoopStatus {
    param([object[]]$Results, [string[]]$ExpectedCases, [bool]$AcceptKnownNoop)
    if (@($Results).Count -ne @($ExpectedCases).Count -or @($ExpectedCases).Count -eq 0) { return 'INCOMPLETE' }
    $seen = @{}
    foreach ($item in $Results) {
        if (!$item -or $item.case -notin $ExpectedCases -or $seen.ContainsKey([string]$item.case)) { return 'INCOMPLETE' }
        if ($item.realEditSaveUndoRedoReopen -ceq 'FAIL') { return 'FAIL' }
        if ($item.noOpDirty -isnot [bool] -or $item.realEditSaveUndoRedoReopen -cne 'PASS') { return 'INCOMPLETE' }
        $seen[[string]$item.case] = $true
    }
    if (@($Results | Where-Object { $_.noOpDirty }).Count) {
        if ($AcceptKnownNoop) { return 'KNOWN FAILURE' }
        return 'FAIL'
    }
    return 'PASS'
}
