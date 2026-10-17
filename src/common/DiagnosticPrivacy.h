#pragma once
#include <string>
#include <cwctype>

// Free text has no safe diagnostic type. Keep only known numeric metadata.
namespace FbeDiagnosticPrivacy
{
inline bool Identifier(const std::wstring& value)
{
    if (value.empty() || value.size() > 64) return false;
    for (wchar_t c : value)
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
            (c >= L'0' && c <= L'9') || c == L'-' || c == L'_' || c == L'.')) return false;
    return true;
}
inline bool Number(const std::wstring& value)
{
    if (value.empty() || value.size() > 32) return false;
    size_t begin = value[0] == L'-' ? 1 : 0;
    const bool hex = value.size() > begin + 2 && value.substr(begin, 2) == L"0x";
    if (hex) begin += 2;
    if (begin == value.size()) return false;
    for (size_t i = begin; i < value.size(); ++i)
        if (hex ? !iswxdigit(value[i]) : !(value[i] >= L'0' && value[i] <= L'9')) return false;
    return true;
}
inline std::wstring Trim(std::wstring value)
{
    const size_t first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return std::wstring();
    return value.substr(first, value.find_last_not_of(L" \t\r\n") - first + 1);
}
inline bool Version(const std::wstring& value)
{
    if(value.empty() || value.size()>64) return false;
    for(wchar_t c:value) if(!(c>=L'0' && c<=L'9') && c!=L'.') return false;
    return true;
}
inline bool EventCode(const std::wstring& value)
{
    size_t i=0; while(i<value.size() && value[i]>=L'A' && value[i]<=L'Z') ++i;
    if(i==0 || i>8 || i==value.size() || value.size()-i>8) return false;
    for(;i<value.size();++i) if(value[i]<L'0' || value[i]>L'9') return false;
    return true;
}
inline bool Allowed(const std::wstring& values,const std::wstring& value) { return !value.empty() && values.find(L"|"+value+L"|")!=std::wstring::npos; }
inline std::wstring Details(const wchar_t* text)
{
    std::wstring result, input(text ? text : L"", text ? wcsnlen_s(text,4096) : 0);
    // Preserve constant lifecycle labels used by existing log consumers.
    if(Allowed(L"|process started|process shutdown|FictionBook Editor diagnostic trace started|",input)) return input;
    const std::wstring keys = L"|hr|win32|error|result|number|line|column|count|duration|duration-ms|elapsed|elapsed-ms|bytes|size|PID|TID|present|loaded|elevated|process-arch|native-arch|acp|oemcp|system-lcid|ui-language|source-present|description-present|message-present|file-present|file-name-length|excep.source-length|excep.description-length|errorInfo.source-length|errorInfo.description-length|excep.wCode|excep.scode|excep.helpContext|";
    size_t begin = 0;
    while (begin < input.size()) {
        const size_t end = input.find(L';', begin);
        const std::wstring field = Trim(input.substr(begin, end == std::wstring::npos ? end : end - begin));
        const size_t equals = field.find(L'=');
        if (equals != std::wstring::npos) {
            const std::wstring key = Trim(field.substr(0, equals)), value = Trim(field.substr(equals + 1));
            bool safe=keys.find(L"|"+key+L"|")!=std::wstring::npos && Number(value);
            if(key==L"module") safe=Allowed(L"|FBE.exe|mshtml.dll|ieframe.dll|urlmon.dll|wininet.dll|jscript.dll|jscript9.dll|msxml6.dll|oleaut32.dll|Scintilla.dll|Lexilla.dll|",value);
            else if(key==L"fbe" || key==L"windows" || key==L"service-pack" || key==L"file-version") safe=Version(value) || value==L"not-loaded" || value==L"unavailable";
            else if(key==L"build-configuration") safe=Allowed(L"|Release|Debug|",value);
            else if(key==L"architecture") safe=Allowed(L"|x86|x64|unknown|",value);
            else if(key==L"deployment") safe=Allowed(L"|installed|portable|unknown|",value);
            else if(key==L"integrity") safe=Allowed(L"|low|medium|high|system|unknown|",value);
            else if(key==L"trace-location") safe=Allowed(L"|TEMP|LOCALAPPDATA|portable|",value);
            else if(key==L"feature") safe=Allowed(L"|FEATURE_BROWSER_EMULATION|FEATURE_DOCUMENT_COMPATIBLE_MODE|FEATURE_LOCALMACHINE_LOCKDOWN|FEATURE_BLOCK_LMZ_SCRIPT|FEATURE_RESTRICT_ACTIVEXINSTALL|FEATURE_ZONE_ELEVATION|",value);
            else if(key==L"hkcu32" || key==L"hkcu64" || key==L"hklm32" || key==L"hklm64") safe=Number(value) || value==L"not-set";
            else if(key==L"phase") safe=Allowed(L"|embedded|registered-direct|registered-direct-after-repair|",value);
            else if(key==L"level") safe=Allowed(L"|info|warning|error|",value);
            else if(key==L"failed-stage" || key==L"operation-stage" || key==L"stage") safe=EventCode(value);
            else if(key==L"operation") safe=Allowed(L"|CSS restore|ShowSource|ShowBody|Load|Save|undo|redo|",value);
            if (safe) {
                if (!result.empty()) result += L"; ";
                result += key + L"=" + value;
            }
        }
        if (end == std::wstring::npos) break;
        begin = end + 1;
    }
    if (!input.empty()) { if (!result.empty()) result += L"; "; result += L"details=omitted"; }
    return result;
}
}
