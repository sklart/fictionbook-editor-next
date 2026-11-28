#include "Fb3Batch.h"
#include "Fb3Package.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string_view>

namespace FbeFb3 {
namespace {

namespace fs = std::filesystem;

[[noreturn]] void Fail(std::wstring message) { throw Error{std::move(message)}; }

class PendingFile {
public:
    explicit PendingFile(const fs::path& destination) {
        GUID guid{};
        wchar_t id[40]{};
        if (FAILED(::CoCreateGuid(&guid)) || !::StringFromGUID2(guid, id, _countof(id))) Fail(L"Cannot create a unique output name.");
        m_path = destination.parent_path() / (L".fb3-" + std::wstring(id) + L".tmp");
        HANDLE file = ::CreateFileW(m_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) Fail(L"Cannot create a temporary file beside " + destination.wstring());
        ::CloseHandle(file);
    }
    ~PendingFile() { if (!m_path.empty()) ::DeleteFileW(m_path.c_str()); }
    PendingFile(const PendingFile&) = delete;
    const fs::path& Path() const { return m_path; }
    void Publish(const fs::path& destination) {
        if (!::MoveFileExW(m_path.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) {
            const auto error = ::GetLastError();
            if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
                Fail(L"Output already exists: " + destination.wstring());
            Fail(L"Cannot publish " + destination.wstring() + L" (Windows error " + std::to_wstring(error) + L").");
        }
        m_path.clear();
    }
private:
    fs::path m_path;
};

struct Options {
    fs::path input, output, report;
    bool recursive = false;
};

Options Parse(int argc, wchar_t** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::wstring_view item(argv[i]);
        if (item == L"--recursive") { options.recursive = true; continue; }
        if (item == L"--output" || item == L"--report") {
            auto& value = item == L"--output" ? options.output : options.report;
            if (i + 1 >= argc || argv[i + 1][0] == L'\0' || std::wstring_view(argv[i + 1]).substr(0, 2) == L"--" || !value.empty())
                Fail(L"Missing or repeated value for " + std::wstring(item));
            value = argv[++i];
        } else if (item.empty() || item.substr(0, 2) == L"--" || !options.input.empty()) {
            Fail(L"Unexpected argument: " + std::wstring(item));
        } else options.input = item;
    }
    if (options.input.empty() || options.output.empty()) Fail(L"Input and --output DIRECTORY are required.");
    return options;
}

bool Matches(const fs::path& path, const wchar_t* extension) {
    return ::CompareStringOrdinal(path.extension().c_str(), -1, extension, -1, TRUE) == CSTR_EQUAL;
}

void CheckEntry(const fs::directory_entry& entry, const wchar_t* extension, std::vector<fs::path>& sources) {
    if (entry.is_regular_file() && !entry.is_symlink() && Matches(entry.path(), extension)) sources.push_back(entry.path());
}

std::vector<fs::path> Sources(const fs::path& input, bool directory, bool recursive, const wchar_t* extension) {
    std::vector<fs::path> sources;
    if (!directory) {
        if (!fs::is_regular_file(input) || !Matches(input, extension)) Fail(L"Unsupported input file: " + input.wstring());
        sources.push_back(input);
    } else if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(input)) CheckEntry(entry, extension, sources);
    } else {
        for (const auto& entry : fs::directory_iterator(input)) CheckEntry(entry, extension, sources);
    }
    std::sort(sources.begin(), sources.end(), [](const auto& a, const auto& b) { return PartNameLess{}(a.native(), b.native()); });
    if (sources.empty()) Fail(L"No matching input files in " + input.wstring());
    return sources;
}

struct Result {
    std::wstring input, output, error;
    std::vector<std::wstring> warnings;
};

std::wstring Json(std::wstring_view value) {
    constexpr wchar_t hex[] = L"0123456789abcdef";
    std::wstring escaped = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"' || ch == L'\\') { escaped += L'\\'; escaped += ch; }
        else if (ch < 32) { escaped += L"\\u00"; escaped += hex[ch >> 4]; escaped += hex[ch & 15]; }
        else escaped += ch;
    }
    return escaped + L'"';
}

std::wstring Summary(size_t total, size_t failed, size_t warnings) {
    return L"Processed " + std::to_wstring(total) + L", converted " + std::to_wstring(total - failed) +
        L", failed " + std::to_wstring(failed) + L", warnings " + std::to_wstring(warnings) + L".\r\n";
}

