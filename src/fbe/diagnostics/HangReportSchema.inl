// Included at global scope after the package privacy helpers.
bool StartupTrace::ValidateHangReport(const CString& text,const CString& expectedSession)
{
    if(text.IsEmpty() || text.GetLength()>256*1024 || !ValidSessionId(expectedSession)) return false;
    const CString numericKeys(L"|hang-monitor-paused-ms|hang-incident|hang-duration-ms|hang-ui-thread-id|hang-worker-thread-id|hang-request-ms|hang-last-reply-ms|hang-heartbeat-sequence|hang-heartbeat-acknowledged|hang-telemetry-age-ms|hang-threshold-suspect-ms|hang-threshold-confirm-ms|");
    const CString booleanKeys(L"|hang-monitoring-interrupted|hang-confirmed|hang-telemetry-available|hang-telemetry-fresh|hang-hresult-available|hang-failure-available|");
    const CString codeKeys(L"|hang-last-event-code|hang-last-document-code|hang-last-script-code|hang-last-script-failure-code|hang-last-hresult-code|");
    unsigned int required=0, records=0; int position=0;
    while(position>=0) {
        CString line=text.Tokenize(L"\n",position); line.Trim(); if(line.IsEmpty()) continue;
        const int split=line.Find(L'='); if(split<=0) return false;
        const CString key=line.Left(split), value=line.Mid(split+1);
        if(key==L"hang-format") {
            if(value!=L"1" || (records && required!=31)) return false;
            ++records; required=1;
        }
        else if(!records) return false;
        else if(key==L"session_id") { if(value!=expectedSession) return false; required|=2; }
        else if(key==L"hang-state") { if(value!=L"suspected" && value!=L"confirmed" && value!=L"recovered") return false; required|=4; }
        else if(key==L"hang-ui-state") { if(value!=L"heartbeat-timeout" && value!=L"responding") return false; }
        else if(key==L"hang-clock") { if(value!=L"unbiased") return false; }
        else if(key==L"hang-dump") { if(value!=L"not-requested") return false; }
        else if(key==L"hang-utc") {
            if(value.GetLength()!=24 || value[4]!=L'-' || value[7]!=L'-' || value[10]!=L'T' || value[13]!=L':' || value[16]!=L':' || value[19]!=L'.' || value[23]!=L'Z' || !NumericText(value,L"-:TZ.")) return false;
        }
        else if(numericKeys.Find(L"|"+key+L"|")>=0) {
            if(!NumericText(value,L"") || value.GetLength()>20) return false;
            if(key==L"hang-duration-ms") required|=8;
        }
        else if(booleanKeys.Find(L"|"+key+L"|")>=0) { if(value!=L"0" && value!=L"1") return false; if(key==L"hang-confirmed") required|=16; }
        else if(codeKeys.Find(L"|"+key+L"|")>=0) { if(value!=L"unknown" && !FbeDiagnosticPrivacy::EventCode(std::wstring(value.GetString()))) return false; }
        else if(key==L"hang-last-hresult" || key==L"hang-last-failure-hresult") {
            if(value.GetLength()!=10 || value.Left(2)!=L"0x" || !FbeDiagnosticPrivacy::Number(std::wstring(value.GetString()))) return false;
        }
        else if(key==L"operation-statistics-source") { if(value!=L"watchdog-snapshot") return false; }
        else if(StatisticsKey(key)) { if(!StatisticsField(key,value)) return false; }
        else if(!EnvironmentSchema(line)) return false;
    }
    return records>0 && required==31;
}
