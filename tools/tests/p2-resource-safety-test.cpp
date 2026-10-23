#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <atlbase.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "../../src/common/DiagnosticLog.h"

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char* message) {
    ++checks;
    if(!value) { ++failures; std::printf("FAIL: %s\n", message); }
}
bool failRegister = false, failWindow = false;
int nullProperties = 0, propertyCalls = 0, unregisterCalls = 0, notifications = 0;
ATOM WINAPI TestRegisterClass(const WNDCLASSW* klass) {
    if(failRegister) { ::SetLastError(ERROR_NOT_ENOUGH_MEMORY); return 0; }
    return ::RegisterClassW(klass);
}
HWND WINAPI TestCreateWindow(DWORD ex, LPCWSTR cls, LPCWSTR title, DWORD style,
    int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID data) {
    if(failWindow) { ::SetLastError(ERROR_NOT_ENOUGH_MEMORY); return NULL; }
    return ::CreateWindowExW(ex, cls, title, style, x, y, width, height, parent, menu, instance, data);
}
BOOL WINAPI TestUnregisterClass(LPCWSTR klass, HINSTANCE instance) {
    ++unregisterCalls;
    return ::UnregisterClassW(klass, instance);
}
HANDLE WINAPI TestGetProp(HWND window, LPCWSTR name) {
    ++propertyCalls; if(!window) ++nullProperties;
    return ::GetPropW(window, name);
}
HANDLE WINAPI TestRemoveProp(HWND window, LPCWSTR name) {
    ++propertyCalls; if(!window) ++nullProperties;
    return ::RemovePropW(window, name);
}
BOOL WINAPI TestSetProp(HWND window, LPCWSTR name, HANDLE value) {
    ++propertyCalls; if(!window) ++nullProperties;
    return ::SetPropW(window, name, value);
}
BOOL WINAPI TestEnumThreadWindows(DWORD thread, WNDENUMPROC callback, LPARAM param) {
    ++notifications; return ::EnumThreadWindows(thread, callback, param);
}
}
#define RegisterClassW TestRegisterClass
#define CreateWindowExW TestCreateWindow
#define UnregisterClassW TestUnregisterClass
#define GetPropW TestGetProp
#define RemovePropW TestRemoveProp
#define SetPropW TestSetProp
#define EnumThreadWindows TestEnumThreadWindows
#include "../../src/common/ModernFileDialog.h"
#undef RegisterClassW
#undef CreateWindowExW
#undef UnregisterClassW
#undef GetPropW
#undef RemovePropW
#undef SetPropW
#undef EnumThreadWindows

namespace {
enum class BitmapFailure { None, ImageList, AddIcon, Screen, Memory, Dib, Draw };
BitmapFailure bitmapFailure = BitmapFailure::None;
int dibCalls = 0, missingBits = 0;
HIMAGELIST WINAPI TestImageListCreate(int x, int y, UINT flags, int initial, int grow) {
    return bitmapFailure == BitmapFailure::ImageList ? NULL : ::ImageList_Create(x, y, flags, initial, grow);
}
int WINAPI TestImageListReplaceIcon(HIMAGELIST list, int index, HICON icon) {
    return bitmapFailure == BitmapFailure::AddIcon ? -1 : ::ImageList_ReplaceIcon(list, index, icon);
}
HDC WINAPI TestGetDC(HWND window) { return bitmapFailure == BitmapFailure::Screen ? NULL : ::GetDC(window); }
HDC WINAPI TestCompatibleDC(HDC dc) { return bitmapFailure == BitmapFailure::Memory ? NULL : ::CreateCompatibleDC(dc); }
HBITMAP WINAPI TestDib(HDC dc, const BITMAPINFO* info, UINT usage, VOID** bits, HANDLE section, DWORD offset) {
    ++dibCalls;
    if(!bits) { ++missingBits; return NULL; }
    return bitmapFailure == BitmapFailure::Dib ? NULL : ::CreateDIBSection(dc, info, usage, bits, section, offset);
}
BOOL WINAPI TestDraw(IMAGELISTDRAWPARAMS* draw) {
    return bitmapFailure == BitmapFailure::Draw ? FALSE : ::ImageList_DrawIndirect(draw);
}
}
#define ImageList_Create TestImageListCreate
#define ImageList_ReplaceIcon TestImageListReplaceIcon
#define GetDC TestGetDC
#define CreateCompatibleDC TestCompatibleDC
#define CreateDIBSection TestDib
#define ImageList_DrawIndirect TestDraw
#include "../../src/fbe/scripts/ScriptMenuBitmap.h"
#undef ImageList_Create
#undef ImageList_ReplaceIcon
#undef GetDC
#undef CreateCompatibleDC
#undef CreateDIBSection
#undef ImageList_DrawIndirect

