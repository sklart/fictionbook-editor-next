// C4 Open-only diagnostic experiment. Reuse the external Release controller;
// no FBE instrumentation or scripting access to binary payload during Open.
#define wmain ThreeWayBenchmarkEntry
#include "binary-store-driver.cpp"
#undef wmain
int wmain(int argc, wchar_t** argv) {
    if (argc != 5) { fputs("driver FBE.exe book.fb2 binary-count report.tsv\n", stderr); return 2; }
    Check(CoInitialize(nullptr));
    try {
        Controller run(argv[4]);
        const double start = Now();
        run.Start(argv[1], argv[2]); run.Ready(_wtol(argv[3]));
        // An empty binary container also exists before apiLoadFB2 completes.
        std::wstring stem=argv[2]; stem=stem.substr(stem.find_last_of(L"/\\")+1); const auto dot=stem.find_last_of(L'.'); if(dot!=std::wstring::npos) stem.resize(dot);
        const double deadline=Now()+180000;
        for(;;){run.Windows(); wchar_t title[1024]={}; GetWindowTextW(run.frame,title,1024); if(std::wstring(title).find(stem)!=std::wstring::npos) break; if(Now()>deadline) throw HRESULT_FROM_WIN32(WAIT_TIMEOUT); Sleep(10);}
        // apiLoadFB2 selects Body; this experiment measures Open only, without an extra UI command.
        run.Timing("open", start); run.Memory("after_open");
        run.output << "status\tPASS\t1\n"; // destructor terminates only this owned test process after sampling.
    } catch (HRESULT hr) { fprintf(stderr, "C4 Open failed HRESULT=0x%08lx\n", hr); return 1; }
    CoUninitialize(); return 0;
}
