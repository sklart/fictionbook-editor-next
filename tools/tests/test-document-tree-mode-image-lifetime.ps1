[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$header = Get-Content -Raw (Join-Path $root 'src\fbe\DocumentTree.h')
$source = Get-Content -Raw (Join-Path $root 'src\fbe\DocumentTree.cpp')
$member = [regex]::Match($header, 'WTL::CImageList(?:Managed)?\s+m_mode_images;')
$destroy = [regex]::Match($source, '(?s)LRESULT CDocumentTree::OnDestroy\([^)]*\)\s*\{.*?\r?\n}')
if (-not $member.Success -or -not $destroy.Success) { throw 'Document tree image ownership source was not found.' }
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$out = Join-Path $root 'out\tests\document-tree-mode-image-lifetime'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$test = @'
#include <atlbase.h>
#include <atlapp.h>
extern WTL::CAppModule _Module;
#include <atlwin.h>
#include <atlctrls.h>
#include <cstdio>
#include <initializer_list>
struct ModeButton {
    bool exists = false;
    HIMAGELIST images = nullptr;
    bool IsWindow() const { return exists; }
    HIMAGELIST SetImageList(HIMAGELIST value) { HIMAGELIST previous = images; images = value; return previous; }
};
class CDocumentTree {
public:
    ModeButton m_mode_button;
    @@MEMBER@@
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
};
@@DESTROY@@
int main() {
    int failures = 0;
    for (bool buttonExists : { false, true }) {
        CDocumentTree tree;
        tree.m_mode_button.exists = buttonExists;
        if (!tree.m_mode_images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 1)) return 2;
        tree.m_mode_button.images = tree.m_mode_images;
        BOOL handled = TRUE;
        tree.OnDestroy(WM_DESTROY, 0, 0, handled);
        if (!tree.m_mode_images.IsNull() || (buttonExists && tree.m_mode_button.images != nullptr) || handled) ++failures;
        if (!tree.m_mode_images.IsNull()) tree.m_mode_images.Destroy();
        tree.OnDestroy(WM_DESTROY, 0, 0, handled);
    }
    const DWORD before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    for (int index = 0; index < 100; ++index) {
        CDocumentTree tree;
        if (!tree.m_mode_images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 1)) return 2;
    }
    const DWORD after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    if (after != before) ++failures;
    printf("Image ownership: failures=%d GDI-before=%lu GDI-after=%lu\n", failures, before, after);
    return failures ? 1 : 0;
}
'@
$test = $test.Replace('@@MEMBER@@', $member.Value).Replace('@@DESTROY@@', $destroy.Value)
$cpp = Join-Path $out 'image-lifetime.cpp'
[IO.File]::WriteAllText($cpp, $test, [Text.UTF8Encoding]::new($false))
$exe = Join-Path $out 'image-lifetime.exe'
& cl.exe /nologo /EHsc /std:c++17 /MT /W4 /WX /utf-8 /DUNICODE /D_UNICODE `
    "/external:I$root\third_party\wtl" /external:W0 "/Fo$out\\" $cpp /link comctl32.lib gdi32.lib user32.lib "/OUT:$exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
exit $LASTEXITCODE
