#pragma once

#include <string>
#include <vector>

namespace FbeFb3 {

enum class Severity { Info, Warning, Error };

struct Diagnostic {
    Severity severity;
    const wchar_t* code;
    std::wstring message;
    std::wstring element;
    std::wstring location;
};

using Diagnostics = std::vector<Diagnostic>;

inline const wchar_t* SeverityName(Severity severity) {
    switch (severity) {
    case Severity::Info: return L"INFO";
    case Severity::Warning: return L"WARNING";
    case Severity::Error: return L"ERROR";
    }
    return L"ERROR";
}

inline std::wstring FormatDiagnostic(const Diagnostic& diagnostic) {
    return std::wstring(SeverityName(diagnostic.severity)) + L" [" + diagnostic.code + L"] " + diagnostic.message;
}

}
