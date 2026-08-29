// SettingsWordsDlg.cpp : Implementation of CSettingsWordsDlg

#include "stdafx.h"
#include "SettingsWordsDlg.h"
#include "Settings.h"
#include "RuntimeLocalization.h"

#define IMG_STAT_WIDTH	40
#define IMG_STAT_HEIGHT	10

extern CSettings _Settings;

static void SetRuntimeSettingsWordsText(HWND dialog, int controlId, LPCWSTR key, LPCWSTR fallback)
{
	const CString text = FbeLoadRuntimeStringByKey(key, fallback);
	if (!text.IsEmpty())
		::SetDlgItemText(dialog, controlId, text);
}

static int compare_percent_asc(const void* v1, const void* v2)
{
	const WordsItem* w1 = (const WordsItem*)v1;
	const WordsItem* w2 = (const WordsItem*)v2; 
	return w1->m_percent - w2->m_percent;
}

static int compare_percent_desc(const void* v1, const void* v2)
{
	const WordsItem* w1 = (const WordsItem*)v1;
	const WordsItem* w2 = (const WordsItem*)v2;
	return w2->m_percent - w1->m_percent;
}

static int compare_counted_asc(const void* v1, const void* v2)
{
	const WordsItem* w1 = (const WordsItem*) v1;
	const WordsItem* w2 = (const WordsItem*) v2;
	return w1->m_count - w2->m_count;
}

static int compare_counted_desc(const void* v1, const void* v2)
{
	const WordsItem* w1 = (const WordsItem*)v1;
	const WordsItem* w2 = (const WordsItem*)v2;
	return w2->m_count - w1->m_count;
}

static int compare_word_asc(const void* v1, const void* v2)
{
	const WordsItem* w1 = (const WordsItem*)v1;
	const WordsItem* w2 = (const WordsItem*)v2;
	return w1->m_word.CompareNoCase(w2->m_word);
}

static int compare_word_desc(const void* v1, const void* v2)
{
	const WordsItem* w1 = (const WordsItem*)v1;
	const WordsItem* w2 = (const WordsItem*)v2;
	return w2->m_word.CompareNoCase(w1->m_word);
}

static int (*g_compare_funcs[])(const void*, const void*) = 
{
	compare_percent_asc,
	compare_percent_desc,
	compare_counted_asc,
	compare_counted_desc,
	compare_word_asc,
	compare_word_desc
};

// CSettingsWordsDlg
CSettingsWordsDlg::CSettingsWordsDlg() : m_sort(0), m_sel_all(false), m_ct(0), m_editidx(-1), m_editActive(false), m_wordsDirty(false)
{
	// Keep edits cancelable without coupling the view to persistent settings.
	m_words = _Settings.m_words;
}

LRESULT CSettingsWordsDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	m_list_words = GetDlgItem(IDC_LIST_WORDS);
	m_list_words.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	RECT rc;
	m_list_words.GetClientRect(&rc);
	int wcWidth = rc.right - rc.left - 80;

	CString header;

