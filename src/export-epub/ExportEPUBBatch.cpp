#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../version.h"

#define EPUB_WIDEN2(x) L##x
#define EPUB_WIDEN(x) EPUB_WIDEN2(x)

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cwctype>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

typedef int (__stdcall *ExportEpubFileWProc)(const wchar_t*, const wchar_t*, int, unsigned, wchar_t*, unsigned);

static constexpr unsigned FLAG_NCX_FALLBACK = 0x0001;
static constexpr unsigned FLAG_COVER_PAGE = 0x0002;
static constexpr unsigned FLAG_ANNOTATION_PAGE = 0x0004;
static constexpr unsigned FLAG_WRITE_LOG = 0x0008;
static constexpr unsigned FLAG_TITLE_PAGE = 0x0010;

enum class FilenameMode {
    Source,
    Title,
    AuthorTitle,
    TitleAuthor,
    SeriesNumberTitle
};

struct Options {
    fs::path input;
    fs::path output;
    fs::path fromList;
    int version = 3;          // 2, 3, or 0 for both.
    bool recursive = false;
    bool overwrite = false;
    bool newerOnly = false;
    bool dryRun = false;
    bool listOnly = false;
    bool keepFolderStructure = true;
    bool reportCsv = false;
    bool reportLog = false;
    size_t startIndex = 1;       // 1-based index after sorting/filtering.
    size_t maxFiles = 0;         // 0 = no limit.
    unsigned progressEvery = 25; // Print progress every N planned export items.
    bool keepFailedOutput = false;
    FilenameMode filenameMode = FilenameMode::Source;
    unsigned flags = FLAG_NCX_FALLBACK | FLAG_COVER_PAGE | FLAG_ANNOTATION_PAGE;
    fs::path dllPath;
    fs::path reportPath;
};

struct BatchRecord {
    fs::path input;
    fs::path output;
    int version = 0;
    int exitCode = 0;
    unsigned long long sourceSize = 0;
    unsigned long long outputSize = 0;
    long long durationMs = 0;
    std::wstring status;
    std::wstring skipReason;
    std::wstring message;
};

// Forward declarations for helpers used before their definitions.
std::wstring SanitizeFileName(std::wstring name, const std::wstring& fallback);

void PrintVersion()
{
    std::wcout << L"ExportEPUBBatch " << EPUB_WIDEN(FBE_VERSION_STRING) << L"\n"
               << L"FictionBook Editor EPUB batch export utility\n"
               << L"Copyright 2026 Artem Sklyarov aka sklart\n";
}

void PrintUsage()
{
    std::wcout <<
        L"ExportEPUBBatch - batch FB2 -> EPUB converter for FictionBook Editor ExportEPUB.dll\n\n"
        L"Usage:\n"
        L"  ExportEPUBBatch.exe --input <file_or_folder> [--output <file_or_folder>] [options]\n\n"
        L"Options:\n"
        L"  --version 2|3|both       EPUB version. Default: 3.\n"
        L"  --recursive              Process input folder recursively.\n"
        L"  --overwrite              Overwrite existing EPUB files.\n"
        L"  --skip-existing          Skip existing EPUB files explicitly. Default without --overwrite.\n"
        L"  --newer-only             Export only missing or outdated EPUB files.\n"
        L"  --dry-run                Show what would be exported, but do not create files.\n"
        L"  --list, --check-input    List found FB2 files and exit without exporting.\n"
        L"  --from-list <path>       Process only FB2 files listed in a UTF-8 text file.\n"
        L"                           Relative paths are resolved from --input folder.\n"
        L"  --start-index <N>        Start from 1-based file number after sorting/list filtering.\n"
        L"  --max-files <N>          Process no more than N input FB2 files. 0 = no limit.\n"
        L"  --progress-every <N>     Print progress after every N planned export items. Default: 25.\n"
        L"  --filename source|title|author-title|title-author|series-number-title\n"
        L"                           Output file naming mode. Default: source.\n"
        L"  --keep-folder-structure  Preserve input subfolders in output. Default for folders.\n"
        L"  --flat-output            Put all EPUB files directly into output folder.\n"
        L"  --report-csv [path]      Write CSV report. Optional path after the switch.\n"
        L"  --report-log [path]      Write text log. Optional path after the switch.\n"
        L"  --dll <path>             Path to ExportEPUB.dll. Default: next to EXE.\n"
        L"  --no-cover-page          Do not create separate cover.xhtml page.\n"
        L"  --no-annotation-page     Do not create separate annotation page.\n"
        L"  --no-ncx-fallback        Do not add NCX fallback to EPUB 3.\n"
        L"  --log                    Create diagnostic .epub.log files.\n"
        L"  --keep-failed-output     Do not delete partial EPUB files after failed export.\n"
        L"  --version-info           Show utility version and exit.\n"
        L"  --help                   Show this help.\n\n"
        L"Examples:\n"
        L"  ExportEPUBBatch.exe --input \"D:\\Books\" --output \"D:\\EPUB\" --version 3 --recursive --report-csv\n"
        L"  ExportEPUBBatch.exe --input \"D:\\Books\" --version both --recursive --dry-run\n"
        L"  ExportEPUBBatch.exe --input \"D:\\Books\" --from-list problem_files.txt --output \"D:\\EPUB\" --version both\n"
        L"  ExportEPUBBatch.exe --input \"D:\\Books\" --output \"D:\\EPUB\" --version 3 --filename author-title --newer-only\n"
        L"  ExportEPUBBatch.exe --input \"D:\\Books\" --output \"D:\\EPUB\" --version 3 --recursive --start-index 1001 --max-files 1000\n";
}

