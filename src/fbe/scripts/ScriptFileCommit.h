#pragma once
#include <windows.h>
#include <comdef.h>

namespace FbeScripts {
template<class Commit>
inline void RetryScriptFileCommit(Commit&& commit)
{
    for (unsigned attempt = 0; ; ++attempt) {
        try { commit(); return; }
        catch (const _com_error& error) {
            if (attempt == 10 || (error.Error() != HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) &&
                error.Error() != HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION) &&
                error.Error() != HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION))) throw;
            ::Sleep(10);
        }
    }
}
}
