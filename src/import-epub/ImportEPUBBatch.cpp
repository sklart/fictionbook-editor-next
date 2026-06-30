#include "stdafx.h"
#include "EpubImport.h"

#include <stdio.h>
#include <stdarg.h>
#include <vector>
#include <set>
#include <string>

namespace
{
    void ConsoleWrite(const CStringW& text)
    {
        if (text.IsEmpty())
            return;

        HANDLE h = ::GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (h != INVALID_HANDLE_VALUE && h != nullptr && ::GetConsoleMode(h, &mode))
        {
            DWORD written = 0;
            ::WriteConsoleW(h, text.GetString(), static_cast<DWORD>(text.GetLength()), &written, nullptr);
            return;
        }

        int size = ::WideCharToMultiByte(CP_UTF8, 0, text.GetString(), text.GetLength(), nullptr, 0, nullptr, nullptr);
        if (size <= 0)
            return;

        std::vector<char> bytes(static_cast<size_t>(size));
        ::WideCharToMultiByte(CP_UTF8, 0, text.GetString(), text.GetLength(), &bytes[0], size, nullptr, nullptr);
        fwrite(&bytes[0], 1, bytes.size(), stdout);
    }

    void ConsolePrintf(const wchar_t* format, ...)
    {
        va_list args;
        va_start(args, format);
        CStringW text;
        text.FormatV(format, args);
        va_end(args);
        ConsoleWrite(text);
    }

    void InitializeConsoleOutput()
    {
        // WriteConsoleW is used for real consoles, so Russian text is displayed
        // independently of the active OEM code page. UTF-8 is still selected as a
        // useful fallback for redirected output and modern Windows terminals.
        ::SetConsoleOutputCP(CP_UTF8);
        ::SetConsoleCP(CP_UTF8);
    }

    struct ConsoleOptions
    {
        bool batchMode;
        bool recursive;
        bool overwrite;
        bool writeReport;
        bool dryRun;
        bool preserveTree;
        bool stats;
        bool quiet;
        bool failFast;
        bool skipExisting;
        bool flushReportEach;
        bool isolateCrashes;
        int maxFiles;
        CStringW reportPath;
        CStringW childReportPath;
        EpubImportOptions importOptions;

        ConsoleOptions()
            : batchMode(false)
            , recursive(false)
            , overwrite(false)
            , writeReport(true)
            , dryRun(false)
            , preserveTree(false)
            , stats(false)
            , quiet(false)
            , failFast(false)
            , skipExisting(false)
            , flushReportEach(false)
            , isolateCrashes(false)
            , maxFiles(0)
        {
            importOptions.saveFb2Copy = false;
            importOptions.writeImportLog = false;
        }
    };

    void PrintUsage()
    {
        ConsolePrintf(L"ImportEPUBBatch.exe input.epub output.fb2 [options]\n");
        ConsolePrintf(L"ImportEPUBBatch.exe --batch input_folder output_folder [options]\n");
        ConsolePrintf(L"ImportEPUBBatch.exe --help\n");
        ConsolePrintf(L"ImportEPUBBatch.exe --version\n");
        ConsolePrintf(L"\n");
        ConsolePrintf(L"Common options:\n");
        ConsolePrintf(L"  --log                Write .ImportEPUB.log near source EPUB\n");
        ConsolePrintf(L"  --no-images          Do not import images\n");
        ConsolePrintf(L"  --no-cover           Do not import cover\n");
        ConsolePrintf(L"  --no-notes           Do not import notes\n");
        ConsolePrintf(L"  --no-tables          Do not import tables\n");
        ConsolePrintf(L"  --no-lists           Do not import lists\n");
        ConsolePrintf(L"  --no-links           Do not preserve links and anchors\n");
        ConsolePrintf(L"  --no-validate        Skip final FB2 XML/reference validation\n");
        ConsolePrintf(L"  --save-on-warning    Save .failed-import.fb2 when warnings are found\n");
        ConsolePrintf(L"  --dry-run            Convert in memory but do not write output FB2\n");
        ConsolePrintf(L"  --stats              Print generated FB2 statistics and add them to CSV report\n");
        ConsolePrintf(L"  --quiet              Reduce console output in batch mode\n");
        ConsolePrintf(L"  --report <file.csv>  Write batch CSV report to a custom path; HTML report is written next to it\n");
        ConsolePrintf(L"  --flush-report-each Write CSV/HTML report after every processed file for long tests\n");
        ConsolePrintf(L"  --isolate-crashes    Batch mode: run each EPUB in a child process so a crashing file is reported and the batch continues\n");
        ConsolePrintf(L"  --profile <name>      Import profile: full, text, minimal\n");
        ConsolePrintf(L"  --svg <mode>          SVG mode: keep, png, jpg, skip. PNG/JPG use optional ImportEPUBLunaSVG.dll, otherwise placeholder\n");
        ConsolePrintf(L"\n");
        ConsolePrintf(L"Import feature toggles:\n");
        ConsolePrintf(L"  --images / --no-images\n");
        ConsolePrintf(L"  --cover / --no-cover\n");
        ConsolePrintf(L"  --notes / --no-notes\n");
        ConsolePrintf(L"  --tables / --no-tables\n");
        ConsolePrintf(L"  --lists / --no-lists\n");
        ConsolePrintf(L"  --links / --no-links\n");
        ConsolePrintf(L"  --subtitles / --no-subtitles\n");
        ConsolePrintf(L"  --poems / --no-poems\n");
        ConsolePrintf(L"  --pagebreaks / --no-pagebreaks\n");
        ConsolePrintf(L"  --split-sections / --no-split-sections\n");
        ConsolePrintf(L"  --css-semantic / --no-css-semantic\n");
        ConsolePrintf(L"  --clean-typography / --no-clean-typography\n");
        ConsolePrintf(L"  --skip-hidden / --include-hidden\n");
        ConsolePrintf(L"  --skip-service-pages / --keep-service-pages\n");
        ConsolePrintf(L"  --remove-service-sections / --keep-service-sections\n");
        ConsolePrintf(L"  --remove-footnote-backlinks / --keep-footnote-backlinks\n");
        ConsolePrintf(L"  --diagnostic-section\n");
        ConsolePrintf(L"  --save-copy\n");
        ConsolePrintf(L"  --keep-temp-on-error\n");
        ConsolePrintf(L"\n");
        ConsolePrintf(L"Batch-only options:\n");
        ConsolePrintf(L"  --recursive          Search EPUB files recursively\n");
        ConsolePrintf(L"  --overwrite          Overwrite existing output FB2 files\n");
        ConsolePrintf(L"  --preserve-tree      Preserve input subfolder structure in output folder\n");
        ConsolePrintf(L"  --no-report          Do not write ImportEPUBBatch_report.csv\n");
        ConsolePrintf(L"  --skip-existing      Skip files whose default output FB2 already exists\n");
        ConsolePrintf(L"  --fail-fast          Stop batch conversion after the first failed EPUB\n");
        ConsolePrintf(L"  --max-files <N>      Process only first N EPUB files; useful for smoke tests\n");
    }

    void PrintVersion()
    {
        ConsolePrintf(L"ImportEPUBBatch / ImportEPUB 1.0.71.0 crash-isolated batch build\n");
        ConsolePrintf(L"FictionBook Editor EPUB import plugin console converter\n");
    }

    void ResetDefaultConsoleImportOptions(ConsoleOptions& options)
    {
        options.importOptions = EpubImportOptions();
        options.importOptions.saveFb2Copy = false;
        options.importOptions.writeImportLog = false;
    }

    bool ApplyProfile(ConsoleOptions& options, const CStringW& profileName)
    {
        CStringW name(profileName);
        name.MakeLower();

        ResetDefaultConsoleImportOptions(options);

        if (name == L"full" || name == L"default")
            return true;

        if (name == L"text")
        {
            options.importOptions.importCover = false;
            options.importOptions.importImages = false;
            options.importOptions.importTables = false;
            options.importOptions.importPageBreaks = false;
            options.importOptions.preserveLinks = true;
            options.importOptions.importNotes = true;
            options.importOptions.importLists = true;
            return true;
        }

        if (name == L"minimal")
        {
            options.importOptions.importCover = false;
            options.importOptions.importImages = false;
            options.importOptions.importNotes = false;
            options.importOptions.importTables = false;
            options.importOptions.importLists = false;
            options.importOptions.importPoemsEpigraphs = false;
            options.importOptions.importSubtitles = false;
            options.importOptions.preserveLinks = false;
            options.importOptions.importPageBreaks = false;
            options.importOptions.useCssSemanticClasses = false;
            options.importOptions.addDiagnosticSection = false;
            return true;
        }

        return false;
    }

