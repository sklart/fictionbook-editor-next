#pragma once

#include "Fb3Diagnostics.h"
#include "Fb3DiagnosticResources.h"
#include "Fb3Package.h"
#include <set>
#include <string_view>

namespace FbeFb3 {

inline bool IsRussianUi(IFBEPluginHost& host) {
    ATL::CComBSTR locale;
    if (FAILED(host.GetUiLocale(&locale)) || !locale) return false;
    const std::wstring_view name(locale, locale.Length());
    return name == L"ru" || name.substr(0, 3) == L"ru-";
}

inline std::wstring DiagnosticSummary(HINSTANCE module, std::wstring_view code, Severity severity, bool russian) {
    struct Resource { std::wstring_view code; UINT id; };
    static constexpr Resource messages[]{
        {L"source-fragment", FB3_MSG_SOURCE_FRAGMENT},
        {L"source-cover", FB3_MSG_SOURCE_COVER},
        {L"source-metadata", FB3_MSG_SOURCE_METADATA},
        {L"list-layout", FB3_MSG_LIST_LAYOUT},
        {L"link-target", FB3_MSG_LINK_TARGET},
        {L"note-groups", FB3_MSG_NOTE_GROUPS},
        {L"missing-note", FB3_MSG_MISSING_NOTE},
        {L"missing-image", FB3_MSG_MISSING_IMAGE},
        {L"missing-link", FB3_MSG_MISSING_LINK},
        {L"invalid-document", FB3_MSG_INVALID_DOCUMENT},
        {L"layout", FB3_MSG_LAYOUT},

    };
    UINT id = severity == Severity::Error ? FB3_MSG_INVALID_DOCUMENT : FB3_MSG_LAYOUT;
    for (const auto& message : messages) {
        if (message.code == code) { id = message.id; break; }
    }
    wchar_t buffer[256]{};
    const int length = ::LoadStringW(module, id + (russian ? FB3_MSG_RU_OFFSET : 0), buffer, _countof(buffer));
    return length ? std::wstring(buffer, length) : (severity == Severity::Error ? L"Conversion stopped." : L"Conversion has limitations.");
}

inline void ReportPluginDiagnostics(IFBEPluginHost& host, HINSTANCE module, const wchar_t* prefix, const Diagnostics& diagnostics) noexcept {
    try {
        if (diagnostics.empty()) return;
        const auto russian = IsRussianUi(host);
        std::set<std::wstring_view> shown;
        for (const auto& diagnostic : diagnostics) {
            const auto code = std::wstring(prefix) + diagnostic.code;
            host.ReportMessage(0, ATL::CComBSTR(code.c_str()), ATL::CComBSTR(FormatDiagnostic(diagnostic).c_str()));
            if (diagnostic.severity != Severity::Warning || !shown.insert(diagnostic.code).second) continue;
            std::wstring summary = DiagnosticSummary(module, diagnostic.code, diagnostic.severity, russian);
            if (!diagnostic.element.empty()) summary += L" <" + diagnostic.element + L">";
            if (!diagnostic.location.empty()) summary += L" [" + diagnostic.location.substr(0, 96) + L"]";
            host.ReportMessage(1, ATL::CComBSTR(code.c_str()), ATL::CComBSTR(summary.c_str()));
        }
    } catch (...) {
        // Reporting cannot change the outcome of the conversion.
    }
}

inline void ReportPluginError(IFBEPluginHost& host, HINSTANCE module, const wchar_t* prefix, const Error& error) noexcept {
    try {
        const auto code = std::wstring(prefix) + error.code;
        host.ReportMessage(0, ATL::CComBSTR(code.c_str()), ATL::CComBSTR(error.message.c_str()));
        std::wstring summary = DiagnosticSummary(module, error.code, Severity::Error, IsRussianUi(host));
        if (!error.location.empty()) summary += L" [" + error.location.substr(0, 96) + L"]";
        if (!error.message.empty()) summary += L"\r\n" + error.message.substr(0, 512);
        host.ReportMessage(2, ATL::CComBSTR(code.c_str()), ATL::CComBSTR(summary.c_str()));
    } catch (...) {
        // Reporting cannot change the outcome of the conversion.
    }
}

}
