#include "stdafx.h"
#include "SourceEditorControl.h"
#include "../../RuntimeLocalization.h"
#include "../../EditorEngine.h"
#include "SciLexer.h"

namespace {
int GetLineNumberDigits(int count) { int digits = 1; for(count = count < 1 ? 1 : count; count >= 10; count /= 10) ++digits; return digits < 4 ? 4 : digits; }
bool IsHighContrastEnabled()
{
	HIGHCONTRAST highContrast = {};
	highContrast.cbSize = sizeof(highContrast);
	return ::SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0) && (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}
COLORREF Color(const SourceEditorConfig& config, SourceEditorColorRole role) { return config.colors[role]; }
}

bool SourceEditorControl::Create(HWND parent) { if(!CWindow::Create(L"Scintilla", parent, rcDefault, NULL, WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0)) return false; Send(SCI_USEPOPUP, SC_POPUP_NEVER); ::SetProp(m_hWnd, L"FBE.Next.SourceEditorControl", this); m_previousWindowProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc))); if(m_previousWindowProc != NULL) return true; ::RemoveProp(m_hWnd, L"FBE.Next.SourceEditorControl"); DestroyWindow(); return false; }
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
void SourceEditorControl::UpdateLineNumberMargin(bool force) { UpdateLineNumberMargin(force, m_config); }
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
	m_config = config;
	Send(SCI_SETCOMMANDEVENTS, FALSE);
	Send(SCI_SETMODEVENTMASK, SC_MOD_CHANGEFOLD | SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);
	Send(SCI_SETUNDOSELECTIONHISTORY, config.undoSelectionHistory ? SC_UNDO_SELECTION_HISTORY_ENABLED | SC_UNDO_SELECTION_HISTORY_SCROLL : 0);
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
	const bool highContrast = IsHighContrastEnabled();
	if(config.syntaxHighlight)
	{
		const COLORREF markerFore = highContrast ? ::GetSysColor(COLOR_WINDOW) : Color(config, SourceEditorColorLineNumber);
		const COLORREF markerBack = highContrast ? ::GetSysColor(COLOR_WINDOWTEXT) : Color(config, SourceEditorColorEditorBackground);
		const COLORREF indicatorColor = highContrast ? ::GetSysColor(COLOR_HIGHLIGHT) : Color(config, SourceEditorColorMatchingTagBorder);
		const COLORREF diagnosticColor = highContrast ? ::GetSysColor(COLOR_HOTLIGHT) : Color(config, SourceEditorColorXmlError);
		Send(SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(CreateEditorLexer("xml")));
		Send(SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL); Send(SCI_SETMARGINWIDTHN, 2, 16); Send(SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS); Send(SCI_SETMARGINSENSITIVEN, 2, 1);
		DefineMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_MINUS, markerFore, markerBack); DefineMarker(SC_MARKNUM_FOLDER, SC_MARK_PLUS, markerFore, markerBack);
		DefineMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_EMPTY, markerFore, markerBack); DefineMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_EMPTY, markerFore, markerBack);
		DefineMarker(SC_MARKNUM_FOLDEREND, SC_MARK_EMPTY, markerFore, markerBack); DefineMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_EMPTY, markerFore, markerBack); DefineMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_EMPTY, markerFore, markerBack);
		Send(SCI_INDICSETSTYLE, EDITOR_INDICATOR_TAG_MATCH, INDIC_ROUNDBOX); Send(SCI_INDICSETALPHA, EDITOR_INDICATOR_TAG_MATCH, 100); Send(SCI_INDICSETUNDER, EDITOR_INDICATOR_TAG_MATCH, TRUE); Send(SCI_INDICSETFORE, EDITOR_INDICATOR_TAG_MATCH, indicatorColor);
		Send(SCI_INDICSETSTYLE, EDITOR_INDICATOR_TAG_ATTRIBUTE, INDIC_ROUNDBOX); Send(SCI_INDICSETALPHA, EDITOR_INDICATOR_TAG_ATTRIBUTE, 100); Send(SCI_INDICSETUNDER, EDITOR_INDICATOR_TAG_ATTRIBUTE, TRUE); Send(SCI_INDICSETFORE, EDITOR_INDICATOR_TAG_ATTRIBUTE, indicatorColor);
		Send(SCI_INDICSETSTYLE, EDITOR_INDICATOR_XML_TAG_INVALID, INDIC_STRIKE); Send(SCI_INDICSETFORE, EDITOR_INDICATOR_XML_TAG_INVALID, diagnosticColor);
		Send(SCI_INDICSETSTYLE, EDITOR_INDICATOR_XML_TAG_MISMATCHED, INDIC_SQUIGGLE); Send(SCI_INDICSETFORE, EDITOR_INDICATOR_XML_TAG_MISMATCHED, diagnosticColor);
		Send(SCI_INDICSETSTYLE, EDITOR_INDICATOR_XML_TAG_MISSING_OPENING, INDIC_DOTS); Send(SCI_INDICSETFORE, EDITOR_INDICATOR_XML_TAG_MISSING_OPENING, highContrast ? ::GetSysColor(COLOR_HOTLIGHT) : Color(config, SourceEditorColorXmlWarning));
		Send(SCI_INDICSETSTYLE, EDITOR_INDICATOR_XML_TAG_MISSING_CLOSING, INDIC_DASH); Send(SCI_INDICSETFORE, EDITOR_INDICATOR_XML_TAG_MISSING_CLOSING, highContrast ? ::GetSysColor(COLOR_HOTLIGHT) : Color(config, SourceEditorColorXmlWarning));
	}
	else { Send(SCI_SETILEXER, 0, 0); Send(SCI_SETMARGINWIDTHN, 2, 0); }
	ApplyStyles(config);
}
void SourceEditorControl::UpdateMetrics(const SourceEditorConfig& config) { ApplyConfiguration(config); }

