#include <cstdlib>
#include <iostream>
#include "SearchViewportPosition.h"

static void Need(bool value, const char* message) { if(!value) { std::cerr << message << std::endl; std::exit(1); } }

static FBESearchViewport::PlacementInput Input(long scrollTop, long matchTop, long clientHeight = 600, long matchHeight = 20)
{
	FBESearchViewport::Rect match = { 200, matchTop, 300, matchTop + matchHeight };
	FBESearchViewport::PlacementInput input = { scrollTop, 5000, clientHeight, match,
		FBESearchViewport::PreferredMatchTop(clientHeight, 96), 0, NULL, 0 };
	return input;
}

int main()
{
	Need(FBESearchViewport::MinimumContextTopForDpi(96) == 64, "100 percent DPI context");
	Need(FBESearchViewport::MinimumContextTopForDpi(120) == 80, "125 percent DPI context");
	Need(FBESearchViewport::MinimumContextTopForDpi(144) == 96, "150 percent DPI context");
	Need(FBESearchViewport::MinimumContextTopForDpi(192) == 128, "200 percent DPI context");
	Need(FBESearchViewport::PreferredMatchTop(300, 96) == 64, "small viewport minimum context");
	Need(FBESearchViewport::PreferredMatchTop(600, 96) == 108, "relative preferred position");
	Need(FBESearchViewport::PreferredMatchTop(900, 96) == 162, "large viewport relative position");
	Need(FBESearchViewport::PreferredMatchTop(600, 192) == 128, "high DPI preferred position");
	Need(FBESearchViewport::ScaleToViewport(300, 600, 400) == 200, "Win32 to MSHTML coordinate scaling");

	Need(FBESearchViewport::ScrollTopForMatch(Input(1000, -200)) == 692, "match above viewport");
	Need(FBESearchViewport::ScrollTopForMatch(Input(1000, 900)) == 1792, "match below viewport");
	Need(FBESearchViewport::ScrollTopForMatch(Input(1000, 300)) == 1192, "match within viewport");
	Need(FBESearchViewport::ScrollTopForMatch(Input(0, 0)) == 0, "document start");
	Need(FBESearchViewport::ScrollTopForMatch(Input(4400, 599)) == 4400, "document end");

	FBESearchViewport::Rect find = { 100, 0, 400, 180 };
	FBESearchViewport::PlacementInput obscured = Input(1000, 300);
	obscured.obstructionMargin = 12;
	obscured.obstructions = &find;
	obscured.obstructionCount = 1;
	Need(FBESearchViewport::ScrollTopForMatch(obscured) == 1108, "find dialog obstruction");
	find.top = 250; find.bottom = 350;
	Need(FBESearchViewport::ScrollTopForMatch(obscured) == 1192, "non-overlapping dialog");
	FBESearchViewport::Rect dialogs[] = { { 100, 0, 400, 150 }, { 100, 160, 400, 260 } };
	obscured.obstructions = dialogs;
	obscured.obstructionCount = 2;
	Need(FBESearchViewport::ScrollTopForMatch(obscured) == 1028, "find and replace obstructions");
	FBESearchViewport::Rect multiLine = { 200, 300, 300, 430 };
	FBESearchViewport::PlacementInput longMatch = { 1000, 5000, 600, multiLine, 108, 12, dialogs, 2 };
	Need(FBESearchViewport::ScrollTopForMatch(longMatch) == 1028, "entire multi-line match avoids dialog");
	using FBESearchViewport::DialogPositionForMatch;
	const FBESearchViewport::Rect work = { -1920, 0, 0, 1080 };
	const FBESearchViewport::Rect viewport = { -1900, 40, -20, 1040 };
	const FBESearchViewport::Rect dialog = { -1000, 100, -400, 400 };
	FBESearchViewport::Rect moved = {};
	Need(!DialogPositionForMatch(dialog, viewport, work, { -1800, 100, -1700, 120 }, moved), "unobscured match keeps dialog");
	Need(DialogPositionForMatch(dialog, viewport, work, { -900, 150, -800, 170 }, moved), "overlap moves dialog");
	Need(moved.top == 740 && moved.left == dialog.left, "top match moves dialog to opposite side");
	Need(moved.left >= work.left && moved.right <= work.right && moved.top >= work.top && moved.bottom <= work.bottom, "negative-monitor work area respected");
	Need(!FBESearchViewport::Intersects(moved, { -900, 150, -800, 170 }), "moved dialog does not obscure match");
	for (unsigned dpi : { 96u, 120u, 144u, 192u }) {
		const long scale = static_cast<long>(dpi);
		FBESearchViewport::Rect d = { 100 * scale / 96, 100 * scale / 96, 400 * scale / 96, 300 * scale / 96 };
		FBESearchViewport::Rect v = { 0, 0, 1000 * scale / 96, 800 * scale / 96 };
		FBESearchViewport::Rect hit = { 120 * scale / 96, 120 * scale / 96, 160 * scale / 96, 140 * scale / 96 };
		Need(DialogPositionForMatch(d, v, v, hit, moved) && !FBESearchViewport::Intersects(moved, hit), "DPI-scaled placement");
	}
	Need(!DialogPositionForMatch({0, 0, 100, 100}, {0, 0, 100, 100}, {0, 0, 100, 100}, {0, 0, 100, 100}, moved), "no room keeps dialog");
	return 0;
}
