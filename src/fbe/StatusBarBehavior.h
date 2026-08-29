#pragma once

#include <string>

namespace FBEStatusBar {
enum Pane { Position, Selection, Character, Encoding, Validation, InsertMode, PaneCount };
enum Action { NoAction, Validate, ToggleSourceOverwrite, ToggleBodyOverwrite, CopyUnicodeReference };

inline unsigned PaneBit(Pane pane) { return 1u << static_cast<unsigned>(pane); }
inline unsigned TogglePaneVisibility(unsigned visible, Pane pane) { return visible ^ PaneBit(pane); }

inline void ApplyPaneVisibility(unsigned visible, int available, int widths[PaneCount])
{
	for(int pane = Position; pane < PaneCount; ++pane)
		if(!(visible & PaneBit(static_cast<Pane>(pane)))) widths[pane] = 0;
	auto total = [&]() { return widths[Position] + widths[Selection] + widths[Character] + widths[Encoding] + widths[Validation]; };
	const Pane autoHideOrder[] = { Selection, Encoding, Validation, Character };
	for(int i = 0; i < sizeof(autoHideOrder) / sizeof(autoHideOrder[0]) && total() > available; ++i)
		widths[autoHideOrder[i]] = 0;
}

inline Action ClickAction(Pane pane) { return pane == Validation ? Validate : NoAction; }
inline Action DoubleClickAction(Pane pane, bool source, bool body) {
	if(pane == InsertMode) return source ? ToggleSourceOverwrite : body ? ToggleBodyOverwrite : NoAction;
	return pane == Character ? CopyUnicodeReference : NoAction;
}

inline std::wstring DecimalXmlReference(const std::wstring& text)
{
	const std::wstring::size_type begin = text.find(L"&#"), end = text.find(L';', begin);
	return begin != std::wstring::npos && end != std::wstring::npos && end > begin + 2 ? text.substr(begin, end - begin + 1) : std::wstring();
}
}
