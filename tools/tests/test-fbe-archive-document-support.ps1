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
$startup = Get-Content -Raw (Join-Path $root 'src\fbe\FBE.cpp')
$picker = Get-Content -Raw (Join-Path $root 'src\fbe\ArchiveEntryPicker.cpp')
$pickerResources = Get-Content -Raw (Join-Path $root 'src\fbe\FBE.rc')

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
Require $writer 'archive_write_free\(writer.value\)[\s\S]{0,900}ReplaceFileW' 'ZIP writer must release its output handle before replacement.'
Require $writer 'archive_read_free\(reader.value\)[\s\S]{0,700}ReplaceFileW' 'ZIP writer must release its input handle before replacement.'
Require $writer 'archive_write_finish_entry' 'ZIP writer must finish every copied entry.'
Require $writer 'target\.occurrence' 'ZIP writer must identify replacement by occurrence.'
Require $doc 'SerializeToMemory' 'Archive saving must serialize to memory.'
Require $frame 'RewriteZipEntry' 'Ctrl+S must call the transactional ZIP writer.'
Require $frame 'DocumentContainerKind::Rar\)\s*return SaveFile\(true\)' 'RAR Ctrl+S must route to Save As.'
Require $frame 'ShowArchiveError' 'Archive failures must be mapped to user-facing error categories.'
Require $frame 'RememberArchiveMruRecord' 'MRU must retain the selected archive entry separately from the storage path.'
Require $frame 'FBE-ARCHIVE-MRU\\t2' 'Archive MRU persistence must be explicitly versioned.'
Require $frame 'ReadArchiveMruRecords' 'Archive MRU must load independent persisted entry identities.'
Require $frame 'SameArchiveMruIdentity' 'Archive MRU must key records by container, storage path, entry path, and occurrence.'
Require $frame 'entryOccurrence == right\.entryOccurrence' 'Archive MRU must retain duplicate archive entries by occurrence.'
Require $frame 'ArchiveMruDisplayName' 'Archive MRU menu entries must identify the selected internal document.'
Require $frame 'AddArchiveMruRecordsToList' 'Archive MRU entries must be restored into the recent-files menu.'
Require $frame 'ErrorCode::EntryNotFound' 'Missing MRU archive entries must fail without selecting another document.'
Require $frame 'archiveMru \? LoadFile\(archiveLocation\.storagePath, &archiveLocation\)' 'Archive MRU must load the physical container path and exact location, never its caption.'
Require $frame 'ArchiveMruKey' 'Archive MRU identity must be separate from its menu caption.'
Require $frame 'RebuildMruMenu' 'MRU submenu must be rebuilt separately from stored identities.'
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
Require $frame 'IsFbeTestScenario\(L"archive-two-phase-runtime"\)' 'Archive two-phase runtime scenario is missing.'
Require $frame 'LoadFile\(failedArchive\)' 'Two-phase runtime scenario must attempt the real archive open path.'
Require $frame 'mruUnchanged' 'Two-phase runtime scenario must verify that the failed archive did not mutate MRU.'
Require $frame 'IsFbeTestScenario\(L"archive-rar-save-runtime"\)' 'RAR Save As runtime scenario must be explicitly isolated.'
Require $frame 'FBE_NEXT_TEST_SAVE_PATH' 'RAR Save As runtime test must use an explicit isolated output path.'
Require $frame 'archive-recovery-external-verify' 'Archive recovery runtime must verify external-modification blocking.'
Require $frame 'FBE_NEXT_TEST_ARCHIVE_SAVE_ERROR' 'Archive recovery runtime must report the precise Save failure reason.'
Require $frame 'ErrorCode::ModifiedExternally' 'Archive recovery runtime must require the external-modification error code.'
Require $frame 'mruAfter == mruBefore' 'Two-phase runtime scenario must compare the entire MRU snapshot.'
Require $frame 'archive-open-runtime"\)' 'Archive-open runtime mode must be explicitly isolated from modal error UI.'
Require $startup 'CommandLineToArgvW' 'CLI parsing must use CommandLineToArgvW.'
if ($startup -match 'static void ParseCommandLine\(') { throw 'Legacy ParseCommandLine must not remain after CommandLineToArgvW migration.' }
Require $picker 'min\(max\(static_cast<int>\(m_entries\.size\(\)\), 5\), 10\)' 'Archive picker must keep space for five to ten visible rows.'
Require $picker 'nonClientWidth[\s\S]{0,500}desiredClientWidth \+ nonClientWidth' 'Archive picker must convert client width to full window width.'
Require $picker 'MonitorFromWindow[\s\S]{0,300}4 / 5' 'Archive picker width must remain within 80% of the work area.'
Require $picker 'LayoutColumns\(\)' 'Archive picker must recompute columns after every resize.'
Require $picker 'SetColumnWidth\(sizeColumn, sizeWidth\)' 'Archive picker Size column must remain compact.'
Require $picker 'FbeLoadRuntimeStringByKey\(L"fbe\.archive\.picker\.open"' 'Archive picker Open button must use the runtime localization key.'
Require $picker 'OnShowWindow[\s\S]{0,300}ApplyRuntimeTexts' 'Archive picker must restore its action caption after activation.'
Require $pickerResources 'IDD_ARCHIVE_ENTRY[\s\S]{0,300}WS_THICKFRAME' 'Archive picker must remain resizable.'
if ($pickerResources -match 'IDD_ARCHIVE_ENTRY[\s\S]{0,300}WS_MAXIMIZEBOX') { throw 'Archive picker must not expose a maximize button.' }

Write-Host 'Archive document support contract passed.'