//	m_list_words.InsertColumn(0, L"%", LVCFMT_CENTER | LVCFMT_IMAGE, IMG_STAT_WIDTH + 10);

	header = FbeLoadRuntimeStringByKey(L"fbe.settings.words.encountered", L"Encountered");
	m_list_words.InsertColumn(0, header, LVCFMT_LEFT, 92);

	header = FbeLoadCString(IDS_SETTINGS_WLIST_WORD);
	m_list_words.InsertColumn(1, header, LVCFMT_LEFT, wcWidth);
	
	m_list_words.SetItemCount(static_cast<int>(m_words.size()));

	m_edt_new = GetDlgItem(IDC_EDIT_NEW);
	m_chk_all = GetDlgItem(IDC_CHECK_SELALL);
	m_btn_add = GetDlgItem(IDC_BUTTON_ADD);
	m_tooltips.Initialize(m_hWnd);
	m_tooltips.Add(m_list_words, L"fbe.settings.tooltip.words.list", L"Words excluded from the Words dialog. The count shows how many times each word was encountered.");
	m_tooltips.Add(m_edt_new, L"fbe.settings.tooltip.words.new", L"Enter a word to exclude, then press Add or Enter.");
	m_tooltips.Add(m_btn_add, L"fbe.settings.tooltip.words.add", L"Add the entered word to the exclusion list.");
	m_tooltips.Add(m_chk_all, L"fbe.settings.tooltip.words.select_all", L"Select or clear every word in the list.");
	m_tooltips.Add(GetDlgItem(IDC_BUTTON_REMOVESEL), L"fbe.settings.tooltip.words.remove", L"Remove the selected words from the exclusion list.");
	m_tooltips.Add(GetDlgItem(IDC_CHECK_SHOW_EXCLUSIONS), L"fbe.settings.tooltip.words.show_exclusions", L"Show excluded words in the Words dialog.");

	SetRuntimeSettingsWordsText(m_hWnd, IDC_BUTTON_ADD, L"fbe.dialog.idd_settings_words.add", L"Add");
	SetRuntimeSettingsWordsText(m_hWnd, IDC_STATIC_WORDS_CHEVRON, L"fbe.dialog.idd_settings_words.group", L"Words");
	SetRuntimeSettingsWordsText(m_hWnd, IDC_CHECK_SHOW_EXCLUSIONS, L"fbe.dialog.idd_settings_words.show_exclusions", L"Show exclusions");
	SetRuntimeSettingsWordsText(m_hWnd, IDC_CHECK_SELALL, L"fbe.dialog.idd_settings_words.select_all", L"Select all");
	SetRuntimeSettingsWordsText(m_hWnd, IDC_BUTTON_REMOVESEL, L"fbe.dialog.idd_settings_words.remove_selected", L"Remove selected");
	SetRuntimeSettingsWordsText(m_hWnd, IDC_STATIC_WORDS_NEW_WORD, L"fbe.dialog.idd_settings_words.new_word", L"New word:");

	// this unuseful code dramatically slowdown application! must be removed
//	CreateStatBitmaps();

	std::sort(m_words.begin(), m_words.end(), [](const WordsItem& left, const WordsItem& right)
	{
		return compare_counted_desc(&left, &right) < 0;
	});

	m_edit = GetDlgItem(IDC_EDIT_LV);
	m_show_words_excls = GetDlgItem(IDC_CHECK_SHOW_EXCLUSIONS);
	m_show_words_excls.SetCheck(_Settings.GetShowWordsExcls());

	return 0;
}

LRESULT CSettingsWordsDlg::OnListDispInfo(int id, NMHDR *hdr, BOOL&)
{
	NMLVDISPINFO *ni = (NMLVDISPINFO*)hdr;

	if (ni->item.iItem < 0 || ni->item.iItem >= static_cast<int>(m_words.size()))
		return 0;

	WordsItem *w = &m_words[ni->item.iItem];
	if (ni->item.mask & LVIF_TEXT)
		switch(ni->item.iSubItem)
		{
			case 0:
			{
				_snwprintf(ni->item.pszText, ni->item.cchTextMax, L"%i", w->m_count);
			}
			break;
			case 1:
				ni->item.pszText = w->m_word.GetBuffer();
			break;
		}

/*	if(ni->item.mask & LVIF_IMAGE)
		ni->item.iImage = w->m_prc_idx; */

	return 0;
}