namespace {
enum class ResourceFailure { None, Find, Size, Load, Lock };
ResourceFailure resourceFailure = ResourceFailure::None;
bool fakeResource = false;
DWORD resourceSize = 0;
const char* resourceBytes = nullptr;
int resourceLoads = 0, resourceLocks = 0;
HRSRC WINAPI TestFindResource(HMODULE module, LPCWSTR name, LPCWSTR type) {
    if(!fakeResource) return ::FindResourceW(module, name, type);
    return resourceFailure == ResourceFailure::Find ? NULL : reinterpret_cast<HRSRC>(1);
}
DWORD WINAPI TestSizeResource(HMODULE module, HRSRC resource) {
    if(!fakeResource) return ::SizeofResource(module, resource);
    return resourceFailure == ResourceFailure::Size ? 0 : resourceSize;
}
HGLOBAL WINAPI TestLoadResource(HMODULE module, HRSRC resource) {
    ++resourceLoads;
    if(!fakeResource) return ::LoadResource(module, resource);
    return resourceFailure == ResourceFailure::Load ? NULL : reinterpret_cast<HGLOBAL>(1);
}
LPVOID WINAPI TestLockResource(HGLOBAL resource) {
    ++resourceLocks;
    if(!fakeResource) return ::LockResource(resource);
    return resourceFailure == ResourceFailure::Lock ? NULL : const_cast<char*>(resourceBytes);
}
}
#define FindResourceW TestFindResource
#define SizeofResource TestSizeResource
#define LoadResource TestLoadResource
#define LockResource TestLockResource
#include "../../src/fbe/AboutContributors.h"
#undef FindResourceW
#undef SizeofResource
#undef LoadResource
#undef LockResource

namespace {
void TestScopes() {
    using namespace ModernFileDialog;
    Check(!IsShowingOnCurrentThread(), "no scope initially");
    for(int kind = 0; kind < 2; ++kind) {
        failRegister = kind == 0; failWindow = kind == 1;
        const int propertiesBefore = propertyCalls, unregisterBefore = unregisterCalls, notifyBefore = notifications;
        {
            ShowingScope failed(NULL);
            Check(!failed.Valid(), "failed window scope is invalid");
            Check(!failed.BindDialog(nullptr), "invalid scope cannot bind a dialog");
        }
        Check(propertyCalls == propertiesBefore, "failed scope never accesses HWND properties");
        Check(unregisterCalls == unregisterBefore + (kind == 1 ? 1 : 0), "only owned class is released after failure");
        Check(notifications == notifyBefore, "failed scope sends no change notifications");
        const Result failedShow = Show(NULL, Request{});
        Check(failedShow.outcome == Outcome::Failed && failedShow.error == HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY),
            "Show returns Failed before trying to create the COM dialog");
    }
    failRegister = false; failWindow = false;
    HWND owner = ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    Check(owner != NULL, "test owner created");
    const int before = notifications;
    {
        ShowingScope outer(owner);
        Check(outer.Valid() && IsShowingOnCurrentThread(), "scope recovers after failed creation");
        Check(CurrentOwnerOnThread() == owner, "outer owner installed");
        CComPtr<IFileDialog> dialog;
        Check(SUCCEEDED(dialog.CoCreateInstance(CLSID_FileOpenDialog)), "native dialog COM object created without showing UI");
        Check(dialog && outer.BindDialog(dialog), "outer dialog bound");
        {
            ShowingScope inner(NULL);
            Check(inner.Valid() && CurrentOwnerOnThread() == NULL, "nested null owner installed");
            Check(::GetPropW(ScopeWindow(), L"FBE.ScopeDialog") == NULL, "nested scope clears borrowed dialog");
        }
        Check(CurrentOwnerOnThread() == owner, "nested scope restores outer owner");
        Check(::GetPropW(ScopeWindow(), L"FBE.ScopeDialog") == reinterpret_cast<HANDLE>(dialog.p), "nested scope restores borrowed dialog");
        Check(notifications == before + 1, "nested scopes do not notify as outer scopes");
        // The COM object must outlive the scope's borrowed pointer.
        ::RemovePropW(ScopeWindow(), L"FBE.ScopeDialog");
    }
    Check(!IsShowingOnCurrentThread() && notifications == before + 2, "outer window destroyed and exit notified");
    ::DestroyWindow(owner);
    WNDCLASSW klass = {}; klass.lpfnWndProc = ::DefWindowProcW;
    klass.hInstance = ::GetModuleHandleW(NULL); klass.lpszClassName = ScopeName().c_str();
    Check(::RegisterClassW(&klass) != 0, "pre-existing class registered");
    const int unregisterBefore = unregisterCalls;
    failWindow = true;
    { ShowingScope failed(NULL); Check(!failed.Valid(), "pre-existing class window can fail"); }
    failWindow = false;
    { ShowingScope success(NULL); Check(success.Valid(), "pre-existing class supports a scope"); }
    Check(unregisterCalls == unregisterBefore, "borrowed class survives both paths");
    Check(::UnregisterClassW(klass.lpszClassName, klass.hInstance) != FALSE, "pre-existing class remains registered");
    Check(nullProperties == 0, "no NULL HWND property calls");
}

