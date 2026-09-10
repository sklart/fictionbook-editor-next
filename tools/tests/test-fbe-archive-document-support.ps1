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
Require $reader 'segmentStart[\s\S]{0,300}L"\.\."' 'Archive entry identities must reject parent-directory segments.'
Require $reader 'entries\.empty\(\).*ErrorCode::NoFictionBookEntries.*return false' 'An archive without FictionBook entries must fail resolution before any entry access.'
Require $frame 'ResolveArchiveOpenRequest[\s\S]{0,1400}DiscardChanges' 'Archive resolve must precede document discard.'
Require $frame 'DetectDocumentContainerKind\(buf\)' 'File drag-and-drop must use archive resolver.'
Require $frame 'startupArchive' 'Command-line opening must use archive resolver.'
Require $writer 'archive_write_set_format_zip' 'ZIP writer is missing.'
Require $writer 'ReplaceFileW' 'ZIP replacement must use ReplaceFileW.'
Require $writer 'archive_entry_clone' 'ZIP writer must preserve unchanged entry metadata.'
if ($writer -match 'archive_write_data_block') { throw 'ZIP writer must use sequential archive_read_data/archive_write_data, not data blocks.' }
Require $writer 'archive_read_data\(reader, buffer' 'ZIP writer must copy unchanged payload through archive_read_data.'
Require $writer 'archive_write_data\(writer, buffer \+ offset' 'ZIP writer must handle partial payload writes.'
Require $writer 'archive_read_close\(reader.value\)' 'ZIP writer must close its input before ReplaceFile.'
Require $writer 'archive_write_finish_entry' 'ZIP writer must finish every copied entry.'
Require $writer 'target\.occurrence' 'ZIP writer must identify replacement by occurrence.'
Require $doc 'SerializeToMemory' 'Archive saving must serialize to memory.'
Require $frame 'RewriteZipEntry' 'Ctrl+S must call the transactional ZIP writer.'
Require $frame 'DocumentContainerKind::Rar\)\s*return SaveFile\(true\)' 'RAR Ctrl+S must route to Save As.'
Require $frame 'ShowArchiveError' 'Archive failures must be mapped to user-facing error categories.'
Require $frame 'RememberArchiveMruRecord' 'MRU must retain the selected archive entry separately from the storage path.'
Require $frame 'WriteArchiveRecoveryLocation' 'Recovery must retain archive source metadata without rewriting the container.'
Require $frame 'containerLastWriteTime.*containerFileSize' 'Recovery must persist the archive fingerprint.'
Require $frame 'SetDocumentFileType\(recoveredArchiveLocation.documentType\)' 'Recovery must restore the authoritative archive FBD type.'
Require $frame 'm_file_size != FileSize' 'Archive save must reject external changes detected by size as well as timestamp.'
Require $frame 'entryName \+ L" :: " \+ containerName' 'Archive window titles must identify the selected entry and its container.'
Require $frame 'OnFileNew[\s\S]{0,700}m_document_location = DocumentLocation\(\)' 'New documents must not retain an archive save target.'
Require $frame 'ReloadFile\(\)[\s\S]{0,220}m_document_location\.IsArchive\(\)[\s\S]{0,180}LoadFile\(m_document_location\.storagePath, &m_document_location\)' 'Archive reload must resolve the already selected entry rather than parse the container as XML.'
Require $frame 'FBE_NEXT_TEST_ARCHIVE_ENTRY' 'Multi-entry archive runtime tests need an isolated entry-selection hook.'
Require $frame 'IsFbeTestScenario\(L"archive-runtime"\)' 'Archive runtime test scenario must run through real FBE document loading and saving.'
Require $frame 'm_document_location\.IsArchive\(\)[\s\S]{0,180}GetDocumentFileType' 'Archive runtime scenario must report archive origin and document type from the loaded document.'

Write-Host 'Archive document support contract passed.'
