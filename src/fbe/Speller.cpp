#include "stdafx.h"
#include "resource.h"
#include "res1.h"
#include <iostream>
#include <fstream>
#include "FBE.h"
#include "Speller.h"
#include "SpellParagraphTraversal.h"
#include "RuntimeLocalization.h"
#include "StartupTrace.h"

static void SetRuntimeSpellText(HWND dialog, int controlId, LPCWSTR key, LPCWSTR fallback)
{
	const CString text = FbeLoadRuntimeStringByKey(key, fallback);
	if (!text.IsEmpty())
		::SetDlgItemText(dialog, controlId, text);
}

const CString Tokens(L" .,?�!��\r\n\t\"������:;<>(){}[]\u00A0\u2003\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u200B\u202F\u205F\u2060\u3000\u2012\u2013\u2014\u00BA\u25A1\u25AB\u25E6\u201e\u201c");

// spell check dialog initialisation
LRESULT CSpellDialog::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_SPELL_CHECK);
	m_BadWord = GetDlgItem(IDC_SPELL_BEDWORD);
	m_Replacement = GetDlgItem(IDC_SPELL_REPLACEMENT);
	m_Suggestions = GetDlgItem(IDC_SPELL_SUGG_LIST);
	m_IgnoreContinue = GetDlgItem(IDC_SPELL_IGNORE);
	m_WasSuspended = false;

	SetWindowText(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_spell_check.caption", L"Spell Check"));
	SetRuntimeSpellText(m_hWnd, IDCANCEL, L"fbe.dialog.idd_spell_check.cancel", L"Cancel");
	SetRuntimeSpellText(m_hWnd, IDC_SPELL_IGNORE, L"fbe.dialog.idd_spell_check.ignore", L"Ignore");
	SetRuntimeSpellText(m_hWnd, IDC_SPELL_IGNOREALL, L"fbe.dialog.idd_spell_check.ignore_all", L"Ignore All");
	SetRuntimeSpellText(m_hWnd, IDC_SPELL_CHANGE, L"fbe.dialog.idd_spell_check.change", L"Change");
	SetRuntimeSpellText(m_hWnd, IDC_SPELL_CHANGEALL, L"fbe.dialog.idd_spell_check.change_all", L"Change All");
	SetRuntimeSpellText(m_hWnd, IDC_SPELL_ADD, L"fbe.dialog.idd_spell_check.add", L"Add");
	SetRuntimeSpellText(m_hWnd, IDC_SPELL_SUGGEST, L"fbe.dialog.idd_spell_check.suggest", L"Suggest");
	SetRuntimeSpellText(m_hWnd, IDC_SPELL_UNDO, L"fbe.dialog.idd_spell_check.undo", L"Undo");

	UpdateData();
	CenterWindow();
	return 1;
}

LRESULT CSpellDialog::OnActivate(UINT, WPARAM wParam, LPARAM, BOOL&)
{
	if (wParam == WA_INACTIVE)
	{
		CString txt;
		txt = FbeLoadCString(IDS_SPELL_CONTINUE);
		m_IgnoreContinue.SetWindowText (txt);

		GetDlgItem(IDC_SPELL_IGNOREALL).EnableWindow(FALSE);
		GetDlgItem(IDC_SPELL_CHANGE).EnableWindow(FALSE);
		GetDlgItem(IDC_SPELL_CHANGEALL).EnableWindow(FALSE);
		GetDlgItem(IDC_SPELL_ADD).EnableWindow(FALSE);
		m_Replacement.EnableWindow(FALSE);
		m_Suggestions.EnableWindow(FALSE);
		m_WasSuspended = true;

		UpdateWindow();
	}
	return 0;
}

LRESULT CSpellDialog::UpdateData()
{
	m_BadWord.SetWindowText (m_sBadWord);
	m_Suggestions.ResetContent();
	m_Replacement.SetWindowText(L"");
	m_sReplacement.Empty();
	if (m_strSuggestions)
		for (int i=0; i<m_strSuggestions->GetSize(); i++)
		{
			m_Suggestions.AddString((*m_strSuggestions)[i]);
		}
	if (m_strSuggestions && m_strSuggestions->GetSize() > 0) {
		m_Suggestions.SetCurSel(0);
		m_Replacement.SetWindowText((*m_strSuggestions)[0]);
	}

	if(m_Speller->GetUndoState())
		GetDlgItem(IDC_SPELL_UNDO).EnableWindow(TRUE);
	else
		GetDlgItem(IDC_SPELL_UNDO).EnableWindow(FALSE);

	return 1;
}

LRESULT CSpellDialog::OnSelChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CString strText;
	if (m_Suggestions.GetText(m_Suggestions.GetCurSel(),strText))
		m_Replacement.SetWindowText(strText);
	return 0;
}

