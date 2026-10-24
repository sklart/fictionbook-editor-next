#pragma once

#include <atlctrls.h>
#include <map>
#include "..\\..\\RuntimeLocalization.h"
#include "..\\..\\UiMetrics.h"

class CSettingsTooltips
{
public:
	void Initialize(HWND owner)
	{
		m_owner = owner;
		RECT tipBounds = {};
		m_tooltips.Create(owner, tipBounds, NULL, WS_POPUP | TTS_ALWAYSTIP);
		ATLASSERT(m_tooltips.IsWindow());
		m_tooltips.Activate(TRUE);
		m_tooltips.SetDelayTime(TTDT_INITIAL, 500);
		RefreshDpiMetrics();
		const BOOL tracking = ::SetWindowSubclass(owner, OwnerSubclassProc, 0, reinterpret_cast<DWORD_PTR>(this));
		ATLASSERT(tracking);
		(void)tracking;
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
		if(control != NULL && m_tooltips.IsWindow())
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
		if(m_tooltips.AddTool(&toolInfo))
		{
			const BOOL tracking = ::SetWindowSubclass(control, DisabledAreaSubclassProc, id, reinterpret_cast<DWORD_PTR>(this));
			ATLASSERT(tracking);
			(void)tracking;
		}
	}

private:
	static LRESULT CALLBACK OwnerSubclassProc(HWND owner, UINT message, WPARAM wParam, LPARAM lParam,
		UINT_PTR subclassId, DWORD_PTR reference)
	{
		CSettingsTooltips* self = reinterpret_cast<CSettingsTooltips*>(reference);
		// The project's Windows 7 SDK target does not declare this newer child-window message.
		constexpr UINT dpiChangedAfterParent = 0x02E3;
		if(message == WM_WINDOWPOSCHANGED || message == WM_DPICHANGED || message == dpiChangedAfterParent)
			self->RefreshDpiMetrics();
		else if(message == WM_NCDESTROY)
		{
			::RemoveWindowSubclass(owner, OwnerSubclassProc, subclassId);
			self->m_owner = NULL;
		}
		return ::DefSubclassProc(owner, message, wParam, lParam);
	}

	void RefreshDpiMetrics()
	{
		if(!m_tooltips.IsWindow() || !::IsWindow(m_owner)) return;
		const UINT dpi = UiMetrics::DpiForWindow(m_owner);
		if(dpi == m_tipDpi) return;
		m_tipDpi = dpi;
		m_tooltips.SetMaxTipWidth(UiMetrics::ScaleForDpi(400, dpi));
	}

	static LRESULT CALLBACK DisabledAreaSubclassProc(HWND control, UINT message, WPARAM wParam, LPARAM lParam,
		UINT_PTR toolId, DWORD_PTR reference)
	{
		CSettingsTooltips* self = reinterpret_cast<CSettingsTooltips*>(reference);
		if(message == WM_WINDOWPOSCHANGED)
			self->RefreshDisabledControlArea(control, toolId);
		else if(message == WM_NCDESTROY)
		{
			::RemoveWindowSubclass(control, DisabledAreaSubclassProc, toolId);
			if(self->m_tooltips.IsWindow()) self->m_tooltips.DelTool(self->m_owner, toolId);
			self->m_rectangleTexts.erase(toolId);
		}
		return ::DefSubclassProc(control, message, wParam, lParam);
	}

	void RefreshDisabledControlArea(HWND control, UINT_PTR toolId)
	{
		if(!m_tooltips.IsWindow() || !::IsWindow(m_owner)) return;
		RECT rect = {};
		if(!::GetWindowRect(control, &rect)) return;
		::MapWindowPoints(NULL, m_owner, reinterpret_cast<LPPOINT>(&rect), 2);
		m_tooltips.SetToolRect(m_owner, toolId, &rect);
	}

	HWND m_owner = NULL;
	UINT m_tipDpi = 0;
	UINT_PTR m_nextRectangleToolId = 1;
	CToolTipCtrl m_tooltips;
	std::map<HWND, CString> m_texts;
	std::map<UINT_PTR, CString> m_rectangleTexts;
};