std::wstring ToLower(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return s;
}

bool IsFb2File(const fs::path& p)
{
    return ToLower(p.extension().wstring()) == L".fb2";
}

unsigned long long FileSizeSafe(const fs::path& p)
{
    std::error_code ec;
    const auto size = fs::file_size(p, ec);
    return ec ? 0 : static_cast<unsigned long long>(size);
}

std::wstring Trim(std::wstring s)
{
    while (!s.empty() && iswspace(s.front())) s.erase(s.begin());
    while (!s.empty() && iswspace(s.back())) s.pop_back();
    if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') {
        s = s.substr(1, s.size() - 2);
        while (!s.empty() && iswspace(s.front())) s.erase(s.begin());
        while (!s.empty() && iswspace(s.back())) s.pop_back();
    }
    return s;
}

std::wstring BytesToWide(const std::string& bytes, UINT codePage)
{
    if (bytes.empty()) return {};
    const DWORD flags = (codePage == CP_UTF8) ? MB_ERR_INVALID_CHARS : 0;
    const int len = MultiByteToWideChar(codePage, flags, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(codePage, flags, bytes.data(), static_cast<int>(bytes.size()), out.data(), len);
    return out;
}

std::wstring ReadTextFileSmart(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    }
    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        std::wstring out;
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            const wchar_t ch = static_cast<wchar_t>(
                static_cast<unsigned char>(bytes[i]) |
                (static_cast<unsigned char>(bytes[i + 1]) << 8));
            out.push_back(ch);
        }
        return out;
    }
    std::wstring utf8 = BytesToWide(bytes, CP_UTF8);
    if (!utf8.empty()) return utf8;
    return BytesToWide(bytes, CP_ACP);
}