/*
void CSettingsWordsDlg::CreateStatBitmaps()
{
	unsigned int size = m_words.GetSize();

	CImageList m_stat_images;
	m_stat_images.Create(IMG_STAT_WIDTH, IMG_STAT_HEIGHT, ILC_COLORDDB, 0, size);

	float total = 0;
	for(unsigned int i = 0; i < size; ++i)
		total += m_words[i].m_count;
	
	for(unsigned int i = 0; i < size; ++i)
	{
		CDC memDC = ::CreateCompatibleDC(GetDC());
		CBitmap newBitmap = ::CreateCompatibleBitmap(GetDC(), IMG_STAT_WIDTH, IMG_STAT_HEIGHT);
		CBitmap oldBitmap = (HBITMAP)SelectObject(memDC, newBitmap);

		CBrush newBrush, oldBrush;
		CPen newPen, oldPen;

		// Clear background
		::FillRect(memDC, CRect(0, 0, IMG_STAT_WIDTH, IMG_STAT_HEIGHT), (HBRUSH)::GetStockObject(WHITE_BRUSH));

		float percent = m_words[i].m_count / total * 100;
		m_words[i].m_percent = (int)percent;

		newBrush = ::CreateSolidBrush(RGB(255 - percent, 127, 127));
		newPen = ::CreatePen(PS_SOLID, 0, 0);
		oldBrush = (HBRUSH)::SelectObject(memDC, newBrush);
		oldPen = (HPEN)::SelectObject(memDC, newPen);

		
		int statWidth = int(IMG_STAT_WIDTH * (percent / 100));
		if(!statWidth) statWidth++;

		::Rectangle(memDC, 0, 2, statWidth, IMG_STAT_HEIGHT - 2);
		::SelectObject(memDC, oldBrush);
		::SelectObject(memDC, oldPen);

		::SelectObject(memDC, oldBitmap);

		m_stat_images.Add(newBitmap);
		m_words[i].m_prc_idx = i;
	}
	
	m_list_words.SetImageList(m_stat_images.Detach(), LVSIL_SMALL);
} */

LRESULT CSettingsWordsDlg::OnListSort(int id, NMHDR *hdr, BOOL&)
{
	NMLISTVIEW*lv = (NMLISTVIEW*)hdr;

	if(lv->iSubItem + 1 == abs(m_sort))
		m_sort = -m_sort;
	else
		m_sort = lv->iSubItem + 1;

	const int comparatorIndex = abs(m_sort) * 2 - (m_sort < 0 ? 0 : 1);
	std::sort(m_words.begin(), m_words.end(), [comparatorIndex](const WordsItem& left, const WordsItem& right)
	{
		return g_compare_funcs[comparatorIndex](&left, &right) < 0;
	});

	m_list_words.InvalidateRect(NULL);

	return 0;
}

LRESULT CSettingsWordsDlg::OnListClick(int id, NMHDR *hdr, BOOL&)
{
	NMITEMACTIVATE *ai = (NMITEMACTIVATE*) hdr;

	if ((::GetTickCount() - m_ct) < 500 || ai->iItem < 0 || ai->iSubItem != 1)
	{
		m_edit.ShowWindow(SW_HIDE);
		return 0;
	}
	if(m_editActive && !FinishInlineEdit())
	{
		m_edit.SetFocus();
		return 0;
	}

	m_editidx = ai->iItem;

	WordsItem* w = &m_words[m_editidx];

	m_edit.SetWindowText(w->m_word);

	RECT rci;
	m_list_words.GetSubItemRect(m_editidx, 1, LVIR_BOUNDS, &rci);
	m_list_words.ClientToScreen(&rci);
	ScreenToClient(&rci);
	m_edit.SetWindowPos(NULL, rci.left, rci.top, rci.right - rci.left, rci.bottom - rci.top + 5, SWP_SHOWWINDOW | SWP_NOACTIVATE);
	m_editActive = true;
	m_edit.SetSel(w->m_word.GetLength(), w->m_word.GetLength());
	m_edit.SetFocus();

	return 0;
}

LRESULT CSettingsWordsDlg::OnEditLVDefocused(int id, NMHDR *hdr, BOOL&)
{
	m_edit.ShowWindow(SW_HIDE);

	return 0;
}

LRESULT CSettingsWordsDlg::OnListChanged(int id, NMHDR *hdr, BOOL&)
{
	m_ct = ::GetTickCount();

	return 0;
}

LRESULT CSettingsWordsDlg::OnBnClickedButtonAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CString newWord;
	m_edt_new.GetWindowText(newWord);

	if(AddNewWord(newWord))
		m_edt_new.SetWindowText(L"");
		
	return 0;
}

