// Runtime localization and toolbar/menu presentation for CMainFrame.
// Included by mainfrm.cpp to retain its narrow private-member boundary.

// Fill current menu with accelerators' text
void CMainFrame::FillMenuWithHkeys(HMENU menu)
{
	for(unsigned int i = 0; i < _Settings.m_hotkey_groups.size(); ++i)
	{
		CHotkeysGroup& group = _Settings.m_hotkey_groups[i];
		std::vector<CHotkey>::iterator begin = group.m_hotkeys.begin();
		if((group.m_reg_name == L"Scripts" || group.m_reg_name == L"Plugins")
			&& begin != group.m_hotkeys.end())
			++begin;

		std::sort(begin, group.m_hotkeys.end());
		for(unsigned int j = 0; j < group.m_hotkeys.size(); ++j)
		{
			CHotkey& hotkey = group.m_hotkeys[j];
			CString text;
			WORD cmd = hotkey.m_accel.cmd;
			LPTSTR buffer = text.GetBufferSetLength(MAX_LOAD_STRING + 1);
			const int menuTextLength = ::GetMenuString(menu, cmd, buffer, MAX_LOAD_STRING + 1, MF_BYCOMMAND);
			text.ReleaseBuffer(menuTextLength > 0 ? menuTextLength : 0);

			if(menuTextLength > 0)
			{
				// При повторном обновлении интерфейса пункт уже мог содержать
				// старую подсказку клавиатурного сокращения. Убираем её перед
				// добавлением актуальной, чтобы текст не накапливался.
				const int acceleratorSeparator = text.Find(L'\t');
				if(acceleratorSeparator >= 0)
					text = text.Left(acceleratorSeparator);
				text += L"\t";
				text += U::AccelToString(hotkey.m_accel);

				MENUITEMINFO miim;
				ZeroMemory(&miim, sizeof(MENUITEMINFO));
				miim.cbSize = sizeof(MENUITEMINFO);
				miim.fMask = MIIM_STRING;
				miim.dwTypeData = text.GetBuffer();
				miim.cch = text.GetLength();
				::SetMenuItemInfo(menu, cmd, FALSE, &miim);
			}
		}
	}
}

LRESULT CMainFrame::OnRuntimeToolTipTextA(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	LPNMTTDISPINFOA pDispInfo = (LPNMTTDISPINFOA)pnmh;
	if((idCtrl == 0) || (pDispInfo->uFlags & TTF_IDISHWND))
	{
		bHandled = FALSE;
		return 0;
	}

	const CString text = GetRuntimeToolbarToolTipText(static_cast<UINT>(idCtrl));
	if (text.IsEmpty())
	{
		bHandled = FALSE;
		return 0;
	}

	::WideCharToMultiByte(CP_ACP, 0, text, -1, pDispInfo->szText, _countof(pDispInfo->szText), NULL, NULL);
	return 0;
}

LRESULT CMainFrame::OnRuntimeToolTipTextW(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	LPNMTTDISPINFOW pDispInfo = (LPNMTTDISPINFOW)pnmh;
	if((idCtrl == 0) || (pDispInfo->uFlags & TTF_IDISHWND))
	{
		bHandled = FALSE;
		return 0;
	}

	const CString text = GetRuntimeToolbarToolTipText(static_cast<UINT>(idCtrl));
	if (text.IsEmpty())
	{
		bHandled = FALSE;
		return 0;
	}

	ATL::Checked::wcsncpy_s(pDispInfo->szText, _countof(pDispInfo->szText), text, _TRUNCATE);
	return 0;
}