// change text to suggested word on doubleclick
LRESULT CSpellDialog::OnSelDblClick(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CString strText;
	if (m_Suggestions.GetText(m_Suggestions.GetCurSel(),strText))
	{
		m_Replacement.SetWindowText(strText);
		OnChange(0, 0, 0, bHandled);
	}
	return 0;
}

LRESULT CSpellDialog::OnEditChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	m_Replacement.GetWindowText(m_sReplacement);
	return 0;
}

LRESULT CSpellDialog::OnCancel(WORD, WORD wID, HWND, BOOL&) 
{ 
	ATLASSERT(m_Speller!=NULL);
	m_Speller->EndDocumentCheck();
	return 0;
}

LRESULT CSpellDialog::OnIgnore(WORD, WORD wID, HWND, BOOL&)
{
	ATLASSERT(m_Speller!=NULL);
	if (m_WasSuspended)
	{
		CString txt;
		txt = FbeLoadCString(IDC_SPELL_IGNORE);
		m_IgnoreContinue.SetWindowText (txt);

		GetDlgItem(IDC_SPELL_IGNOREALL).EnableWindow(TRUE);
		GetDlgItem(IDC_SPELL_CHANGE).EnableWindow(TRUE);
		GetDlgItem(IDC_SPELL_CHANGEALL).EnableWindow(TRUE);
		GetDlgItem(IDC_SPELL_ADD).EnableWindow(TRUE);
		m_Replacement.EnableWindow(TRUE);
		m_Suggestions.EnableWindow(TRUE);
		m_WasSuspended = false;

		UpdateWindow();
		m_Speller->StartDocumentCheck();
	}
	else 
		m_Speller->ContinueDocumentCheck();

	return 0;
}

LRESULT CSpellDialog::OnIgnoreAll(WORD, WORD wID, HWND, BOOL&)
{
	ATLASSERT(m_Speller!=NULL);
	m_Speller->IgnoreAll(m_sBadWord);
	m_Speller->ContinueDocumentCheck();
	return 0;
}

LRESULT CSpellDialog::OnChange(WORD, WORD wID, HWND, BOOL&)
{
	ATLASSERT(m_Speller!=NULL);
	m_Speller->BeginUndoUnit(L"replace word");
	m_Speller->Replace(m_sReplacement);
	m_Speller->EndUndoUnit();
	m_Speller->ContinueDocumentCheck();
	return 0;
}

LRESULT CSpellDialog::OnChangeAll(WORD, WORD wID, HWND, BOOL&)
{
	ATLASSERT(m_Speller!=NULL);
	m_Speller->AddReplacement(m_sBadWord,m_sReplacement);
	m_Speller->BeginUndoUnit(L"replace word");
	m_Speller->Replace(m_sReplacement);
	m_Speller->EndUndoUnit();
	m_Speller->ContinueDocumentCheck();
	return 0;
}

LRESULT CSpellDialog::OnAdd(WORD, WORD wID, HWND, BOOL&)
{
	ATLASSERT(m_Speller!=NULL);
	m_Speller->AddToDictionary(m_sBadWord);
	m_Speller->ContinueDocumentCheck();
	return 0;
}

