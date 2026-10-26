#pragma once

#include <atlwin.h>
#include "Scintilla.h"
#include "../../xmlMatchedTagsHighlighter.h"
#include "SourceEditorConfig.h"

#ifdef FBE_SOURCE_MEMORY_BENCHMARK
inline bool IsSourceBenchmarkUserInput(UINT message)
{
	return (message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
		(message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) ||
		(message >= WM_IME_SETCONTEXT && message <= WM_IME_KEYUP) ||
		message == WM_IME_STARTCOMPOSITION || message == WM_IME_ENDCOMPOSITION ||
		message == WM_IME_COMPOSITION || message == WM_INPUT || message == WM_CONTEXTMENU ||
		message == WM_PASTE || message == WM_CUT || message == WM_CLEAR || message == WM_UNDO ||
		message == WM_DROPFILES;
}
#endif

struct SourceEditorControlDiagnostics
{
	bool created = false;
	bool utf8 = false;
	bool eol = false;
	bool eolVisibility = false;
	bool wrapping = false;
	bool whitespace = false;
	bool lineNumbers = false;
	bool folding = false;
	bool styles = false;
	bool tagState = false;
	bool metrics = false;
	bool reapply = false;
	bool caretEnhanced = false;
	bool caretCustom = false;
	bool caretDpi = false;
	bool caretSelection = false;
	bool caretUndo = false;
};

class SourceEditorControl : public CWindow
{
public:
	bool Create(HWND parent);
	void Destroy();
	HWND Handle() const { return m_hWnd; }
	sptr_t Send(UINT message, uptr_t wParam = 0, sptr_t lParam = 0) const;
	void ApplyConfiguration(const SourceEditorConfig& config);
	void UpdateMetrics(const SourceEditorConfig& config);
	void UpdateLineNumberMargin(bool force);
	void UpdateLineNumberMargin(bool force, const SourceEditorConfig& config);
	void ConfigureSpecialCharacterRepresentations(const SourceEditorConfig& config);
	void FoldAll();
	void HandleMarginClick(const SCNotification& notification);
	void HandleModified(const SCNotification& notification);
	bool UpdateTagHighlight(const XmlTagHighlightOptions& options);
	bool HasMatchingTag();
	bool GotoMatchingTag();
	void GotoWrongTag();
	SourceEditorControlDiagnostics RunDiagnostics();
#ifdef FBE_SOURCE_MEMORY_BENCHMARK
	void IsolateBenchmarkInput() { m_benchmarkInputIsolated = true; }
	unsigned long long BenchmarkDiscardedInputMessages() const { return m_benchmarkDiscardedInput; }
#endif
private:
#ifdef FBE_SOURCE_MEMORY_BENCHMARK
	bool m_benchmarkInputIsolated = false;
	unsigned long long m_benchmarkDiscardedInput = 0;
#endif
	int m_lineNumberDigits = -1;
	SourceEditorConfig m_config;
	XmlMatchedTagsState m_tagMatchState;
	WNDPROC m_previousWindowProc = NULL;
	void ApplyStyles(const SourceEditorConfig& config);
	void DefineMarker(int marker, int markerType, COLORREF fore, COLORREF back);
	void ExpandFold(int& line, bool expand, bool force = false, int visibleLevels = 0, int level = -1);
	static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	void ShowContextMenu(LPARAM screenPosition);
};
