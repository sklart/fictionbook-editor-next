[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'PortableHotkeyFixture.ps1')
$entries='<Hotkeys><Hotkey><Name>E:\BeforeMove\Scripts\tools\foo.js</Name><Accel>9;120</Accel></Hotkey><Hotkey><Name>E:\BeforeMove\Scripts\missing.js</Name><Accel>9;121</Accel></Hotkey></Hotkeys>'
foreach($existing in @('<Hotkeys/>','<Hotkeys></Hotkeys>','<Hotkeys><Hotkey><Name>LastScript</Name><Accel>9;192</Accel></Hotkey><Hotkey><Name>script:fixture</Name><Accel>1;0</Accel></Hotkey></Hotkeys>')) {
    [xml]$document="<FBE><HkGroups><HkGroup><GroupName>Tools</GroupName><Hotkeys><Hotkey><Name>Words</Name><Accel>13;135</Accel></Hotkey></Hotkeys></HkGroup><HkGroup><GroupName>Scripts</GroupName>$existing</HkGroup></HkGroups></FBE>"
    $otherBefore=$document.SelectSingleNode("//HkGroup[GroupName='Tools']").OuterXml
    Set-PortableLegacyScriptHotkeys -Document $document -EntriesXml $entries
    $scripts=$document.SelectSingleNode("//HkGroup[GroupName='Scripts']")
    if($scripts.Hotkeys.OuterXml -ne ([xml]$entries).DocumentElement.OuterXml -or
        $document.SelectSingleNode("//HkGroup[GroupName='Tools']").OuterXml -cne $otherBefore) {throw 'Legacy fixture changed other groups or lost exact migration entries.'}
}
foreach($invalid in @('<FBE/>','<FBE><HkGroup><GroupName>Scripts</GroupName></HkGroup></FBE>','<FBE><HkGroup><GroupName>Scripts</GroupName><Hotkeys/></HkGroup><HkGroup><GroupName>Scripts</GroupName><Hotkeys/></HkGroup></FBE>')) {
    $rejected=$false
    try {Set-PortableLegacyScriptHotkeys -Document ([xml]$invalid) -EntriesXml $entries}catch{$rejected=$true}
    if(!$rejected){throw 'Invalid hotkey fixture schema was accepted.'}
}
Write-Host 'Portable legacy hotkey fixture: PASS (empty/populated groups, exact entries, other groups intact, invalid schema rejected).'
