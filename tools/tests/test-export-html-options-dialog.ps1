$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw (Join-Path $root 'src\export-html\HtmlExportOptionsDialog.h')
$implementation = Get-Content -Raw (Join-Path $root 'src\export-html\HtmlExportOptionsDialog.cpp')
$resource = Get-Content -Raw (Join-Path $root 'src\export-html\ExportHTML.rc')
$plugin = Get-Content -Raw (Join-Path $root 'src\export-html\ExportHTMLPlugin.cpp')
$project = Get-Content -Raw (Join-Path $root 'src\export-html\ExportHTML.vcxproj')
$filters = Get-Content -Raw (Join-Path $root 'src\export-html\ExportHTML.vcxproj.filters')
$pages = @('HtmlExportGeneralPage', 'HtmlExportAppearancePage', 'HtmlExportImagesPage', 'HtmlExportNotesMetadataPage')
$allSource = $source + $implementation + $resource + $plugin
foreach ($token in @('class CHtmlExportOptionsDialog : public CDialogImpl', 'IDD_HTML_EXPORT_OPTIONS', 'NOTIFY_HANDLER(IDC_OPTIONS_TABS, TCN_SELCHANGE', 'MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)', 'COMMAND_ID_HANDLER(IDOK, OnOk)', 'COMMAND_ID_HANDLER(IDCANCEL, OnCancel)', 'HRESULT Persist(const HtmlExportSettings& settings)')) { if (-not $source.Contains($token)) { throw "HTML options dialog is missing: $token" } }
foreach ($page in $pages) {
    $header = Get-Content -Raw (Join-Path $root "src\export-html\$page.h")
    $pageSource = Get-Content -Raw (Join-Path $root "src\export-html\$page.cpp")
    $allSource += $header + $pageSource
    foreach ($token in @('LoadFromSettings()', 'SaveToSettings(HtmlExportSettings& candidate, CString& error)', 'UpdateEnabledState()')) { if (-not $header.Contains($token)) { throw "V2 settings page $page is missing: $token" } }
    if ($pageSource -match '\b_Settings\.\b|Persist\(|ExportCore|IXSLProcessor|addParameter') { throw "Settings page $page leaks exporter or persistence dependencies." }
}
foreach ($token in @('IDD_HTML_EXPORT_OPTIONS DIALOGEX', 'IDD_HTML_EXPORT_GENERAL_PAGE DIALOGEX', 'IDD_HTML_EXPORT_APPEARANCE_PAGE DIALOGEX', 'IDD_HTML_EXPORT_IMAGES_PAGE DIALOGEX', 'IDD_HTML_EXPORT_NOTES_PAGE DIALOGEX', 'DS_CONTROL | WS_CHILD')) { if ($resource -notmatch [regex]::Escape($token)) { throw "HTML options resource is missing: $token" } }
foreach ($token in @('m_workingSettings=m_settings', 'HtmlExportSettings candidate = m_workingSettings', 'm_workingSettings = candidate', 'SelectPage(0)', 'SelectPage(1)', 'SelectPage(2)', 'SelectPage(3)', 'LayoutPages()', 'Persist(candidate)')) { if ($allSource -notmatch [regex]::Escape($token)) { throw "V2 HTML transactional or lifecycle regression: $token" } }
foreach ($token in @('BuildHtmlModernFileTypes', 'IDS_SAVE_FILE_FILTER', 'IDS_OPEN_CSS_FILTER', 'Outcome::Failed', 'FbeDiagnostic::HResult')) { if ($allSource -notmatch [regex]::Escape($token)) { throw "HTML modern dialog regression: $token" } }
foreach ($token in @('IDS_OPEN_TEMPLATE_FILTER', 'templatePath', 'usingCustomTemplate', 'ExportHtmlPathsEqual', 'ModernFileDialog', 'FD204')) { if ($allSource -notmatch [regex]::Escape($token)) { throw "Custom XSL regression: $token" } }
foreach ($token in @('IDS_OPTIONS_ERROR_TEMPLATE', 'GetFileAttributes', 'FILE_ATTRIBUTE_DIRECTORY', 'includeToc && (!translated || depth < 1 || depth > 10)')) { if ($allSource -notmatch [regex]::Escape($token)) { throw "Template or disabled TOC validation regression: $token" } }
$appearanceSource = Get-Content -Raw (Join-Path $root 'src\export-html\HtmlExportAppearancePage.cpp')
foreach ($token in @('Outcome::Cancelled', 'Outcome::Failed', 'FD205', 'Browse HTML CSS file')) { if ($appearanceSource -notmatch [regex]::Escape($token)) { throw "CSS browse regression: $token" } }
if ($allSource.Contains('OutputDebugStringW')) { throw 'Modern file dialog errors must use persistent logging.' }
foreach ($caption in @('HTML with external images', 'XSL files (*.xsl)', 'CSS files (*.css)', 'All files (*.*)')) { if ($allSource.Contains($caption)) { throw "HTML modern dialog contains a hardcoded filter caption: $caption" } }
if ($plugin -match 'options\.Persist' -or $implementation -notmatch 'OnOk[\s\S]*Persist\(candidate\)[\s\S]*return 0;[\s\S]*m_settings = candidate;[\s\S]*EndDialog\(IDOK\)') { throw 'HTML settings must save inside confirmed options, retain edits after failure, and commit the model only on success.' }
if ($implementation -notmatch 'Open\(HWND owner\)[\s\S]*LoadSettings\(\)[\s\S]*DoModal\(owner\)' -or $implementation -notmatch 'store.ExpectRevision\(&m_revision\)') { throw 'HTML options must capture the revision at actual opening and check it at confirmation.' }
if ($implementation -match 'OnInitDialog[^{]*\{[^}]*HtmlExportSettingsStore::Load') { throw 'HTML options OnInitDialog must not reload persistent settings.' }
if ($plugin -notmatch 'options\.LoadSettings\(\);[\s\S]*ModernFileDialog::Show') { throw 'HTML options must load settings once before opening Save dialog.' }
if ($implementation -notmatch 'OnCancel[\s\S]*EndDialog\(IDCANCEL\)') { throw 'Nested HTML options Cancel must leave the current object unchanged.' }
if ($plugin -notmatch 'request\.okButtonLabel\s*=\s*LoadExportHtmlString\(IDS_SAVE_BUTTON\)\.GetString\(\)') { throw 'ExportHTML Save dialog must request the localized Save caption.' }
$settingsModalIndex = $implementation.IndexOf('options->Open(h)')
$restoreSaveCaptionIndex = $implementation.IndexOf('fileDialog->SetOkButtonLabel(LoadExportHtmlString(IDS_SAVE_BUTTON))')
if ($settingsModalIndex -lt 0 -or $restoreSaveCaptionIndex -lt $settingsModalIndex) { throw 'ExportHTML must restore the Save caption after closing export options.' }
if ($resource -notmatch 'COMBOBOX\s+IDC_CUSTOM_FONT[^\r\n]*CBS_DROPDOWN[^\r\n]*CBS_AUTOHSCROLL') { throw 'Custom font control must be an editable combo box.' }
$customFontContracts = @('EnumFontFamiliesExW', 'EnumFontFamilyProc', 'ContainsFontFamily', 'std::sort', 'PopulateCustomFontCombo', 'combo.ResetContent()', 'combo.AddString(fonts[index])', 'combo.SetWindowText(selected)', 'candidate.customFontFamily = U::GetWindowText(GetDlgItem(IDC_CUSTOM_FONT))')
foreach ($contract in $customFontContracts) { if ($appearanceSource -notmatch [regex]::Escape($contract)) { throw "Custom font picker contract is missing: $contract" } }
if ($appearanceSource -notmatch 'name\[0\]\s*==\s*L''@''\s*\|\|\s*ContainsFontFamily') { throw 'Custom font list must exclude duplicate vertical font families.' }
foreach ($preset in @('IDS_OPTIONS_VALUE_SERIF', 'IDS_OPTIONS_VALUE_SANS', 'IDS_OPTIONS_VALUE_SYSTEM', 'IDS_OPTIONS_VALUE_CUSTOM', 'max(0, min(3, CComboBox(GetDlgItem(IDC_FONT_FAMILY)).GetCurSel()))')) {
    if ($appearanceSource -notmatch [regex]::Escape($preset)) { throw "Font preset regression: $preset" }
}
if (([regex]::Matches($project, '<ClInclude Include="HtmlExportOptionsDialog\.h"').Count) -ne 1) { throw 'ExportHTML.vcxproj must contain one final options-dialog header entry.' }
if (([regex]::Matches($filters, '<ClInclude Include="HtmlExportOptionsDialog\.h"').Count) -ne 1) { throw 'ExportHTML.vcxproj.filters must contain one final options-dialog header entry.' }
$generalPage = Get-Content -Raw (Join-Path $root 'src\export-html\HtmlExportGeneralPage.cpp')
$generalHeader = Get-Content -Raw (Join-Path $root 'src\export-html\HtmlExportGeneralPage.h')
foreach ($token in @('enum class ExportHtmlFormatMode', 'Standalone = 1', 'ExternalImages = 2', 'MHT = 3', 'HtmlOnly = 4', 'ExportHtmlFormatModeFromFilterIndex', 'ExportHtmlFormatSupportsSplit', 'mode == ExportHtmlFormatMode::ExternalImages || mode == ExportHtmlFormatMode::HtmlOnly', 'OnTypeChange)(IFileDialog* dialog)', 'm_templateSupportsSplit', 'IDC_DOCUMENT_STRUCTURE')) {
    if ($allSource + $generalPage -notmatch [regex]::Escape($token)) { throw "Split availability contract is missing: $token" }
}
if ($plugin -notmatch 'request\.filterIndex\s*=\s*static_cast<UINT>\(ExportHtmlFormatMode::Standalone\)') { throw 'Standalone HTML must be the initial Save-dialog filter.' }
if ($plugin -match 'filterIndex\s*==\s*[1-4]|nFilterIndex\s*==\s*[1-4]') { throw 'ExportHTML format selection must not use numeric mode magic values.' }
$structureTooltipContracts = @('AddDisabledStructureTooltip()', 'StructureTooltipTextId()', 'TTF_SUBCLASS', 'info.hwnd = m_hWnd', '::UnionRect', 'TTM_ADDTOOL', 'TTM_UPDATETIPTEXT', 'IDS_TOOLTIP_DOCUMENT_STRUCTURE_FORMAT', 'IDS_TOOLTIP_DOCUMENT_STRUCTURE_CUSTOM_TEMPLATE')
foreach ($contract in $structureTooltipContracts) { if (($generalPage + $generalHeader) -notmatch [regex]::Escape($contract)) { throw "Document structure tooltip contract is missing: $contract" } }
$structureTooltipFunction = $generalPage.Substring($generalPage.IndexOf('UINT HtmlExportGeneralPage::StructureTooltipTextId() const'))
$formatTooltipIndex = $structureTooltipFunction.IndexOf('if (!m_splitSupported)')
$customTooltipIndex = $structureTooltipFunction.IndexOf('if (!m_templateSupportsSplit)')
if ($formatTooltipIndex -lt 0 -or $customTooltipIndex -lt $formatTooltipIndex) { throw 'Disabled format tooltip must take precedence over the custom-template tooltip.' }
$templatePathIndex = $generalPage.IndexOf('candidate.templatePath = U::GetWindowText')
$customTemplateIndex = $generalPage.IndexOf('candidate.usingCustomTemplate = !ExportHtmlPathsEqual')
$structureIndex = $generalPage.IndexOf('candidate.documentStructure = m_splitSupported && m_templateSupportsSplit')
if ($templatePathIndex -lt 0 -or $customTemplateIndex -lt $templatePathIndex -or $structureIndex -lt $customTemplateIndex) { throw 'Split must be calculated after the current template path and bundled-template check.' }
if ($generalPage -notmatch 'const BOOL splitSupported = m_splitSupported && m_templateSupportsSplit') { throw 'Split UI must require both export mode and bundled html.xsl.' }
if (($generalPage + $generalHeader) -notmatch 'COMMAND_HANDLER\(IDC_TEMPLATE, EN_CHANGE, OnTemplateChanged\)' -or
    $generalPage -notmatch 'LRESULT HtmlExportGeneralPage::OnTemplateChanged' -or
    $generalPage -notmatch 'm_templateSupportsSplit = ExportHtmlPathsEqual\(templatePath') { throw 'Manual template changes must immediately update Split availability.' }
