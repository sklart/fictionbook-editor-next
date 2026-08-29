#include <cstdlib>
#include <iostream>
#include "StatusBarBehavior.h"

static void Need(bool value, const char* message) { if(!value) { std::cerr << message << std::endl; std::exit(1); } }
static void SetWidths(int widths[FBEStatusBar::PaneCount]) { for(int i = 0; i < FBEStatusBar::PaneCount; ++i) widths[i] = 100; }

int main()
{
	const unsigned all = (1u << FBEStatusBar::PaneCount) - 1;
	unsigned visible = FBEStatusBar::TogglePaneVisibility(all, FBEStatusBar::Encoding);
	Need(!(visible & FBEStatusBar::PaneBit(FBEStatusBar::Encoding)), "persist user hidden");
	visible = FBEStatusBar::TogglePaneVisibility(visible, FBEStatusBar::Encoding);
	Need(visible == all, "restore user hidden");

	int narrow[FBEStatusBar::PaneCount]; SetWidths(narrow);
	visible = all & ~FBEStatusBar::PaneBit(FBEStatusBar::Validation);
	FBEStatusBar::ApplyPaneVisibility(visible, 250, narrow);
	Need(narrow[FBEStatusBar::Validation] == 0 && narrow[FBEStatusBar::Selection] == 0 && narrow[FBEStatusBar::Encoding] == 0 && narrow[FBEStatusBar::Character] == 0 && narrow[FBEStatusBar::Position] == 100, "user hidden plus auto hide");
	int wide[FBEStatusBar::PaneCount]; SetWidths(wide);
	FBEStatusBar::ApplyPaneVisibility(visible, 1100, wide);
	Need(wide[FBEStatusBar::Selection] == 100 && wide[FBEStatusBar::Encoding] == 100 && wide[FBEStatusBar::Character] == 100 && wide[FBEStatusBar::Validation] == 0, "auto panes restore without user hidden");
	int insertHidden[FBEStatusBar::PaneCount]; SetWidths(insertHidden);
	visible = all & ~FBEStatusBar::PaneBit(FBEStatusBar::InsertMode);
	FBEStatusBar::ApplyPaneVisibility(visible, 500, insertHidden);
	Need(insertHidden[FBEStatusBar::InsertMode] == 0 && insertHidden[FBEStatusBar::Position] == 100 && insertHidden[FBEStatusBar::Selection] == 100 && insertHidden[FBEStatusBar::Character] == 100 && insertHidden[FBEStatusBar::Encoding] == 100 && insertHidden[FBEStatusBar::Validation] == 100, "hidden insert mode does not reserve width");

	Need(FBEStatusBar::ClickAction(FBEStatusBar::Validation) == FBEStatusBar::Validate, "validation click");
	Need(FBEStatusBar::DoubleClickAction(FBEStatusBar::InsertMode, true, false) == FBEStatusBar::ToggleSourceOverwrite, "source overtype");
	Need(FBEStatusBar::DoubleClickAction(FBEStatusBar::InsertMode, false, true) == FBEStatusBar::ToggleBodyOverwrite, "body overtype");
	Need(FBEStatusBar::DoubleClickAction(FBEStatusBar::Character, false, false) == FBEStatusBar::CopyUnicodeReference, "unicode double click");
	Need(FBEStatusBar::DecimalXmlReference(L"U+1F600  &#128512;") == L"&#128512;", "unicode clipboard reference");
	return 0;
}
