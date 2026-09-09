[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Require([string]$text, [string]$pattern, [string]$message) {
    if ($text -notmatch $pattern) { throw $message }
}

$reader = Get-Content -Raw (Join-Path $root 'src\fbe\archive\ArchiveReader.cpp')
$writer = Get-Content -Raw (Join-Path $root 'src\fbe\archive\ZipArchiveWriter.cpp')
$frame = Get-Content -Raw (Join-Path $root 'src\fbe\mainfrm.cpp')
$doc = Get-Content -Raw (Join-Path $root 'src\fbe\FBDoc.cpp')
$location = Get-Content -Raw (Join-Path $root 'src\fbe\DocumentLocation.h')

Require $location 'enum class DocumentContainerKind' 'Container kind must be separate from FictionBookFileType.'
Require $location 'entryOccurrence' 'Archive identity must retain duplicate-entry occurrence.'
Require $reader 'archive_read_support_format_zip' 'ZIP read support is missing.'
Require $reader 'archive_read_support_format_rar5' 'RAR5 read support is missing.'
Require $reader 'kMaximumDocumentBytes' 'Archive entry memory budget is missing.'
Require $reader 'IsSafeEntryPath' 'Archive entry identities must reject unsafe path forms.'
Require $frame 'ResolveArchiveOpenRequest[\s\S]{0,1400}DiscardChanges' 'Archive resolve must precede document discard.'
Require $frame 'DetectDocumentContainerKind\(buf\)' 'File drag-and-drop must use archive resolver.'
Require $frame 'startupArchive' 'Command-line opening must use archive resolver.'
Require $writer 'archive_write_set_format_zip' 'ZIP writer is missing.'
Require $writer 'ReplaceFileW' 'ZIP replacement must use ReplaceFileW.'
Require $writer 'archive_entry_clone' 'ZIP writer must preserve unchanged entry metadata.'
Require $writer 'archive_write_finish_entry' 'ZIP writer must finish every copied entry.'
Require $writer 'target\.occurrence' 'ZIP writer must identify replacement by occurrence.'
Require $doc 'SerializeToMemory' 'Archive saving must serialize to memory.'
Require $frame 'RewriteZipEntry' 'Ctrl+S must call the transactional ZIP writer.'
Require $frame 'DocumentContainerKind::Rar\)\s*return SaveFile\(true\)' 'RAR Ctrl+S must route to Save As.'
Require $frame 'ShowArchiveError' 'Archive failures must be mapped to user-facing error categories.'
Require $frame 'RememberArchiveMruRecord' 'MRU must retain the selected archive entry separately from the storage path.'
Require $frame 'WriteArchiveRecoveryLocation' 'Recovery must retain archive source metadata without rewriting the container.'
Require $frame 'entryName \+ L" :: " \+ containerName' 'Archive window titles must identify the selected entry and its container.'

Write-Host 'Archive document support contract passed.'