void SourceEditorControl::DefineMarker(int marker, int markerType, COLORREF fore, COLORREF back) { Send(SCI_MARKERDEFINE, marker, markerType); Send(SCI_MARKERSETFORE, marker, fore); Send(SCI_MARKERSETBACK, marker, back); }

void SourceEditorControl::ApplyStyles(const SourceEditorConfig& config)
{
	const bool highContrast = IsHighContrastEnabled();
	const COLORREF windowText = highContrast ? ::GetSysColor(COLOR_WINDOWTEXT) : Color(config, SourceEditorColorEditorForeground);
	const COLORREF windowBackground = highContrast ? ::GetSysColor(COLOR_WINDOW) : Color(config, SourceEditorColorEditorBackground);
	Send(WM_SETREDRAW, FALSE); Send(SCI_STYLERESETDEFAULT);
	CT2A font(config.fontName); Send(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>(font.m_psz)); Send(SCI_STYLESETSIZE, STYLE_DEFAULT, config.fontSize); Send(SCI_STYLESETFORE, STYLE_DEFAULT, windowText); Send(SCI_STYLESETBACK, STYLE_DEFAULT, windowBackground);
	Send(SCI_STYLECLEARALL); Send(SCI_STYLESETFORE, STYLE_LINENUMBER, highContrast ? windowText : Color(config, SourceEditorColorLineNumber)); Send(SCI_STYLESETBACK, STYLE_LINENUMBER, windowBackground); Send(SCI_SETCARETFORE, highContrast ? windowText : Color(config, SourceEditorColorCaret));
	if(highContrast) Send(SCI_SETCARETLINEVISIBLE, FALSE); else { Send(SCI_SETCARETLINEBACK, Color(config, SourceEditorColorCurrentLineBackground)); Send(SCI_SETCARETLINEVISIBLE, TRUE); }
	Send(SCI_SETSELFORE, TRUE, highContrast ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : Color(config, SourceEditorColorSelectionForeground)); Send(SCI_SETSELBACK, TRUE, highContrast ? ::GetSysColor(COLOR_HIGHLIGHT) : Color(config, SourceEditorColorSelectionBackground));
	Send(SCI_STYLESETFORE, STYLE_BRACELIGHT, highContrast ? windowText : Color(config, SourceEditorColorXmlTagName)); Send(SCI_STYLESETBACK, STYLE_BRACELIGHT, highContrast ? windowBackground : Color(config, SourceEditorColorMatchingTagBackground));
	struct Style { int scintilla; SourceEditorColorRole color; };
	static const Style styles[] = {{SCE_H_DEFAULT, SourceEditorColorXmlText},{SCE_H_TAG, SourceEditorColorXmlTagName},{SCE_H_TAGUNKNOWN, SourceEditorColorXmlTagName},{SCE_H_ATTRIBUTE, SourceEditorColorXmlAttributeName},{SCE_H_ATTRIBUTEUNKNOWN, SourceEditorColorXmlAttributeName},{SCE_H_NUMBER, SourceEditorColorXmlAttributeValue},{SCE_H_DOUBLESTRING, SourceEditorColorXmlAttributeValue},{SCE_H_SINGLESTRING, SourceEditorColorXmlAttributeValue},{SCE_H_OTHER, SourceEditorColorXmlTagDelimiter},{SCE_H_COMMENT, SourceEditorColorXmlComment},{SCE_H_ENTITY, SourceEditorColorXmlEntity},{SCE_H_TAGEND, SourceEditorColorXmlTagDelimiter},{SCE_H_XMLSTART, SourceEditorColorXmlProcessingInstruction},{SCE_H_XMLEND, SourceEditorColorXmlProcessingInstruction},{SCE_H_SCRIPT, SourceEditorColorXmlAttributeValue},{SCE_H_ASP, SourceEditorColorXmlProcessingInstruction},{SCE_H_ASPAT, SourceEditorColorXmlProcessingInstruction},{SCE_H_CDATA, SourceEditorColorXmlCdata},{SCE_H_QUESTION, SourceEditorColorXmlProcessingInstruction},{SCE_H_VALUE, SourceEditorColorXmlAttributeValue},{SCE_H_XCCOMMENT, SourceEditorColorXmlComment},{SCE_H_SGML_DEFAULT, SourceEditorColorXmlDoctype},{SCE_H_SGML_COMMAND, SourceEditorColorXmlDoctype},{SCE_H_SGML_1ST_PARAM, SourceEditorColorXmlAttributeName},{SCE_H_SGML_DOUBLESTRING, SourceEditorColorXmlAttributeValue},{SCE_H_SGML_SIMPLESTRING, SourceEditorColorXmlAttributeValue},{SCE_H_SGML_ERROR, SourceEditorColorXmlError},{SCE_H_SGML_SPECIAL, SourceEditorColorXmlDoctype},{SCE_H_SGML_ENTITY, SourceEditorColorXmlEntity},{SCE_H_SGML_COMMENT, SourceEditorColorXmlComment},{SCE_H_SGML_1ST_PARAM_COMMENT, SourceEditorColorXmlComment},{SCE_H_SGML_BLOCK_DEFAULT, SourceEditorColorXmlDoctype}};
	if(config.syntaxHighlight && !highContrast) for(size_t i = 0; i < _countof(styles); ++i) Send(SCI_STYLESETFORE, styles[i].scintilla, Color(config, styles[i].color));
	Send(SCI_COLOURISE, 0, -1); Send(WM_SETREDRAW, TRUE); Invalidate();
}

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