LRESULT CMainFrame::OnCommandToolbarCustomDraw(int, LPNMHDR pnmh, BOOL& bHandled)
{
	const bool isCommandToolbar = pnmh->hwndFrom == m_CmdToolbar.m_hWnd;
	bool isScriptsToolbar = pnmh->hwndFrom == m_ScriptsToolbar.m_hWnd;
	for(size_t index = 0; !isScriptsToolbar && index < m_scriptToolbars.Items().size(); ++index)
		isScriptsToolbar = m_scriptToolbars.Items()[index].window == pnmh->hwndFrom;
	const bool isContextAttributeBar = m_contextAttributeBars.IsBar(pnmh->hwndFrom);
	if (!isCommandToolbar && !isScriptsToolbar && !isContextAttributeBar)
	{
		bHandled = FALSE;
		return 0;
	}

	NMTBCUSTOMDRAW* customDraw = reinterpret_cast<NMTBCUSTOMDRAW*>(pnmh);
	if (customDraw->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		if(ThemeManager::IsDark() && !ThemeManager::IsHighContrast())
		{
			RECT client = {};
			::GetClientRect(pnmh->hwndFrom, &client);
			::FillRect(customDraw->nmcd.hdc, &client, ThemeManager::ControlBrush());
		}
		return CDRF_NOTIFYITEMDRAW;
	}

	if (customDraw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
	{
		const bool disabled = (customDraw->nmcd.uItemState & (CDIS_DISABLED | CDIS_GRAYED)) != 0;
		customDraw->clrText = disabled ? ThemeManager::DisabledTextColor() : ThemeManager::TextColor();
		customDraw->clrTextHighlight = ThemeManager::SelectionTextColor();
		customDraw->clrBtnFace = ThemeManager::ControlColor();
		const bool pressed = (customDraw->nmcd.uItemState & CDIS_SELECTED) != 0;
		customDraw->clrBtnHighlight = pressed ? ThemeManager::PressedColor() : ThemeManager::HoverColor();
		customDraw->clrHighlightHotTrack = ThemeManager::HoverColor();
		if(isScriptsToolbar && ThemeManager::IsDark() && !ThemeManager::IsHighContrast())
			return CDRF_NOTIFYPOSTPAINT;
		const UINT commandId = static_cast<UINT>(customDraw->nmcd.dwItemSpec);
		if (isCommandToolbar && IsTableToolbarCommand(commandId) &&
			disabled)
		{
			const int imageIndex = static_cast<int>(m_CmdToolbar.SendMessage(TB_GETBITMAP, commandId, 0));
			HIMAGELIST imageList = m_CmdToolbar.GetImageList();
			if (imageIndex < 0 || imageList == NULL)
				return CDRF_DODEFAULT;

			const RECT& rect = customDraw->nmcd.rc;
			::DrawThemeParentBackground(m_CmdToolbar.m_hWnd, customDraw->nmcd.hdc, &rect);
			IMAGELISTDRAWPARAMS draw = {};
			draw.cbSize = sizeof(draw);
			draw.himl = imageList;
			draw.i = imageIndex;
			draw.hdcDst = customDraw->nmcd.hdc;
			draw.x = rect.left + (rect.right - rect.left - 24) / 2;
			draw.y = rect.top + (rect.bottom - rect.top - 24) / 2;
			draw.cx = 24;
			draw.cy = 24;
			draw.rgbBk = CLR_NONE;
			draw.rgbFg = CLR_NONE;
			draw.fStyle = ILD_TRANSPARENT;
			draw.fState = ILS_SATURATE;
			return ::ImageList_DrawIndirect(&draw) ? CDRF_SKIPDEFAULT : CDRF_DODEFAULT;
		}
		// The legacy command bitmaps contain a number of nearly-black strokes.
		// Blend a light foreground over the normal image in dark mode so these
		// strokes remain visible without replacing application-owned resources.
		if (isCommandToolbar && ThemeManager::IsDark() && !ThemeManager::IsHighContrast() && !disabled)
			return CDRF_NOTIFYPOSTPAINT;
	}

	if (isScriptsToolbar && customDraw->nmcd.dwDrawStage == CDDS_ITEMPOSTPAINT && ThemeManager::IsDark() && !ThemeManager::IsHighContrast())
	{
		const UINT state = customDraw->nmcd.uItemState;
		if((state & (CDIS_HOT | CDIS_SELECTED)) != 0)
			::FrameRect(customDraw->nmcd.hdc, &customDraw->nmcd.rc, ThemeManager::Brush(THEME_COLOR_BORDER));
		return CDRF_DODEFAULT;
	}

	if (isCommandToolbar && customDraw->nmcd.dwDrawStage == CDDS_ITEMPOSTPAINT && ThemeManager::IsDark() && !ThemeManager::IsHighContrast())
	{
		const UINT commandId = static_cast<UINT>(customDraw->nmcd.dwItemSpec);
		const int imageIndex = static_cast<int>(m_CmdToolbar.SendMessage(TB_GETBITMAP, commandId, 0));
		HIMAGELIST imageList = m_CmdToolbar.GetImageList();
		if (imageIndex < 0 || imageList == NULL)
			return CDRF_DODEFAULT;

		const RECT& rect = customDraw->nmcd.rc;
		IMAGELISTDRAWPARAMS draw = {};
		draw.cbSize = sizeof(draw);
		draw.himl = imageList;
		draw.i = imageIndex;
		draw.hdcDst = customDraw->nmcd.hdc;
		draw.x = rect.left + (rect.right - rect.left - 24) / 2;
		draw.y = rect.top + (rect.bottom - rect.top - 24) / 2;
		draw.cx = 24;
		draw.cy = 24;
		draw.rgbBk = CLR_NONE;
		draw.rgbFg = ThemeManager::TextColor();
		draw.fStyle = ILD_TRANSPARENT | ILD_BLEND50;
	::ImageList_DrawIndirect(&draw);
	return CDRF_DODEFAULT;
	}

	return CDRF_DODEFAULT;
}

void CMainFrame::RefreshLocalizedToolbarButtonTexts(CToolBarCtrl& toolbar)
{
	if(!toolbar.IsWindow())
		return;
	const bool profileLocalization = StartupTrace::Enabled();
	const ULONGLONG started = profileLocalization ? ::GetTickCount64() : 0;

	const int buttonCount = toolbar.GetButtonCount();
	for(int i = 0; i < buttonCount; ++i)
	{
		TBBUTTON button = {};
		if(!toolbar.GetButton(i, &button))
			continue;
		if(button.fsStyle & BTNS_SEP)
			continue;
		if(button.idCommand <= 0)
			continue;

		wchar_t buf[MAX_LOAD_STRING + 1];
		if(!FbeLoadString(_Module.GetResourceInstance(), button.idCommand, buf, MAX_LOAD_STRING))
			continue;

		const wchar_t* text = wcschr(buf, L'\n');
		text = (text != NULL) ? text + 1 : buf;

		TBBUTTONINFO info = {};
		info.cbSize = sizeof(info);
		info.dwMask = TBIF_TEXT;
		info.pszText = const_cast<wchar_t*>(text);
		toolbar.SetButtonInfo(button.idCommand, &info);
	}

	toolbar.AutoSize();
	toolbar.Invalidate();
	if (profileLocalization) { ++g_idleProfile.toolbarLocalizationUpdates; g_idleProfile.toolbarLocalizationMilliseconds += ::GetTickCount64() - started; }
}
void CMainFrame::RefreshLocalizedToolbarCaptions()
{
	m_contextAttributeBars.UpdateLocalization();
	// Панели ссылок и таблиц используют toolbar-кнопки только как разметку:
	// поверх каждой из них находится CCustomStatic или combo-box. Простая
	// смена текста static-контрола оставляла старую ширину кнопки-разметки,
	// из-за чего подписи накладывались друг на друга после смены языка.
	// Пересобираем только эти пустые кнопки и заново размещаем уже созданные
	// дочерние контролы. Содержимое combo-box при этом не затрагивается.
	FbeLoadString(_Module.GetResourceInstance(), IDS_PANE_INS, strINS, MAX_LOAD_STRING);
	FbeLoadString(_Module.GetResourceInstance(), IDS_PANE_OVR, strOVR, MAX_LOAD_STRING);

	m_status.SetPaneText(ID_PANE_INS, CurrentOverwriteMode() ? strOVR : strINS);

	RefreshLocalizedToolbarButtonTexts(m_CmdToolbar);
	RefreshLocalizedToolbarButtonTexts(m_ScriptsToolbar);
	UpdateStatusBar();
}

void CMainFrame::RefreshLocalizedMainFrameUi()
{
	// Меню и панели уже построены при старте. Переводим их на месте: повторный
	// AttachMenu + InitializeExtensionUi создавал новые toolbar-кнопки и GDI-изображения
	// при каждом переключении языка, вызывая задержку, рост памяти и артефакты UI.
	HMENU menu = m_MenuBar.GetMenu();
	if(menu != NULL)
	{
		// Update the only localized dynamic MRU string before rebuilding its
		// menu.  RebuildMruMenu preserves captions of actual documents.
		RefreshMruEmptyStateText(m_recentDocuments.List());
		FbeRecentDocuments::RebuildMruMenu(m_recentDocuments.List());
		ApplyRuntimeMainFrameMenuLocalization(menu);

		HMENU fileMenu = ::GetSubMenu(menu, 0);
		if(fileMenu != NULL)
		{
			RefreshBundledPluginMenuTexts(m_plugins.Manager(), ::GetSubMenu(fileMenu, 6), L"Import", ID_IMPORT_BASE);
			RefreshBundledPluginMenuTexts(m_plugins.Manager(), ::GetSubMenu(fileMenu, 7), L"Export", ID_EXPORT_BASE);
		}

		FillMenuWithHkeys(m_MenuBar.GetMenu());
		const int menuItemCount = ::GetMenuItemCount(menu);
		const int buttonCount = m_MenuBar.GetButtonCount();
		for(int index = 0; index < menuItemCount && index < buttonCount; ++index)
		{
			wchar_t text[MAX_LOAD_STRING + 1] = {};
			const int textLength = ::GetMenuString(menu, index, text, _countof(text), MF_BYPOSITION);
			if(textLength <= 0)
				continue;

			TBBUTTONINFO buttonInfo = {};
			buttonInfo.cbSize = sizeof(buttonInfo);
			buttonInfo.dwMask = TBIF_TEXT;
			buttonInfo.pszText = text;
			m_MenuBar.SetButtonInfo(index, &buttonInfo);
		}
		m_MenuBar.AutoSize();
		m_MenuBar.Invalidate();
	}

	RefreshLocalizedToolbarCaptions();
	m_document_tree.RefreshLocalizedTitle();
	UpdateLayout();
}