    bool WriteUtf8ConsoleFile(const CStringW& path, const CStringW& text)
    {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.GetString(), text.GetLength(), nullptr, 0, nullptr, nullptr);
        if (size < 0)
            return false;

        std::vector<char> bytes;
        bytes.push_back(static_cast<char>(0xEF));
        bytes.push_back(static_cast<char>(0xBB));
        bytes.push_back(static_cast<char>(0xBF));

        if (size > 0)
        {
            const size_t oldSize = bytes.size();
            bytes.resize(oldSize + static_cast<size_t>(size));
            WideCharToMultiByte(CP_UTF8, 0, text.GetString(), text.GetLength(), &bytes[oldSize], size, nullptr, nullptr);
        }

        HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        DWORD written = 0;
        const DWORD toWrite = static_cast<DWORD>(bytes.size());
        BOOL ok = WriteFile(h, bytes.empty() ? nullptr : &bytes[0], toWrite, &written, nullptr);
        CloseHandle(h);
        return ok && written == toWrite;
    }

    bool FileExistsLocal(const CStringW& path)
    {
        DWORD attrs = GetFileAttributesW(path);
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool DirectoryExistsLocal(const CStringW& path)
    {
        DWORD attrs = GetFileAttributesW(path);
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool CreateDirectoryRecursiveLocal(const CStringW& path)
    {
        if (path.IsEmpty())
            return false;
        if (DirectoryExistsLocal(path))
            return true;

        CStringW parent(path);
        parent.TrimRight(L"\\/");
        int slash1 = parent.ReverseFind(L'\\');
        int slash2 = parent.ReverseFind(L'/');
        int slash = max(slash1, slash2);
        if (slash > 0)
        {
            CStringW parentDir = parent.Left(slash);
            if (!parentDir.IsEmpty() && !DirectoryExistsLocal(parentDir))
                CreateDirectoryRecursiveLocal(parentDir);
        }

        if (CreateDirectoryW(path, nullptr))
            return true;
        return GetLastError() == ERROR_ALREADY_EXISTS;
    }

    CStringW DirectoryOfLocal(const CStringW& path)
    {
        int slash1 = path.ReverseFind(L'\\');
        int slash2 = path.ReverseFind(L'/');
        int slash = max(slash1, slash2);
        if (slash < 0)
            return CStringW();
        return path.Left(slash);
    }

    CStringW FileStemLocal(const CStringW& path)
    {
        int slash1 = path.ReverseFind(L'\\');
        int slash2 = path.ReverseFind(L'/');
        int slash = max(slash1, slash2);
        CStringW name = slash >= 0 ? path.Mid(slash + 1) : path;
        int dot = name.ReverseFind(L'.');
        if (dot > 0)
            name = name.Left(dot);
        return name;
    }

    CStringW SanitizeFileName(const CStringW& value)
    {
        CStringW out(value);
        out.Trim();
        if (out.IsEmpty())
            out = L"book";

        for (int i = 0; i < out.GetLength(); ++i)
        {
            wchar_t ch = out[i];
            if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' || ch == L'/' ||
                ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*' || ch < 32)
            {
                out.SetAt(i, L'_');
            }
        }
        out.TrimRight(L" .");
        if (out.IsEmpty())
            out = L"book";
        return out;
    }

    CStringW CombinePathLocal(const CStringW& dir, const CStringW& name)
    {
        CStringW out(dir);
        if (!out.IsEmpty() && out.Right(1) != L"\\" && out.Right(1) != L"/")
            out += L"\\";
        out += name;
        return out;
    }

    CStringW CsvEscape(const CStringW& value)
    {
        CStringW s(value);
        s.Replace(L"\"", L"\"\"");
        return L"\"" + s + L"\"";
    }


    CStringW BatchCsvHeader(bool stats)
    {
        return stats
            ? CStringW(L"Source;Output;Status;ElapsedMs;Sections;Paragraphs;Images;Binaries;Tables;NoteLinks;Chars;MissingImageBinaries;MissingInternalLinks;MissingNoteTargets;DuplicateIds;ExternalLinks;AnchorsImported;HrefsResolved;HrefsUnresolved;NoteRefsDetected;NoteRefsResolved;SvgImages;SvgConverted;SvgPlaceholders;SvgSkipped;SvgBackend;Warnings;Message\r\n")
            : CStringW(L"Source;Output;Status;ElapsedMs;Message\r\n");
    }

    CStringW QuoteCommandArg(const CStringW& value)
    {
        CStringW out(L"\"");
        int slashCount = 0;
        for (int i = 0; i < value.GetLength(); ++i)
        {
            wchar_t ch = value[i];
            if (ch == L'\\')
            {
                ++slashCount;
                continue;
            }
            if (ch == L'\"')
            {
                for (int n = 0; n < slashCount * 2 + 1; ++n)
                    out += L'\\';
                out += L'\"';
                slashCount = 0;
                continue;
            }
            for (int n = 0; n < slashCount; ++n)
                out += L'\\';
            slashCount = 0;
            out += ch;
        }
        for (int n = 0; n < slashCount * 2; ++n)
            out += L'\\';
        out += L"\"";
        return out;
    }

    void AppendCommandArg(CStringW& commandLine, const CStringW& arg)
    {
        if (!commandLine.IsEmpty())
            commandLine += L" ";
        commandLine += QuoteCommandArg(arg);
    }

    CStringW CurrentExePath()
    {
        std::vector<wchar_t> buffer(MAX_PATH);
        DWORD len = 0;
        for (;;)
        {
            len = GetModuleFileNameW(nullptr, buffer.empty() ? nullptr : &buffer[0], static_cast<DWORD>(buffer.size()));
            if (len == 0)
                return L"ImportEPUBBatch.exe";
            if (len < buffer.size() - 1)
                break;
            buffer.resize(buffer.size() * 2);
        }
        return CStringW(&buffer[0], static_cast<int>(len));
    }

    bool ReadUtf8ConsoleFile(const CStringW& path, CStringW& text)
    {
        text.Empty();
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER liSize = {};
        if (!GetFileSizeEx(h, &liSize) || liSize.QuadPart < 0 || liSize.QuadPart > 64 * 1024 * 1024)
        {
            CloseHandle(h);
            return false;
        }

        std::vector<char> bytes(static_cast<size_t>(liSize.QuadPart));
        DWORD read = 0;
        BOOL ok = TRUE;
        if (!bytes.empty())
            ok = ReadFile(h, &bytes[0], static_cast<DWORD>(bytes.size()), &read, nullptr);
        CloseHandle(h);
        if (!ok || read != bytes.size())
            return false;

        const char* data = bytes.empty() ? "" : &bytes[0];
        int size = static_cast<int>(bytes.size());
        if (size >= 3 && static_cast<unsigned char>(data[0]) == 0xEF && static_cast<unsigned char>(data[1]) == 0xBB && static_cast<unsigned char>(data[2]) == 0xBF)
        {
            data += 3;
            size -= 3;
        }

        int wsize = MultiByteToWideChar(CP_UTF8, 0, data, size, nullptr, 0);
        if (wsize < 0)
            return false;
        if (wsize == 0)
            return true;
        std::vector<wchar_t> wide(static_cast<size_t>(wsize) + 1, 0);
        if (MultiByteToWideChar(CP_UTF8, 0, data, size, &wide[0], wsize) != wsize)
            return false;
        text = CStringW(&wide[0], wsize);
        return true;
    }

    std::vector<CStringW> SplitCsvLine(const CStringW& line)
    {
        std::vector<CStringW> fields;
        CStringW field;
        bool inQuotes = false;
        for (int i = 0; i < line.GetLength(); ++i)
        {
            wchar_t ch = line[i];
            if (ch == L'\"')
            {
                if (inQuotes && i + 1 < line.GetLength() && line[i + 1] == L'\"')
                {
                    field += L'\"';
                    ++i;
                }
                else
                {
                    inQuotes = !inQuotes;
                }
            }
            else if (ch == L';' && !inQuotes)
            {
                fields.push_back(field);
                field.Empty();
            }
            else
            {
                field += ch;
            }
        }
        fields.push_back(field);
        return fields;
    }

    CStringW FirstDataLineFromCsvText(const CStringW& csvText)
    {
        int start = 0;
        int lineNo = 0;
        while (start < csvText.GetLength())
        {
            int end = csvText.Find(L"\n", start);
            if (end < 0)
                end = csvText.GetLength();
            CStringW line = csvText.Mid(start, end - start);
            line.TrimRight(L"\r\n");
            if (!line.IsEmpty())
            {
                ++lineNo;
                if (lineNo == 2)
                    return line;
            }
            start = end + 1;
        }
        return L"";
    }

    CStringW ExitCodeText(DWORD exitCode)
    {
        CStringW text;
        text.Format(L"%lu / 0x%08lX", exitCode, exitCode);
        return text;
    }

    CStringW HtmlEscape(const CStringW& value)
    {
        CStringW s(value);
        s.Replace(L"&", L"&amp;");
        s.Replace(L"<", L"&lt;");
        s.Replace(L">", L"&gt;");
        s.Replace(L"\"", L"&quot;");
        return s;
    }

    bool IsEpubFileName(const CStringW& name)
    {
        return name.GetLength() > 5 && name.Right(5).CompareNoCase(L".epub") == 0;
    }

    void CollectEpubFiles(const CStringW& folder, bool recursive, std::vector<CStringW>& files)
    {
        CStringW mask = CombinePathLocal(folder, L"*");
        WIN32_FIND_DATAW fd = {};
        HANDLE h = FindFirstFileW(mask, &fd);
        if (h == INVALID_HANDLE_VALUE)
            return;

        do
        {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;

            CStringW full = CombinePathLocal(folder, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (recursive && (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
                    CollectEpubFiles(full, recursive, files);
            }
            else if (IsEpubFileName(fd.cFileName))
            {
                files.push_back(full);
            }
        }
        while (FindNextFileW(h, &fd));

        FindClose(h);
    }

    CStringW MakeUniqueOutputPath(const CStringW& outputDir, const CStringW& sourceEpub, bool overwrite)
    {
        CStringW base = SanitizeFileName(FileStemLocal(sourceEpub));
        CStringW path = CombinePathLocal(outputDir, base + L".fb2");
        if (overwrite || !FileExistsLocal(path))
            return path;

        for (int i = 2; i < 10000; ++i)
        {
            CStringW suffix;
            suffix.Format(L" (%d).fb2", i);
            CStringW candidate = CombinePathLocal(outputDir, base + suffix);
            if (!FileExistsLocal(candidate))
                return candidate;
        }

        return path;
    }

    bool StartsWithPathNoCase(const CStringW& path, const CStringW& prefix)
    {
        CStringW p(path);
        CStringW pref(prefix);
        p.TrimRight(L"\\/");
        pref.TrimRight(L"\\/");
        if (p.GetLength() < pref.GetLength())
            return false;
        if (p.Left(pref.GetLength()).CompareNoCase(pref) != 0)
            return false;
        if (p.GetLength() == pref.GetLength())
            return true;
        wchar_t next = p[pref.GetLength()];
        return next == L'\\' || next == L'/';
    }

    CStringW RelativePathFromBase(const CStringW& baseDir, const CStringW& fullPath)
    {
        if (!StartsWithPathNoCase(fullPath, baseDir))
            return FileStemLocal(fullPath) + L".fb2";
        CStringW rel = fullPath.Mid(baseDir.GetLength());
        rel.TrimLeft(L"\\/");
        int dot = rel.ReverseFind(L'.');
        int slash1 = rel.ReverseFind(L'\\');
        int slash2 = rel.ReverseFind(L'/');
        if (dot > max(slash1, slash2))
            rel = rel.Left(dot);
        rel += L".fb2";
        return rel;
    }

    CStringW MakeBatchOutputPath(const CStringW& inputDir, const CStringW& outputDir, const CStringW& sourceEpub, bool preserveTree, bool overwrite)
    {
        if (!preserveTree)
            return MakeUniqueOutputPath(outputDir, sourceEpub, overwrite);

        CStringW rel = RelativePathFromBase(inputDir, sourceEpub);
        CStringW relDir = DirectoryOfLocal(rel);
        CStringW baseName = FileStemLocal(rel);
        CStringW targetDir = relDir.IsEmpty() ? outputDir : CombinePathLocal(outputDir, relDir);
        CreateDirectoryRecursiveLocal(targetDir);

        CStringW path = CombinePathLocal(targetDir, SanitizeFileName(baseName) + L".fb2");
        if (overwrite || !FileExistsLocal(path))
            return path;
        for (int i = 2; i < 10000; ++i)
        {
            CStringW suffix;
            suffix.Format(L" (%d).fb2", i);
            CStringW candidate = CombinePathLocal(targetDir, SanitizeFileName(baseName) + suffix);
            if (!FileExistsLocal(candidate))
                return candidate;
        }
        return path;
    }

    CStringW MakeDefaultBatchOutputPath(const CStringW& inputDir, const CStringW& outputDir, const CStringW& sourceEpub, bool preserveTree)
    {
        if (!preserveTree)
            return CombinePathLocal(outputDir, SanitizeFileName(FileStemLocal(sourceEpub)) + L".fb2");

        CStringW rel = RelativePathFromBase(inputDir, sourceEpub);
        CStringW relDir = DirectoryOfLocal(rel);
        CStringW baseName = FileStemLocal(rel);
        CStringW targetDir = relDir.IsEmpty() ? outputDir : CombinePathLocal(outputDir, relDir);
        return CombinePathLocal(targetDir, SanitizeFileName(baseName) + L".fb2");
    }

    int CountOccurrences(const CStringW& text, const CStringW& token)
    {
        if (token.IsEmpty())
            return 0;
        int count = 0;
        int pos = 0;
        while ((pos = text.Find(token, pos)) >= 0)
        {
            ++count;
            pos += token.GetLength();
        }
        return count;
    }

    struct Fb2QualityStats
    {
        int sections;
        int paragraphs;
        int images;
        int binaries;
        int tables;
        int noteLinks;
        int chars;
        int missingImageBinaries;
        int missingInternalLinks;
        int missingNoteTargets;
        int duplicateIds;
        int externalLinks;
        int anchorsImported;
        int hrefsResolved;
        int hrefsUnresolved;
        int noteRefsDetected;
        int noteRefsResolved;
        int svgImages;
        int svgConverted;
        int svgPlaceholders;
        int svgSkipped;
        CStringW svgBackend;

        Fb2QualityStats()
            : sections(0)
            , paragraphs(0)
            , images(0)
            , binaries(0)
            , tables(0)
            , noteLinks(0)
            , chars(0)
            , missingImageBinaries(0)
            , missingInternalLinks(0)
            , missingNoteTargets(0)
            , duplicateIds(0)
            , externalLinks(0)
            , anchorsImported(0)
            , hrefsResolved(0)
            , hrefsUnresolved(0)
            , noteRefsDetected(0)
            , noteRefsResolved(0)
            , svgImages(0)
            , svgConverted(0)
            , svgPlaceholders(0)
            , svgSkipped(0)
        {
        }

        int WarningCount() const
        {
            return missingImageBinaries + missingInternalLinks + missingNoteTargets + duplicateIds;
        }
    };

    CStringW IntText(int value)
    {
        CStringW text;
        text.Format(L"%d", value);
        return text;
    }

    CStringW HtmlClassForStatus(const CStringW& status)
    {
        if (status.Find(L"FAILED") >= 0)
            return L"failed";
        if (status.Find(L"WARNING") >= 0)
            return L"warn";
        if (status.Find(L"SKIPPED") >= 0)
            return L"skipped";
        return L"ok";
    }

    CStringW BuildHtmlReportStart(const CStringW& inputDir, const CStringW& outputDir)
    {
        CStringW html;
        html += L"<!doctype html>\r\n<html lang=\"ru\">\r\n<head>\r\n";
        html += L"<meta charset=\"utf-8\">\r\n";
        html += L"<title>ImportEPUB batch report</title>\r\n";
        html += L"<style>\r\n";
        html += L"body{font-family:Segoe UI,Arial,sans-serif;margin:20px;background:#fff;color:#222}\r\n";
        html += L"h1{font-size:22px;margin:0 0 12px 0}.meta{margin:8px 0 16px 0;color:#555}\r\n";
        html += L"table{border-collapse:collapse;width:100%;font-size:13px}th,td{border:1px solid #ddd;padding:4px 6px;vertical-align:top}\r\n";
        html += L"th{background:#f3f3f3;position:sticky;top:0;z-index:2}.path{font-family:Consolas,monospace;font-size:12px;word-break:break-all}\r\n";
        html += L"tr.ok{background:#f7fff7}tr.warn{background:#fffbe6}tr.failed{background:#fff0f0}tr.skipped{background:#f5f5f5;color:#666}\r\n";
        html += L".status-ok{color:#0a7a0a;font-weight:600}.status-warn{color:#9a6a00;font-weight:600}.status-failed{color:#b00020;font-weight:600}.num{text-align:right;font-family:Consolas,monospace}\r\n";
        html += L"</style>\r\n</head>\r\n<body>\r\n";
        html += L"<h1>ImportEPUB batch report</h1>\r\n";
        html += L"<div class=\"meta\">Input: <span class=\"path\">" + HtmlEscape(inputDir) + L"</span><br>Output: <span class=\"path\">" + HtmlEscape(outputDir) + L"</span></div>\r\n";
        html += L"<table>\r\n<thead><tr>";
        html += L"<th>#</th><th>Source</th><th>Output</th><th>Status</th><th>Elapsed, ms</th>";
        html += L"<th>Sections</th><th>Paragraphs</th><th>Images</th><th>Binaries</th><th>Tables</th><th>NoteLinks</th><th>Chars</th>";
        html += L"<th>Missing image binaries</th><th>Missing internal links</th><th>Missing note targets</th><th>Duplicate ids</th><th>External links</th>";
        html += L"<th>Anchors imported</th><th>Hrefs resolved</th><th>Hrefs unresolved</th><th>Note refs detected</th><th>Note refs resolved</th>";
        html += L"<th>SVG images</th><th>SVG converted</th><th>SVG placeholders</th><th>SVG skipped</th><th>SVG backend</th><th>Warnings</th><th>Message</th>";
        html += L"</tr></thead>\r\n<tbody>\r\n";
        return html;
    }

    CStringW HtmlTdNum(int value)
    {
        return L"<td class=\"num\">" + IntText(value) + L"</td>";
    }

    CStringW BuildHtmlReportRow(unsigned index, const CStringW& source, const CStringW& output, const CStringW& status, const CStringW& elapsedText, bool hasStats, const Fb2QualityStats& q, const CStringW& message)
    {
        CStringW html;
        CStringW cls = HtmlClassForStatus(status);
        html.Format(L"<tr class=\"%s\"><td class=\"num\">%u</td>", cls.GetString(), index);
        html += L"<td class=\"path\">" + HtmlEscape(source) + L"</td>";
        html += L"<td class=\"path\">" + HtmlEscape(output) + L"</td>";
        CStringW statusClass = L"status-ok";
        if (cls == L"warn")
            statusClass = L"status-warn";
        else if (cls == L"failed")
            statusClass = L"status-failed";
        html += L"<td class=\"" + statusClass + L"\">" + HtmlEscape(status) + L"</td>";
        html += L"<td class=\"num\">" + HtmlEscape(elapsedText) + L"</td>";
        if (hasStats)
        {
            html += HtmlTdNum(q.sections);
            html += HtmlTdNum(q.paragraphs);
            html += HtmlTdNum(q.images);
            html += HtmlTdNum(q.binaries);
            html += HtmlTdNum(q.tables);
            html += HtmlTdNum(q.noteLinks);
            html += HtmlTdNum(q.chars);
            html += HtmlTdNum(q.missingImageBinaries);
            html += HtmlTdNum(q.missingInternalLinks);
            html += HtmlTdNum(q.missingNoteTargets);
            html += HtmlTdNum(q.duplicateIds);
            html += HtmlTdNum(q.externalLinks);
            html += HtmlTdNum(q.anchorsImported);
            html += HtmlTdNum(q.hrefsResolved);
            html += HtmlTdNum(q.hrefsUnresolved);
            html += HtmlTdNum(q.noteRefsDetected);
            html += HtmlTdNum(q.noteRefsResolved);
            html += HtmlTdNum(q.svgImages);
            html += HtmlTdNum(q.svgConverted);
            html += HtmlTdNum(q.svgPlaceholders);
            html += HtmlTdNum(q.svgSkipped);
            html += L"<td>" + HtmlEscape(q.svgBackend) + L"</td>";
            html += HtmlTdNum(q.WarningCount());
        }
        else
        {
            html += L"<td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td>";
        }
        html += L"<td>" + HtmlEscape(message) + L"</td></tr>\r\n";
        return html;
    }

    CStringW BuildHtmlReportFinish(int okCount, int warningCount, int failCount, int skipCount)
    {
        CStringW html;
        html += L"</tbody></table>\r\n";
        CStringW summary;
        summary.Format(L"<p><b>Summary:</b> OK: %d, OK_WITH_WARNINGS: %d, FAILED: %d, SKIPPED: %d</p>\r\n", okCount, warningCount, failCount, skipCount);
        html += summary;
        html += L"</body>\r\n</html>\r\n";
        return html;
    }

    CStringW MakeHtmlReportPath(const CStringW& reportPath)
    {
        CStringW out(reportPath);
        int slash1 = out.ReverseFind(L'\\');
        int slash2 = out.ReverseFind(L'/');
        int dot = out.ReverseFind(L'.');
        if (dot > max(slash1, slash2))
            out = out.Left(dot);
        out += L".html";
        return out;
    }

    void WriteBatchReportsSnapshot(const CStringW& reportPath, const CStringW& htmlPath, const CStringW& csvReport, const CStringW& htmlBody, int okCount, int warningCount, int failCount, int skipCount)
    {
        if (reportPath.IsEmpty())
            return;
        CStringW reportDir = DirectoryOfLocal(reportPath);
        if (!reportDir.IsEmpty())
            CreateDirectoryRecursiveLocal(reportDir);
        WriteUtf8ConsoleFile(reportPath, csvReport);
        if (!htmlPath.IsEmpty())
            WriteUtf8ConsoleFile(htmlPath, htmlBody + BuildHtmlReportFinish(okCount - warningCount, warningCount, failCount, skipCount));
    }

    void CollectAttributeValuesInXml(const CStringW& xml, const CStringW& token, std::set<std::wstring>& values)
    {
        int pos = 0;
        while ((pos = xml.Find(token, pos)) >= 0)
        {
            pos += token.GetLength();
            int end = xml.Find(L"\"", pos);
            if (end < 0)
                break;
            CStringW value = xml.Mid(pos, end - pos);
            if (!value.IsEmpty())
                values.emplace(value.GetString());
            pos = end + 1;
        }
    }

    int FindPreviousCharLocal(const CStringW& text, wchar_t ch, int beforePos)
    {
        int pos = min(beforePos, text.GetLength() - 1);
        for (int i = pos; i >= 0; --i)
        {
            if (text[i] == ch)
                return i;
        }
        return -1;
    }

    bool TagContainsToken(const CStringW& xml, int tagStart, int tagEnd, const CStringW& token)
    {
        if (tagStart < 0 || tagEnd <= tagStart)
            return false;
        CStringW tag = xml.Mid(tagStart, tagEnd - tagStart + 1);
        return tag.Find(token) >= 0;
    }

    Fb2QualityStats AnalyzeFb2Quality(const CStringW& fb2)
    {
        Fb2QualityStats q;
        q.sections = CountOccurrences(fb2, L"<section");
        q.paragraphs = CountOccurrences(fb2, L"<p>") + CountOccurrences(fb2, L"<p ");
        q.images = CountOccurrences(fb2, L"<image ");
        q.binaries = CountOccurrences(fb2, L"<binary ");
        q.tables = CountOccurrences(fb2, L"<table");
        q.noteLinks = CountOccurrences(fb2, L" type=\"note\"");
        q.chars = fb2.GetLength();

        std::set<std::wstring> binaryIds;
        std::set<std::wstring> allIds;
        std::set<std::wstring> duplicateIds;
        CollectAttributeValuesInXml(fb2, L"<binary id=\"", binaryIds);

        int idSearchPos = 0;
        while ((idSearchPos = fb2.Find(L" id=\"", idSearchPos)) >= 0)
        {
            idSearchPos += 5;
            int end = fb2.Find(L"\"", idSearchPos);
            if (end < 0)
                break;
            CStringW id = fb2.Mid(idSearchPos, end - idSearchPos);
            if (!allIds.emplace(id.GetString()).second)
                duplicateIds.emplace(id.GetString());
            idSearchPos = end + 1;
        }
        q.duplicateIds = static_cast<int>(duplicateIds.size());
        q.anchorsImported = static_cast<int>(allIds.size());

        int imagePos = 0;
        while ((imagePos = fb2.Find(L"<image ", imagePos)) >= 0)
        {
            int hrefPos = fb2.Find(L"l:href=\"#", imagePos);
            int tagEnd = fb2.Find(L">", imagePos);
            if (hrefPos >= 0 && tagEnd >= 0 && hrefPos < tagEnd)
            {
                hrefPos += 9;
                int end = fb2.Find(L"\"", hrefPos);
                if (end > hrefPos)
                {
                    CStringW id = fb2.Mid(hrefPos, end - hrefPos);
                    if (binaryIds.find(std::wstring(id.GetString())) == binaryIds.end())
                        ++q.missingImageBinaries;
                }
            }
            imagePos += 7;
        }

        CStringW lowerFb2(fb2);
        lowerFb2.MakeLower();
        int externalPos = 0;
        while ((externalPos = lowerFb2.Find(L"l:href=\"http", externalPos)) >= 0)
        {
            ++q.externalLinks;
            externalPos += 12;
        }

        int internalHrefCount = 0;
        int hrefPos = 0;
        while ((hrefPos = fb2.Find(L"l:href=\"#", hrefPos)) >= 0)
        {
            int tagStart = FindPreviousCharLocal(fb2, L'<', hrefPos);
            int tagEnd = fb2.Find(L">", hrefPos);
            int valueStart = hrefPos + 9;
            int valueEnd = fb2.Find(L"\"", valueStart);
            if (tagStart >= 0 && tagEnd > tagStart && valueEnd > valueStart)
            {
                const bool isImage = TagContainsToken(fb2, tagStart, tagEnd, L"<image");
                if (!isImage)
                {
                    ++internalHrefCount;
                    CStringW id = fb2.Mid(valueStart, valueEnd - valueStart);
                    if (allIds.find(std::wstring(id.GetString())) == allIds.end())
                    {
                        if (TagContainsToken(fb2, tagStart, tagEnd, L"type=\"note\""))
                            ++q.missingNoteTargets;
                        else
                            ++q.missingInternalLinks;
                    }
                }
            }
            hrefPos += 9;
        }

        q.hrefsUnresolved = q.missingInternalLinks;
        q.hrefsResolved = internalHrefCount - q.missingInternalLinks - q.missingNoteTargets;
        if (q.hrefsResolved < 0)
            q.hrefsResolved = 0;
        q.noteRefsDetected = q.noteLinks;
        q.noteRefsResolved = q.noteLinks - q.missingNoteTargets;
        if (q.noteRefsResolved < 0)
            q.noteRefsResolved = 0;

        return q;
    }

    CStringW BuildStatsSummary(const Fb2QualityStats& q)
    {
        CStringW stats;
        stats.Format(L"sections=%d; paragraphs=%d; images=%d; binaries=%d; tables=%d; note-links=%d; chars=%d; anchors=%d; hrefs=%d/%d resolved; svg=%d/%d converted/%d placeholders",
            q.sections, q.paragraphs, q.images, q.binaries, q.tables, q.noteLinks, q.chars,
            q.anchorsImported, q.hrefsResolved, q.hrefsResolved + q.hrefsUnresolved,
            q.svgImages, q.svgConverted, q.svgPlaceholders);
        return stats;
    }

    CStringW BuildQualityMessage(const Fb2QualityStats& q)
    {
        CStringW message;
        if (q.missingImageBinaries > 0)
            message += L"MissingImageBinaries=" + IntText(q.missingImageBinaries) + L"; ";
        if (q.missingInternalLinks > 0)
            message += L"MissingInternalLinks=" + IntText(q.missingInternalLinks) + L"; ";
        if (q.missingNoteTargets > 0)
            message += L"MissingNoteTargets=" + IntText(q.missingNoteTargets) + L"; ";
        if (q.duplicateIds > 0)
            message += L"DuplicateIds=" + IntText(q.duplicateIds) + L"; ";
        // External HTTP/HTTPS links are informational. They stay in the CSV
        // column, but do not make the result OK_WITH_WARNINGS.
        if (message.Right(2) == L"; ")
            message = message.Left(message.GetLength() - 2);
        return message;
    }

    CStringW BuildStatsCsvColumns(const Fb2QualityStats& q)
    {
        CStringW line;
        line += IntText(q.sections) + L";";
        line += IntText(q.paragraphs) + L";";
        line += IntText(q.images) + L";";
        line += IntText(q.binaries) + L";";
        line += IntText(q.tables) + L";";
        line += IntText(q.noteLinks) + L";";
        line += IntText(q.chars) + L";";
        line += IntText(q.missingImageBinaries) + L";";
        line += IntText(q.missingInternalLinks) + L";";
        line += IntText(q.missingNoteTargets) + L";";
        line += IntText(q.duplicateIds) + L";";
        line += IntText(q.externalLinks) + L";";
        line += IntText(q.anchorsImported) + L";";
        line += IntText(q.hrefsResolved) + L";";
        line += IntText(q.hrefsUnresolved) + L";";
        line += IntText(q.noteRefsDetected) + L";";
        line += IntText(q.noteRefsResolved) + L";";
        line += IntText(q.svgImages) + L";";
        line += IntText(q.svgConverted) + L";";
        line += IntText(q.svgPlaceholders) + L";";
        line += IntText(q.svgSkipped) + L";";
        line += CsvEscape(q.svgBackend) + L";";
        line += IntText(q.WarningCount());
        return line;
    }

    CStringW EmptyStatsCsvColumns()
    {
        return L";;;;;;;;;;;;;;;;;;;;;;";
    }

    bool ApplySvgMode(ConsoleOptions& options, const CStringW& modeName)
    {
        CStringW mode(modeName);
        mode.MakeLower();
        if (mode == L"keep" || mode == L"svg")
            options.importOptions.svgConversionMode = SVG_IMPORT_KEEP;
        else if (mode == L"png")
            options.importOptions.svgConversionMode = SVG_IMPORT_CONVERT_PNG;
        else if (mode == L"jpg" || mode == L"jpeg")
            options.importOptions.svgConversionMode = SVG_IMPORT_CONVERT_JPEG;
        else if (mode == L"skip" || mode == L"none")
            options.importOptions.svgConversionMode = SVG_IMPORT_SKIP;
        else
            return false;
        return true;
    }

    bool ApplyOption(ConsoleOptions& options, int& index, int argc, wchar_t** argv)
    {
        const CStringW arg(argv[index]);
        if (arg.CompareNoCase(L"--log") == 0)
            options.importOptions.writeImportLog = true;
        else if (arg.CompareNoCase(L"--no-log") == 0)
            options.importOptions.writeImportLog = false;
        else if (arg.CompareNoCase(L"--profile") == 0)
        {
            if (index + 1 >= argc)
            {
                ConsolePrintf(L"Missing profile name after --profile. Valid profiles: full, text, minimal.\n");
                return false;
            }
            ++index;
            if (!ApplyProfile(options, CStringW(argv[index])))
            {
                ConsolePrintf(L"Unknown profile: %s. Valid profiles: full, text, minimal.\n", argv[index]);
                return false;
            }
        }
        else if (arg.CompareNoCase(L"--svg") == 0)
        {
            if (index + 1 >= argc)
            {
                ConsolePrintf(L"Missing mode after --svg. Valid modes: keep, png, jpg, skip. Modes png/jpg use optional ImportEPUBLunaSVG.dll; without it a placeholder is embedded.\n");
                return false;
            }
            ++index;
            if (!ApplySvgMode(options, CStringW(argv[index])))
            {
                ConsolePrintf(L"Unknown SVG mode: %s. Valid modes: keep, png, jpg, skip. Modes png/jpg use optional ImportEPUBLunaSVG.dll; without it a placeholder is embedded.\n", argv[index]);
                return false;
            }
        }
        else if (arg.CompareNoCase(L"--images") == 0)
            options.importOptions.importImages = true;
        else if (arg.CompareNoCase(L"--no-images") == 0)
            options.importOptions.importImages = false;
        else if (arg.CompareNoCase(L"--cover") == 0)
            options.importOptions.importCover = true;
        else if (arg.CompareNoCase(L"--no-cover") == 0)
            options.importOptions.importCover = false;
        else if (arg.CompareNoCase(L"--notes") == 0)
            options.importOptions.importNotes = true;
        else if (arg.CompareNoCase(L"--no-notes") == 0)
            options.importOptions.importNotes = false;
        else if (arg.CompareNoCase(L"--tables") == 0)
            options.importOptions.importTables = true;
        else if (arg.CompareNoCase(L"--no-tables") == 0)
            options.importOptions.importTables = false;
        else if (arg.CompareNoCase(L"--lists") == 0)
            options.importOptions.importLists = true;
        else if (arg.CompareNoCase(L"--no-lists") == 0)
            options.importOptions.importLists = false;
        else if (arg.CompareNoCase(L"--links") == 0)
            options.importOptions.preserveLinks = true;
        else if (arg.CompareNoCase(L"--no-links") == 0)
            options.importOptions.preserveLinks = false;
        else if (arg.CompareNoCase(L"--subtitles") == 0)
            options.importOptions.importSubtitles = true;
        else if (arg.CompareNoCase(L"--no-subtitles") == 0)
            options.importOptions.importSubtitles = false;
        else if (arg.CompareNoCase(L"--poems") == 0)
            options.importOptions.importPoemsEpigraphs = true;
        else if (arg.CompareNoCase(L"--no-poems") == 0)
            options.importOptions.importPoemsEpigraphs = false;
        else if (arg.CompareNoCase(L"--pagebreaks") == 0)
            options.importOptions.importPageBreaks = true;
        else if (arg.CompareNoCase(L"--no-pagebreaks") == 0)
            options.importOptions.importPageBreaks = false;
        else if (arg.CompareNoCase(L"--split-sections") == 0)
            options.importOptions.splitSectionsByHeadings = true;
        else if (arg.CompareNoCase(L"--no-split-sections") == 0)
            options.importOptions.splitSectionsByHeadings = false;
        else if (arg.CompareNoCase(L"--css-semantic") == 0)
            options.importOptions.useCssSemanticClasses = true;
        else if (arg.CompareNoCase(L"--no-css-semantic") == 0)
            options.importOptions.useCssSemanticClasses = false;
        else if (arg.CompareNoCase(L"--clean-typography") == 0)
            options.importOptions.cleanTypography = true;
        else if (arg.CompareNoCase(L"--no-clean-typography") == 0)
            options.importOptions.cleanTypography = false;
        else if (arg.CompareNoCase(L"--skip-hidden") == 0)
            options.importOptions.skipHiddenElements = true;
        else if (arg.CompareNoCase(L"--include-hidden") == 0)
            options.importOptions.skipHiddenElements = false;
        else if (arg.CompareNoCase(L"--skip-service-pages") == 0)
            options.importOptions.skipServicePages = true;
        else if (arg.CompareNoCase(L"--keep-service-pages") == 0)
            options.importOptions.skipServicePages = false;
        else if (arg.CompareNoCase(L"--remove-service-sections") == 0)
            options.importOptions.removeServiceSections = true;
        else if (arg.CompareNoCase(L"--keep-service-sections") == 0)
            options.importOptions.removeServiceSections = false;
        else if (arg.CompareNoCase(L"--remove-footnote-backlinks") == 0)
            options.importOptions.removeFootnoteBacklinks = true;
        else if (arg.CompareNoCase(L"--keep-footnote-backlinks") == 0)
            options.importOptions.removeFootnoteBacklinks = false;
        else if (arg.CompareNoCase(L"--diagnostic-section") == 0)
            options.importOptions.addDiagnosticSection = true;
        else if (arg.CompareNoCase(L"--save-copy") == 0)
            options.importOptions.saveFb2Copy = true;
        else if (arg.CompareNoCase(L"--keep-temp-on-error") == 0)
            options.importOptions.keepTempOnError = true;
        else if (arg.CompareNoCase(L"--validate") == 0)
            options.importOptions.validateResult = true;
        else if (arg.CompareNoCase(L"--no-validate") == 0)
            options.importOptions.validateResult = false;
        else if (arg.CompareNoCase(L"--save-on-warning") == 0)
            options.importOptions.saveIntermediateFb2OnError = true;
        else if (arg.CompareNoCase(L"--recursive") == 0)
            options.recursive = true;
        else if (arg.CompareNoCase(L"--overwrite") == 0)
            options.overwrite = true;
        else if (arg.CompareNoCase(L"--preserve-tree") == 0)
            options.preserveTree = true;
        else if (arg.CompareNoCase(L"--dry-run") == 0)
            options.dryRun = true;
        else if (arg.CompareNoCase(L"--stats") == 0)
            options.stats = true;
        else if (arg.CompareNoCase(L"--quiet") == 0)
            options.quiet = true;
        else if (arg.CompareNoCase(L"--fail-fast") == 0)
            options.failFast = true;
        else if (arg.CompareNoCase(L"--skip-existing") == 0)
            options.skipExisting = true;
        else if (arg.CompareNoCase(L"--flush-report-each") == 0)
            options.flushReportEach = true;
        else if (arg.CompareNoCase(L"--isolate-crashes") == 0)
            options.isolateCrashes = true;
        else if (arg.CompareNoCase(L"--child-report") == 0)
        {
            if (index + 1 >= argc)
            {
                ConsolePrintf(L"Missing file path after --child-report.\n");
                return false;
            }
            ++index;
            options.childReportPath = argv[index];
        }
        else if (arg.CompareNoCase(L"--max-files") == 0)
        {
            if (index + 1 >= argc)
            {
                ConsolePrintf(L"Missing value after --max-files.\n");
                return false;
            }
            ++index;
            options.maxFiles = _wtoi(argv[index]);
            if (options.maxFiles < 0)
                options.maxFiles = 0;
        }
        else if (arg.CompareNoCase(L"--no-report") == 0)
            options.writeReport = false;
        else if (arg.CompareNoCase(L"--report") == 0)
        {
            if (index + 1 >= argc)
            {
                ConsolePrintf(L"Missing file path after --report.\n");
                return false;
            }
            ++index;
            options.reportPath = argv[index];
        }
        else
        {
            ConsolePrintf(L"Unknown option: %s\n", arg.GetString());
            return false;
        }
        return true;
    }

    bool ConvertOneFile(const CStringW& inputPath, const CStringW& outputPath, const EpubImportOptions& options, bool dryRun, bool collectStats, CStringW& error, CStringW& statsSummary, Fb2QualityStats& quality)
    {
        CStringW fb2;
        error.Empty();
        statsSummary.Empty();
        quality = Fb2QualityStats();
        bool ok = BuildFb2XmlFromEpub(inputPath, options, fb2, error);
        if (!ok)
            return false;
        quality = AnalyzeFb2Quality(fb2);
        EpubImportRuntimeStats runtimeStats = GetLastEpubImportRuntimeStats();
        quality.svgImages = runtimeStats.svgImages;
        quality.svgConverted = runtimeStats.svgConverted;
        quality.svgPlaceholders = runtimeStats.svgPlaceholders;
        quality.svgSkipped = runtimeStats.svgSkipped;
        quality.svgBackend = runtimeStats.svgBackend;
        if (collectStats)
            statsSummary = BuildStatsSummary(quality);
        if (dryRun)
            return true;
        if (!WriteUtf8ConsoleFile(outputPath, fb2))
        {
            error = L"Unable to write output file: " + outputPath;
            return false;
        }
        return true;
    }

    CStringW BuildOneCsvReport(const CStringW& inputPath, const CStringW& outputPath, const CStringW& status, const CStringW& elapsedText, bool stats, bool hasStats, const Fb2QualityStats& quality, const CStringW& message)
    {
        CStringW report = BatchCsvHeader(stats);
        if (stats)
        {
            report += CsvEscape(inputPath) + L";" + CsvEscape(outputPath) + L";" + status + L";" + elapsedText + L";";
            report += hasStats ? BuildStatsCsvColumns(quality) : EmptyStatsCsvColumns();
            report += L";" + CsvEscape(message) + L"\r\n";
        }
        else
        {
            report += CsvEscape(inputPath) + L";" + CsvEscape(outputPath) + L";" + status + L";" + elapsedText + L";" + CsvEscape(message) + L"\r\n";
        }
        return report;
    }

    int RunSingle(const CStringW& inputPath, const CStringW& outputPath, const ConsoleOptions& consoleOptions)
    {
        CStringW outDir = DirectoryOfLocal(outputPath);
        if (!outDir.IsEmpty())
            CreateDirectoryRecursiveLocal(outDir);

        CStringW error;
        CStringW statsSummary;
        Fb2QualityStats quality;
        ULONGLONG started = GetTickCount64();
        bool ok = ConvertOneFile(inputPath, outputPath, consoleOptions.importOptions, consoleOptions.dryRun, consoleOptions.stats, error, statsSummary, quality);
        ULONGLONG elapsed = GetTickCount64() - started;
        CStringW elapsedText;
        elapsedText.Format(L"%I64u", elapsed);

        CStringW qualityMessage;
        CStringW status;
        if (ok)
        {
            qualityMessage = BuildQualityMessage(quality);
            status = consoleOptions.dryRun ? CStringW(L"OK_DRY_RUN") : CStringW(L"OK");
            if (quality.WarningCount() > 0)
                status = consoleOptions.dryRun ? CStringW(L"OK_DRY_RUN_WITH_WARNINGS") : CStringW(L"OK_WITH_WARNINGS");
        }
        else
        {
            status = L"FAILED";
            qualityMessage = error;
        }

        if (!consoleOptions.childReportPath.IsEmpty())
            WriteUtf8ConsoleFile(consoleOptions.childReportPath, BuildOneCsvReport(inputPath, outputPath, status, elapsedText, consoleOptions.stats, ok, quality, qualityMessage));

        if (!ok)
        {
            ConsolePrintf(L"Import failed:\n%s\n", error.GetString());
            return 1;
        }

        if (consoleOptions.dryRun)
            ConsolePrintf(L"OK: dry-run completed, output was not written.\n");
        else
            ConsolePrintf(L"OK: %s\n", outputPath.GetString());
        if (consoleOptions.stats && !statsSummary.IsEmpty())
            ConsolePrintf(L"Stats: %s\n", statsSummary.GetString());
        if (!qualityMessage.IsEmpty())
            ConsolePrintf(L"Warnings: %s\n", qualityMessage.GetString());
        return 0;
    }


    CStringW SvgModeArg(int mode)
    {
        switch (mode)
        {
        case SVG_IMPORT_KEEP: return L"keep";
        case SVG_IMPORT_CONVERT_JPEG: return L"jpg";
        case SVG_IMPORT_SKIP: return L"skip";
        case SVG_IMPORT_CONVERT_PNG:
        default: return L"png";
        }
    }

    CStringW MakeChildReportPath(size_t index)
    {
        wchar_t temp[MAX_PATH] = {};
        DWORD len = GetTempPathW(MAX_PATH, temp);
        CStringW dir = (len > 0 && len < MAX_PATH) ? CStringW(temp) : CStringW(L".");
        CStringW name;
        GUID g = {};
        CoCreateGuid(&g);
        name.Format(L"ImportEPUB_child_%lu_%u_%08lX.csv", GetCurrentProcessId(), static_cast<unsigned>(index + 1), g.Data1);
        return CombinePathLocal(dir, name);
    }

    void AppendBoolOption(CStringW& cmd, bool value, const wchar_t* trueArg, const wchar_t* falseArg)
    {
        AppendCommandArg(cmd, value ? CStringW(trueArg) : CStringW(falseArg));
    }

    void AppendChildImportOptions(CStringW& cmd, const ConsoleOptions& options, const CStringW& childReportPath)
    {
        if (options.importOptions.writeImportLog)
            AppendCommandArg(cmd, L"--log");
        else
            AppendCommandArg(cmd, L"--no-log");

        AppendBoolOption(cmd, options.importOptions.importImages, L"--images", L"--no-images");
        AppendBoolOption(cmd, options.importOptions.importCover, L"--cover", L"--no-cover");
        AppendBoolOption(cmd, options.importOptions.importNotes, L"--notes", L"--no-notes");
        AppendBoolOption(cmd, options.importOptions.importTables, L"--tables", L"--no-tables");
        AppendBoolOption(cmd, options.importOptions.importLists, L"--lists", L"--no-lists");
        AppendBoolOption(cmd, options.importOptions.preserveLinks, L"--links", L"--no-links");
        AppendBoolOption(cmd, options.importOptions.importSubtitles, L"--subtitles", L"--no-subtitles");
        AppendBoolOption(cmd, options.importOptions.importPoemsEpigraphs, L"--poems", L"--no-poems");
        AppendBoolOption(cmd, options.importOptions.importPageBreaks, L"--pagebreaks", L"--no-pagebreaks");
        AppendBoolOption(cmd, options.importOptions.splitSectionsByHeadings, L"--split-sections", L"--no-split-sections");
        AppendBoolOption(cmd, options.importOptions.useCssSemanticClasses, L"--css-semantic", L"--no-css-semantic");
        AppendBoolOption(cmd, options.importOptions.cleanTypography, L"--clean-typography", L"--no-clean-typography");
        AppendBoolOption(cmd, options.importOptions.skipHiddenElements, L"--skip-hidden", L"--include-hidden");
        AppendBoolOption(cmd, options.importOptions.skipServicePages, L"--skip-service-pages", L"--keep-service-pages");
        AppendBoolOption(cmd, options.importOptions.removeServiceSections, L"--remove-service-sections", L"--keep-service-sections");
        AppendBoolOption(cmd, options.importOptions.removeFootnoteBacklinks, L"--remove-footnote-backlinks", L"--keep-footnote-backlinks");
        AppendBoolOption(cmd, options.importOptions.validateResult, L"--validate", L"--no-validate");

        AppendCommandArg(cmd, L"--svg");
        AppendCommandArg(cmd, SvgModeArg(options.importOptions.svgConversionMode));

        if (options.importOptions.addDiagnosticSection)
            AppendCommandArg(cmd, L"--diagnostic-section");
        if (options.importOptions.saveFb2Copy)
            AppendCommandArg(cmd, L"--save-copy");
        if (options.importOptions.keepTempOnError)
            AppendCommandArg(cmd, L"--keep-temp-on-error");
        if (options.importOptions.saveIntermediateFb2OnError)
            AppendCommandArg(cmd, L"--save-on-warning");
        if (options.dryRun)
            AppendCommandArg(cmd, L"--dry-run");
        if (options.stats)
            AppendCommandArg(cmd, L"--stats");
        if (options.quiet)
            AppendCommandArg(cmd, L"--quiet");

        AppendCommandArg(cmd, L"--child-report");
        AppendCommandArg(cmd, childReportPath);
    }

    struct ChildBatchResult
    {
        DWORD exitCode;
        bool hasReport;
        CStringW row;
        CStringW status;
        CStringW elapsedText;
        CStringW message;

        ChildBatchResult()
            : exitCode(0)
            , hasReport(false)
        {
        }
    };

    ChildBatchResult RunOneFileInChildProcess(const CStringW& inputPath, const CStringW& outputPath, const ConsoleOptions& options, size_t index)
    {
        ChildBatchResult result;
        CStringW childReportPath = MakeChildReportPath(index);
        DeleteFileW(childReportPath);

        CStringW exePath = CurrentExePath();
        CStringW cmd;
        AppendCommandArg(cmd, exePath);
        AppendCommandArg(cmd, inputPath);
        AppendCommandArg(cmd, outputPath);
        AppendChildImportOptions(cmd, options, childReportPath);

        STARTUPINFOW si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::vector<wchar_t> mutableCmd(static_cast<size_t>(cmd.GetLength()) + 1, 0);
        wcscpy_s(&mutableCmd[0], mutableCmd.size(), cmd.GetString());

        BOOL created = CreateProcessW(nullptr, &mutableCmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (!created)
        {
            result.exitCode = GetLastError();
            result.status = L"FAILED_CRASH";
            result.elapsedText = L"0";
            result.message.Format(L"Unable to start child converter. Win32 error: %lu", result.exitCode);
            return result;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        result.exitCode = code;

        CStringW reportText;
        if (ReadUtf8ConsoleFile(childReportPath, reportText))
        {
            CStringW row = FirstDataLineFromCsvText(reportText);
            if (!row.IsEmpty())
            {
                result.hasReport = true;
                result.row = row;
                std::vector<CStringW> fields = SplitCsvLine(row);
                if (fields.size() >= 5)
                {
                    result.status = fields[2];
                    result.elapsedText = fields[3];
                    result.message = fields.back();
                }
            }
        }
        DeleteFileW(childReportPath);

        if (!result.hasReport)
        {
            result.status = L"FAILED_CRASH";
            result.elapsedText = L"0";
            result.message = L"Child converter terminated before writing a report row. Exit code: " + ExitCodeText(code);
        }
        return result;
    }

    int RunBatch(const CStringW& inputDir, const CStringW& outputDir, const ConsoleOptions& options)
    {
        if (!DirectoryExistsLocal(inputDir))
        {
            ConsolePrintf(L"Input folder does not exist: %s\n", inputDir.GetString());
            return 5;
        }

        if (!CreateDirectoryRecursiveLocal(outputDir))
        {
            ConsolePrintf(L"Unable to create output folder: %s\n", outputDir.GetString());
            return 6;
        }

        std::vector<CStringW> files;
        CollectEpubFiles(inputDir, options.recursive, files);
        if (files.empty())
        {
            ConsolePrintf(L"No EPUB files found: %s\n", inputDir.GetString());
            return 0;
        }

        if (options.maxFiles > 0 && files.size() > static_cast<size_t>(options.maxFiles))
            files.resize(static_cast<size_t>(options.maxFiles));

        CStringW report;
        report += BatchCsvHeader(options.stats);
        CStringW htmlReport = BuildHtmlReportStart(inputDir, outputDir);
        CStringW reportPath;
        CStringW htmlPath;
        if (options.writeReport)
        {
            reportPath = options.reportPath.IsEmpty() ? CombinePathLocal(outputDir, L"ImportEPUBBatch_report.csv") : options.reportPath;
            htmlPath = MakeHtmlReportPath(reportPath);
        }

        int okCount = 0;
        int warningCount = 0;
        int failCount = 0;
        int skipCount = 0;
        for (size_t i = 0; i < files.size(); ++i)
        {
            CStringW outputPath = MakeBatchOutputPath(inputDir, outputDir, files[i], options.preserveTree, options.overwrite);
            CStringW error;
            CStringW statsSummary;
            Fb2QualityStats quality;
            ULONGLONG started = GetTickCount64();
            if (!options.quiet)
                ConsolePrintf(L"[%u/%u] %s\n", static_cast<unsigned>(i + 1), static_cast<unsigned>(files.size()), files[i].GetString());

            if (options.skipExisting)
            {
                CStringW defaultOutputPath = MakeDefaultBatchOutputPath(inputDir, outputDir, files[i], options.preserveTree);
                if (FileExistsLocal(defaultOutputPath))
                {
                    CStringW elapsedText(L"0");
                    if (!options.quiet)
                        ConsolePrintf(L"  SKIPPED: output exists -> %s\n", defaultOutputPath.GetString());
                    ++skipCount;
                    report += options.stats
                        ? CsvEscape(files[i]) + L";" + CsvEscape(defaultOutputPath) + L";SKIPPED;" + elapsedText + L";" + EmptyStatsCsvColumns() + L";\"output exists\"\r\n"
                        : CsvEscape(files[i]) + L";" + CsvEscape(defaultOutputPath) + L";SKIPPED;" + elapsedText + L";\"output exists\"\r\n";
                    htmlReport += BuildHtmlReportRow(static_cast<unsigned>(i + 1), files[i], defaultOutputPath, L"SKIPPED", elapsedText, false, Fb2QualityStats(), L"output exists");
                    if (options.writeReport && options.flushReportEach)
                        WriteBatchReportsSnapshot(reportPath, htmlPath, report, htmlReport, okCount, warningCount, failCount, skipCount);
                    continue;
                }
            }

            if (options.isolateCrashes)
            {
                ChildBatchResult child = RunOneFileInChildProcess(files[i], outputPath, options, i);
                ULONGLONG elapsed = GetTickCount64() - started;
                CStringW elapsedText = child.elapsedText;
                if (elapsedText.IsEmpty() || elapsedText == L"0")
                    elapsedText.Format(L"%I64u", elapsed);

                CStringW status = child.status.IsEmpty() ? CStringW(L"FAILED_CRASH") : child.status;
                CStringW message = child.message;
                bool isFailed = status.Find(L"FAILED") >= 0;
                bool isWarn = status.Find(L"WARNING") >= 0;

                if (isFailed)
                    ++failCount;
                else
                {
                    ++okCount;
                    if (isWarn)
                        ++warningCount;
                }

                if (child.hasReport)
                    report += child.row + L"\r\n";
                else
                {
                    report += options.stats
                        ? CsvEscape(files[i]) + L";" + CsvEscape(outputPath) + L";" + status + L";" + elapsedText + L";" + EmptyStatsCsvColumns() + L";" + CsvEscape(message) + L"\r\n"
                        : CsvEscape(files[i]) + L";" + CsvEscape(outputPath) + L";" + status + L";" + elapsedText + L";" + CsvEscape(message) + L"\r\n";
                }
                htmlReport += BuildHtmlReportRow(static_cast<unsigned>(i + 1), files[i], outputPath, status, elapsedText, false, Fb2QualityStats(), message);

                if (!options.quiet)
                {
                    if (isFailed)
                        ConsolePrintf(L"  %s: %s\n", status.GetString(), message.GetString());
                    else if (isWarn)
                        ConsolePrintf(L"  OK with warnings -> %s\n", outputPath.GetString());
                    else if (options.dryRun)
                        ConsolePrintf(L"  OK dry-run\n");
                    else
                        ConsolePrintf(L"  OK -> %s\n", outputPath.GetString());
                }

                if (options.writeReport && options.flushReportEach)
                    WriteBatchReportsSnapshot(reportPath, htmlPath, report, htmlReport, okCount, warningCount, failCount, skipCount);
                if (options.failFast && isFailed)
                    break;
                continue;
            }

            if (ConvertOneFile(files[i], outputPath, options.importOptions, options.dryRun, options.stats, error, statsSummary, quality))
            {
                ++okCount;
                ULONGLONG elapsed = GetTickCount64() - started;
                CStringW elapsedText;
                elapsedText.Format(L"%I64u", elapsed);
                CStringW qualityMessage = BuildQualityMessage(quality);
                CStringW okStatus = options.dryRun ? CStringW(L"OK_DRY_RUN") : CStringW(L"OK");
                if (quality.WarningCount() > 0)
                {
                    okStatus = options.dryRun ? CStringW(L"OK_DRY_RUN_WITH_WARNINGS") : CStringW(L"OK_WITH_WARNINGS");
                    ++warningCount;
                }
                htmlReport += BuildHtmlReportRow(static_cast<unsigned>(i + 1), files[i], outputPath, okStatus, elapsedText, true, quality, qualityMessage);
                if (!options.quiet)
                {
                    if (options.dryRun)
                        ConsolePrintf(quality.WarningCount() > 0 ? L"  OK dry-run with warnings\n" : L"  OK dry-run\n");
                    else
                        ConsolePrintf(quality.WarningCount() > 0 ? L"  OK with warnings -> %s\n" : L"  OK -> %s\n", outputPath.GetString());
                    if (options.stats && !statsSummary.IsEmpty())
                        ConsolePrintf(L"  Stats: %s\n", statsSummary.GetString());
                    if (!qualityMessage.IsEmpty())
                        ConsolePrintf(L"  Warnings: %s\n", qualityMessage.GetString());
                }
                report += options.stats
                    ? CsvEscape(files[i]) + L";" + CsvEscape(outputPath) + L";" + okStatus + L";" + elapsedText + L";" + BuildStatsCsvColumns(quality) + L";" + CsvEscape(qualityMessage) + L"\r\n"
                    : CsvEscape(files[i]) + L";" + CsvEscape(outputPath) + L";" + okStatus + L";" + elapsedText + L";" + CsvEscape(qualityMessage) + L"\r\n";
                if (options.writeReport && options.flushReportEach)
                    WriteBatchReportsSnapshot(reportPath, htmlPath, report, htmlReport, okCount, warningCount, failCount, skipCount);
            }
            else
            {
                ++failCount;
                ULONGLONG elapsed = GetTickCount64() - started;
                CStringW elapsedText;
                elapsedText.Format(L"%I64u", elapsed);
                if (!options.quiet)
                    ConsolePrintf(L"  FAILED: %s\n", error.GetString());
                report += options.stats
                    ? CsvEscape(files[i]) + L";" + CsvEscape(outputPath) + L";FAILED;" + elapsedText + L";" + EmptyStatsCsvColumns() + L";" + CsvEscape(error) + L"\r\n"
                    : CsvEscape(files[i]) + L";" + CsvEscape(outputPath) + L";FAILED;" + elapsedText + L";" + CsvEscape(error) + L"\r\n";
                htmlReport += BuildHtmlReportRow(static_cast<unsigned>(i + 1), files[i], outputPath, L"FAILED", elapsedText, false, Fb2QualityStats(), error);
                if (options.writeReport && options.flushReportEach)
                    WriteBatchReportsSnapshot(reportPath, htmlPath, report, htmlReport, okCount, warningCount, failCount, skipCount);
                if (options.failFast)
                    break;
            }
        }

        if (options.writeReport)
        {
            WriteBatchReportsSnapshot(reportPath, htmlPath, report, htmlReport, okCount, warningCount, failCount, skipCount);
            if (!options.quiet)
            {
                ConsolePrintf(L"Report: %s\n", reportPath.GetString());
                ConsolePrintf(L"HTML report: %s\n", htmlPath.GetString());
            }
        }

        ConsolePrintf(L"Batch completed. OK: %d, OK_WITH_WARNINGS: %d, skipped: %d, failed: %d\n", okCount - warningCount, warningCount, skipCount, failCount);
        return failCount == 0 ? 0 : 10;
    }
}

int wmain(int argc, wchar_t** argv)
{
    InitializeConsoleOutput();

    if (argc == 2 && CStringW(argv[1]).CompareNoCase(L"--help") == 0)
    {
        PrintUsage();
        return 0;
    }
    if (argc == 2 && CStringW(argv[1]).CompareNoCase(L"--version") == 0)
    {
        PrintVersion();
        return 0;
    }
    if (argc < 3)
    {
        PrintUsage();
        return 2;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        ConsolePrintf(L"COM initialization failed: 0x%08lX\n", hr);
        return 3;
    }

    ConsoleOptions options;
    int firstPathArg = 1;
    if (CStringW(argv[1]).CompareNoCase(L"--batch") == 0)
    {
        if (argc < 4)
        {
            PrintUsage();
            CoUninitialize();
            return 2;
        }
        options.batchMode = true;
        firstPathArg = 2;
    }

    CStringW inputPath(argv[firstPathArg]);
    CStringW outputPath(argv[firstPathArg + 1]);

    for (int i = firstPathArg + 2; i < argc; ++i)
    {
        if (!ApplyOption(options, i, argc, argv))
        {
            CoUninitialize();
            return 2;
        }
    }

    int rc = 0;
    if (options.batchMode)
        rc = RunBatch(inputPath, outputPath, options);
    else
        rc = RunSingle(inputPath, outputPath, options);

    CoUninitialize();
    return rc;
}
