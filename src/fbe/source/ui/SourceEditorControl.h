#pragma once

#include <atlwin.h>
#include "Scintilla.h"
#include "../../xmlMatchedTagsHighlighter.h"
#include "SourceEditorConfig.h"

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
	bool GotoMatchingTag();
	void GotoWrongTag();
private:
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