std::vector<std::wstring> SplitLines(const std::wstring& text)
{
    std::vector<std::wstring> lines;
    std::wstring current;
    for (wchar_t ch : text) {
        if (ch == L'\r') continue;
        if (ch == L'\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

fs::path ExeDirectory()
{
    std::wstring buf(32768, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (len == 0 || len >= buf.size()) return fs::current_path();
    buf.resize(len);
    return fs::path(buf).parent_path();
}

bool LooksLikeValue(const std::wstring& s)
{
    return !s.empty() && s.rfind(L"--", 0) != 0;
}

fs::path DefaultReportPath(const wchar_t* ext)
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t name[128] = {};
    swprintf_s(name, L"export_epub_%04u%02u%02u_%02u%02u%02u.%s",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, ext);
    return fs::current_path() / name;
}

bool ParseOptionalPath(int argc, wchar_t** argv, int& i, fs::path& out, const wchar_t* defaultExt)
{
    if (i + 1 < argc && LooksLikeValue(argv[i + 1])) {
        out = argv[++i];
    } else {
        out = DefaultReportPath(defaultExt);
    }
    return true;
}


bool ParseSizeTValue(const wchar_t* value, size_t& out, const wchar_t* optionName, bool allowZero)
{
    if (value == nullptr || value[0] == L'\0') {
        std::wcerr << L"Invalid value for " << optionName << L"\n";
        return false;
    }
    wchar_t* end = nullptr;
    const unsigned long long parsed = wcstoull(value, &end, 10);
    if (end == value || *end != L'\0' || (!allowZero && parsed == 0)) {
        std::wcerr << L"Invalid value for " << optionName << L": " << value << L"\n";
        return false;
    }
    out = static_cast<size_t>(parsed);
    return true;
}

bool ParseUIntValue(const wchar_t* value, unsigned& out, const wchar_t* optionName, bool allowZero)
{
    size_t tmp = 0;
    if (!ParseSizeTValue(value, tmp, optionName, allowZero)) return false;
    if (tmp > 1000000) {
        std::wcerr << L"Too large value for " << optionName << L": " << value << L"\n";
        return false;
    }
    out = static_cast<unsigned>(tmp);
    return true;
}

bool ParseArgs(int argc, wchar_t** argv, Options& opt)
{
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        auto needValue = [&](const wchar_t* name) -> const wchar_t* {
            if (i + 1 >= argc) {
                std::wcerr << L"Missing value for " << name << L"\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (a == L"--help" || a == L"-h" || a == L"/?") {
            PrintUsage();
            return false;
        } else if (a == L"--version-info" || a == L"--about") {
            PrintVersion();
            return false;
        } else if (a == L"--input" || a == L"-i") {
            const wchar_t* v = needValue(a.c_str()); if (!v) return false;
            opt.input = v;
        } else if (a == L"--output" || a == L"-o") {
            const wchar_t* v = needValue(a.c_str()); if (!v) return false;
            opt.output = v;
        } else if (a == L"--version" || a == L"-v") {
            const wchar_t* v = needValue(a.c_str()); if (!v) return false;
            std::wstring sv = ToLower(v);
            if (sv == L"2") opt.version = 2;
            else if (sv == L"3") opt.version = 3;
            else if (sv == L"both") opt.version = 0;
            else { std::wcerr << L"Invalid --version value: " << v << L"\n"; return false; }
        } else if (a == L"--recursive" || a == L"-r") {
            opt.recursive = true;
        } else if (a == L"--overwrite") {
            opt.overwrite = true;
        } else if (a == L"--skip-existing") {
            opt.overwrite = false;
        } else if (a == L"--newer-only") {
            opt.newerOnly = true;
        } else if (a == L"--dry-run") {
            opt.dryRun = true;
        } else if (a == L"--list" || a == L"--check-input") {
            opt.listOnly = true;
        } else if (a == L"--from-list") {
            const wchar_t* v = needValue(a.c_str()); if (!v) return false;
            opt.fromList = v;
        } else if (a == L"--start-index") {
            const wchar_t* v = needValue(a.c_str()); if (!v) return false;
            if (!ParseSizeTValue(v, opt.startIndex, L"--start-index", false)) return false;
        } else if (a == L"--max-files") {
            const wchar_t* v = needValue(a.c_str()); if (!v) return false;
            if (!ParseSizeTValue(v, opt.maxFiles, L"--max-files", true)) return false;
        } else if (a == L"--progress-every") {
            const wchar_t* v = needValue(a.c_str()); if (!v) return false;
            if (!ParseUIntValue(v, opt.progressEvery, L"--progress-every", true)) return false;
        } else if (a == L"--filename") {
            const wchar_t* v = needValue(a.c_str()); if (!v) return false;
            std::wstring mode = ToLower(v);
            if (mode == L"source") opt.filenameMode = FilenameMode::Source;
            else if (mode == L"title") opt.filenameMode = FilenameMode::Title;
            else if (mode == L"author-title") opt.filenameMode = FilenameMode::AuthorTitle;
            else if (mode == L"title-author") opt.filenameMode = FilenameMode::TitleAuthor;
            else if (mode == L"series-number-title") opt.filenameMode = FilenameMode::SeriesNumberTitle;
            else { std::wcerr << L"Invalid --filename value: " << v << L"\n"; return false; }
        } else if (a == L"--keep-folder-structure") {
            opt.keepFolderStructure = true;
        } else if (a == L"--flat-output") {
            opt.keepFolderStructure = false;
        } else if (a == L"--report-csv") {
            opt.reportCsv = true;
            ParseOptionalPath(argc, argv, i, opt.reportPath, L"csv");
        } else if (a == L"--report-log") {
            opt.reportLog = true;
            ParseOptionalPath(argc, argv, i, opt.reportPath, L"log");
        } else if (a == L"--dll") {
            const wchar_t* v = needValue(a.c_str()); if (!v) return false;
            opt.dllPath = v;
        } else if (a == L"--title-page") {
            opt.flags |= FLAG_TITLE_PAGE;
        } else if (a == L"--no-title-page") {
            opt.flags &= ~FLAG_TITLE_PAGE;
        } else if (a == L"--no-cover-page") {
            opt.flags &= ~FLAG_COVER_PAGE;
        } else if (a == L"--no-annotation-page") {
            opt.flags &= ~FLAG_ANNOTATION_PAGE;
        } else if (a == L"--no-ncx-fallback") {
            opt.flags &= ~FLAG_NCX_FALLBACK;
        } else if (a == L"--log") {
            opt.flags |= FLAG_WRITE_LOG;
        } else if (a == L"--keep-failed-output") {
            opt.keepFailedOutput = true;
        } else {
            std::wcerr << L"Unknown option: " << a << L"\n";
            return false;
        }
    }

    if (opt.input.empty()) {
        PrintUsage();
        return false;
    }
    if (opt.dllPath.empty()) {
        opt.dllPath = ExeDirectory() / L"ExportEPUB.dll";
    }
    return true;
}

std::vector<fs::path> CollectInputFilesFromList(const Options& opt)
{
    std::vector<fs::path> files;
    const std::wstring text = ReadTextFileSmart(opt.fromList);
    if (text.empty()) {
        std::wcerr << L"Cannot read --from-list file or it is empty: " << opt.fromList << L"\n";
        return files;
    }

    std::set<std::wstring> seen;
    const bool inputIsDir = !opt.input.empty() && fs::is_directory(opt.input);
    size_t lineNo = 0;

    for (std::wstring line : SplitLines(text)) {
        ++lineNo;
        line = Trim(line);
        if (line.empty()) continue;
        if (line[0] == L'#' || line[0] == L';') continue;

        fs::path p(line);
        if (p.is_relative()) {
            if (inputIsDir) p = opt.input / p;
            else p = fs::current_path() / p;
        }

        std::error_code ec;
        fs::path keyPath = fs::absolute(p, ec);
        const std::wstring key = ToLower(ec ? p.wstring() : keyPath.wstring());
        if (!seen.insert(key).second) continue;

        if (!IsFb2File(p)) {
            std::wcerr << L"WARNING: --from-list line " << lineNo << L" is not an .fb2 file: " << line << L"\n";
            continue;
        }
        if (!fs::is_regular_file(p)) {
            std::wcerr << L"WARNING: --from-list file not found at line " << lineNo << L": " << p << L"\n";
            continue;
        }
        files.push_back(p);
    }
    return files;
}

std::vector<fs::path> CollectInputFiles(const Options& opt)
{
    if (!opt.fromList.empty()) {
        return CollectInputFilesFromList(opt);
    }

    std::vector<fs::path> files;
    if (fs::is_regular_file(opt.input)) {
        if (IsFb2File(opt.input)) files.push_back(opt.input);
        return files;
    }
    if (!fs::is_directory(opt.input)) return files;

    if (opt.recursive) {
        for (const auto& e : fs::recursive_directory_iterator(opt.input)) {
            if (e.is_regular_file() && IsFb2File(e.path())) files.push_back(e.path());
        }
    } else {
        for (const auto& e : fs::directory_iterator(opt.input)) {
            if (e.is_regular_file() && IsFb2File(e.path())) files.push_back(e.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

fs::path OutputPathFor(const Options& opt, const fs::path& inputFile, int version, const std::wstring& baseName)
{
    const bool both = (opt.version == 0);
    std::wstring stem = baseName.empty() ? SanitizeFileName(inputFile.stem().wstring(), L"book") : baseName;
    if (both) stem += (version == 2) ? L"_epub2" : L"_epub3";
    const fs::path fileName = stem + L".epub";

    if (opt.output.empty()) return inputFile.parent_path() / fileName;
    if (fs::is_regular_file(opt.input) && ToLower(opt.output.extension().wstring()) == L".epub" && !both) return opt.output;

    fs::path rel;
    if (opt.keepFolderStructure && fs::is_directory(opt.input)) {
        try { rel = fs::relative(inputFile.parent_path(), opt.input); } catch (...) { rel.clear(); }
    }
    return opt.output / rel / fileName;
}

bool EnsureParentDirectory(const fs::path& p)
{
    std::error_code ec;
    const fs::path parent = p.parent_path();
    if (!parent.empty() && !fs::exists(parent, ec)) return fs::create_directories(parent, ec) || fs::exists(parent, ec);
    return true;
}

std::wstring CsvEscape(const std::wstring& s)
{
    std::wstring out = L"\"";
    for (wchar_t ch : s) {
        if (ch == L'\"') out += L"\"\""; else out.push_back(ch);
    }
    out += L"\"";
    return out;
}

void WriteUtf8Bom(std::ofstream& out)
{
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    out.write(reinterpret_cast<const char*>(bom), 3);
}

std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), len, nullptr, nullptr);
    return s;
}

std::wstring SingleLineMessage(std::wstring s)
{
    for (wchar_t& ch : s) {
        if (ch == L'\r' || ch == L'\n' || ch == L'\t') {
            ch = L' ';
        }
    }
    while (s.find(L"  ") != std::wstring::npos) {
        s.erase(s.find(L"  "), 1);
    }
    while (!s.empty() && s.front() == L' ') s.erase(s.begin());
    while (!s.empty() && s.back() == L' ') s.pop_back();
    return s;
}



std::wstring DecodeXmlEntities(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != L'&') {
            out.push_back(s[i]);
            continue;
        }
        const size_t semi = s.find(L';', i + 1);
        if (semi == std::wstring::npos || semi - i > 16) {
            out.push_back(s[i]);
            continue;
        }
        const std::wstring ent = s.substr(i + 1, semi - i - 1);
        if (ent == L"amp") out.push_back(L'&');
        else if (ent == L"lt") out.push_back(L'<');
        else if (ent == L"gt") out.push_back(L'>');
        else if (ent == L"quot") out.push_back(L'"');
        else if (ent == L"apos") out.push_back(L'\'');
        else if (!ent.empty() && ent[0] == L'#') {
            wchar_t* end = nullptr;
            unsigned long code = 0;
            if (ent.size() > 2 && (ent[1] == L'x' || ent[1] == L'X')) {
                code = wcstoul(ent.c_str() + 2, &end, 16);
            } else {
                code = wcstoul(ent.c_str() + 1, &end, 10);
            }
            if (end != nullptr && *end == L'\0' && code > 0 && code <= 0xFFFF) {
                out.push_back(static_cast<wchar_t>(code));
            }
        } else {
            out.append(s, i, semi - i + 1);
        }
        i = semi;
    }
    return out;
}

std::wstring StripTagsAndNormalize(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    bool inTag = false;
    for (wchar_t ch : s) {
        if (ch == L'<') { inTag = true; continue; }
        if (ch == L'>') { inTag = false; out.push_back(L' '); continue; }
        if (!inTag) out.push_back(ch);
    }
    return SingleLineMessage(DecodeXmlEntities(out));
}

bool IsNameChar(wchar_t ch)
{
    return iswalnum(ch) || ch == L'-' || ch == L'_' || ch == L':';
}

size_t FindElementOpen(const std::wstring& xml, const std::wstring& tag, size_t start = 0)
{
    const std::wstring needle = L"<" + tag;
    size_t pos = start;
    while ((pos = xml.find(needle, pos)) != std::wstring::npos) {
        const size_t after = pos + needle.size();
        if (after >= xml.size() || !IsNameChar(xml[after])) return pos;
        pos = after;
    }
    return std::wstring::npos;
}

std::wstring ExtractElementInner(const std::wstring& xml, const std::wstring& tag, size_t start = 0)
{
    const size_t open = FindElementOpen(xml, tag, start);
    if (open == std::wstring::npos) return {};
    const size_t gt = xml.find(L'>', open);
    if (gt == std::wstring::npos) return {};
    if (gt > open && xml[gt - 1] == L'/') return {};
    const std::wstring closeTag = L"</" + tag + L">";
    const size_t close = xml.find(closeTag, gt + 1);
    if (close == std::wstring::npos) return {};
    return xml.substr(gt + 1, close - gt - 1);
}

std::wstring ExtractAttributeFromOpenTag(const std::wstring& openTag, const std::wstring& attr)
{
    const std::wstring needle = attr + L"=";
    size_t pos = openTag.find(needle);
    while (pos != std::wstring::npos) {
        if (pos == 0 || !IsNameChar(openTag[pos - 1])) break;
        pos = openTag.find(needle, pos + 1);
    }
    if (pos == std::wstring::npos) return {};
    pos += needle.size();
    if (pos >= openTag.size()) return {};
    const wchar_t quote = openTag[pos];
    if (quote != L'"' && quote != L'\'') return {};
    const size_t end = openTag.find(quote, pos + 1);
    if (end == std::wstring::npos) return {};
    return DecodeXmlEntities(openTag.substr(pos + 1, end - pos - 1));
}

std::wstring ExtractFirstElementText(const std::wstring& xml, const std::wstring& tag)
{
    return StripTagsAndNormalize(ExtractElementInner(xml, tag));
}

struct BookNameInfo {
    std::wstring title;
    std::wstring author;
    std::wstring series;
    std::wstring seriesNumber;
};

std::wstring JoinNameParts(const std::vector<std::wstring>& parts)
{
    std::wstring out;
    for (const auto& p : parts) {
        const std::wstring t = SingleLineMessage(p);
        if (t.empty()) continue;
        if (!out.empty()) out += L" ";
        out += t;
    }
    return out;
}

std::wstring FormatSeriesNumber(std::wstring number)
{
    number = SingleLineMessage(number);
    if (number.empty()) return {};
    bool digits = true;
    for (wchar_t ch : number) {
        if (!iswdigit(ch)) { digits = false; break; }
    }
    if (!digits) return number;
    const int n = _wtoi(number.c_str());
    wchar_t buf[32] = {};
    swprintf_s(buf, L"%03d", n);
    return buf;
}

BookNameInfo ReadBookNameInfo(const fs::path& inputFile)
{
    BookNameInfo info;
    const std::wstring xml = ReadTextFileSmart(inputFile);
    if (xml.empty()) return info;

    const std::wstring titleInfo = ExtractElementInner(xml, L"title-info");
    const std::wstring scope = titleInfo.empty() ? xml : titleInfo;

    info.title = ExtractFirstElementText(scope, L"book-title");

    const std::wstring author = ExtractElementInner(scope, L"author");
    if (!author.empty()) {
        const std::wstring first = ExtractFirstElementText(author, L"first-name");
        const std::wstring middle = ExtractFirstElementText(author, L"middle-name");
        const std::wstring last = ExtractFirstElementText(author, L"last-name");
        if (!last.empty() || !first.empty() || !middle.empty()) {
            info.author = JoinNameParts({last, first, middle});
        } else {
            info.author = ExtractFirstElementText(author, L"nickname");
        }
    }

    const size_t seq = FindElementOpen(scope, L"sequence");
    if (seq != std::wstring::npos) {
        const size_t gt = scope.find(L'>', seq);
        if (gt != std::wstring::npos) {
            const std::wstring openTag = scope.substr(seq, gt - seq + 1);
            info.series = SingleLineMessage(ExtractAttributeFromOpenTag(openTag, L"name"));
            info.seriesNumber = FormatSeriesNumber(ExtractAttributeFromOpenTag(openTag, L"number"));
        }
    }
    return info;
}

std::wstring SanitizeFileName(std::wstring name, const std::wstring& fallback)
{
    name = SingleLineMessage(name);
    const std::wstring forbidden = L"<>:\"/\\|?*";
    for (wchar_t& ch : name) {
        if (ch < 32 || forbidden.find(ch) != std::wstring::npos) ch = L' ';
    }
    name = SingleLineMessage(name);
    while (!name.empty() && (name.back() == L'.' || name.back() == L' ')) name.pop_back();
    while (!name.empty() && (name.front() == L'.' || name.front() == L' ')) name.erase(name.begin());
    if (name.empty()) name = fallback;
    if (name.size() > 180) {
        name.resize(180);
        while (!name.empty() && (name.back() == L'.' || name.back() == L' ')) name.pop_back();
    }
    return name.empty() ? L"book" : name;
}

std::wstring BaseNameFor(const Options& opt, const fs::path& inputFile)
{
    const std::wstring source = SanitizeFileName(inputFile.stem().wstring(), L"book");
    if (opt.filenameMode == FilenameMode::Source) return source;

    const BookNameInfo info = ReadBookNameInfo(inputFile);
    std::wstring candidate;
    switch (opt.filenameMode) {
    case FilenameMode::Title:
        candidate = info.title;
        break;
    case FilenameMode::AuthorTitle:
        candidate = JoinNameParts({info.author, info.author.empty() || info.title.empty() ? L"" : L"-", info.title});
        break;
    case FilenameMode::TitleAuthor:
        candidate = JoinNameParts({info.title, info.author.empty() || info.title.empty() ? L"" : L"-", info.author});
        break;
    case FilenameMode::SeriesNumberTitle:
        candidate = JoinNameParts({info.series, info.seriesNumber, info.title});
        break;
    case FilenameMode::Source:
    default:
        candidate = source;
        break;
    }
    return SanitizeFileName(candidate, source);
}

std::wstring OutputKey(const fs::path& p)
{
    std::error_code ec;
    const fs::path abs = fs::absolute(p, ec);
    return ToLower(ec ? p.wstring() : abs.wstring());
}

fs::path MakeUniqueReservedPath(const fs::path& desired, std::set<std::wstring>& reserved)
{
    fs::path candidate = desired;
    int index = 2;
    while (!reserved.emplace(OutputKey(candidate)).second) {
        candidate = desired.parent_path() / (desired.stem().wstring() + L"_" + std::to_wstring(index) + desired.extension().wstring());
        ++index;
    }
    return candidate;
}

bool ExistingOutputIsUpToDate(const fs::path& source, const fs::path& output)
{
    if (!fs::exists(output) || FileSizeSafe(output) == 0) return false;
    std::error_code ec1, ec2;
    const auto srcTime = fs::last_write_time(source, ec1);
    const auto outTime = fs::last_write_time(output, ec2);
    if (ec1 || ec2) return false;
    return outTime >= srcTime;
}

void CopyErrorToBuffer(wchar_t* buffer, unsigned bufferChars, const wchar_t* message)
{
    if (buffer == nullptr || bufferChars == 0) {
        return;
    }
    buffer[0] = L'\0';
    if (message != nullptr && message[0] != L'\0') {
        wcsncpy_s(buffer, bufferChars, message, _TRUNCATE);
    }
}

int SafeExportCall(
    ExportEpubFileWProc exportFn,
    const wchar_t* input,
    const wchar_t* output,
    int version,
    unsigned flags,
    wchar_t* errorBuffer,
    unsigned errorBufferChars)
{
#if defined(_MSC_VER)
    __try {
        return exportFn(input, output, version, flags, errorBuffer, errorBufferChars);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DWORD code = GetExceptionCode();
        wchar_t tmp[128] = {};
        swprintf_s(tmp, L"ExportEPUB.dll crashed while processing this file (SEH 0x%08X)", code);
        CopyErrorToBuffer(errorBuffer, errorBufferChars, tmp);
        return 200;
    }
#else
    return exportFn(input, output, version, flags, errorBuffer, errorBufferChars);
#endif
}

void WriteReports(const Options& opt, const std::vector<BatchRecord>& records)
{
    if (!opt.reportCsv && !opt.reportLog) return;
    fs::path path = opt.reportPath;
    if (path.empty()) path = DefaultReportPath(opt.reportCsv ? L"csv" : L"log");

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return;
    WriteUtf8Bom(out);
    if (opt.reportCsv) {
        out << "Input;Output;Version;Status;ExitCode;DurationMs;SourceSize;OutputSize;SkipReason;Message\r\n";
        for (const auto& r : records) {
            std::wstring line =
                CsvEscape(r.input.wstring()) + L";" +
                CsvEscape(r.output.wstring()) + L";" +
                std::to_wstring(r.version) + L";" +
                CsvEscape(r.status) + L";" +
                std::to_wstring(r.exitCode) + L";" +
                std::to_wstring(r.durationMs) + L";" +
                std::to_wstring(r.sourceSize) + L";" +
                std::to_wstring(r.outputSize) + L";" +
                CsvEscape(r.skipReason) + L";" +
                CsvEscape(SingleLineMessage(r.message)) + L"\r\n";
            const std::string utf8 = WideToUtf8(line);
            out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
        }
    } else {
        for (const auto& r : records) {
            std::wstringstream w;
            w << L"[" << r.status << L"] EPUB " << r.version << L": " << r.input << L" -> " << r.output
              << L" | rc=" << r.exitCode
              << L" | duration_ms=" << r.durationMs
              << L" | source_size=" << r.sourceSize
              << L" | output_size=" << r.outputSize;
            if (!r.skipReason.empty()) w << L" | skip_reason=" << r.skipReason;
            if (!r.message.empty()) w << L" | " << SingleLineMessage(r.message);
            w << L"\r\n";
            const std::string utf8 = WideToUtf8(w.str());
            out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
        }
    }
    std::wcout << L"Report written: " << path << L"\n";
}


void DeletePartialOutputIfNeeded(const Options& opt, const fs::path& out)
{
    if (opt.keepFailedOutput) return;
    std::error_code ec;
    if (fs::exists(out, ec)) {
        fs::remove(out, ec);
    }
}

int wmain(int argc, wchar_t** argv)
{
    Options opt;
    if (!ParseArgs(argc, argv, opt)) return 2;

    auto files = CollectInputFiles(opt);
    const size_t foundBeforeRange = files.size();
    if (files.empty()) {
        std::wcerr << L"No .fb2 files found.\n";
        return 5;
    }

    if (opt.startIndex > 1 || opt.maxFiles > 0) {
        const size_t begin = (opt.startIndex > 0) ? (opt.startIndex - 1) : 0;
        if (begin >= files.size()) {
            files.clear();
        } else {
            const size_t end = (opt.maxFiles > 0) ? std::min(files.size(), begin + opt.maxFiles) : files.size();
            files = std::vector<fs::path>(files.begin() + static_cast<std::ptrdiff_t>(begin), files.begin() + static_cast<std::ptrdiff_t>(end));
        }
    }

    if (files.empty()) {
        std::wcerr << L"No .fb2 files selected by --start-index/--max-files. Found before range: " << foundBeforeRange << L"\n";
        return 5;
    }

    std::wcout << L"Found .fb2 files: " << foundBeforeRange << L"\n";
    if (files.size() != foundBeforeRange) {
        std::wcout << L"Selected .fb2 files for this run: " << files.size()
                   << L" (start-index=" << opt.startIndex
                   << L", max-files=" << opt.maxFiles << L")\n";
    }
    if (opt.listOnly) {
        for (const auto& file : files) {
            std::wcout << file << L"\n";
        }
        return 0;
    }

    HMODULE dll = nullptr;
    ExportEpubFileWProc exportFn = nullptr;
    if (!opt.dryRun) {
        dll = LoadLibraryW(opt.dllPath.c_str());
        if (!dll) { std::wcerr << L"Cannot load DLL: " << opt.dllPath << L"\n"; return 3; }
        exportFn = reinterpret_cast<ExportEpubFileWProc>(GetProcAddress(dll, "ExportEpubFileW"));
        if (!exportFn) { std::wcerr << L"ExportEpubFileW was not found in DLL.\n"; FreeLibrary(dll); return 4; }
    }

    size_t ok = 0, failed = 0, skipped = 0, planned = 0;
    const size_t totalPlannedItems = files.size() * ((opt.version == 0) ? 2u : 1u);
    std::vector<BatchRecord> records;
    std::set<std::wstring> reservedOutputs;

    for (const auto& file : files) {
        const std::wstring baseName = BaseNameFor(opt, file);
        std::vector<int> versions = (opt.version == 0) ? std::vector<int>{2, 3} : std::vector<int>{opt.version};
        for (int v : versions) {
            const fs::path out = MakeUniqueReservedPath(OutputPathFor(opt, file, v, baseName), reservedOutputs);
            ++planned;
            BatchRecord rec;
            rec.input = file;
            rec.output = out;
            rec.version = v;
            rec.sourceSize = FileSizeSafe(file);
            if (fs::exists(out)) {
                if (FileSizeSafe(out) == 0) {
                    std::wcout << L"RETRY: removing zero-size existing output " << out << L"\n";
                    std::error_code removeEc;
                    fs::remove(out, removeEc);
                } else if (opt.newerOnly && ExistingOutputIsUpToDate(file, out)) {
                    std::wcout << L"SKIP: " << out << L" is up to date\n";
                    ++skipped; rec.status = L"SKIP"; rec.skipReason = L"up to date"; rec.message = L"output file is newer than or same age as source"; rec.outputSize = FileSizeSafe(out); records.push_back(std::move(rec));
                    if (opt.progressEvery > 0 && planned % opt.progressEvery == 0) std::wcout << L"Progress: " << planned << L"/" << totalPlannedItems << L" items, OK=" << ok << L", skipped=" << skipped << L", failed=" << failed << L"\n";
                    continue;
                } else if (!opt.overwrite && !opt.newerOnly) {
                    std::wcout << L"SKIP: " << out << L" already exists\n";
                    ++skipped; rec.status = L"SKIP"; rec.skipReason = L"already exists"; rec.message = L"already exists"; rec.outputSize = FileSizeSafe(out); records.push_back(std::move(rec));
                    if (opt.progressEvery > 0 && planned % opt.progressEvery == 0) std::wcout << L"Progress: " << planned << L"/" << totalPlannedItems << L" items, OK=" << ok << L", skipped=" << skipped << L", failed=" << failed << L"\n";
                    continue;
                } else if (opt.overwrite || opt.newerOnly) {
                    std::error_code removeEc;
                    fs::remove(out, removeEc);
                }
            }
            if (opt.dryRun) {
                std::wcout << L"DRY-RUN EPUB " << v << L": " << file << L" -> " << out << L"\n";
                rec.status = L"PLAN"; rec.message = L"dry run"; records.push_back(std::move(rec));
                if (opt.progressEvery > 0 && planned % opt.progressEvery == 0) std::wcout << L"Progress: " << planned << L"/" << totalPlannedItems << L" items, OK=" << ok << L", skipped=" << skipped << L", failed=" << failed << L"\n";
                continue;
            }
            if (!EnsureParentDirectory(out)) {
                std::wcerr << L"FAIL: cannot create output directory for " << out << L"\n";
                ++failed; rec.status = L"FAIL"; rec.exitCode = 6; rec.message = L"cannot create output directory"; records.push_back(std::move(rec));
                if (opt.progressEvery > 0 && planned % opt.progressEvery == 0) std::wcout << L"Progress: " << planned << L"/" << totalPlannedItems << L" items, OK=" << ok << L", skipped=" << skipped << L", failed=" << failed << L"\n";
                continue;
            }
            wchar_t err[4096] = {};
            std::wcout << L"EXPORT EPUB " << v << L": " << file << L" -> " << out << std::endl;
            const auto started = std::chrono::steady_clock::now();
            int rc = SafeExportCall(exportFn, file.c_str(), out.c_str(), v, opt.flags, err, static_cast<unsigned>(sizeof(err) / sizeof(err[0])));
            const auto finished = std::chrono::steady_clock::now();
            rec.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count();
            rec.exitCode = rc;
            rec.outputSize = FileSizeSafe(out);
            if (rc == 0 && (!fs::exists(out) || rec.outputSize == 0)) {
                ++failed;
                rec.status = L"FAIL";
                rec.exitCode = 201;
                rec.message = fs::exists(out) ? L"export returned success, but output file is empty" : L"export returned success, but output file was not created";
                DeletePartialOutputIfNeeded(opt, out);
                rec.outputSize = FileSizeSafe(out);
                std::wcerr << L"FAIL rc=" << rec.exitCode << L": " << rec.message << L"\n";
            } else if (rc == 0) {
                ++ok;
                rec.status = L"OK";
            } else {
                ++failed;
                rec.status = L"FAIL";
                rec.message = SingleLineMessage(err[0] ? err : L"unknown error");
                DeletePartialOutputIfNeeded(opt, out);
                rec.outputSize = FileSizeSafe(out);
                std::wcerr << L"FAIL rc=" << rc << L": " << rec.message << L"\n";
            }
            records.push_back(std::move(rec));
            if (opt.progressEvery > 0 && planned % opt.progressEvery == 0) {
                std::wcout << L"Progress: " << planned << L"/" << totalPlannedItems
                           << L" items, OK=" << ok << L", skipped=" << skipped
                           << L", failed=" << failed << L"\n";
            }
        }
    }

    if (dll) FreeLibrary(dll);
    WriteReports(opt, records);
    std::wcout << L"\nDone. Planned: " << planned << L", OK: " << ok << L", skipped: " << skipped << L", failed: " << failed << L"\n";
    return failed == 0 ? 0 : 1;
}
