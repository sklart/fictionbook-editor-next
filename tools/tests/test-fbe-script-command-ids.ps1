param(
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'

function Read-ProjectFile([string]$RelativePath) {
    Get-Content -Raw -LiteralPath (Join-Path $RepoRoot $RelativePath)
}

$mainFrame = Read-ProjectFile 'src\fbe\mainfrm.cpp'
$settings = Read-ProjectFile 'src\fbe\Settings.cpp'

foreach($required in @(
    'NormalizeScriptRelativePath',
    'HashScriptRelativePath',
    'AssignScriptCommandIds',
    'ParseScriptCommandIds',
    'SerializeScriptCommandIds',
    'GetScriptCommandIds',
    'SetScriptCommandIds')) {
    if($mainFrame -notmatch [regex]::Escape($required) -and $settings -notmatch [regex]::Escape($required)) {
        throw "Missing required stable script ID component: $required"
    }
}

if($mainFrame -match 'static\s+int\s+SCRIPT_COMMAND_ID') {
    throw 'Script IDs still depend on a global scan-order counter.'
}

function Get-Fnv1a32([string]$Path) {
    [uint32]$hash = 2166136261
    [uint64]$mask = 4294967295
    foreach($character in $Path.ToLowerInvariant().Replace('\', '/').ToCharArray()) {
        $hash = $hash -bxor [uint32][char]$character
        $hash = [uint32](([uint64]$hash * 16777619) -band $mask)
    }
    return $hash
}

function Assign-Ids([string[]]$Paths, [hashtable]$Persisted = @{}) {
    $assigned = @{}
    $used = @{}
    foreach($pair in $Persisted.GetEnumerator()) {
        $assigned[$pair.Key] = $pair.Value
        $used[$pair.Value] = $pair.Key
    }

    foreach($path in $Paths) {
        $normalized = $path.ToLowerInvariant().Replace('\', '/')
        if($assigned.ContainsKey($normalized)) { continue }
        $first = ([int]((Get-Fnv1a32 $normalized) % 999)) + 1
        for($attempt = 0; $attempt -lt 999; ++$attempt) {
            $candidate = (($first - 1 + $attempt) % 999) + 1
            if(-not $used.ContainsKey($candidate)) {
                $assigned[$normalized] = $candidate
                $used[$candidate] = $normalized
                break
            }
        }
        if(-not $assigned.ContainsKey($normalized)) { throw "Не выдан ID для $path" }
    }
    return $assigned
}

$initial = Assign-Ids @('one\test.js', 'two\test.js', 'book.js')
if($initial['one/test.js'] -eq $initial['two/test.js']) {
    throw 'Two test.js files in different folders received the same command ID.'
}

$afterAddition = Assign-Ids @('one\test.js', 'two\test.js', 'book.js', 'another.js') $initial
foreach($path in @('one/test.js', 'two/test.js', 'book.js')) {
    if($initial[$path] -ne $afterAddition[$path]) { throw "Adding another script changed ID $path." }
}

$reordered = Assign-Ids @('book.js', 'two\test.js', 'one\test.js') $initial
foreach($path in @('one/test.js', 'two/test.js', 'book.js')) {
    if($initial[$path] -ne $reordered[$path]) { throw "Changing file order changed ID $path." }
}

$firstBySlot = @{}
$collision = $null
for($index = 0; $index -lt 2000 -and $null -eq $collision; ++$index) {
    $path = "collision/$index.js"
    $slot = (([int]((Get-Fnv1a32 $path) % 999)) + 1)
    if($firstBySlot.ContainsKey($slot)) {
        $collision = @($firstBySlot[$slot], $path)
    } else {
        $firstBySlot[$slot] = $path
    }
}
if($null -eq $collision) { throw 'The collision fixture did not find two paths with the same initial hash slot.' }

$beforeCollision = Assign-Ids @($collision[0])
$afterCollision = Assign-Ids @($collision[0], $collision[1]) $beforeCollision
if($afterCollision[$collision[0]] -ne $beforeCollision[$collision[0]] -or
   $afterCollision[$collision[0]] -eq $afterCollision[$collision[1]]) {
    throw 'A hash collision changed an existing ID or merged two distinct script paths.'
}

Write-Host 'FBE script command ID contract passed.'