bool CSettingsWordsDlg::AddNewWord(CString& word, bool test)
{
	word.Trim();
	bool ambigous = word.IsEmpty();
	int symbol = 0;

	int hyphens = 0;

	while(symbol != word.GetLength())
	{
		if(!iswalpha(word[symbol]) && word[symbol] != L'-')
		{
			ambigous = true;
			break;
		}

		if(word[symbol] == L'-')
		{
			if(symbol == 0 || symbol == word.GetLength() - 1 || word[symbol - 1] == L'-')
			{
				ambigous = true;
				break;
			}

			hyphens++;
		}

		symbol++;
	}

	if(hyphens == 0)
		ambigous = true;

	if(!ambigous)
	{
		for(size_t i = 0; i < m_words.size(); ++i)
		{
			if(word.CompareNoCase(m_words[i].m_word) == 0)
			{
				CString errMsg[2];
				errMsg[0] = FbeLoadCString(IDS_SETTINGS_WORDS_ADD_ERR_TEXT);
				errMsg[1] = FbeLoadCString(IDS_SETTINGS_WORDS_ADD_ERR_CAP);
				MessageBox(errMsg[0], errMsg[1], MB_OK | MB_ICONERROR);

				return false;
			}
		}

		if(!test)
		{
			WordsItem wi(word.MakeLower(), 0);
			wi.m_prc_idx = m_list_words.GetItemCount() + 1;

			m_words.push_back(wi);
			m_wordsDirty = true;
			// In owner-data mode the control owns no per-row items.  Changing the
			// item count makes the new model row visible on demand.
			m_list_words.SetItemCount(static_cast<int>(m_words.size()));
			m_list_words.RedrawItems(static_cast<int>(m_words.size()) - 1, static_cast<int>(m_words.size()) - 1);
		}

		return true;
	}
	else
	{
		CString errMsg[2];
		errMsg[0] = FbeLoadCString(IDS_SETTINGS_WORDS_ADD_ERR_SYM);
		errMsg[1] = FbeLoadCString(IDS_SETTINGS_WORDS_ADD_ERR_CAP);
		MessageBox(errMsg[0], errMsg[1], MB_OK | MB_ICONERROR);

		return false;
	}
}

LRESULT CSettingsWordsDlg::OnCustomDraw(int id, NMHDR *hdr, BOOL&)
{
	if(hdr->hwndFrom == m_list_words.GetHeader())
	{
		NMCUSTOMDRAW *cd =(NMCUSTOMDRAW*)hdr;

		switch(cd->dwDrawStage)
		{
		  case CDDS_PREPAINT:
			  return CDRF_NOTIFYITEMDRAW;
		  case CDDS_ITEMPREPAINT:
			  return CDRF_NOTIFYPOSTPAINT;
		  case CDDS_ITEMPOSTPAINT:
			  // paint sort indicator on top of the item
			  if (cd->dwItemSpec + 1 == (unsigned)abs(m_sort))
			  {
				  HGDIOBJ old=::SelectObject(cd->hdc,::GetSysColorBrush(COLOR_BTNTEXT));
				  int	h=cd->rc.bottom-cd->rc.top;
				  int	ah=h/4;
				  if (ah<5)
					  ah=5;
				  if (ah>20)
					  ah=20;
				  enum { off=5 };
				  POINT	  pt[3];
				  pt[0].x=cd->rc.right-off-ah;
				  pt[0].y=cd->rc.top+(h-ah)/2;
				  pt[1].x=cd->rc.right-off;
				  pt[1].y=pt[0].y+ah;
				  pt[2].x=cd->rc.right-off-ah*2;
				  pt[2].y=pt[1].y;
				  if (m_sort<0) {
					  pt[0].y=cd->rc.bottom+cd->rc.top-pt[0].y;
					  pt[1].y=cd->rc.bottom+cd->rc.top-pt[1].y;
					  pt[2].y=cd->rc.bottom+cd->rc.top-pt[2].y;
				  }
				  ::Polygon(cd->hdc,pt, 3);
				  ::SelectObject(cd->hdc, old);
			  }
			  return 0;
		}
	}
	return 0;
}
LRESULT CSettingsWordsDlg::OnBnClickedCheckSelall(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(!m_list_words.GetItemCount())
	{
		m_chk_all.SetCheck(m_sel_all = false);
		return 0;
	}

	// -1 applies the state to the complete virtual range in one ListView call.
	m_list_words.SetItemState(-1, m_sel_all ? 0 : LVIS_SELECTED, LVIS_SELECTED);
	if(!m_sel_all)
		::SetFocus(m_list_words);

	m_sel_all = !m_sel_all;

	return 0;
}

