<# Exercises complete, HTML-only and self-contained ExportHTML XSL modes. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$xslPath = Join-Path $repoRoot 'runtime\html.xsl'
if ((Get-FileHash -LiteralPath $xslPath -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repoRoot 'src\export-html\html.xsl') -Algorithm SHA256).Hash) { throw 'ExportHTML XSL source and runtime differ.' }
$xsl = New-Object -ComObject Msxml2.DOMDocument.6.0; $xsl.async = $false
if (-not $xsl.load($xslPath)) { throw $xsl.parseError.reason }
Add-Type -AssemblyName System.Drawing
$stream = New-Object IO.MemoryStream; $bitmap = New-Object Drawing.Bitmap 1,1
try { $bitmap.SetPixel(0,0,[Drawing.Color]::Red); $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Jpeg); $jpeg=[Convert]::ToBase64String($stream.ToArray()) } finally { $bitmap.Dispose(); $stream.Dispose() }
$source = New-Object -ComObject Msxml2.DOMDocument.6.0; $source.async = $false
$xml = @"
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Modes</book-title><lang>en</lang><coverpage><image l:href="#cover.png"/></coverpage></title-info></description><body><section><p>Text</p><image l:href="#body.png"/><image l:href="#body.jpg"/></section></body><binary id="cover.png" content-type="image/png">iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAF/gL+MZ7M0QAAAABJRU5ErkJggg==</binary><binary id="body.png" content-type="image/png">iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAF/gL+MZ7M0QAAAABJRU5ErkJggg==</binary><binary id="body.jpg" content-type="image/jpeg">$jpeg</binary></FictionBook>
"@
if (-not $source.loadXML($xml)) { throw $source.parseError.reason }
function Transform([bool]$save,[bool]$embed) { $template=New-Object -ComObject Msxml2.XSLTemplate.6.0;$template.stylesheet=$xsl;$p=$template.createProcessor();$p.input=$source;$p.addParameter('saveimages',$save,'');$p.addParameter('embedimages',$embed,'');[void]$p.transform();return [string]$p.output }
$complete=Transform $true $false
if ($complete -notmatch '<img class="cover"' -or $complete -notmatch 'src="cover\.png"' -or $complete -notmatch 'src="body\.png"' -or $complete -match 'data:image/') { throw 'Complete HTML image mode regressed.' }
$htmlOnly=Transform $false $false
if ([regex]::Matches($htmlOnly,'<img\b').Count -ne 0 -or $htmlOnly -match 'class="cover"') { throw 'HTML-only mode must not emit cover or body images.' }
$standalone=Transform $true $true
if ($standalone -notmatch 'data:image/png;base64,' -or $standalone -notmatch 'data:image/jpeg;base64,' -or $standalone -match 'src="(?:cover|body)\.(?:png|jpg)"') { throw 'Self-contained HTML image mode regressed.' }
Write-Host 'ExportHTML mode regression passed.'
