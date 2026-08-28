#pragma once

#include <atlctrls.h>
#include <map>
#include "RuntimeLocalization.h"

class CSettingsTooltips
{
public:
	void Initialize(HWND owner)
	{
		m_tooltips.Create(owner);
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

private:
	CToolTipCtrl m_tooltips;
	std::map<HWND, CString> m_texts;
};
