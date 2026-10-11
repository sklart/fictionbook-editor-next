#include "stdafx.h"
#include "RegexHelpLauncher.h"
#include "..\..\..\common\DeploymentContext.h"
#include "..\..\..\common\RuntimeLocalizationCommon.h"
#include "..\..\RuntimeLocalization.h"
#include "..\..\StartupTrace.h"
#include <shellapi.h>

namespace
{
CString HtmlPath(const CString& root, LPCWSTR locale, FbeSearchPresets::SearchUiContext context)
{
    CString path(root);
    if (!path.IsEmpty() && path.Right(1) != L"\\") path += L"\\";
    path += L"Help\\";
    path += locale;
    path += context == FbeSearchPresets::SearchUiContext::Source ? L"\\regex-source.html" : L"\\regex-design.html";
    return path;
}

bool FileExists(const CString& path)
{
    const DWORD attributes = ::GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool ResolveHtmlPath(const CString& root, LPCWSTR locale, FbeSearchPresets::SearchUiContext context, CString& target)
{
    target = HtmlPath(root, FbeRuntimeLocalization::IsKnownRuntimeLocaleName(locale) ? locale : L"en-US", context);
    if (FileExists(target)) return true;
    target = HtmlPath(root, L"en-US", context);
    if (FileExists(target)) return true;
    target.Empty();
    return false;
}

bool LaunchHtml(HWND owner, const CString& root, LPCWSTR locale, FbeSearchPresets::SearchUiContext context,
    bool testOnly, CString& target, ULONGLONG& resolveMs, ULONGLONG& executeMs)
{
    const ULONGLONG start = ::GetTickCount64();
    const bool found = ResolveHtmlPath(root, locale, context, target);
    resolveMs = ::GetTickCount64() - start;
    executeMs = 0;
    if (!found) return false;
    if (testOnly) return true;
    SHELLEXECUTEINFOW info = {}; info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_ASYNCOK | SEE_MASK_FLAG_NO_UI;
    info.hwnd = owner; info.lpVerb = L"open"; info.lpFile = target;
    info.nShow = SW_SHOWNORMAL;
    const ULONGLONG shellStart = ::GetTickCount64();
    const bool opened = ::ShellExecuteExW(&info) != FALSE;
    executeMs = ::GetTickCount64() - shellStart;
    return opened;
}

bool WriteFixture(const CString& root, LPCWSTR locale, LPCWSTR file)
{
    CString help(root); help += L"\\Help";
    ::CreateDirectoryW(help, NULL);
    help += L"\\"; help += locale;
    ::CreateDirectoryW(help, NULL);
    help += L"\\"; help += file;
    HANDLE handle = ::CreateFileW(help, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) return false;
    const char content[] = "<!doctype html><html></html>";
    DWORD written = 0;
    const bool success = ::WriteFile(handle, content, sizeof(content) - 1, &written, NULL) != FALSE && written == sizeof(content) - 1;
    ::CloseHandle(handle);
    return success;
}
}

void OpenFullRegexHelp(HWND owner, FbeSearchPresets::SearchUiContext context)
{
    const CString root(DeploymentContext::ExecutableDirectory().c_str());
    const CString locale(FbeRuntimeLocalization::GetPreferredRuntimeLocaleName());
    CString target;
    ULONGLONG resolveMs = 0, executeMs = 0;
    const bool opened = LaunchHtml(owner, root, locale, context, false, target, resolveMs, executeMs);
    CString details; details.Format(L"help_target_resolve_ms=%I64u shell_execute_ms=%I64u", resolveMs, executeMs);
    StartupTrace::Event(L"regex-help", opened ? L"RH200" : L"RH500", details);
    if (!opened)
        ::MessageBoxW(owner,
            FbeLoadRuntimeStringByKey(L"fbe.regex_help.open_failed", L"Cannot open the full regular-expression help in the browser."),
            FbeLoadRuntimeStringByKey(L"fbe.regex_help.title", L"Regular expression help"), MB_OK | MB_ICONERROR);
}

bool RunRegexHelpLauncherRuntimeSmoke(CStringA& report)
{
    wchar_t temporary[MAX_PATH] = {};
    if (::GetTempPathW(_countof(temporary), temporary) == 0) return false;
    wchar_t unique[MAX_PATH] = {};
    if (!::GetTempFileNameW(temporary, L"rhl", 0, unique)) return false;
    ::DeleteFileW(unique);
    CString root(unique); root += L" space Юникод";
    if (!::CreateDirectoryW(root, NULL)) return false;
    const bool fixtures = WriteFixture(root, L"ru-RU", L"regex-design.html") &&
        WriteFixture(root, L"ru-RU", L"regex-source.html") &&
        WriteFixture(root, L"de-DE", L"regex-design.html") &&
        WriteFixture(root, L"en-US", L"regex-design.html") &&
        WriteFixture(root, L"en-US", L"regex-source.html");
    CString target; ULONGLONG resolveMs = 0, executeMs = 0;
    const auto probe = [&](LPCWSTR locale, FbeSearchPresets::SearchUiContext context, LPCWSTR expected) {
        return LaunchHtml(NULL, root, locale, context, true, target, resolveMs, executeMs) &&
            target.Find(expected) >= 0 && FileExists(target) && executeMs == 0;
    };
    const bool ruDesign = fixtures && probe(L"ru-RU", FbeSearchPresets::SearchUiContext::Design, L"ru-RU\\regex-design.html");
    const bool ruSource = fixtures && probe(L"ru-RU", FbeSearchPresets::SearchUiContext::Source, L"ru-RU\\regex-source.html");
    const bool otherLocale = fixtures && probe(L"de-DE", FbeSearchPresets::SearchUiContext::Design, L"de-DE\\regex-design.html");
    const bool fallback = fixtures && probe(L"fr-FR", FbeSearchPresets::SearchUiContext::Design, L"en-US\\regex-design.html");
    const bool unicode = fixtures && target.Find(L"space Юникод") >= 0;
    CString missing(root); missing += L"\\missing"; ::CreateDirectoryW(missing, NULL);
    const bool noFiles = !LaunchHtml(NULL, missing, L"ru-RU", FbeSearchPresets::SearchUiContext::Design,
        true, target, resolveMs, executeMs) && target.IsEmpty();
    const bool portableLayout = fixtures && probe(L"en-US", FbeSearchPresets::SearchUiContext::Source, L"Help\\en-US\\regex-source.html");
    CString installed(root); installed += L"\\Program Files FBE Next";
    const bool installedLayout = ::CreateDirectoryW(installed, NULL) && WriteFixture(installed, L"en-US", L"regex-design.html") &&
        LaunchHtml(NULL, installed, L"en-US", FbeSearchPresets::SearchUiContext::Design, true, target, resolveMs, executeMs) &&
        target.Find(L"Program Files FBE Next\\Help\\en-US\\regex-design.html") >= 0;
    ::DeleteFileW(HtmlPath(installed, L"en-US", FbeSearchPresets::SearchUiContext::Design));
    CString installedLocale(installed); installedLocale += L"\\Help\\en-US"; ::RemoveDirectoryW(installedLocale);
    CString installedHelp(installed); installedHelp += L"\\Help"; ::RemoveDirectoryW(installedHelp);
    ::RemoveDirectoryW(installed);
    for(LPCWSTR locale : { L"ru-RU", L"de-DE", L"en-US" })
    {
        for(LPCWSTR file : { L"regex-design.html", L"regex-source.html" }) ::DeleteFileW(HtmlPath(root, locale,
            wcscmp(file, L"regex-source.html") == 0 ? FbeSearchPresets::SearchUiContext::Source : FbeSearchPresets::SearchUiContext::Design));
        CString folder(root); folder += L"\\Help\\"; folder += locale; ::RemoveDirectoryW(folder);
    }
    CString help(root); help += L"\\Help"; ::RemoveDirectoryW(help);
    ::RemoveDirectoryW(missing); ::RemoveDirectoryW(root);
    const bool passed = ruDesign && ruSource && otherLocale && fallback && unicode && noFiles && portableLayout && installedLayout;
    report.Format("ru_design=%d\r\nru_source=%d\r\nother_locale=%d\r\nfallback=%d\r\nunicode=%d\r\nmissing=%d\r\nportable_layout=%d\r\ninstalled_layout=%d\r\nresult=%s\r\n",
        ruDesign, ruSource, otherLocale, fallback, unicode, noFiles, portableLayout, installedLayout, passed ? "pass" : "fail");
    return passed;
}