LRESULT CSpellDialog::OnUndo(WORD, WORD wID, HWND, BOOL&)
{
	ATLASSERT(m_Speller!=NULL);
	m_Speller->Undo();
	UpdateData();
	return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////
//
// CSpeller methods
//
////////////////////////////////////////////////////////////////////////////////////////////

Hunhandle* CSpeller::LoadDictionary(CString dictPath, CString dictName)
{
	USES_CONVERSION;
	Hunhandle* dict = NULL;

	if ( ATLPath::FileExists(dictPath+dictName+L".aff") && ATLPath::FileExists(dictPath+dictName+L".dic"))
	{
		// create dictionary from file
		const CStringA affPath = T2A(dictPath + dictName + L".aff");
		const CStringA dicPath = T2A(dictPath + dictName + L".dic");
		dict = Hunspell_create(affPath, dicPath);
	}
	return dict;
}
UINT CSpeller::DetectDictionaryCodePage(Hunhandle* dict, UINT fallbackCodePage)
{
	if (!dict)
		return fallbackCodePage;
	const char* encoding = Hunspell_get_dic_encoding(dict);
	if (!encoding || !*encoding)
		return fallbackCodePage;
	CStringA encodingName(encoding);
	encodingName.MakeUpper();
	if (encodingName == "UTF-8" || encodingName == "UTF8")
		return CP_UTF8;
	if (encodingName == "KOI8-R")
		return 20866;
	if (encodingName == "WINDOWS-1251" || encodingName == "CP1251")
		return 1251;
	if (encodingName == "ISO8859-1" || encodingName == "ISO-8859-1")
		return 28591;
	if (encodingName == "ISO8859-2" || encodingName == "ISO-8859-2")
		return 28592;
	if (encodingName == "ISO8859-15" || encodingName == "ISO-8859-15")
		return 28605;
	return fallbackCodePage;
}

//
// CSpeller constructor
//
CSpeller::CSpeller(CString dictPath):
	m_prevSelRange(nullptr), m_spell_dlg(nullptr), m_Enabled(true),
	m_HighlightMisspells(false), m_testCheckElementCalls(0), m_testVisitedParagraphs(0), m_prevY(0), m_codePage(CP_UTF8),
	m_frame(nullptr), m_Lang(LANG_EN),
	m_menuSuggestions(nullptr), m_DictPath(dictPath),
	m_CustomDictCodepage(CP_UTF8), splitter(nullptr)
{

	// initialize all dictionaries
	for (int i=LANG_EN; i<=LANG_NONE; i++)
		m_Dictionaries.Add(dicts[i]);

	// but load only English and Russian dictionaries 
	m_Dictionaries[LANG_EN].handle = LoadDictionary(dictPath, dicts[LANG_EN].name);
	 m_Dictionaries[LANG_EN].codepage = DetectDictionaryCodePage(m_Dictionaries[LANG_EN].handle, m_Dictionaries[LANG_EN].codepage);
	 m_Dictionaries[LANG_RU].handle = LoadDictionary(dictPath, dicts[LANG_RU].name);
	 m_Dictionaries[LANG_RU].codepage = DetectDictionaryCodePage(m_Dictionaries[LANG_RU].handle, m_Dictionaries[LANG_RU].codepage);

	// Keep ASCII, typographic, and modifier-letter apostrophes inside tokens.
	// The complete English and Ukrainian word must reach Hunspell.
	splitter = new CSplitter(FbeSpellAlphaExceptions());
}

//
// CSpeller destructor
//
CSpeller::~CSpeller()
{
	EndDocumentCheck();
	delete m_menuSuggestions;
	delete splitter;
	// unload dictionaries
	for (int i=0; i<m_Dictionaries.GetSize(); i++)
		if (m_Dictionaries[i].handle) 
			Hunspell_destroy (m_Dictionaries[i].handle);
}

void CSpeller::AttachDocument(MSHTML::IHTMLDocumentPtr doc)
{
	if (!doc)
	{
		StartupTrace::Warning(L"speller", L"SP100", L"AttachDocument deferred: document is null");
		return;
	}
	// cansel spell check and destroy dialog
	EndDocumentCheck();

	// clear previous marks
	m_ElementHighlights.clear();

	// clear collected ID's
	m_uniqIDs.clear();

	// clear (assigned to previous document) service arrays
	m_IgnoreWords.RemoveAll();
	m_ChangeWords.RemoveAll();
	m_ChangeWordsTo.RemoveAll();

	// assign new document: all interfaces and variables
	m_doc2 = MSHTML::IHTMLDocument2Ptr(doc);

	// get web browser component
	MSHTML::IHTMLWindow2Ptr pWin(m_doc2->parentWindow);
	IServiceProviderPtr pISP(pWin);
	pISP->QueryService(IID_IWebBrowserApp, IID_IWebBrowser2,(void **)&m_browser); 

	m_doc3 = MSHTML::IHTMLDocument3Ptr(doc);
	m_doc4 = MSHTML::IHTMLDocument4Ptr(doc);
	m_fbw_body = m_doc3->getElementById(L"fbw_body");
	m_scrollElement = m_doc3->documentElement;
	m_mkc = MSHTML::IMarkupContainer2Ptr(m_doc4);
	m_ims = MSHTML::IMarkupServicesPtr(m_doc4);
	m_ihrs = MSHTML::IHighlightRenderingServicesPtr(m_doc4);
    m_ids = MSHTML::IDisplayServicesPtr(m_doc4);

	// create a render style (wavy red line)
	_bstr_t b;
	m_irs = m_doc4->createRenderStyle(b);
	m_irs->defaultTextSelection = "false";
	m_irs->textBackgroundColor = "transparent";
	m_irs->textColor = "transparent";
	m_irs->textDecoration = "underline";
	m_irs->textDecorationColor = "red";
	m_irs->textUnderlineStyle = "wave";

	// detect document language
	SetDocumentLanguage();
}

void CSpeller::SetDocumentLanguage()
{
	m_Lang = LANG_NONE;
	MSHTML::IHTMLSelectElementPtr elem = MSHTML::IHTMLDocument3Ptr(m_doc4)->getElementById(L"tiLang");
	if (elem)
	{
		CString lang = elem->value;
		for (int i=LANG_EN; i<LANG_NONE; i++)
			if (m_Dictionaries[i].name.Find(lang) == 0)
			{
				// load dictionary (if needed)
				if (!m_Dictionaries[i].handle)
					m_Dictionaries[i].handle = LoadDictionary(m_DictPath, dicts[i].name);
				m_Dictionaries[i].codepage = DetectDictionaryCodePage(m_Dictionaries[i].handle, m_Dictionaries[i].codepage);
				m_Lang = dicts[i].lang;
				// special hack: for the bilingual spell-check of Russian texts
				// let's select English as a second language 
				// Russian words will be detected automatically
				if (m_Lang == LANG_RU) m_Lang = LANG_EN;
				break;
			}
	}
	// initiate background check
	if (m_Dictionaries[m_Lang].handle) HighlightMisspells();
	// no dictionary or language not supported
	else SetEnabled (false);
}

// 
// Return selected word or word under caret
// 
MSHTML::IHTMLTxtRangePtr CSpeller::GetSelWordRange()
{
	MSHTML::IHTMLTxtRangePtr rng = 0;
	// fetch selection
	MSHTML::IHTMLTxtRangePtr selRange(m_doc2->selection->createRange());
	if (selRange) rng = selRange->duplicate();
	if (rng)
	{
		CString s = rng->text;
		if (!s.IsEmpty()) rng->collapse(VARIANT_TRUE);
		rng->expand(L"word");
		return rng;
	}
	else return 0;
}

// 
// Return word under caret
// 
CString CSpeller::GetSelWord()
{
	CString word(L"");
	MSHTML::IHTMLTxtRangePtr range = GetSelWordRange();
	if (range) word.SetString(range->text);
	return word.Trim();
}

//
// Do a spell-check and append popup menu (if nessasary)
// 
void CSpeller::AppendSpellMenu (HMENU menu)
{
	CString word = GetSelWord();
	if (word.IsEmpty()) return;
	
	if (SpellCheck(word) != SPELL_OK)
	{
		CStrings* suggestions = GetSuggestions(word);
		delete m_menuSuggestions;
		m_menuSuggestions = suggestions;
		const int suggestionCount = m_menuSuggestions->GetSize();
		const int commandCapacity = ID_SPELL_REPLACE_LAST - ID_SPELL_REPLACE_FIRST + 1;
		// Bundled Hunspell limits suggest() to MAXSUGGESTION (15), well inside
		// the reserved command range.  Do not trim or reorder this list.
		if (suggestionCount > commandCapacity) {
			ATLASSERT(FALSE);
			return;
		}
		const int numSuggestions = suggestionCount;

		::AppendMenu(menu, MF_SEPARATOR, 0, NULL);
		const int primarySuggestionCount = numSuggestions < 8 ? numSuggestions : 8;
		for (int i=0; i<primarySuggestionCount; i++)
			::AppendMenu(menu, MF_STRING, ID_SPELL_REPLACE_FIRST+i, (*m_menuSuggestions)[i]);
		if (numSuggestions > primarySuggestionCount) {
			HMENU moreMenu = ::CreatePopupMenu();
			for (int i=primarySuggestionCount; i<numSuggestions; i++)
				::AppendMenu(moreMenu, MF_STRING, ID_SPELL_REPLACE_FIRST+i, (*m_menuSuggestions)[i]);
			CString moreLabel = FbeLoadRuntimeStringByKey(L"fbe.spelling.menu.more_suggestions", L"More suggestions");
			::AppendMenu(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(moreMenu), moreLabel);
		}
		if (numSuggestions > 0)
			::AppendMenu(menu, MF_SEPARATOR, 0, NULL);

		CString itemName;
		itemName = FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_spell_check.ignore_all", L"Ignore all");
		::AppendMenu(menu, MF_STRING, IDC_SPELL_IGNOREALL, itemName);

		itemName = FbeLoadRuntimeStringByKey(L"fbe.spelling.menu.add_to_dictionary", L"Add to dictionary");
		::AppendMenu(menu, MF_STRING, IDC_SPELL_ADD2DICT, itemName);
	}
}

//
// Replace misspelled word from the correct variant (from popup menu)
//
void CSpeller::Replace(int nIndex)
{
	if (m_menuSuggestions == nullptr || nIndex < 0 || nIndex >= m_menuSuggestions->GetSize()) return;
	MSHTML::IHTMLTxtRangePtr range = GetSelWordRange();
	CString addSpace = range->text;
	if (addSpace.Right(1) == L" ") addSpace.SetString(L" "); else addSpace.SetString(L"");
	try
	{ 
		CString replace = FbeRestoreSourceApostropheStyle(m_CurrentSpellWord, (*m_menuSuggestions)[nIndex]);
		 replace = replace + addSpace; 
		_bstr_t b = replace.AllocSysString();
		range->put_text(b);
	}
	catch(...){}
}

//
// Replace misspelled word from the correct variant (from dialog)
//
void CSpeller::Replace(CString word)
{
	if (m_selRange)
	{
		word = FbeRestoreSourceApostropheStyle(m_CurrentSpellWord, word);
		_bstr_t b = word.AllocSysString();
		m_selRange->put_text(b);
	}
}

void CSpeller::IgnoreAll(CString word)
{
	if (word.IsEmpty()) word = GetSelWord();
	if (SpellCheck(word) != SPELL_OK)
	{
		m_IgnoreWords.Add(word);
		// recheck page
		ClearAllMarks();
		HighlightMisspells();
	}
}

void CSpeller::AddToDictionary()
{
	CString word = GetSelWord();
	if (SpellCheck(word) != SPELL_OK)
	{
		Hunhandle* currDict = GetDictionary(word);
		// add to Hunspell's runtime dictionary
		CStringA str = FbeEncodeDictionaryWord(word, m_codePage);
		Hunspell_add(currDict, str);
		// add to custom dictionary
		m_CustomDict.Add(word);
		if(!SaveCustomDict()) ::MessageBox(m_frame, FbeLoadRuntimeStringByKey(L"fbe.spelling.custom_dictionary.save_failed", L"The word was added for this session, but the custom dictionary could not be saved."), FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_spell_check.caption", L"Spell Check"), MB_OK | MB_ICONERROR);
		// recheck page
		ClearAllMarks();
		HighlightMisspells();
	}
}

void CSpeller::AddToDictionary(CString word)
{
	Hunhandle* currDict = GetDictionary(word);
	// add to Hunspell's runtime dictionary
	CStringA str = FbeEncodeDictionaryWord(word, m_codePage);
	Hunspell_add(currDict, str);
	// add to custom dictionary
	m_CustomDict.Add(word);
	if(!SaveCustomDict()) ::MessageBox(m_frame, FbeLoadRuntimeStringByKey(L"fbe.spelling.custom_dictionary.save_failed", L"The word was added for this session, but the custom dictionary could not be saved."), FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_spell_check.caption", L"Spell Check"), MB_OK | MB_ICONERROR);
	// recheck page
	ClearAllMarks();
	HighlightMisspells();
}

//
// Return dictionary based on word (bi-lingual hack)
//
Hunhandle* CSpeller::GetDictionary(CString word)
{
	// select document dictionary (based on FB2 document settings)
	m_codePage = m_Dictionaries[m_Lang].codepage;
	Hunhandle* currDict = m_Dictionaries[m_Lang].handle;

	// special fix for Russian words at non-Russian document
	if (m_Lang != LANG_RU && m_Lang != LANG_UA && m_Lang != LANG_BY)
	{
		// try to detect Russian language: too dirty but simple
		// 0x0 - English or other latin, 0x4 - Russian
		unsigned char* sData = (unsigned char*)word.GetString();
		if (sData[1] == 0x4)
		{
			// Russian language detected
			currDict = m_Dictionaries[LANG_RU].handle;
			m_codePage = m_Dictionaries[LANG_RU].codepage;
		}
	}
	return currDict;
}

//
// Return suggestions for misspell word
//
CStrings* CSpeller::GetSuggestions(CString word)
{
	word = FbePrepareDictionaryWord(word);

	CStrings* suggestions = new CStrings();
	Hunhandle* currDict = GetDictionary(word);
	if (!currDict)
		return suggestions;

	// Encode using the same normalized input path as SpellCheck.
	CStringA str = FbeEncodeDictionaryWord(word, m_codePage);
	char **list = NULL;
	int listLength = 0;

	try { listLength = Hunspell_suggest(currDict, &list, str); }
	catch(...) { return suggestions; }

	if (listLength <= 0 || !list)
		return suggestions;

	char** p = list;
	for (int i=0; i<listLength; i++)
	{
		CString s = FbeDecodeDictionaryWord(*p, m_codePage);
		suggestions->Add(s);
		p++;
	}
	Hunspell_free_list(currDict, &list, listLength);
	return suggestions;
}

//
// Spell check the word
// CString "word" in UTF encoding
// if no "range" assigned, spellcheck is non-interactive
// "suggestions" (if assigned) will be filled by a replacement variants
//
SPELL_RESULT CSpeller::SpellCheck(CString word)
{
	SPELL_RESULT spellResult(SPELL_OK);
	if (word.IsEmpty()) return SPELL_OK;
	Hunhandle* currDict = GetDictionary(word);

	// do spell check
	if (currDict)
	{
		CString checkWord(word);

		if (splitter->AlphaExceptions().Find(word[word.GetLength()-1]) > -1)
			checkWord.Delete(word.GetLength()-1);

		m_CurrentSpellWord = word;
		checkWord = FbePrepareDictionaryWord(checkWord);
		// remove accent
		checkWord.Replace(L"\u0301", L"");
		// special case for Russian letter "�"
		if (currDict == m_Dictionaries[LANG_RU].handle) checkWord.Replace(L"�", L"�");

		// encode string to the dictionary encoding 
		CStringA str = FbeEncodeDictionaryWord(checkWord, m_codePage);

		try { spellResult = (SPELL_RESULT) Hunspell_spell(currDict, str);  }
		catch(...) { spellResult = SPELL_OK; }

		if (spellResult != SPELL_OK)
		{
			// check ignore_all list first
			int nIdx = m_IgnoreWords.Find(word);
			if (nIdx > -1)
			{
				spellResult = SPELL_OK;
			}
			else
			{
				// check auto-replacement list
				nIdx = m_ChangeWords.Find(word);
				if (nIdx > -1)
				{
					spellResult = SPELL_CHANGEALL;
				}
				else
				{
					// check in custom dictionary
					if (FbeCustomDictionaryContains(m_CustomDict, word)) spellResult = SPELL_OK;
				}
			}
		}
	}
	return spellResult;
}

//
// Highlight word at the pos in the element
// 
void CSpeller::MarkElement(MSHTML::IHTMLElementPtr elem, long uniqID, CString word, int pos)
{
	MSHTML::IMarkupPointerPtr impStart;
	MSHTML::IMarkupPointerPtr impEnd;

	// Create start markup pointer
	m_ims->CreateMarkupPointer(&impStart);
	impStart->MoveAdjacentToElement(elem, MSHTML::ELEM_ADJ_AfterBegin);
	for (int i=0; i<pos; i++)
		impStart->MoveUnit (MSHTML::MOVEUNIT_NEXTCHAR);

	// Create end markup pointer
	m_ims->CreateMarkupPointer(&impEnd);
	impEnd->MoveAdjacentToElement(elem, MSHTML::ELEM_ADJ_BeforeEnd);

	// Locate the misspelled word
	_bstr_t w = word.AllocSysString();
	// First: exact match
//	if (impStart->findText (w, FINDTEXT_WHOLEWORD | FINDTEXT_MATCHCASE, impEnd, NULL) == S_FALSE)
	// Second: partial match
	if (impStart->findText (w, FINDTEXT_MATCHCASE, impEnd, NULL) == S_FALSE)
		return;

	// Create a display pointers from the markup pointers
	MSHTML::IDisplayPointerPtr idpStart;
	m_ids->CreateDisplayPointer(&idpStart);
	idpStart->MoveToMarkupPointer(impStart, NULL);

	MSHTML::IDisplayPointerPtr idpEnd;
	m_ids->CreateDisplayPointer(&idpEnd);
	idpEnd->MoveToMarkupPointer(impEnd, NULL);

	// Add or remove the segment
	MSHTML::IHighlightSegmentPtr ihs;
	m_ihrs->AddSegment(idpStart, idpEnd, m_irs, &ihs);

	m_ElementHighlights.emplace(uniqID, ihs);
}

//
// Removes all highlights in the document and clear saved highlights
//
void CSpeller::ClearAllMarks()
{
	for each (HIGHLIGHT itr in m_ElementHighlights)
	{
		m_ihrs->RemoveSegment(itr.second);
	}
	m_ElementHighlights.clear();
}

//
// Removes highlights for the unique element
//
void CSpeller::ClearMarks (int elemID)
{
	HIGHLIGHTS::iterator itr, lastElement;

	itr = m_ElementHighlights.find(elemID);
	if (itr != m_ElementHighlights.end())
	{
		lastElement = m_ElementHighlights.upper_bound(elemID);
		for ( ; itr != lastElement; ++itr)
			if (itr->second) 
				m_ihrs->RemoveSegment(itr->second);

		m_ElementHighlights.erase(elemID);
	}
}

//
// ���������� �����, ���������� �������� �������. ��������� MSHTML �����
// ���������� ������ EM, A ��� ������� inline-��������, �� �������������
// ������ ���� ������� � ���������� ����������� P.
//
static MSHTML::IHTMLElementPtr GetParagraphContainer(MSHTML::IHTMLElementPtr element)
{
	while (element)
	{
		if (U::scmp(element->tagName, L"P") == 0)
			return element;
		element = element->parentElement;
	}
	return MSHTML::IHTMLElementPtr();
}

//
// ���� ��������� ����� ����� DOM-�������, �� ����� ��������� ���� P
// ���������. ��� ��������� ����������� ������ � ����� �������� �������.
//
static MSHTML::IHTMLElementPtr GetNextParagraph(
	MSHTML::IHTMLElementPtr element,
	MSHTML::IHTMLElementPtr documentBody)
{
	struct Accessor
	{
		MSHTML::IHTMLDOMNodePtr FirstChild(MSHTML::IHTMLDOMNodePtr node) const { return node->firstChild; }
		MSHTML::IHTMLDOMNodePtr NextSibling(MSHTML::IHTMLDOMNodePtr node) const { return node->nextSibling; }
		MSHTML::IHTMLDOMNodePtr Parent(MSHTML::IHTMLDOMNodePtr node) const { return node->parentNode; }
		bool IsParagraph(MSHTML::IHTMLDOMNodePtr node) const
		{
			MSHTML::IHTMLElementPtr candidate(node);
			return candidate && U::scmp(candidate->tagName, L"P") == 0;
		}
	};
	return MSHTML::IHTMLElementPtr(FbeNextParagraphInDocumentOrder(
		MSHTML::IHTMLDOMNodePtr(element), MSHTML::IHTMLDOMNodePtr(documentBody), Accessor()));
}

//
// ��������� �����, ���������� ���������.
//
void CSpeller::CheckElement(MSHTML::IHTMLElementPtr elem, long uniqID)
{
	wchar_t testMode[2] = {};
	const bool collectDiagnostic = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode)) == 1 && testMode[0] == L'1';
	if (collectDiagnostic) ++m_testCheckElementCalls;
	CWords words;
	elem = GetParagraphContainer(elem);
	if (!elem)
		return;
	if (collectDiagnostic) ++m_testVisitedParagraphs;

	CString html = elem->innerHTML;
	if (html.Find(L"<DIV") >= 0) return;

	CString innerText = elem->innerText;

	if (!innerText.Trim().IsEmpty())
	{
		if (uniqID < 0)	uniqID = MSHTML::IHTMLUniqueNamePtr(elem)->uniqueNumber;

		// ���������� inline-���� ������ ������ ���� �����. ������� ����
		// ������������� ������ ��������� ������ ���������������� ������� ���������.
		ClearMarks(uniqID);

		// tokenize and spellcheck
		splitter->Split(&innerText, &words);
		for (int i=0; i<words.GetSize(); i++)
		{
			if (SpellCheck(words.GetValueAt(i)) == SPELL_MISSPELL)
				MarkElement (elem, uniqID, words.GetValueAt(i), words.GetKeyAt(i));
		}
	}
}

// 
// Check visible part of the document, run background check if view changed (scrolled)
//
void CSpeller::CheckScroll()
{
	if (m_scrollElement)
	{
		long Y = m_scrollElement->scrollTop;
		if (Y != m_prevY)
		{
			HighlightMisspells();
			m_prevY = Y;
		}
	}
}

void CSpeller::HighlightMisspells()
{
	if (m_HighlightMisspells && m_Enabled)
		CheckCurrentPage();
}


void CSpeller::CheckCurrentPage()
{
	if (!m_doc2 || !m_doc3 || !m_doc4 || !m_scrollElement || !m_fbw_body)
	{
		StartupTrace::Warning(L"speller", L"SP110", L"CheckCurrentPage skipped: HTML document is not ready");
		return;
	}
	CWords words;
	std::pair< std::set<long>::iterator, bool > pr;
	int currNum, numChanges = 0;
	MSHTML::IHTMLElementPtr elem, endElem;

	// lookup first element on page
	for (int y=10; y<m_scrollElement->clientHeight; y+=10)
	{
		elem = GetParagraphContainer(m_doc2->elementFromPoint(63, y));
		if (elem) break;
	}
	// lookup last element on page
	for (int y=m_scrollElement->clientHeight; y>10; y-=10)
	{
		endElem = GetParagraphContainer(m_doc2->elementFromPoint(63, y));
		if (endElem) break;
	}

	if (!elem)
		return;

	// If the visible bottom paragraph is known, visit every paragraph through it.
	// A bounded fallback is used only when hit-testing cannot determine it.
	const int fallbackParagraphLimit = 96;
	for (int checked = 0; elem && (endElem || checked < fallbackParagraphLimit); ++checked)
	{
		wchar_t testMode[2] = {};
		if (::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode)) == 1 && testMode[0] == L'1')
			++m_testVisitedParagraphs;
		currNum = MSHTML::IHTMLUniqueNamePtr(elem)->uniqueNumber;
		
		// Getting uniqueNumber from IHTMLUniqueName interface changes
		// the internal HTML document version. We need to correct this
		// issue, because document not really changed
		pr = m_uniqIDs.insert(currNum);
		if (pr.second) numChanges++;

		CString innerText = elem->innerText;
		if(!innerText.IsEmpty())
		{
			// remove underline
			ClearMarks(currNum);
			splitter->Split(&innerText, &words);
			for (int i=0; i<words.GetSize(); i++)
			{
				CString wrd = words.GetValueAt(i);
				if (SpellCheck(wrd) == SPELL_MISSPELL)
					MarkElement(elem, currNum, wrd, words.GetKeyAt(i));
			}
		}

		if (elem == endElem)
			break;
		elem = GetNextParagraph(elem, m_fbw_body);
	}
	
	if (numChanges) AdvanceVersionNumber(numChanges);
}

