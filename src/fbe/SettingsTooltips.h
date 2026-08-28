#pragma once

#include <atlctrls.h>
#include <map>
#include "RuntimeLocalization.h"

class CSettingsTooltips
{
public:
	void Initialize(HWND owner)
	{
		m_owner = owner;
		m_tooltips.Create(owner, NULL, NULL, WS_POPUP | TTS_ALWAYSTIP);
		ATLASSERT(m_tooltips.IsWindow());
		m_tooltips.Activate(TRUE);
		m_tooltips.SetDelayTime(TTDT_INITIAL, 500);
		m_tooltips.SetMaxTipWidth(400);
	}

	void Add(HWND control, LPCWSTR key, LPCWSTR fallback)
	{
		if(control != NULL)
		{
			CString& text = m_texts[control];
			text = FbeLoadRuntimeStringByKey(key, fallback);
			CToolInfo toolInfo(TTF_SUBCLASS, control, 0, NULL, text.GetBuffer());
			m_tooltips.AddTool(&toolInfo);
		}
	}

	void UpdateText(HWND control, const CString& text)
	{
		if(control != NULL)
		{
			m_texts[control] = text;
			m_tooltips.UpdateTipText(m_texts[control].GetString(), control);
		}
	}

	// Disabled child windows do not reliably receive mouse messages. Register
	// the child's client rectangle on its enabled page instead, so the tooltip
	// control subclasses the page and receives the parent's mouse stream.
	void AddDisabledControlArea(HWND control, LPCWSTR key, LPCWSTR fallback)
	{
		if(control == NULL || m_owner == NULL)
			return;
		RECT rect = {};
		::GetWindowRect(control, &rect);
		::MapWindowPoints(NULL, m_owner, reinterpret_cast<LPPOINT>(&rect), 2);
		const UINT_PTR id = m_nextRectangleToolId++;
		CString& text = m_rectangleTexts[id];
		text = FbeLoadRuntimeStringByKey(key, fallback);
		CToolInfo toolInfo(TTF_SUBCLASS, m_owner, id, &rect, text.GetBuffer());
		m_tooltips.AddTool(&toolInfo);
	}

private:
	HWND m_owner = NULL;
	UINT_PTR m_nextRectangleToolId = 1;
	CToolTipCtrl m_tooltips;
	std::map<HWND, CString> m_texts;
	std::map<UINT_PTR, CString> m_rectangleTexts;
};
