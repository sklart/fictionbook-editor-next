#pragma once

#include <atlctrls.h>
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
			const CString text = FbeLoadRuntimeStringByKey(key, fallback);
			CToolInfo toolInfo(TTF_SUBCLASS, control, 0, NULL, text);
			m_tooltips.AddTool(&toolInfo);
		}
	}

	void UpdateText(HWND control, const CString& text)
	{
		if(control != NULL)
			m_tooltips.UpdateTipText(text, control);
	}

private:
	CToolTipCtrl m_tooltips;
};
