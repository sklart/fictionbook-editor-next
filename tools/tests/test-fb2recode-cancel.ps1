$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$recoder=Join-Path $root 'runtime\Utilities\fb2recode\fb2recode.js'
$runner=Join-Path $PSScriptRoot 'fb2recode-cancel.js'
$dir=Join-Path ([IO.Path]::GetTempPath()) ('fbe-recode-cancel-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $dir | Out-Null
try {
  $files=@(1..3 | ForEach-Object { Join-Path $dir ("$_.fb2") })
  foreach($file in $files) { [IO.File]::WriteAllText($file,'<?xml version="1.0" encoding="UTF-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><body><section><p>cancel test</p></section></body></FictionBook>',(New-Object Text.UTF8Encoding($false))) }
  $before=@($files | ForEach-Object { (Get-FileHash -Algorithm SHA256 -LiteralPath $_).Hash })
  & cscript.exe //nologo $runner $recoder $files[0] $files[1] $files[2]
  if($LASTEXITCODE -ne 0) { throw "FB2Recode cancel runner returned $LASTEXITCODE." }
  if((Get-Content -Raw $files[0]) -notmatch 'windows-1251') { throw 'Первый файл не обработан до отмены.' }
  if((Get-FileHash -Algorithm SHA256 -LiteralPath $files[1]).Hash -ne $before[1] -or (Get-FileHash -Algorithm SHA256 -LiteralPath $files[2]).Hash -ne $before[2]) { throw 'Отмена изменила необработанные файлы.' }
} finally { if(Test-Path $dir){Remove-Item -LiteralPath $dir -Recurse -Force} }
$global:LASTEXITCODE=0
Write-Host 'FB2Recode cooperative cancellation regression passed.'
