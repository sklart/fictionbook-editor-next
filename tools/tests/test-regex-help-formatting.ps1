[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$project = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.vcxproj')
$resource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.rc')
$launcher = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpLauncher.cpp')
$generator = Get-Content -Raw -LiteralPath (Join-Path $root 'tools\build\generate-regex-help-html.ps1')
foreach($name in @('RegexHelpDialog.cpp','RegexHelpMarkdown.cpp','RegexHelpDialog.h','RegexHelpMarkdown.h')) {
    if($project.Contains($name)) { throw "Obsolete RichEdit help source is still built: $name" }
}
if($resource.Contains('IDD_REGEX_HELP')) { throw 'Native Full Help dialog remains in resources.' }
foreach($token in @('ShellExecuteExW','GetPreferredRuntimeLocaleName','regex-design.html','regex-source.html','L"en-US"','fbe.regex_help.open_failed')) {
    if(-not $launcher.Contains($token)) { throw "HTML Help launcher misses $token" }
}
foreach($token in @('function Encode','&amp;','&lt;','&gt;','&quot;','&#39;','<table>','<thead>','<tbody>','class=`"regex`"','class=`"example`"','class=`"note`"','class=`"warning`"')) {
    if(-not $generator.Contains($token)) { throw "HTML Help generator misses $token" }
}
Write-Host 'Regex Help HTML migration contract passed.'