void Console(const std::wstring& text, FILE* output) {
    const auto handle = ::GetStdHandle(output == stdout ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
    DWORD mode = 0, written = 0;
    if (::GetConsoleMode(handle, &mode)) ::WriteConsoleW(handle, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
    else {
        const auto bytes = Utf8(text);
        std::fwrite(bytes.data(), 1, bytes.size(), output);
    }
}

void Reports(const fs::path& base, const std::vector<Result>& results, size_t failures, size_t warnings) {
    std::wstring text = Summary(results.size(), failures, warnings);
    std::wstring json = L"{\"converted\":" + std::to_wstring(results.size() - failures) + L",\"failed\":" + std::to_wstring(failures) +
        L",\"warnings\":" + std::to_wstring(warnings) + L",\"files\":[";
    bool next = false;
    for (const auto& result : results) {
        text += result.input + L" -> " + result.output + L"\r\n";
        if (!result.error.empty()) text += L"  ERROR " + result.error + L"\r\n";
        if (next) json += L',';
        next = true;
        json += L"{\"input\":" + Json(result.input) + L",\"output\":" + Json(result.output) +
            L",\"status\":" + Json(result.error.empty() ? L"converted" : L"failed") + L",\"error\":" + Json(result.error) + L",\"warnings\":[";
        bool more = false;
        for (const auto& warning : result.warnings) {
            text += L"  WARNING " + warning + L"\r\n";
            if (more) json += L',';
            more = true;
            json += Json(warning);
        }
        json += L"]}";
    }
    json += L"]}\r\n";
    const auto textPath = fs::path(base.wstring() + L".txt"), jsonPath = fs::path(base.wstring() + L".json");
    if (fs::exists(textPath) || fs::exists(jsonPath)) Fail(L"Report already exists: " + base.wstring());
    PendingFile textFile(textPath), jsonFile(jsonPath);
    WriteUtf8(textFile.Path(), text);
    WriteUtf8(jsonFile.Path(), json);
    textFile.Publish(textPath);
    jsonFile.Publish(jsonPath);
}

fs::path RuntimeDirectory() {
    std::wstring path(32768, L'\0');
    const auto length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) Fail(L"Cannot locate the batch runtime directory.");
    path.resize(length);
    return fs::path(path).parent_path();
}

int Execute(const Options& options, const wchar_t* inputExtension, const wchar_t* outputExtension, BatchConvert convert) {
    const auto input = fs::absolute(options.input).lexically_normal();
    const auto output = fs::absolute(options.output).lexically_normal();
    const bool directory = fs::is_directory(input);
    const auto sources = Sources(input, directory, options.recursive, inputExtension);
    const auto runtime = RuntimeDirectory();
    std::vector<Result> results;
    size_t failures = 0, warnings = 0;
    for (const auto& source : sources) {
        auto relative = directory ? source.lexically_relative(input) : source.filename();
        const auto destination = output / relative.replace_extension(outputExtension);
        Result result{source.wstring(), destination.wstring(), {}, {}};
        try {
            if (fs::exists(destination)) Fail(L"Output already exists: " + destination.wstring());
            fs::create_directories(destination.parent_path());
            PendingFile pending(destination);
            result.warnings = convert(source, pending.Path(), runtime);
            pending.Publish(destination);
        } catch (const Error& error) { result.error = error.message; }
        catch (const ATL::CAtlException& error) { result.error = L"COM failure " + std::to_wstring(error.m_hr); }
        catch (const fs::filesystem_error& error) { result.error = L"File system error " + std::to_wstring(error.code().value()); }
        catch (const std::bad_alloc&) { result.error = L"Insufficient memory."; }
        catch (const std::exception&) { result.error = L"Cannot convert this input file."; }
        if (!result.error.empty()) {
            ++failures;
            Console(result.input + L"\r\n  ERROR " + result.error + L"\r\n", stderr);
        } else Console(result.input + L" -> " + result.output + L"\r\n", stdout);
        for (const auto& warning : result.warnings) Console(L"  WARNING " + warning + L"\r\n", stdout);
        warnings += result.warnings.size();
        results.push_back(std::move(result));
    }
    Console(Summary(results.size(), failures, warnings), stdout);
    if (!options.report.empty()) Reports(fs::absolute(options.report), results, failures, warnings);
    return failures ? 1 : 0;
}

}

std::string Utf8(const std::wstring& text) {
    if (text.empty()) return {};
    if (text.size() > INT_MAX) Fail(L"Text exceeds the supported size.");
    const int length = static_cast<int>(text.size());
    const auto bytes = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) Fail(L"Cannot encode text as UTF-8.");
    std::string utf8(static_cast<size_t>(bytes), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length, utf8.data(), bytes, nullptr, nullptr) != bytes)
        Fail(L"Cannot encode text as UTF-8.");
    return utf8;
}

void WriteUtf8(const fs::path& destination, const std::wstring& text) {
    const auto bytes = Utf8(text);
    std::ofstream output(destination, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) Fail(L"Cannot write " + destination.wstring());
}

int RunBatch(int argc, wchar_t** argv, const wchar_t* inputExtension, const wchar_t* outputExtension, BatchConvert convert) {
    const auto usage = L"Usage: " + fs::path(argv[0]).filename().wstring() + L" FILE|DIRECTORY --output DIRECTORY [--recursive] [--report PATH_WITHOUT_EXTENSION]\r\n";
    if (argc == 2 && (std::wstring_view(argv[1]) == L"--help" || std::wstring_view(argv[1]) == L"-h")) {
        Console(usage, stdout);
        return 0;
    }
    const auto hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 2;
    struct Apartment { ~Apartment() { ::CoUninitialize(); } } apartment;
    try { return Execute(Parse(argc, argv), inputExtension, outputExtension, convert); }
    catch (const Error& error) { Console(L"ERROR " + error.message + L"\r\n" + usage, stderr); }
    catch (const fs::filesystem_error& error) { Console(L"File system error " + std::to_wstring(error.code().value()) + L".\r\n", stderr); }
    catch (...) { Console(L"Cannot complete batch conversion.\r\n", stderr); }
    return 2;
}

}
