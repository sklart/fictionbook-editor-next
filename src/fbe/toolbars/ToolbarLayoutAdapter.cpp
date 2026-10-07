#include "stdafx.h"
#include "ToolbarLayoutAdapter.h"
#include "..\\UiMetrics.h"

namespace
{
	const int kDefaultSeparatorWidth = 8;
	ToolbarLayoutAdapter::TestFailurePoint g_testFailurePoint = ToolbarLayoutAdapter::TestFailureNone;

	bool ConsumeTestFailure(ToolbarLayoutAdapter::TestFailurePoint point)
	{
		if(g_testFailurePoint != point) return false;
		g_testFailurePoint = ToolbarLayoutAdapter::TestFailureNone;
		return true;
	}

	int LogicalSeparatorWidth(int physicalWidth, UINT dpi)
	{
		return physicalWidth > 0 ? (std::max)(1, ::MulDiv(physicalWidth, 96, (std::max)(1U, dpi))) : kDefaultSeparatorWidth;
	}

	int PhysicalSeparatorWidth(int logicalWidth, UINT dpi)
	{
		return UiMetrics::ScaleForDpi(logicalWidth > 0 ? logicalWidth : kDefaultSeparatorWidth, dpi);
	}

	bool CaptureExact(HWND toolbar, std::vector<PortableToolbarItem>& items)
	{
		items.clear();
		if(!::IsWindow(toolbar)) return false;
		CToolBarCtrl source = toolbar;
		const UINT dpi = UiMetrics::DpiForWindow(toolbar);
		const int count = source.GetButtonCount();
		if(count < 0) return false;
		for(int index = 0; index < count; ++index)
		{
			TBBUTTON button = {};
			if(!source.GetButton(index, &button)) { items.clear(); return false; }
			PortableToolbarItem item = {};
			item.separator = (button.fsStyle & TBSTYLE_SEP) != 0;
			item.command = button.idCommand;
			item.width = item.separator ? LogicalSeparatorWidth(button.iBitmap, dpi) : 0;
			items.push_back(item);
		}
		return true;
	}

	bool BuildButtons(const std::vector<PortableToolbarItem>& items, const std::vector<TBBUTTON>& catalog, UINT dpi, std::vector<TBBUTTON>& buttons)
	{
		buttons.clear();
		for(size_t index = 0; index < items.size(); ++index)
		{
			const PortableToolbarItem& item = items[index];
			if(item.separator)
			{
				TBBUTTON separator = {};
				separator.iBitmap = PhysicalSeparatorWidth(item.width, dpi);
				separator.fsStyle = TBSTYLE_SEP;
				buttons.push_back(separator);
				continue;
			}
			bool found = false;
			for(size_t buttonIndex = 0; buttonIndex < catalog.size(); ++buttonIndex)
				if(catalog[buttonIndex].idCommand == item.command) { buttons.push_back(catalog[buttonIndex]); found = true; break; }
			// Missing Script UIDs are deliberately retained in the persisted layout,
			// but cannot be rendered until the script is available again.
			if(!found && item.scriptUid.IsEmpty() && item.command != 0) return false;
		}
		return true;
	}

	bool Matches(HWND toolbar, const std::vector<TBBUTTON>& expected)
	{
		CToolBarCtrl target = toolbar;
		if(target.GetButtonCount() != static_cast<int>(expected.size())) return false;
		for(size_t index = 0; index < expected.size(); ++index)
		{
			TBBUTTON actual = {};
			if(!target.GetButton(static_cast<int>(index), &actual)) return false;
			const bool expectedSeparator = (expected[index].fsStyle & TBSTYLE_SEP) != 0;
			if(((actual.fsStyle & TBSTYLE_SEP) != 0) != expectedSeparator || actual.idCommand != expected[index].idCommand) return false;
			if(expectedSeparator && actual.iBitmap != expected[index].iBitmap) return false;
			if(!expectedSeparator && actual.fsStyle != expected[index].fsStyle) return false;
		}
		return true;
	}

	bool ReplaceChecked(HWND toolbar, const std::vector<TBBUTTON>& buttons, bool rollback)
	{
		if(!::IsWindow(toolbar)) return false;
		if(rollback && ConsumeTestFailure(ToolbarLayoutAdapter::TestFailureBeforeRollback)) return false;
		CToolBarCtrl target = toolbar;
		while(target.GetButtonCount() > 0)
		{
			if(!rollback && ConsumeTestFailure(ToolbarLayoutAdapter::TestFailureBeforeDelete)) return false;
			if(!target.DeleteButton(0)) return false;
		}
		if(!buttons.empty())
		{
			if(!rollback && ConsumeTestFailure(ToolbarLayoutAdapter::TestFailureBeforeAdd)) return false;
			if(!target.AddButtons(static_cast<int>(buttons.size()), const_cast<TBBUTTON*>(&buttons[0]))) return false;
		}
		target.AutoSize();
		return Matches(toolbar, buttons);
	}
}

bool ToolbarLayoutAdapter::Capture(HWND toolbar, std::vector<PortableToolbarItem>& items)
{
	return CaptureExact(toolbar, items);
}

bool ToolbarLayoutAdapter::Apply(HWND toolbar, const std::vector<PortableToolbarItem>& items, const std::vector<TBBUTTON>& catalog)
{
	if(!::IsWindow(toolbar)) return false;
	std::vector<PortableToolbarItem> previous;
	if(!CaptureExact(toolbar, previous)) return false;
	const UINT dpi = UiMetrics::DpiForWindow(toolbar);
	std::vector<TBBUTTON> target, original;
	if(!BuildButtons(items, catalog, dpi, target) || !BuildButtons(previous, catalog, dpi, original)) return false;
	if(ReplaceChecked(toolbar, target, false)) return true;
	if(!ReplaceChecked(toolbar, original, true)) ::OutputDebugString(L"ToolbarLayoutAdapter rollback failed.\n");
	return false;
}

void ToolbarLayoutAdapter::SetTestFailurePointForTest(TestFailurePoint point)
{
	g_testFailurePoint = point;
}
