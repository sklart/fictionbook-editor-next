$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$hta=Join-Path $root 'runtime\Utilities\Save Sections As Separate Documents\SaveSectionsAsSeparateDocuments.hta'
$runner=Join-Path $PSScriptRoot 'save-sections-split-behavior.js'
$dir=Join-Path $root ('out\tests\save-sections-split-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $dir|Out-Null
try {
 $source=Join-Path $dir 'source.fb2'; $status=Join-Path $dir 'status.txt'
 @'
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:x="http://www.w3.org/1999/xlink" xmlns:r="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>A</first-name><last-name>B</last-name></author><book-title>Book</book-title><lang>en</lang></title-info><document-info><author><first-name>A</first-name><last-name>B</last-name></author><program-used>x</program-used><date>2026-01-01</date><id>x</id><version>1.0</version></document-info></description><body><section><title><p>Container</p></title><section><title><p>Split title</p></title><p>text <a x:href="#n1">n</a><a r:href="#c1">c</a></p><image x:href="#pic"/></section></section></body><body name="notes"><section id="n1"><p>note</p></section><section id="unused"><p>unused</p></section></body><body name="comments"><section id="c1"><p>comment</p></section><section id="unusedc"><p>unused</p></section></body><binary id="pic" content-type="image/png">AA==</binary></FictionBook>
'@ | Set-Content -LiteralPath $source -Encoding utf8
 & cscript.exe //nologo $runner $hta $source $dir $status
 if($LASTEXITCODE -ne 0 -or -not (Test-Path $status) -or (Get-Content -Raw $status) -notmatch '^ok') { throw "Save Sections split behavioral regression failed: $(if(Test-Path $status){Get-Content -Raw $status})" }
} finally { Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue }
Write-Host 'Save Sections split behavioral regression passed.'
