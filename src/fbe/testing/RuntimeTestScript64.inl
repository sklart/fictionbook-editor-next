if(IsFbeTestScenario(L"script64-runtime"))
{
	const CString script64Report = CString(DeploymentContext::DiagnosticsDirectory().c_str()) + L"script64-report.txt";
	wchar_t probePath[MAX_PATH] = {};
	const DWORD probeLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SCRIPT64_PATH", probePath, _countof(probePath));
	bool ran = false, undone = false;
	CString results;
	try
	{
		if(m_doc && probeLength > 0 && probeLength < _countof(probePath) &&
			DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable)
		{
			::ShowWindow(m_hWnd, SW_SHOWNORMAL);
			::SetForegroundWindow(m_hWnd);
			m_doc->m_body.SetFocus();
			m_doc->RunScript(probePath);
			results = m_status_state.EffectiveMainText(false, false, CString());
			ran = results.Find(L"ordinary=") >= 0;
			auto pump = []() {
                const ULONGLONG until = ::GetTickCount64() + 100;
                while(::GetTickCount64() < until) { MSG message = {}; while(::PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessage(&message); } ::Sleep(1); }
            };
            pump();
            IServiceProviderPtr service(m_doc->m_body.Document());
            CComPtr<IOleUndoManager> manager;
            if(service) service->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager));
            BSTR description = NULL;
            const HRESULT undoState = manager ? manager->GetLastUndoDescription(&description) : E_NOINTERFACE;
            results.AppendFormat(L"\nundo-state=0x%08lx\nundo-description=%s\n", static_cast<unsigned long>(undoState), description ? description : L"");
            if(description) ::SysFreeString(description);
            m_doc->m_body.SetFocus();
            SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_UNDO, 0), 0);
			pump();
			MSHTML::IHTMLElementPtr paragraph = MSHTML::IHTMLDocument3Ptr(m_doc->m_body.Document())->getElementById(L"script64-probe");
			if(paragraph)
			{
				const _bstr_t text(paragraph->innerText);
				undone = CString(static_cast<const wchar_t*>(text)) == L"abc ab";
				results += L"\nundo-actual=" + CString(static_cast<const wchar_t*>(text)) + L"\n";
			}
		}
	}
	catch(const _com_error& error)
	{
		results.AppendFormat(L"\nnative-error=0x%08lx\n", static_cast<unsigned long>(error.Error()));
	}
	bool passed = ran && undone;
	for(const wchar_t* key : { L"ordinary=1", L"special=1", L"backslash=1", L"quote=1", L"replacement-special=1", L"empty-selection=1", L"empty-list=1", L"invalid-list=1" })
		passed = passed && results.Find(key) >= 0;
	CStringA output;
	output.Format("phase=script64\nharness-ran=%d\nundo=%d\n%s\nresult=%s\n", ran, undone,
		static_cast<LPCSTR>(CW2A(results, CP_UTF8)), passed ? "pass" : "fail");
	WritePortableStateTestText(script64Report, output);
	PostMessage(WM_CLOSE);
	return;
}