//
// Serialize custom dictionary
//
void CSpeller::LoadCustomDict()
{
	FbeLoadCustomDictionary(m_CustomDictPath, m_CustomDictCodepage, m_CustomDict);
}

bool CSpeller::SaveCustomDict()
{
	return FbeSaveCustomDictionary(m_CustomDictPath, m_CustomDictCodepage, m_CustomDict);
}

void CSpeller::StartDocumentCheck(MSHTML::IMarkupServices2Ptr undoSrv)
{
	if (!Available() || !m_doc2 || !m_ims || !m_fbw_body || !m_doc2->selection)
		return;

	try
	{
		// save current selection
		if (!m_prevSelRange)
		{
			m_ims->CreateMarkupPointer(&m_impStart);
			m_ims->CreateMarkupPointer(&m_impEnd);

			m_prevSelRange = m_doc2->selection->createRange();
			if (!m_prevSelRange)
				return;
			m_ims->MovePointersToRange(m_prevSelRange, m_impStart, m_impEnd);
		}

		// fetch selection
		CString selType((const wchar_t*)m_doc2->selection->type);
		m_selRange = m_doc2->selection->createRange();
		if (!m_selRange)
			return;

		MSHTML::IHTMLElementPtr elem(m_selRange->parentElement());
		CString tag;
		if (elem)
			tag = (BSTR)elem->tagName;

		// if no caret (no focus) and no text selected, start from the beginning of displayed text
		if (tag.Compare(L"P") && selType.CompareNoCase(L"text"))
		{
			MSHTML::IHTMLElementPtr startElement(m_doc2->elementFromPoint(65, 15));
			if (startElement)
			{
				m_selRange->moveToElementText(startElement);
				m_selRange->collapse(VARIANT_TRUE);
			}
		}
		m_selRange->moveStart(L"word", -1);
//		m_selRange->moveEnd(L"word", 1);

		// create and show spell dialog only after we have a valid selection
		if (!m_spell_dlg)
		{
			m_spell_dlg = new CSpellDialog(this);
			m_undoSrv = undoSrv;
			if (!m_spell_dlg->ShowDialog())
			{
				delete m_spell_dlg;
				m_spell_dlg = 0;
				return;
			}
		}
		else
		{
			m_undoSrv = undoSrv;
		}

		ContinueDocumentCheck();
	}
	catch(...)
	{
		EndDocumentCheck(true);
	}
}

