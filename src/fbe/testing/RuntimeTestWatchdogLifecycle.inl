        // All filesystem fixtures belong to this disposable runtime's diagnostic directory.
        m_ui_watchdog.Stop();
        FbeDiagnostics::UiWatchdog probe;
        WNDCLASS probeClass={}; probeClass.hInstance=_Module.GetModuleInstance(); probeClass.lpszClassName=L"FbeWatchdogRuntimeProbe";
        probeClass.lpfnWndProc=[](HWND hwnd,UINT message,WPARAM wp,LPARAM lp)->LRESULT {
            if(message==WM_NCCREATE) ::SetWindowLongPtr(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams));
            auto monitor=reinterpret_cast<FbeDiagnostics::UiWatchdog*>(::GetWindowLongPtr(hwnd,GWLP_USERDATA));
            if(message==FbeDiagnostics::WatchdogHeartbeatMessage && monitor) { monitor->Acknowledge(static_cast<DWORD>(wp)); return 0; }
            return ::DefWindowProc(hwnd,message,wp,lp);
        };
        ::RegisterClass(&probeClass);
        HWND probeWindow=::CreateWindowEx(0,probeClass.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,NULL,probeClass.hInstance,&probe);
        auto await=[&](const std::function<bool()>& condition) {
            const ULONGLONG end=::GetTickCount64()+1500;
            do { pump(10); if(condition()) return true; } while(::GetTickCount64()<end);
            return false;
        };
        CString folder(directory); folder.TrimRight(L"\\/"); const CString backup=folder+L"-runtime-backup";
        ::CreateDirectory(folder,NULL);
        bool lifecycle=probeWindow && !probe.Start(NULL) && !probe.Running() && probe.LastError()==ERROR_INVALID_WINDOW_HANDLE;
        lifecycle=lifecycle && ::MoveFile(folder,backup)!=FALSE;
        HANDLE obstruction=::CreateFile(folder,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(obstruction!=INVALID_HANDLE_VALUE) ::CloseHandle(obstruction);
        bool directoryFailure=obstruction!=INVALID_HANDLE_VALUE && probe.Start(probeWindow) && await([&]() { return probe.LastError()==ERROR_DIRECTORY && !probe.Running(); });
        const DWORD directoryCode=probe.LastError();
        probe.Stop(); probe.Stop();
        directoryFailure=directoryFailure && probe.LastError()==ERROR_DIRECTORY && !probe.Running();
        const bool restored=::DeleteFile(folder)!=FALSE && ::MoveFile(backup,folder)!=FALSE;
        // Deny actual FILE_ADD_FILE access on this isolated folder, and restore its exact DACL.
        struct DirectoryDenial {
            CString path; PSECURITY_DESCRIPTOR original=nullptr; PACL previous=nullptr, denied=nullptr;
            bool installed=false; SECURITY_DESCRIPTOR_CONTROL control=0;
            explicit DirectoryDenial(const CString& folder) : path(folder) {
                if(::GetNamedSecurityInfo(const_cast<LPWSTR>(path.GetString()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL,NULL,&previous,NULL,&original)!=ERROR_SUCCESS) return;
                DWORD revision=0; if(!::GetSecurityDescriptorControl(original,&control,&revision)) return;
                BYTE world[SECURITY_MAX_SID_SIZE]={}; DWORD bytes=sizeof(world);
                if(!::CreateWellKnownSid(WinWorldSid,NULL,world,&bytes)) return;
                EXPLICIT_ACCESS entry={}; entry.grfAccessPermissions=FILE_ADD_FILE; entry.grfAccessMode=DENY_ACCESS;
                entry.Trustee.TrusteeForm=TRUSTEE_IS_SID; entry.Trustee.TrusteeType=TRUSTEE_IS_WELL_KNOWN_GROUP; entry.Trustee.ptstrName=reinterpret_cast<LPWSTR>(world);
                if(::SetEntriesInAcl(1,&entry,previous,&denied)!=ERROR_SUCCESS) return;
                installed=::SetNamedSecurityInfo(const_cast<LPWSTR>(path.GetString()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,NULL,NULL,denied,NULL)==ERROR_SUCCESS;
            }
            bool Restore() {
                if(!installed) return false;
                const DWORD flags=DACL_SECURITY_INFORMATION|((control&SE_DACL_PROTECTED) ? PROTECTED_DACL_SECURITY_INFORMATION : UNPROTECTED_DACL_SECURITY_INFORMATION);
                if(::SetNamedSecurityInfo(const_cast<LPWSTR>(path.GetString()),SE_FILE_OBJECT,flags,NULL,NULL,previous,NULL)!=ERROR_SUCCESS) return false;
                installed=false; return true;
            }
            ~DirectoryDenial() { if(installed) Restore(); if(denied) ::LocalFree(denied); if(original) ::LocalFree(original); }
        } denial(folder);
        const bool blockedFailure=restored && denial.installed && probe.Start(probeWindow) && await([&]() { return probe.LastError()==ERROR_ACCESS_DENIED && !probe.Running(); });
        const DWORD blockedCode=probe.LastError(); directoryFailure=directoryFailure && blockedFailure;
        const bool permissionsRestored=denial.Restore(); directoryFailure=directoryFailure && permissionsRestored;
        // No Stop between the failed worker and restart: Start must reap the finished thread itself.
        bool restart=await([&]() { return probe.Start(probeWindow); }) && await([&]() { return probe.Running() && probe.Replies()>=2; });
        const DWORD retained=probe.LastError();
        restart=restart && retained==ERROR_ACCESS_DENIED && !probe.Start(probeWindow) && probe.LastError()==retained;
        probe.Stop(); restart=restart && !probe.Running() && probe.LastError()==retained;
        ::SetEnvironmentVariable(L"FBE_NEXT_TEST_WATCHDOG_FAULT",L"exception");
        bool workerException=probe.Start(probeWindow) && await([&]() { return probe.LastError()==ERROR_UNHANDLED_EXCEPTION && !probe.Running(); });
        ::SetEnvironmentVariable(L"FBE_NEXT_TEST_WATCHDOG_FAULT",NULL);
        workerException=workerException && await([&]() { return probe.Start(probeWindow); }) && await([&]() { return probe.Running() && probe.Replies()>=2; }) && probe.LastError()==ERROR_UNHANDLED_EXCEPTION;
        probe.Stop();
        // Race two control callers. The worker and heartbeat never take the lifecycle mutex.
        std::atomic<unsigned int> accepted{0}; HANDLE gate=::CreateEvent(NULL,TRUE,FALSE,NULL);
        auto startProbe=[&]() { ::WaitForSingleObject(gate,INFINITE); if(probe.Start(probeWindow)) accepted.fetch_add(1); };
        std::thread first(startProbe), second(startProbe); ::SetEvent(gate); first.join(); second.join(); ::CloseHandle(gate);
        bool concurrent=accepted.load()==1 && await([&]() { return probe.Running() && probe.Replies()>=2; });
        const ULONGLONG stopBegin=::GetTickCount64();
        std::thread stopFirst([&]() { probe.Stop(); }), stopSecond([&]() { probe.Stop(); }); stopFirst.join(); stopSecond.join();
        concurrent=concurrent && !probe.Running() && ::GetTickCount64()-stopBegin<1000 && probe.LastError()==ERROR_UNHANDLED_EXCEPTION;
        lifecycle=lifecycle && probe.Start(probeWindow) && await([&]() { return probe.Running() && probe.Replies()>=2; });
        probe.Stop(); lifecycle=lifecycle && !probe.Running();
        ::DestroyWindow(probeWindow); ::UnregisterClass(probeClass.lpszClassName,probeClass.hInstance);