void SourceEditorControl::HandleModified(const SCNotification& notification)
{
	if(notification.modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) m_tagMatchState.Invalidate();
	if(!(notification.modificationType & SC_MOD_CHANGEFOLD)) return;
	if(notification.foldLevelNow & SC_FOLDLEVELHEADERFLAG)
	{
		if(!(notification.foldLevelPrev & SC_FOLDLEVELHEADERFLAG)) Send(SCI_SETFOLDEXPANDED, notification.line, 1);
	}
	else if((notification.foldLevelPrev & SC_FOLDLEVELHEADERFLAG) && !Send(SCI_GETFOLDEXPANDED, notification.line))
	{
		int line = notification.line;
		ExpandFold(line, true, false, 0, notification.foldLevelPrev);
	}
}

bool SourceEditorControl::UpdateTagHighlight(const XmlTagHighlightOptions& options)
{
	XmlSourceTagHighlighter highlighter(this, &m_tagMatchState);
	return highlighter.UpdateHighlight(options);
}

bool SourceEditorControl::GotoMatchingTag()
{
	XmlSourceTagHighlighter highlighter(this, &m_tagMatchState);
	return highlighter.GotoMatchingTag();
}

void SourceEditorControl::GotoWrongTag()
{
	XmlSourceTagHighlighter highlighter(this, &m_tagMatchState);
	highlighter.GotoWrongTag();
}

SourceEditorControlDiagnostics SourceEditorControl::RunDiagnostics()
{
	SourceEditorControlDiagnostics diagnostics;
	diagnostics.created = IsWindow() != FALSE;
	if(!diagnostics.created) return diagnostics;
	diagnostics.utf8 = Send(SCI_GETCODEPAGE) == SC_CP_UTF8;
	diagnostics.eol = Send(SCI_GETEOLMODE) == SC_EOL_CRLF;
	diagnostics.wrapping = Send(SCI_GETWRAPMODE) == (m_config.wrap ? SC_WRAP_WORD : SC_WRAP_NONE);
	diagnostics.whitespace = Send(SCI_GETVIEWWS) == static_cast<sptr_t>(m_config.showWhitespace);
	diagnostics.lineNumbers = Send(SCI_GETMARGINWIDTHN, 0) >= 0;
	diagnostics.folding = Send(SCI_GETMARGINWIDTHN, 2) == (m_config.syntaxHighlight ? 16 : 0);
	ApplyConfiguration(m_config);
	UpdateMetrics(m_config);
	diagnostics.reapply = IsWindow() != FALSE && Send(SCI_GETCODEPAGE) == SC_CP_UTF8;
	return diagnostics;
}
