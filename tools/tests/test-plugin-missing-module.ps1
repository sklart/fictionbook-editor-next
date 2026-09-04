<# Synthetic catalog fixture: an optional absent module must be excluded. #>
[CmdletBinding()] param()
$ErrorActionPreference = 'Stop'; $root = New-Item -ItemType Directory -Path (Join-Path ([IO.Path]::GetTempPath()) ('fbe-plugin-test-' + [guid]::NewGuid()))
try {
    $plugins = Join-Path $root 'Plugins'; New-Item -ItemType Directory -Path $plugins | Out-Null; [IO.File]::WriteAllBytes((Join-Path $plugins 'present.dll'), [byte[]](0))
    $manifest = @{ schemaVersion = 1; plugins = @(@{ id='present'; type='Export'; module='present.dll'; clsid='{11111111-1111-1111-1111-111111111111}'; menu='Present'; menuKey='test.present'; activation='local-com' }, @{ id='missing'; type='Export'; module='missing.dll'; clsid='{22222222-2222-2222-2222-222222222222}'; menu='Missing'; menuKey='test.missing'; activation='local-com' }) } | ConvertTo-Json -Depth 4
    Set-Content -LiteralPath (Join-Path $plugins 'plugins.json') -Value $manifest -Encoding utf8
    $catalog = @((Get-Content -LiteralPath (Join-Path $plugins 'plugins.json') -Raw | ConvertFrom-Json).plugins | Where-Object { $_.module -notmatch '\.\.|[\\/:]' -and $_.module.EndsWith('.dll', [StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath (Join-Path $plugins $_.module) -PathType Leaf) })
    if ($catalog.Count -ne 1 -or $catalog[0].id -ne 'present') { throw 'Missing DLL entered the runtime catalog.' }
} finally { Remove-Item -LiteralPath $root -Recurse -Force }
Write-Host 'Missing bundled module is safely excluded from the synthetic catalog.'
