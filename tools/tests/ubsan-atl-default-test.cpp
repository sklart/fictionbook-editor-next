#include <atlbase.h>
#include <atlcom.h>
#include <atlhost.h>
#include <climits>

__declspec(noinline) int OwnOverflow(int value) { return value + 1; }

int wmain(int argc, wchar_t**)
{
    const HRESULT initialized = ::CoInitialize(nullptr);
    if (FAILED(initialized)) return 2;
    ATL::CComObjectStackEx<ATL::CAxFrameWindow> frame;
    HWND window = nullptr;
    const HRESULT created = frame.GetWindow(&window);
    if (window) frame.DestroyWindow();
    ::CoUninitialize();
    if (FAILED(created) || !window) return 2;
    return argc > 1 ? OwnOverflow(INT_MAX) : 0;
}
