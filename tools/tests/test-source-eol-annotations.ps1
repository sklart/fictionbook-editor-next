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

function Assert-Order([string]$Text, [string]$First, [string]$Second, [string]$Description) {
    $firstIndex = $Text.IndexOf($First, [System.StringComparison]::Ordinal)
    $secondIndex = $Text.IndexOf($Second, [System.StringComparison]::Ordinal)
    if ($firstIndex -lt 0 -or $secondIndex -lt 0 -or $firstIndex -ge $secondIndex) { throw "Invalid $Description order." }
}

Assert-Contains $documentHeader 'SetXMLAndValidate\([^\)]*CString\*\s+errorMessage' 'validator error-message output'
Assert-Contains $document '\*errorMessage\s*=\s*eh->m_msg' 'validator diagnostic propagation'
$validateMatch = [regex]::Match($mainFrame, 'LRESULT\s+CMainFrame::OnFileValidate\([^\)]*\)\s*\{(?<body>.*?)^\}', [System.Text.RegularExpressions.RegexOptions]::Singleline -bor [System.Text.RegularExpressions.RegexOptions]::Multiline)
if (-not $validateMatch.Success) { throw 'Missing CMainFrame::OnFileValidate implementation.' }
$validateBody = $validateMatch.Groups['body'].Value
Assert-Contains $validateBody 'ClearSourceValidationAnnotations\(\);\s*if \(IsSourceActive\(\)\)' 'annotation clear before XML/XSD validation'
Assert-Contains $validateBody 'if \(fv\)\s*\{\s*CString binaryWarning;\s*if \(!ValidateFb2BinarySemantics\(m_doc, validationError, binaryWarning\)\)' 'binary inspection after successful XML/XSD validation'
Assert-Contains $validateBody 'if \(!ValidateFb2BinarySemantics\(m_doc, validationError, binaryWarning\)\)\s*\{\s*fv\s*=\s*false;\s*line\s*=\s*1;\s*col\s*=\s*1;' 'binary inspection invalid path'
Assert-Contains $validateBody 'else\s*\{\s*ClearSourceValidationAnnotations\(\);\s*SetValidationStatus\(FBEStatusBar::ValidationStatus::Valid\);[\s\S]*?return 0;' 'final valid path after binary inspection'
Assert-Contains $validateBody 'if \(!fv\)\s*\{\s*SetValidationStatus\(FBEStatusBar::ValidationStatus::Invalid\);[\s\S]*?ShowSourceValidationAnnotation\(line, col, validationError\)' 'annotation at validation or binary inspection failure'
Assert-Order $validateBody 'if (fv)' 'ValidateFb2BinarySemantics' 'XML/XSD then binary inspection'
Assert-Order $validateBody 'ValidateFb2BinarySemantics' 'SetValidationStatus(FBEStatusBar::ValidationStatus::Valid)' 'binary inspection then final valid status'
$binaryFailure = [regex]::Match($validateBody, 'if \(!ValidateFb2BinarySemantics\(m_doc, validationError, binaryWarning\)\)\s*\{(?<failure>.*?)\}\s*else', [System.Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $binaryFailure.Success -or $binaryFailure.Groups['failure'].Value -match 'ClearSourceValidationAnnotations') { throw 'Binary inspection failure must preserve annotations until the invalid path.' }
Assert-Contains $mainFrame 'SCI_EOLANNOTATIONSETTEXT' 'EOL annotation text'
Assert-Contains $mainFrame 'SCI_EOLANNOTATIONSETSTYLE' 'EOL annotation style'
Assert-Contains $mainFrame 'SCI_EOLANNOTATIONSETVISIBLE' 'EOL annotation visibility'
Assert-Contains $mainFrame 'SCI_EOLANNOTATIONCLEARALL' 'EOL annotation clearing'
Assert-Contains $mainFrameHeader 'SC_UPDATE_TEXT\)\s*(?:\r?\n\s*\{)?\s*ClearSourceValidationAnnotations\(\)' 'annotation clear after source edit'

Write-Host 'Source EOL validation annotation contract passed.'
