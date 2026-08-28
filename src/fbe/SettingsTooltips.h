#pragma once

#include <atlctrls.h>
#include "RuntimeLocalization.h"

class CSettingsTooltips
{
public:
	void Initialize(HWND owner)
	{
		m_tooltips.Create(owner);
		m_tooltips.SetDelayTime(TTDT_INITIAL, 500);
		m_tooltips.SetMaxTipWidth(400);
	}

	void Add(HWND control, LPCWSTR key, LPCWSTR fallback)
	{
		if(control != NULL)
			m_tooltips.AddTool(control, FbeLoadRuntimeStringByKey(key, fallback).GetString());
	}

private:
	CToolTipCtrl m_tooltips;
};