Write-Host 'HTML export options dialog contract passed.'

$runtimeLocalization = Get-Content -Raw (Join-Path $root 'src\export-html\RuntimeLocalization.cpp')
$catalog = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'localization\plugin-ui\catalog.json') | ConvertFrom-Json
$requiredTooltipIds = @('IDS_TOOLTIP_INCLUDE_TOC', 'IDS_TOOLTIP_STYLE', 'IDS_TOOLTIP_FONT', 'IDS_TOOLTIP_FONT_SIZE', 'IDS_TOOLTIP_LINE_HEIGHT', 'IDS_TOOLTIP_CONTENT_WIDTH', 'IDS_TOOLTIP_MARGINS', 'IDS_TOOLTIP_TEXT_ALIGNMENT', 'IDS_TOOLTIP_HEADING_ALIGNMENT', 'IDS_TOOLTIP_CSS_CLEAR', 'IDS_TOOLTIP_COVER_MODE', 'IDS_TOOLTIP_IMAGES_FOLDER', 'IDS_TOOLTIP_IMAGES_FOLDER_NAME', 'IDS_TOOLTIP_WARNING_MIB', 'IDS_TOOLTIP_NOTE_PLACEMENT', 'IDS_TOOLTIP_METADATA', 'IDS_TOOLTIP_METADATA_CHILD', 'IDS_TOOLTIP_DOCUMENT_STRUCTURE', 'IDS_TOOLTIP_DOCUMENT_STRUCTURE_FORMAT', 'IDS_TOOLTIP_DOCUMENT_STRUCTURE_CUSTOM_TEMPLATE')
foreach ($id in @('IDS_OPTIONS_CSS_CLEAR') + $requiredTooltipIds) {
    if ($runtimeLocalization -notmatch [regex]::Escape($id)) { throw "Runtime localization binding is missing: $id" }
    $entry = @($catalog.strings.PSObject.Properties | Where-Object { $_.Value.resourceId -eq $id -and $_.Value.component -like 'export-html.*' })
    if ($entry.Count -ne 1) { throw "Catalog entry is missing or duplicated: $id" }
    foreach ($language in $catalog.targetLanguages) { if ([string]::IsNullOrWhiteSpace([string]$entry[0].Value.translations.PSObject.Properties[$language].Value)) { throw "Missing $id translation: $language" } }
}
foreach ($pageSource in @($appearanceSource, (Get-Content -Raw (Join-Path $root 'src\export-html\HtmlExportImagesPage.cpp')), (Get-Content -Raw (Join-Path $root 'src\export-html\HtmlExportNotesMetadataPage.cpp')))) { if ($pageSource -match 'IDS_TOOLTIP_OPTION_VALUE') { throw 'Each HTML export control must use a specific tooltip.' } }
if ($appearanceSource -notmatch 'IDC_CLEAR_CSS, IDS_TOOLTIP_CSS_CLEAR') { throw 'CSS Clear tooltip is not wired.' }