LRESULT CSettingsWordsDlg::OnBnClickedButtonRemovesel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::vector<int> selected;
	for(int index = m_list_words.GetNextItem(-1, LVNI_SELECTED); index != -1;
		index = m_list_words.GetNextItem(index, LVNI_SELECTED))
		selected.push_back(index);

	// Erase the selected rows in one model compaction pass.  This avoids the
	// quadratic shifting previously caused by deleting every ListView item.
	if(!selected.empty())
	{
		std::vector<bool> remove(m_words.size(), false);
		for(size_t i = 0; i < selected.size(); ++i)
			remove[selected[i]] = true;
		std::vector<WordsItem> kept;
		kept.reserve(m_words.size() - selected.size());
		for(size_t i = 0; i < m_words.size(); ++i)
			if(!remove[i])
				kept.push_back(m_words[i]);
		m_words.swap(kept);
		m_wordsDirty = true;
		m_list_words.SetItemCount(static_cast<int>(m_words.size()));
		m_sel_all = false;
		m_chk_all.SetCheck(BST_UNCHECKED);
	}

	return 0;
}

void CSettingsWordsDlg::RemoveWord(int index)
{
	if(index < 0 || index >= static_cast<int>(m_words.size()))
		return;
	m_words.erase(m_words.begin() + index);
	m_wordsDirty = true;
	m_list_words.SetItemCount(static_cast<int>(m_words.size()));
}

LRESULT CSettingsWordsDlg::OnOK(WORD, WORD wID, HWND, BOOL&)
{
	HandleDefaultAction();
	return 0;
}

LRESULT CSettingsWordsDlg::OnCancel(WORD, WORD wID, HWND, BOOL&)
{
	return CancelChanges() ? 1 : 0;
}

bool CSettingsWordsDlg::FinishInlineEdit()
{
	if(!m_editActive)
		return true;
	if(m_editidx < 0 || m_editidx >= static_cast<int>(m_words.size()))
		return false;

	CString editedWord = U::GetWindowText(m_edit);
	editedWord.Trim();
	if(m_words[m_editidx].m_word != editedWord)
	{
		if(!AddNewWord(editedWord, true))
			return false;
		RemoveWord(m_editidx);
		if(!AddNewWord(editedWord))
			return false;
	}
	m_edit.ShowWindow(SW_HIDE);
	m_editActive = false;
	m_editidx = -1;
	return true;
}

bool CSettingsWordsDlg::FinishNewWord()
{
	CString newWord = U::GetWindowText(m_edt_new);
	newWord.Trim();
	if(newWord.IsEmpty())
		return true;
	if(!AddNewWord(newWord))
		return false;
	m_edt_new.SetWindowText(L"");
	return true;
}

bool CSettingsWordsDlg::HandleDefaultAction()
{
	if(m_editActive)
	{
		if(!FinishInlineEdit())
			m_edit.SetFocus();
		return true;
	}
	if(GetFocus() == m_edt_new)
	{
		if(!FinishNewWord())
			m_edt_new.SetFocus();
		return true;
	}
	return false;
}

bool CSettingsWordsDlg::Validate()
{
	return FinishInlineEdit() && FinishNewWord();
}

void CSettingsWordsDlg::Commit()
{
	_Settings.SetShowWordsExcls(m_show_words_excls.GetCheck() != 0);
	if(m_wordsDirty)
	{
		std::vector<WordsItem> persistentWords = m_words;
		std::sort(persistentWords.begin(), persistentWords.end(), [](const WordsItem& left, const WordsItem& right)
		{
			const int comparison = left.m_word.CompareNoCase(right.m_word);
			return comparison != 0 ? comparison < 0 : left.m_word < right.m_word;
		});
		_Settings.m_words = persistentWords;
		_Settings.SaveWords();
	}
}

bool CSettingsWordsDlg::CancelChanges()
{
	if(m_editActive && GetFocus() == m_edit)
	{
		m_edit.ShowWindow(SW_HIDE);
		m_editActive = false;
		m_editidx = -1;
		return false;
	}
	return true;
}
