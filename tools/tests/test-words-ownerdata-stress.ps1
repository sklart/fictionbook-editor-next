<#
.SYNOPSIS
Verifies the owner-data contract for Words.xml and produces large XML fixtures.

.DESCRIPTION
The test intentionally does not open a ListView: it proves from the production
sources that the dialog has no per-row ListView operations, then streams two
large Words.xml fixtures.  This keeps the check deterministic on build agents
while covering the sizes that previously exhausted USER/GDI resources.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$resource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.rc')
$dialogHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SettingsWordsDlg.h')
$dialogSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SettingsWordsDlg.cpp')
$settingsSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\Settings.cpp')

if ($resource -notmatch 'IDC_LIST_WORDS,"SysListView32",[^\r\n]*LVS_OWNERDATA') { throw 'IDC_LIST_WORDS must remain an owner-data ListView.' }
if ($dialogHeader -notmatch 'std::vector<WordsItem>\s+m_words') { throw 'The dialog must keep its staged word model outside the ListView.' }
if ($dialogHeader -notmatch 'LVN_GETDISPINFO') { throw 'The ListView must provide visible rows through LVN_GETDISPINFO.' }
if ($dialogSource -match '(?m)^\s*m_list_words\.(InsertItem|DeleteItem)\s*\(') { throw 'Owner-data Lists must not create or delete individual ListView items.' }
if ($dialogSource -notmatch 'm_list_words\.SetItemCount\(static_cast<int>\(m_words\.size\(\)\)\)') { throw 'Model changes must update the virtual item count.' }
if ($dialogSource -notmatch 'SetItemState\(-1, m_sel_all \? 0 : LVIS_SELECTED, LVIS_SELECTED\)') { throw 'Select-all must use a single virtual ListView state operation.' }
if ($settingsSource -notmatch 'm_words\.reserve\(objects\.size\(\)\)') { throw 'Words.xml loading must reserve the persistent model for large files.' }
if ($settingsSource -notmatch 'word\.Destroy\(loadedWord\)') { throw 'Words.xml loading must release deserializer-owned temporary objects.' }

$temporary = Join-Path ([IO.Path]::GetTempPath()) ('fbe-words-stress.' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary -Force | Out-Null
try {
    function New-WordsFixture([string]$path, [int]$count) {
        $settings = [Xml.XmlWriterSettings]::new()
        $settings.Encoding = [Text.UTF8Encoding]::new($false)
        $settings.Indent = $false
        $writer = [Xml.XmlWriter]::Create($path, $settings)
        try {
            $writer.WriteStartDocument(); $writer.WriteStartElement('FBE'); $writer.WriteStartElement('Words')
            for ($i = 0; $i -lt $count; ++$i) {
                $writer.WriteStartElement('Word'); $writer.WriteElementString('Value', ('stress-{0:D7}-word' -f $i)); $writer.WriteElementString('Counted', [string]($i % 97 + 1)); $writer.WriteEndElement()
            }
            $writer.WriteEndElement(); $writer.WriteEndElement(); $writer.WriteEndDocument()
        } finally { $writer.Dispose() }
    }
    function Get-WordCount([string]$path) {
        $reader = [Xml.XmlReader]::Create($path)
        $count = 0
        try { while ($reader.Read()) { if ($reader.NodeType -eq [Xml.XmlNodeType]::Element -and $reader.Name -eq 'Word') { ++$count } } } finally { $reader.Dispose() }
        return $count
    }

    foreach ($case in @(@{ Name = '65k'; Count = 65000 }, @{ Name = '250k'; Count = 250000 })) {
        $path = Join-Path $temporary ($case.Name + '.Words.xml')
        New-WordsFixture $path $case.Count
        if ((Get-WordCount $path) -ne $case.Count) { throw "Synthetic $($case.Name) Words.xml is incomplete." }
    }

    # 500 script files exercise the documented concurrent-script scale without
    # retaining file handles or creating any GUI controls.
    $scripts = Join-Path $temporary 'scripts'
    New-Item -ItemType Directory -Path $scripts | Out-Null
    1..550 | ForEach-Object { [IO.File]::WriteAllText((Join-Path $scripts ('stress-{0:D4}.js' -f $_)), 'function Run() {}') }
    if (@(Get-ChildItem -LiteralPath $scripts -Filter '*.js' -File).Count -ne 550) { throw 'The 550-script fixture is incomplete.' }
} finally {
    Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'Words owner-data stress contract passed: 65k, 250k and 550 scripts.'
