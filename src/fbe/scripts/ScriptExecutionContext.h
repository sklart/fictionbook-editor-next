#pragma once
#include <atlstr.h>
#include <vector>
#include <memory>

namespace FbeScripts {
inline CString AbsoluteScriptPath(const CString& input)
{
    if (input.IsEmpty() || wcslen(input) != static_cast<size_t>(input.GetLength())) return CString();
    const DWORD size = ::GetFullPathNameW(input, 0, NULL, NULL);
    if (!size || size > 32768) return CString();
    std::vector<wchar_t> buffer(size);
    const DWORD written = ::GetFullPathNameW(input, size, buffer.data(), NULL);
    if (!written || written >= size) return CString();
    CString path(buffer.data()); path.Replace(L'/', L'\\'); return path;
}
struct ScriptResourceLifetime { virtual ~ScriptResourceLifetime() = default; };
struct ScriptExecutionContext {
    CString path, uid; long token;
    std::shared_ptr<ScriptResourceLifetime> documentResources, progressResources;
    CString lastScriptingError;
    unsigned loggedMessages = 0;
};
inline std::vector<ScriptExecutionContext>& ExecutionContexts()
{
    static thread_local std::vector<ScriptExecutionContext> contexts;
    return contexts;
}
inline long PushExecutionContext(const CString& path, const CString& uid)
{
    static thread_local long nextToken = 0;
    if (nextToken == LONG_MAX) nextToken = 0;
    const long token = ++nextToken;
    ExecutionContexts().push_back({ path, uid, token, {}, {}, {}, 0 });
    return token;
}
inline bool PopExecutionContext(long token)
{
    auto& stack = ExecutionContexts();
    if (stack.empty() || stack.back().token != token) return false;
    stack.pop_back(); return true;
}
inline const ScriptExecutionContext* CurrentExecutionContext()
{
    const auto& stack = ExecutionContexts();
    return stack.empty() ? NULL : &stack.back();
}
class ScriptExecutionScope
{
    size_t m_depth;
public:
    explicit ScriptExecutionScope(const CString& path) : m_depth(ExecutionContexts().size())
    { PushExecutionContext(AbsoluteScriptPath(path), CString()); }
    ~ScriptExecutionScope() { ExecutionContexts().resize(m_depth); }
    ScriptExecutionScope(const ScriptExecutionScope&) = delete;
    ScriptExecutionScope& operator=(const ScriptExecutionScope&) = delete;
};
}
