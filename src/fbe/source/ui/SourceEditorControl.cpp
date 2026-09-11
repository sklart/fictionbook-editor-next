#include "stdafx.h"
#include "SourceEditorControl.h"
#include "../../RuntimeLocalization.h"

namespace { int GetLineNumberDigits(int count) { int digits = 1; for(count = count < 1 ? 1 : count; count >= 10; count /= 10) ++digits; return digits < 4 ? 4 : digits; } }

bool SourceEditorControl::Create(HWND parent) { if(!CWindow::Create(L"Scintilla", parent, rcDefault, NULL, WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0)) return false; Send(SCI_USEPOPUP, SC_POPUP_NEVER); ::SetProp(m_hWnd, L"FBE.Next.SourceEditorControl", this); m_previousWindowProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc))); return m_previousWindowProc != NULL; }
void SourceEditorControl::Destroy() { if(IsWindow()) { if(m_previousWindowProc) ::SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_previousWindowProc)); ::RemoveProp(m_hWnd, L"FBE.Next.SourceEditorControl"); DestroyWindow(); } m_previousWindowProc = NULL; m_lineNumberDigits = -1; }
sptr_t SourceEditorControl::Send(UINT message, uptr_t wParam, sptr_t lParam) const { return ::SendMessage(m_hWnd, message, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam)); }
LRESULT CALLBACK SourceEditorControl::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	SourceEditorControl* editor = reinterpret_cast<SourceEditorControl*>(::GetProp(window, L"FBE.Next.SourceEditorControl"));
	if(editor != NULL && message == WM_CONTEXTMENU) { editor->ShowContextMenu(lParam); return 0; }
	return editor != NULL && editor->m_previousWindowProc != NULL ? ::CallWindowProc(editor->m_previousWindowProc, window, message, wParam, lParam) : ::DefWindowProc(window, message, wParam, lParam);
}
void SourceEditorControl::ShowContextMenu(LPARAM screenPosition)
{
	enum Command { Undo = 1, Redo, Cut, Copy, Paste };
	CPoint point(screenPosition);
	if(point.x == -1 && point.y == -1)
	{
		const sptr_t position = Send(SCI_GETCURRENTPOS);
		point.x = static_cast<int>(Send(SCI_POINTXFROMPOSITION, 0, position)); point.y = static_cast<int>(Send(SCI_POINTYFROMPOSITION, 0, position));
		ClientToScreen(&point);
	}
	CMenu menu; menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING | (Send(SCI_CANUNDO) ? MF_ENABLED : MF_GRAYED), Undo, FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.edit.undo", L"Undo"));
	menu.AppendMenu(MF_STRING | (Send(SCI_CANREDO) ? MF_ENABLED : MF_GRAYED), Redo, FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.edit.redo", L"Redo"));
	menu.AppendMenu(MF_SEPARATOR);
	const bool selection = Send(SCI_GETSELECTIONSTART) != Send(SCI_GETSELECTIONEND);
	menu.AppendMenu(MF_STRING | (selection ? MF_ENABLED : MF_GRAYED), Cut, FbeLoadRuntimeStringByKey(L"fbe.context.cut", L"Cut"));
	menu.AppendMenu(MF_STRING | (selection ? MF_ENABLED : MF_GRAYED), Copy, FbeLoadRuntimeStringByKey(L"fbe.context.copy", L"Copy"));
	menu.AppendMenu(MF_STRING | (Send(SCI_CANPASTE) ? MF_ENABLED : MF_GRAYED), Paste, FbeLoadRuntimeStringByKey(L"fbe.context.paste", L"Paste"));
	switch(menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, point.x, point.y, GetParent()))
	{
	case Undo: Send(SCI_UNDO); break; case Redo: Send(SCI_REDO); break; case Cut: Send(SCI_CUT); break; case Copy: Send(SCI_COPY); break; case Paste: Send(SCI_PASTE); break;
	}
}
void SourceEditorControl::UpdateLineNumberMargin(bool force, const SourceEditorConfig& config)
{
	if(!IsWindow()) return;
	if(!config.showLineNumbers) { if(force || m_lineNumberDigits != 0) Send(SCI_SETMARGINWIDTHN, 0, 0); m_lineNumberDigits = 0; return; }
	const int digits = GetLineNumberDigits(static_cast<int>(Send(SCI_GETLINECOUNT)));
	if(!force && m_lineNumberDigits == digits) return;
	CStringA sample; for(int i = 0; i < digits; ++i) sample += '9';
	const int measured = static_cast<int>(Send(SCI_TEXTWIDTH, STYLE_LINENUMBER, reinterpret_cast<sptr_t>(sample.GetString())));
	Send(SCI_SETMARGINWIDTHN, 0, measured > 0 ? measured + 8 : 64); m_lineNumberDigits = digits;
}
void SourceEditorControl::ConfigureSpecialCharacterRepresentations(const SourceEditorConfig& config)
{
	struct Representation { const char* character; const char* label; };
	static const Representation symbols[] = { { "\xC2\xA0", "\xC2\xB0" }, { "\xC2\xAD", "\xC2\xAC" }, { "\xE2\x80\x8B", "ZWSP" }, { "\xE2\x80\x8C", "ZWNJ" }, { "\xE2\x80\x8D", "ZWJ" }, { "\xE2\x80\xAF", "NNBSP" }, { "\xE2\x81\xA0", "WJ" }, { "\xEF\xBB\xBF", "BOM" } };
	static const Representation labels[] = { { "\xC2\xA0", "NBSP" }, { "\xC2\xAD", "SHY" }, { "\xE2\x80\x8B", "ZWSP" }, { "\xE2\x80\x8C", "ZWNJ" }, { "\xE2\x80\x8D", "ZWJ" }, { "\xE2\x80\xAF", "NNBSP" }, { "\xE2\x81\xA0", "WJ" }, { "\xEF\xBB\xBF", "BOM" } };
	const Representation* active = config.specialCharactersStyle ? labels : symbols;
	for(size_t i = 0; i < _countof(symbols); ++i)
	{
		if(config.showSpecialCharacters)
		{
			Send(SCI_SETREPRESENTATION, reinterpret_cast<uptr_t>(symbols[i].character), reinterpret_cast<sptr_t>(active[i].label));
			Send(SCI_SETREPRESENTATIONAPPEARANCE, reinterpret_cast<uptr_t>(symbols[i].character), SC_REPRESENTATION_PLAIN);
		}
		else Send(SCI_CLEARREPRESENTATION, reinterpret_cast<uptr_t>(symbols[i].character));
	}
}
void SourceEditorControl::ApplyConfiguration(const SourceEditorConfig& config)
{
	if(!IsWindow()) return;
	Send(SCI_SETCOMMANDEVENTS, FALSE);
	Send(SCI_SETMODEVENTMASK, SC_MOD_CHANGEFOLD | SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);
	Send(SCI_SETCODEPAGE, SC_CP_UTF8); Send(SCI_SETEOLMODE, SC_EOL_CRLF);
	Send(SCI_SETVIEWEOL, config.showEol); Send(SCI_SETVIEWWS, config.showWhitespace);
	Send(SCI_SETWRAPMODE, config.wrap ? SC_WRAP_WORD : SC_WRAP_NONE);
	Send(SCI_SETLAYOUTCACHE, SC_CACHE_DOCUMENT);
	Send(SCI_SETXCARETPOLICY, CARET_SLOP | CARET_EVEN, 50); Send(SCI_SETYCARETPOLICY, CARET_SLOP | CARET_EVEN, 50);
	ConfigureSpecialCharacterRepresentations(config); UpdateLineNumberMargin(true, config);
	Send(SCI_SETMARGINWIDTHN, 1, 0); Send(SCI_SETFOLDFLAGS, 16);
	Send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("fold"), reinterpret_cast<sptr_t>("1"));
	Send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("fold.html"), reinterpret_cast<sptr_t>("1"));
	Send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("fold.compact"), reinterpret_cast<sptr_t>("1"));
	Send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("fold.flags"), reinterpret_cast<sptr_t>("16"));
	Send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("lexer.xml.allow.asp"), reinterpret_cast<sptr_t>("0"));
	Send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("lexer.xml.allow.php"), reinterpret_cast<sptr_t>("0"));
	Send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("lexer.xml.allow.scripts"), reinterpret_cast<sptr_t>("0"));
	static const char ctrl[] = { 'Q','E','R','S','K',':' };
	for(size_t i = 0; i < _countof(ctrl); ++i) Send(SCI_ASSIGNCMDKEY, ctrl[i] + (SCMOD_CTRL << 16), SCI_NULL);
	static const char ctrlShift[] = { 'Q','W','E','R','Y','O','P','A','S','D','F','G','H','K','Z','X','C','V','B','N',':' };
	for(size_t i = 0; i < _countof(ctrlShift); ++i) Send(SCI_ASSIGNCMDKEY, ctrlShift[i] + ((SCMOD_CTRL + SCMOD_SHIFT) << 16), SCI_NULL);
}
void SourceEditorControl::UpdateMetrics(const SourceEditorConfig& config) { ApplyConfiguration(config); }

