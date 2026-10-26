// Separate C4 diagnostic controller. Original C3 and scaling drivers stay intact.
#define wmain ThreeWayBenchmarkEntry
#include "binary-store-driver.cpp"
#undef wmain
struct C4CommandState { bool done=false; LRESULT result=0; };
void CALLBACK C4CommandDone(HWND,UINT,ULONG_PTR data,LRESULT result) {
    auto& state=*reinterpret_cast<C4CommandState*>(data);state.result=result;state.done=true;
}
void C4PumpedCommand(Controller& run,WORD command) {
    static C4CommandState state;state.done=false;
    DWORD owner=0;GetWindowThreadProcessId(run.frame,&owner);if(owner!=run.pid)throw E_UNEXPECTED;
    if(!SendMessageCallbackW(run.frame,WM_COMMAND,command,0,C4CommandDone,reinterpret_cast<ULONG_PTR>(&state)))throw HRESULT_FROM_WIN32(GetLastError());
    const double deadline=Now()+180000;
    while(!state.done){
        MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT)throw E_ABORT;TranslateMessage(&message);DispatchMessageW(&message);}
        if(state.done)break;
        if(WaitForSingleObject(run.process,0)!=WAIT_TIMEOUT)throw E_FAIL;
        if(Now()>deadline)throw HRESULT_FROM_WIN32(WAIT_TIMEOUT);
        MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);
    }
}
void C4Collect(Controller& run) {
    auto doc=run.Document();CComPtr<IHTMLWindow2> window;Check(doc->get_parentWindow(&window));CComVariant result;
    Check(window->execScript(CComBSTR(L"CollectGarbage();"),CComBSTR(L"JScript"),&result));
}
void C4Profile(Controller& run,const wchar_t* report) {
    auto doc=run.Document();CComPtr<IDispatch> script;Check(doc->get_Script(&script));CComDispatchDriver driver(script);CComVariant value;
    Check(driver.Invoke0(L"_c4_profile_json",&value));if(value.vt!=VT_BSTR)throw DISP_E_TYPEMISMATCH;
    std::ofstream output((std::wstring(report)+L".profile.json").c_str());
    for(UINT i=0;i<SysStringLen(value.bstrVal);++i){if(value.bstrVal[i]>127)throw DISP_E_TYPEMISMATCH;output.put(static_cast<char>(value.bstrVal[i]));}
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=9){fputs("driver exe book count report protocol(ready/pumped/legacy) gc profile save\n",stderr);return 2;}
    Check(CoInitialize(nullptr));
    try {
        Controller run(argv[4]);const double start=Now();run.Start(argv[1],argv[2]);run.Ready(_wtol(argv[3]));
        {std::ofstream pid((std::wstring(argv[4])+L".pid").c_str());pid<<run.pid;}
        const std::wstring protocol=argv[5];
        if(protocol==L"pumped"){const double commandStart=Now();C4PumpedCommand(run,32810);run.Timing("body_ready_command",commandStart);}
        else if(protocol==L"legacy"){
            const double commandStart=Now();DWORD_PTR result=0;
            if(!SendMessageTimeoutW(run.frame,WM_COMMAND,32810,0,SMTO_ABORTIFHUNG,10000,&result))throw HRESULT_FROM_WIN32(GetLastError());
            run.Timing("body_ready_command",commandStart);
        } else {
            std::wstring stem=argv[2];stem=stem.substr(stem.find_last_of(L"/\\")+1);const auto dot=stem.find_last_of(L'.');if(dot!=std::wstring::npos)stem.resize(dot);
            const double deadline=Now()+180000;
            for(;;){run.Windows();wchar_t title[1024]={};GetWindowTextW(run.frame,title,1024);if(std::wstring(title).find(stem)!=std::wstring::npos)break;if(Now()>deadline)throw HRESULT_FROM_WIN32(WAIT_TIMEOUT);Sleep(10);}
        }
        run.Timing("open",start);run.Memory("after_open");
        if(_wtol(argv[7]))C4Profile(run,argv[4]);
        if(_wtol(argv[6])){const double gcStart=Now();C4Collect(run);run.Timing("explicit_gc",gcStart);run.Memory("after_gc");}
        if(_wtol(argv[8])){const double saveStart=Now();C4PumpedCommand(run,0xE103);run.Timing("verification_save",saveStart);}
        run.output<<"status\tPASS\t1\n";
    } catch(HRESULT hr){fprintf(stderr,"C4 diagnostic driver failed HRESULT=0x%08lx\n",hr);return 1;}
    CoUninitialize();return 0;
}