//
// Spellcheck whole document from beginning
// Returns true if some changes was made, or false if no changes
//
void CSpeller::ContinueDocumentCheck()
{
	if (!Available() || !m_selRange || !m_spell_dlg)
	{
		EndDocumentCheck(true);
		return;
	}

	long compareEnd;
	SPELL_RESULT result;
	CString word;
	_bstr_t b;
	bool bHyphen = false;

	// find next misspell word from the beginning or current position
	do
	{
		// shift to the next word 
		m_selRange->move(L"word", 1);
		m_selRange->moveEnd(L"word", 1);

		result = SPELL_OK;
		word.SetString (m_selRange->text);
		word.Trim();
		// special check for hyphen
		if (word.Compare(L"-")==0)
		{
			m_selRange->moveStart(L"word", -1);
			m_selRange->moveEnd(L"word", 1);
			word.SetString (m_selRange->text);
			word.Trim();
			bHyphen = true;
		}

		// if word != words delimiter
		if (word.FindOneOf(Tokens)==-1)
			result = SpellCheck(word);

		// select exact word
		if ((result == SPELL_CHANGE) || (result == SPELL_CHANGEALL) || (result == SPELL_MISSPELL))
		{
			b = word.AllocSysString();
			m_selRange->findText(b, 1073741824, 2);
			m_selRange->select();
		}

		switch (result)
		{
			case SPELL_CHANGEALL:
			{
				CString replaceStr = FbeRestoreSourceApostropheStyle(word, m_ChangeWordsTo[m_ChangeWords.Find(word)]);
				BeginUndoUnit(L"replace word");
				b = replaceStr.AllocSysString();
				m_selRange->put_text(b);
				EndUndoUnit();
				break;
			}
			case SPELL_MISSPELL:
			{
				m_spell_dlg->m_sBadWord = word;
				delete m_spell_dlg->m_strSuggestions;
				m_spell_dlg->m_strSuggestions = GetSuggestions(word);
				m_spell_dlg->UpdateData();
				break;
			}
		}

		// check for end of selection
		if (bHyphen)
		{
			m_selRange->moveStart(L"word", 2);
			m_selRange->moveEnd(L"word", 1);
			bHyphen = false;
		}
		compareEnd = m_selRange->compareEndPoints(L"StartToEnd", m_selRange);
		if (compareEnd == 0)
		{
			m_selRange->move(L"word", 1);
			m_selRange->moveEnd(L"word", 1);
			compareEnd = m_selRange->compareEndPoints(L"StartToEnd", m_selRange);
		}

	} while (result != SPELL_MISSPELL && compareEnd != 0);

	if (!compareEnd)
		EndDocumentCheck(false);
}

void CSpeller::EndDocumentCheck(bool bCancel)
{
	if (m_spell_dlg)
	{
		m_spell_dlg->DestroyWindow();
		m_spell_dlg = 0;
	}
	// display message box
	if (!bCancel)
	{
		U::MessageBox(MB_OK | MB_ICONINFORMATION, IDR_MAINFRAME, IDS_SPELL_CHECK_COMPLETED);
	}
	// restore previous selection
	if (m_prevSelRange)
	{
		m_ims->MoveRangeToPointers(m_impStart, m_impEnd, m_prevSelRange);
		m_prevSelRange->select();

		// delete objects
		m_prevSelRange = 0;
	}
	// delete spell-check selection range
	if (m_selRange) 
	{
		m_selRange = 0;
	}
}
