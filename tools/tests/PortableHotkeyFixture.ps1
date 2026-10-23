function Set-PortableLegacyScriptHotkeys {
    param([Parameter(Mandatory)][xml]$Document, [Parameter(Mandatory)][string]$EntriesXml)
    $groups=@($Document.SelectNodes("//HkGroup[GroupName='Scripts']"))
    if($groups.Count -ne 1){throw 'Legacy hotkey fixture requires exactly one Scripts group.'}
    $containers=@($groups[0].SelectNodes('Hotkeys'))
    if($containers.Count -ne 1){throw 'Legacy hotkey fixture requires exactly one Hotkeys container.'}
    [xml]$replacement=$EntriesXml
    if($replacement.DocumentElement.LocalName -ne 'Hotkeys' -or $replacement.DocumentElement.NamespaceURI){throw 'Legacy hotkey fixture replacement must be unqualified Hotkeys.'}
    [void]$groups[0].ReplaceChild($Document.ImportNode($replacement.DocumentElement,$true),$containers[0])
}
