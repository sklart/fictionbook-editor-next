#pragma once
#include "PortableToolbarLayout.h"

// PortableToolbarItem::width is always measured in logical 96-DPI pixels.
// Keeping this conversion here makes every toolbar customization route use
// exactly the same separator semantics.
namespace ToolbarLayoutAdapter
{
	enum TestFailurePoint
	{
		TestFailureNone,
		TestFailureBeforeDelete,
		TestFailureBeforeAdd,
		TestFailureBeforeRollback
	};

	bool Capture(HWND toolbar, std::vector<PortableToolbarItem>& items);
	bool Apply(HWND toolbar, const std::vector<PortableToolbarItem>& items, const std::vector<TBBUTTON>& catalog);

	// Deterministic fault injection is intentionally limited to the native
	// regression host.  Production callers never set this value.
	void SetTestFailurePointForTest(TestFailurePoint point);
}
