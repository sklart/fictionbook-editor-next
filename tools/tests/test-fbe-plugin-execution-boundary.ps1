<# Guards plugin COM execution ownership separately from plugin UI discovery. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$executionHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\plugins\PluginExecutionController.h')
$executionSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\plugins\PluginExecutionController.cpp')
$uiHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\plugins\PluginUiController.h')
$uiSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\plugins\PluginUiController.cpp')
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')

foreach($required in @('class\s+PluginExecutionController', 'PluginImportResult', 'PluginExportRequest', 'PluginExecutionResult', 'CreateInstance\s*\(', 'NegotiateApi\s*\(', 'IFBEImportPlugin2', 'IFBEExportPlugin2', 'CreateHost\s*\(', 'CreateSnapshot\s*\(', 'TracePluginExecution')) {
    if(($executionHeader + $executionSource) -notmatch $required) { throw "Plugin execution boundary is missing: $required" }
}
foreach($unit in @($executionHeader, $executionSource)) {
    foreach($forbidden in @('mainfrm\.h', 'CMainFrame', 'RecoveryController', 'RecentDocumentsController', 'AppendMenu', 'CreateToolbar', 'DocumentSession')) {
        if($unit -match $forbidden) { throw "PluginExecutionController must not depend on $forbidden." }
    }
}
foreach($required in @('PluginManager', 'ImportPlugins', 'ExportPlugins', 'SetLastCommand', 'InitializeType')) {
    if(($uiHeader + $uiSource) -notmatch $required) { throw "PluginUiController UI/discovery ownership is missing: $required" }
}
foreach($forbidden in @('IFBEImportPlugin2', 'IFBEExportPlugin2', 'CreateHost\s*\(', 'CreateSnapshot\s*\(', 'NegotiateApi\s*\(')) {
    if(($uiHeader + $uiSource) -match $forbidden) { throw "PluginUiController must not own execution protocol: $forbidden" }
}
foreach($name in @('OnToolsImport', 'OnToolsExport')) {
    $handler = [regex]::Match($mainSource, "(?s)LRESULT\s+CMainFrame::$name\(.*?(?=LRESULT\s+CMainFrame::)").Value
    if([string]::IsNullOrEmpty($handler)) { throw "Unable to locate $name." }
    if($handler -notmatch 'm_plugin_execution\.(Import|Export)\s*\(') { throw "$name must delegate to PluginExecutionController." }
    foreach($forbidden in @('CreateInstance\s*\(', 'NegotiateApi\s*\(', 'IFBEImportPlugin2', 'IFBEExportPlugin2', 'CreateHost\s*\(', 'CreateSnapshot\s*\(', 'TracePluginDiagnostic')) {
        if($handler -match $forbidden) { throw "$name retains plugin execution protocol: $forbidden" }
    }
}
Write-Host 'Plugin execution boundary contract passed.'
