if (IsFbeTestScenario(L"community-settings-search-performance"))
{
	static LARGE_INTEGER frequency = {}, openedAt = {};
	static CStringA settingsRows;
	static bool settingsOk = false;
	::QueryPerformanceFrequency(&frequency);
	auto now = []() { LARGE_INTEGER value = {}; ::QueryPerformanceCounter(&value); return value.QuadPart * 1000.0 / frequency.QuadPart; };
	CStringA report("operation\trepeat\tpage\telapsed_ms\tvalid\r\n");
	bool passed = true;
	for (int repeat = 0; repeat < 6; ++repeat)
	{
		settingsRows.Empty(); settingsOk = false;
		::QueryPerformanceCounter(&openedAt);
		const UINT_PTR timer = ::SetTimer(NULL, 0, 1, [](HWND, UINT, UINT_PTR id, DWORD) {
			::KillTimer(NULL, id);
			const HWND dialog = ::GetActiveWindow();
			const HWND navigation = ::GetDlgItem(dialog, IDC_SETTINGS_NAV);
			LARGE_INTEGER current = {}; ::QueryPerformanceCounter(&current);
			const double opening = (current.QuadPart - openedAt.QuadPart) * 1000.0 / frequency.QuadPart;
			if (!navigation) return;
			settingsOk = ::SendMessage(navigation, LB_GETCOUNT, 0, 0) == 8;
			settingsRows.AppendFormat("settings_open\t%d\t%d\t%.3f\t%d\r\n", 0, -1, opening, settingsOk ? 1 : 0);
			for (int page = 1; page < 8; ++page)
			{
				::QueryPerformanceCounter(&current); const LONGLONG start = current.QuadPart;
				::SendMessage(navigation, LB_SETCURSEL, page, 0);
				::SendMessage(dialog, WM_COMMAND, MAKEWPARAM(IDC_SETTINGS_NAV, LBN_SELCHANGE), reinterpret_cast<LPARAM>(navigation));
				::QueryPerformanceCounter(&current);
				const bool valid = ::SendMessage(navigation, LB_GETCURSEL, 0, 0) == page;
				settingsOk = settingsOk && valid;
				settingsRows.AppendFormat("settings_page\t%d\t%d\t%.3f\t%d\r\n", 0, page, (current.QuadPart - start) * 1000.0 / frequency.QuadPart, valid ? 1 : 0);
			}
			::SendMessage(dialog, WM_COMMAND, IDCANCEL, 0);
		});
		if (!timer) { passed = false; break; }
		ShowSettingsDialog(m_hWnd);
		::KillTimer(NULL, timer);
		passed = passed && settingsOk;
		CStringA repeatMarker; repeatMarker.Format("\t%d\t", repeat);
		settingsRows.Replace("\t0\t", repeatMarker);
		report += settingsRows;
	}
	for (int source = 0; source < 2; ++source)
	{
		ShowView(source ? SOURCE : BODY);
		for (int repeat = 0; repeat < 6; ++repeat)
		{
			::SetActiveWindow(m_hWnd);
			if (source) m_source.SetFocus(); else m_doc->m_body.SetFocus();
			const double start = now();
			SendMessage(WM_COMMAND, ID_EDIT_FIND, 0);
			const double elapsed = now() - start;
			const HWND dialog = ::GetActiveWindow();
			const HWND input = ::GetDlgItem(dialog, IDC_TEXT);
			COMBOBOXINFO combo = {}; combo.cbSize = sizeof(combo);
			const bool hasEdit = input && ::GetComboBoxInfo(input, &combo) && combo.hwndItem;
			const bool focused = hasEdit && (::GetFocus() == combo.hwndItem || ::GetFocus() == input);
			if (focused) { ::SetWindowText(combo.hwndItem, L""); ::SendMessage(combo.hwndItem, WM_CHAR, L'x', 0); }
			const bool valid = focused && ::IsWindowEnabled(input) && ::GetWindowTextLength(combo.hwndItem) == 1;
			report.AppendFormat("find_%s\t%d\t-1\t%.3f\t%d\r\n", source ? "source" : "body", repeat, elapsed, valid ? 1 : 0);
			passed = passed && valid;
			if (valid) ::SendMessage(dialog, WM_CLOSE, 0, 0);
		}
	}
	DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
	::PostQuitMessage(passed ? 0 : 1); return 0;
}
