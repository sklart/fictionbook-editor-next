[CmdletBinding()]
param()
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root "src\fbe\mainfrm.cpp")
$themes = Get-ChildItem -LiteralPath (Join-Path $root "runtime\Themes") -Filter *.fbetheme -File
if($source -notmatch 'SCI_SETCARETLINEBACK[\s\S]*XML_SRC_STYLE_CURRENT_LINE_BACKGROUND') { throw 'Current-line theme color is not sent to Scintilla.' }
if($source -notmatch 'if\(highContrast\)[\s\S]*SCI_SETCARETLINEVISIBLE, FALSE') { throw 'High contrast must disable themed current-line background.' }
if($source -notmatch 'UpdateSourceLineNumberMargin\(true\)') { throw 'Theme/configuration paths do not refresh editor decoration.' }
$colors = @{}
foreach($file in $themes) { $theme = Get-Content -Raw $file.FullName | ConvertFrom-Json; $colors[$theme.id] = $theme.colors.'editor.currentLine.background' }
foreach($id in @('everforest-light-medium','dracula')) { if([string]::IsNullOrWhiteSpace($colors[$id])) { throw "Theme $id has no currentLine color." } }
if($colors['everforest-light-medium'] -eq $colors['dracula']) { throw 'Light and dark built-in themes must visibly distinguish currentLine.background.' }
$settings = Get-Content -Raw -LiteralPath (Join-Path $root "src\fbe\Settings.cpp")
$themeSource = Get-Content -Raw -LiteralPath (Join-Path $root "src\fbe\XmlSourceThemes.cpp")
$groupStart = $settings.IndexOf('XmlSrcColorGroup CSettings::GetXmlSrcColorGroup', [StringComparison]::Ordinal)
$styleStart = $settings.IndexOf('DWORD CSettings::GetXmlSrcStyleColor', [StringComparison]::Ordinal)
$nextStart = $settings.IndexOf('bool CSettings::XmlSrcTagHL', [StringComparison]::Ordinal)
if($groupStart -lt 0 -or $styleStart -le $groupStart -or $nextStart -le $styleStart) { throw 'Cannot inspect XML source color-group resolver.' }
$groupResolver = $settings.Substring($groupStart, $styleStart - $groupStart)
if($groupResolver -match 'case XML_SRC_STYLE_CURRENT_LINE_BACKGROUND') { throw 'Current-line background must not be mapped to an editable XML color group.' }
$styleResolver = $settings.Substring($styleStart, $nextStart - $styleStart)
if($styleResolver -notmatch 'GetThemeColor\(GetXmlSrcThemeId\(\), token, color\)' -or $styleResolver -notmatch 'GetXmlSrcThemeColor\(m_xml_src_color_palette, token\)') { throw 'Style resolver must resolve each token from the active theme rather than XML text.' }
if($themeSource -notmatch 'color = kBuiltInThemeColors\[GetBuiltInThemeIndex\(normalized\)\]\[token\]') { throw 'Built-in FBE Light/FBE Dark colors are not resolved by token.' }
$custom = [pscustomobject]@{ format='FictionBookEditorNext.CodeTheme'; formatVersion=1; id='current-line-test'; name='Current line test'; isDark=$true; colors=[pscustomobject]@{'editor.currentLine.background'='#123456'} }
$roundTrip = $custom | ConvertTo-Json -Depth 4 | ConvertFrom-Json
if($roundTrip.colors.'editor.currentLine.background' -ne '#123456') { throw 'A user theme must preserve its currentLine.background color.' }

Write-Host 'Current-line Scintilla contract passed.'
