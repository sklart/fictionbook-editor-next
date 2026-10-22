[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'tools/build/verify-release.ps1'),[ref]$tokens,[ref]$errors)
if($errors){throw 'Release gate parse failure'}
$calls=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.CommandAst] -and $node.Extent.Text -match '^&\s*\(Join-Path[^\r\n]*test-issue64-runtime\.ps1'},$true))
if($calls.Count -ne 1){throw "Expected exactly one mandatory issue64 step, got $($calls.Count)"}
for($parent=$calls[0].Parent;$parent;$parent=$parent.Parent){if($parent -is [Management.Automation.Language.IfStatementAst] -or $parent -is [Management.Automation.Language.LoopStatementAst]){throw 'Issue64 runtime must be unconditional in the release gate'}}
if($calls[0].Extent.Text -notmatch '-Mode portable'){throw 'Mandatory issue64 test must use an isolated portable profile'}
$menuCalls=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.CommandAst] -and $node.Extent.Text -match '^&\s*\(Join-Path[^\r\n]*test-fbe-ui-defects-runtime\.ps1'},$true))
if($menuCalls.Count -ne 1 -or $menuCalls[0].Extent.Text -notmatch '-MenuCycles 1'){throw 'Expected one bounded mandatory real-menu regression'}
for($parent=$menuCalls[0].Parent;$parent;$parent=$parent.Parent){if($parent -is [Management.Automation.Language.IfStatementAst] -or $parent -is [Management.Automation.Language.LoopStatementAst]){throw 'Real-menu runtime must be unconditional in the release gate'}}
Write-Host 'Issue64 mandatory release-step contract PASS'
