#pragma once

#include <windows.h>

// Event-driven diagnostics. Enable with FBE_NEXT_CARET_DIAGNOSTICS=1.
// HWNDs and caret state are logged; document text and selection data are not.
inline void ReportCaretFocusState(LPCWSTR event, HWND expectedEditor, bool sourceMode)
{
	wchar_t enabled[2] = {};
	if(::GetEnvironmentVariableW(L"FBE_NEXT_CARET_DIAGNOSTICS", enabled, _countof(enabled)) != 1 || enabled[0] != L'1') return;
	GUITHREADINFO state = { sizeof(state) };
	if(!::GetGUIThreadInfo(::GetCurrentThreadId(), &state)) return;
	wchar_t message[320] = {};
	const bool active = expectedEditor != NULL &&
		(state.hwndFocus == expectedEditor || ::IsChild(expectedEditor, state.hwndFocus));
	::swprintf_s(message, L"FBE caret: %s mode=%s active=%d expected=%p focus=%p caret=%p flags=0x%lx blink=%u\n",
		event, sourceMode ? L"source" : L"design", active ? 1 : 0, expectedEditor,
		state.hwndFocus, state.hwndCaret, state.flags, ::GetCaretBlinkTime());
	::OutputDebugStringW(message);
}
