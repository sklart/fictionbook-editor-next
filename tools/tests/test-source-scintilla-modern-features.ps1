[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$mainFrame = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$mainFrameHeader = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$matchedTags = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\xmlMatchedTagsHighlighter.h')

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) {
        throw "Missing $Description."
    }
}

Assert-Contains $mainFrame 'SCI_SETCOMMANDEVENTS\s*,\s*FALSE' 'disabled legacy Scintilla command events'
Assert-Contains $mainFrame 'SCI_SETMODEVENTMASK\s*,\s*SC_MOD_CHANGEFOLD' 'fold-only modification event mask'
Assert-Contains $mainFrame 'SCI_SETUNDOSELECTIONHISTORY\s*,\s*AU::_ARGS\.disable_undo_selection_history\s*\?\s*0\s*:\s*\r?\n\s*SC_UNDO_SELECTION_HISTORY_ENABLED\s*\|\s*SC_UNDO_SELECTION_HISTORY_SCROLL' 'configurable undo selection and scroll history'
Assert-Contains $mainFrame 'if\s*\(m_doc->DocRelChanged\(\)\)\s*\{\s*const DWORD nch\s*=\s*::WideCharToMultiByte\(CP_UTF8,0,src,src\.length\(\),\s*NULL,0,NULL,NULL\)' 'UTF-8 size pass only during Source reload'
Assert-Contains $mainFrame 'lexer\.xml\.allow\.asp"\s*,\s*\(LPARAM\)"0"' 'disabled XML ASP lexer mode'
Assert-Contains $mainFrame 'lexer\.xml\.allow\.php"\s*,\s*\(LPARAM\)"0"' 'disabled XML PHP lexer mode'
Assert-Contains $mainFrame 'lexer\.xml\.allow\.scripts"\s*,\s*\(LPARAM\)"0"' 'disabled XML script lexer mode'
Assert-Contains $matchedTags 'class\s+ScintillaDirectCall' 'Scintilla direct-call wrapper'
Assert-Contains $matchedTags 'SCI_GETDIRECTFUNCTION' 'direct function lookup'
Assert-Contains $matchedTags 'SCI_GETDIRECTPOINTER' 'direct pointer lookup'
Assert-Contains $matchedTags 'return\s+m_source->SendMessage\(' 'safe SendMessage fallback'
Assert-Contains $matchedTags '~XmlMatchedTagsHighlighter\(\)\s*\{\s*delete\s+_pEditView;' 'matched-tags wrapper cleanup'
${autocompleteHeader} = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\Fb2SourceAutocomplete.h')
${autocomplete} = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\Fb2SourceAutocomplete.cpp')
${generator} = Get-Content -Raw (Join-Path $repoRoot 'tools\build\generate-fb2-schema-metadata.ps1')
${generatedMetadata} = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\generated\Fb2SchemaMetadata.h')
Assert-Contains $mainFrame 'm_fb2_autocomplete\.Complete' 'schema-derived autocomplete integration'
Assert-Contains $mainFrame 'SCI_AUTOCSHOW' 'Scintilla autocomplete invocation'
Assert-Contains $mainFrame 'character != .<. && character != ./. && character != . . && character != .:. && character != .#.' 'structural-only autocomplete trigger'
Assert-Contains $mainFrameHeader 'ShowFb2Autocomplete\(reinterpret_cast<const SCNotification' 'Source character-added autocomplete routing'
Assert-Contains $autocompleteHeader 'class\s+Fb2SourceAutocomplete' 'separate autocomplete component'
Assert-Contains $autocomplete 'OpenElements' 'parent and closing-tag context parser'
Assert-Contains $autocomplete 'CompleteIds' 'lazy ID completion'
Assert-Contains $autocomplete 'IsSuppressed' 'comment and CDATA suppression'
Assert-Contains $autocomplete 'opened != std::string::npos' 'correct suppression of first unclosed XML construct'
Assert-Contains $autocomplete 'closed == std::string::npos' 'correct handling of missing XML construct terminator'
Assert-Contains $generator 'FictionBookLinks\.xsd' 'XLink schema generator input'
Assert-Contains $generatedMetadata 'xlink:href' 'generated XLink attribute metadata'

Write-Host 'Source modern Scintilla feature contract passed.'
