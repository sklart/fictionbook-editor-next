// Included in StartupTrace.cpp's private namespace. No I/O under this independent lock.
    SRWLOCK hangTelemetryLock=SRWLOCK_INIT;
    StartupTrace::HangTelemetry hangTelemetry;
    void HangCode(wchar_t (&destination)[20],const wchar_t* code) noexcept
    {
        size_t letters=0, digits=0;
        if(code) {
            while(letters<9 && code[letters]>=L'A' && code[letters]<=L'Z') ++letters;
            if(letters>0 && letters<=8) while(digits<9 && code[letters+digits]>=L'0' && code[letters+digits]<=L'9') ++digits;
        }
        if(!code || !letters || letters>8 || !digits || digits>8 || code[letters+digits]) wcscpy_s(destination,L"unknown");
        else wcsncpy_s(destination,code,_TRUNCATE);
    }
    void PublishHangOperations() noexcept
    {
        if(!::TryAcquireSRWLockExclusive(&hangTelemetryLock)) return;
        std::copy(std::begin(operationCounters),std::end(operationCounters),std::begin(hangTelemetry.operations));
        ActiveOperation latest; for(const auto& active:activeOperations) if(active.token>latest.token) latest=active;
        hangTelemetry.mode=activeEditorMode; hangTelemetry.lastCompleted=lastCompletedOperation;
        hangTelemetry.lastSucceeded=lastCompletedSucceeded; hangTelemetry.lastDuration=lastCompletedMs;
        hangTelemetry.inProgress=latest.operation; hangTelemetry.inProgressSince=latest.started;
        hangTelemetry.dropped=droppedOperations; hangTelemetry.sampledAt=::GetTickCount64();
        ::ReleaseSRWLockExclusive(&hangTelemetryLock);
    }
    void PublishHangStage(const CString& category,const CString& code,bool script) noexcept
    {
        if(!::TryAcquireSRWLockExclusive(&hangTelemetryLock)) return;
        HangCode(hangTelemetry.eventCode,code);
        if(category==L"document") HangCode(hangTelemetry.documentCode,code);
        if(script) HangCode(hangTelemetry.scriptCode,code);
        hangTelemetry.sampledAt=::GetTickCount64(); ::ReleaseSRWLockExclusive(&hangTelemetryLock);
    }
    void PublishHangHresult(const wchar_t* code,HRESULT value) noexcept
    {
        if(!::TryAcquireSRWLockExclusive(&hangTelemetryLock)) return;
        HangCode(hangTelemetry.hresultCode,code); hangTelemetry.hasHresult=true; hangTelemetry.hresult=static_cast<DWORD>(value);
        if(FAILED(value)) { hangTelemetry.hasFailure=true; hangTelemetry.failureHresult=static_cast<DWORD>(value); }
        hangTelemetry.sampledAt=::GetTickCount64(); ::ReleaseSRWLockExclusive(&hangTelemetryLock);
    }
    void PublishHangScriptFailure(const CString& code) noexcept
    {
        if(!::TryAcquireSRWLockExclusive(&hangTelemetryLock)) return;
        HangCode(hangTelemetry.scriptFailureCode,code); ::ReleaseSRWLockExclusive(&hangTelemetryLock);
    }