void TestBitmaps() {
    HICON icon = ::LoadIconW(NULL, IDI_APPLICATION);
    Check(icon != NULL, "native test icon available");
    Check(FbeScripts::Detail::CreateMenuBitmap(NULL) == NULL, "missing icon returns no bitmap");
    // Warm up common controls before checking process GDI handle counts.
    HBITMAP warmup = FbeScripts::Detail::CreateMenuBitmap(icon);
    Check(warmup != NULL, "native DIB creation succeeds");
    if(warmup) ::DeleteObject(warmup);
    const DWORD handles = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    for(int repeat = 0; repeat < 20; ++repeat) {
        bitmapFailure = BitmapFailure::None;
        HBITMAP bitmap = FbeScripts::Detail::CreateMenuBitmap(icon);
        Check(bitmap != NULL, "native alpha bitmap created");
        if(bitmap) {
            DIBSECTION section = {};
            Check(::GetObjectW(bitmap, sizeof(section), &section) == sizeof(section) && section.dsBm.bmBits &&
                section.dsBm.bmWidth == 16 && section.dsBm.bmHeight == 16 && section.dsBm.bmBitsPixel == 32,
                "menu bitmap has real 16x16 32-bit storage");
            ::DeleteObject(bitmap);
        }
        for(BitmapFailure failure : {BitmapFailure::ImageList, BitmapFailure::AddIcon, BitmapFailure::Screen,
            BitmapFailure::Memory, BitmapFailure::Dib, BitmapFailure::Draw}) {
            bitmapFailure = failure;
            HBITMAP failed = FbeScripts::Detail::CreateMenuBitmap(icon);
            Check(failed == NULL, "injected bitmap API failure returns no bitmap");
            if(failed) ::DeleteObject(failed);
        }
    }
    bitmapFailure = BitmapFailure::None;
    Check(dibCalls > 0 && missingBits == 0, "CreateDIBSection always receives an output address");
    Check(::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS) == handles, "success and failure paths leak no GDI handles");
}

void TestResources() {
    const std::wstring real = AboutContributors::Load(::GetModuleHandleW(NULL));
    Check(real.find(L"FictionBook Editor Next") == 0 && real.find(L"contributors listed below") != std::wstring::npos,
        "actual embedded contributors resource loads");
    fakeResource = true;
    resourceBytes = "valid"; resourceSize = 5;
    for(ResourceFailure failure : {ResourceFailure::Find, ResourceFailure::Size, ResourceFailure::Load, ResourceFailure::Lock}) {
        resourceFailure = failure;
        const int loadsBefore = resourceLoads, locksBefore = resourceLocks;
        Check(AboutContributors::Load(NULL).empty(), "resource API failure returns empty text");
        Check(resourceLoads == loadsBefore + (failure == ResourceFailure::Find || failure == ResourceFailure::Size ? 0 : 1),
            "no LoadResource after unavailable metadata");
        Check(resourceLocks == locksBefore + (failure == ResourceFailure::Lock ? 1 : 0), "no LockResource after failed load");
    }
    resourceFailure = ResourceFailure::None;
    resourceSize = 0; Check(AboutContributors::Load(NULL).empty(), "empty resource is safe");
    resourceSize = MAXDWORD; Check(AboutContributors::Load(NULL).empty(), "oversized resource is rejected before int conversion");
    SYSTEM_INFO system = {}; ::GetSystemInfo(&system);
    char* pages = static_cast<char*>(::VirtualAlloc(NULL, 2 * system.dwPageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(pages != nullptr, "guard-page fixture allocated");
    if(pages) {
        DWORD oldProtection = 0;
        Check(::VirtualProtect(pages + system.dwPageSize, system.dwPageSize, PAGE_NOACCESS, &oldProtection) != FALSE,
            "resource end is followed by inaccessible memory");
        const char utf8[] = "A\r\n\xD0\xAF\xE2\x82\xAC\xF0\x9F\x98\x80";
        resourceSize = sizeof(utf8) - 1;
        char* end = pages + system.dwPageSize;
        resourceBytes = end - resourceSize;
        std::memcpy(const_cast<char*>(resourceBytes), utf8, resourceSize);
        Check(AboutContributors::Load(NULL) == L"A\r\n\x042F\x20AC\xD83D\xDE00", "bounded non-NUL UTF-8 with surrogate pair");
        resourceSize = 1; resourceBytes = end - 1;
        *const_cast<char*>(resourceBytes) = 'Z';
        Check(AboutContributors::Load(NULL) == L"Z", "one-byte non-NUL resource does not read past its end");
        *const_cast<char*>(resourceBytes) = static_cast<char>(0xD0);
        Check(AboutContributors::Load(NULL).empty(), "truncated UTF-8 is rejected without overread");
        *const_cast<char*>(resourceBytes) = static_cast<char>(0xFF);
        Check(AboutContributors::Load(NULL).empty(), "invalid UTF-8 is rejected");
        ::VirtualFree(pages, 0, MEM_RELEASE);
    }
    fakeResource = false;
}
}

int main() {
    const HRESULT com = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    Check(SUCCEEDED(com), "COM initialized");
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_WIN95_CLASSES};
    Check(::InitCommonControlsEx(&controls) != FALSE, "common controls initialized");
    TestScopes(); TestBitmaps(); TestResources();
    if(SUCCEEDED(com)) ::CoUninitialize();
    std::printf("P2 resource safety: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
