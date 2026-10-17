// Included by DiagnosticPackage.cpp: short headless checks exercise production snapshots.
int StartupTrace::RunDiagnosticPackageTest()
{
    wchar_t testMode[2] = {}, output[32768] = {}, selected[128] = {};
    if (::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, 2) != 1 || testMode[0] != L'1') return 90;
    const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_PACKAGE_DIRECTORY", output, _countof(output));
    if (!length || length >= _countof(output)) return 91;
    CString directory(output); directory.TrimRight(L"\\"); directory += L"\\";
    if (::SHCreateDirectoryEx(NULL, directory, NULL) != ERROR_SUCCESS && ::GetFileAttributes(directory) == INVALID_FILE_ATTRIBUTES) return 92;
    // The public plug-in helper must route into the host and suppress arbitrary details.
    FbeDiagnostic::HResult(L"file-dialog", L"FD203", E_ACCESSDENIED, L"PRIVATE_USER C:\\secret\\book.fb2 <p>PRIVATE_BOOK</p>");
    Event(L"diagnostic", L"DGTEST", L"PRIVATE_USER; hr=0x80004005; C:\\secret\\book.fb2; <p>PRIVATE_BOOK</p>");
    ::GetEnvironmentVariable(L"FBE_NEXT_TEST_PACKAGE_SESSION", selected, _countof(selected));
    const CString id = selected[0] ? CString(selected) : SessionId();
    DiagnosticSession session; bool found = false;
    for (const auto& item : EnumerateDiagnosticSessions()) if (item.id == id) { session = item; found = true; break; }
    CString report(L"session_id=" + SessionId() + L"\r\nselected=" + id + L"\r\n");
    auto write = [&](const CString& name, const std::vector<BYTE>& bytes) {
        HANDLE file = ::CreateFile(directory + name, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0; const bool ok = bytes.empty() || (::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) && written == bytes.size());
        ::CloseHandle(file); return ok;
    };
    CString error; std::shared_ptr<const DiagnosticPackageSnapshot> snapshot;
    const bool prepared = found && PrepareDiagnosticPackage(session, {}, snapshot, error);
    if (snapshot) for (const auto& item : snapshot->Components()) {
        report += item.name + L"=" + item.status + L"\r\n";
        if (item.status == L"included") {
            CString name(item.name); name.Replace(L"/", L"_");
            if (!write(name + L".preview", item.bytes)) return 93;
        }
    }
    if(prepared) {
        std::shared_ptr<const DiagnosticPackageSnapshot> excludedSnapshot; CString excludedError;
        if(!PrepareDiagnosticPackage(session,{L"diagnostic-modules.txt"},excludedSnapshot,excludedError) || !excludedSnapshot) return 95;
        bool excluded=false;
        for(const auto& item:excludedSnapshot->Components()) if(item.name==L"diagnostic-modules.txt") excluded=item.status==L"excluded-by-user" && item.bytes.empty();
        if(!excluded) return 96;
    }
    // New events after preview must not change the saved package.
    Error(L"diagnostic", L"DGAFTER", L"hr=0x80070005");
    const bool saved = prepared && SaveDiagnosticPackage(*snapshot, directory + L"package.zip", error);
    if(saved) {
        std::vector<BYTE> before,after; ReadBytes(directory+L"package.zip",before); CString collisionError;
        if(SaveDiagnosticPackage(*snapshot,directory+L"package.zip",collisionError)) return 97;
        ReadBytes(directory+L"package.zip",after); if(before!=after) return 98;
    }
    wchar_t cleanup[2]={};
    if(::GetEnvironmentVariable(L"FBE_NEXT_TEST_PACKAGE_CLEANUP",cleanup,2)==1 && cleanup[0]==L'1') {
        const auto result=ClearOldLogSessions(); CString details;
        details.Format(L"cleanup-files=%u\r\ncleanup-bytes=%llu\r\ncleanup-failed=%u\r\n",result.filesDeleted,result.bytesDeleted,result.filesFailed); report+=details;
    }
    report += L"error=" + error + L"\r\n";
    write(L"result.txt", Utf8(report));
    return saved ? 0 : 94;
}