void SourceEditorControl::ExpandFold(int& line, bool expand, bool force, int visibleLevels, int level)
{
	const int lastChild = static_cast<int>(Send(SCI_GETLASTCHILD, line, level & SC_FOLDLEVELNUMBERMASK));
	++line;
	while(line <= lastChild)
	{
		if(force) Send(visibleLevels > 0 ? SCI_SHOWLINES : SCI_HIDELINES, line, line);
		else if(expand) Send(SCI_SHOWLINES, line, line);
		int currentLevel = level == -1 ? static_cast<int>(Send(SCI_GETFOLDLEVEL, line)) : level;
		if(currentLevel & SC_FOLDLEVELHEADERFLAG)
		{
			if(force) { Send(SCI_SETFOLDEXPANDED, line, visibleLevels > 1); ExpandFold(line, expand, true, visibleLevels - 1); }
			else { if(expand && !Send(SCI_GETFOLDEXPANDED, line)) Send(SCI_SETFOLDEXPANDED, line, 1); ExpandFold(line, expand, false, visibleLevels - 1); }
		}
		else ++line;
	}
}

void SourceEditorControl::FoldAll()
{
	Send(SCI_COLOURISE, 0, -1);
	const int lines = static_cast<int>(Send(SCI_GETLINECOUNT)); bool expanding = true;
	for(int line = 0; line < lines; ++line) if(Send(SCI_GETFOLDLEVEL, line) & SC_FOLDLEVELHEADERFLAG) { expanding = !Send(SCI_GETFOLDEXPANDED, line); break; }
	for(int line = 0; line < lines; ++line)
	{
		const int level = static_cast<int>(Send(SCI_GETFOLDLEVEL, line));
		if((level & SC_FOLDLEVELHEADERFLAG) && SC_FOLDLEVELBASE == (level & SC_FOLDLEVELNUMBERMASK))
		{
			if(expanding) { Send(SCI_SETFOLDEXPANDED, line, 1); ExpandFold(line, true, false, 0, level); --line; }
			else { const int last = static_cast<int>(Send(SCI_GETLASTCHILD, line, -1)); Send(SCI_SETFOLDEXPANDED, line, 0); if(last > line) Send(SCI_HIDELINES, line + 1, last); }
		}
	}
}

void SourceEditorControl::HandleMarginClick(const SCNotification& notification)
{
	const int line = static_cast<int>(Send(SCI_LINEFROMPOSITION, notification.position));
	if((notification.modifiers & SCMOD_SHIFT) && (notification.modifiers & SCMOD_CTRL)) { FoldAll(); return; }
	const int level = static_cast<int>(Send(SCI_GETFOLDLEVEL, line)); if(!(level & SC_FOLDLEVELHEADERFLAG)) return;
	if(notification.modifiers & SCMOD_SHIFT) { Send(SCI_SETFOLDEXPANDED, line, 1); int target = line; ExpandFold(target, true, true, 100, level); }
	else if(notification.modifiers & SCMOD_CTRL) { const bool expanded = Send(SCI_GETFOLDEXPANDED, line) != 0; Send(SCI_SETFOLDEXPANDED, line, !expanded); int target = line; ExpandFold(target, !expanded, true, expanded ? 0 : 100, level); }
	else Send(SCI_TOGGLEFOLD, line);
}
