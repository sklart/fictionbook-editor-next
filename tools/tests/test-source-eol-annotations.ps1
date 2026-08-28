[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$mainFrameHeader = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$mainFrame = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$documentHeader = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\FBDoc.h')
$document = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\FBDoc.cpp')

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) { throw "Missing $Description." }
}

Assert-Contains $documentHeader 'SetXMLAndValidate\([^\)]*CString\*\s+errorMessage' 'validator error-message output'
Assert-Contains $document '\*errorMessage\s*=\s*eh->m_msg' 'validator diagnostic propagation'
Assert-Contains $mainFrame 'ClearSourceValidationAnnotations\(\);\s*if \(IsSourceActive\(\)\)' 'annotation clear before validation'
Assert-Contains $mainFrame 'if \(fv\) \{\s*ClearSourceValidationAnnotations\(\);\s*(?:SetValidationStatus\(VALIDATION_VALID\);\s*)?return 0;' 'annotation clear after successful validation'
Assert-Contains $mainFrame 'ShowSourceValidationAnnotation\(line, col, validationError\)' 'annotation at validation failure'
Assert-Contains $mainFrame 'SCI_EOLANNOTATIONSETTEXT' 'EOL annotation text'
Assert-Contains $mainFrame 'SCI_EOLANNOTATIONSETSTYLE' 'EOL annotation style'
Assert-Contains $mainFrame 'SCI_EOLANNOTATIONSETVISIBLE' 'EOL annotation visibility'
Assert-Contains $mainFrame 'SCI_EOLANNOTATIONCLEARALL' 'EOL annotation clearing'
Assert-Contains $mainFrameHeader 'SC_UPDATE_TEXT\)\s*(?:\r?\n\s*\{)?\s*ClearSourceValidationAnnotations\(\)' 'annotation clear after source edit'

Write-Host 'Source EOL validation annotation contract passed.'
