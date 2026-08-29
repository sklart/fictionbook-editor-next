#include <cstdlib>
#include <iostream>
#include "SearchViewportPosition.h"

static void Need(bool value, const char* message) { if(!value) { std::cerr << message << std::endl; std::exit(1); } }

static FBESearchViewport::PlacementInput Input(long scrollTop, long matchTop, long clientHeight = 600)
{
	FBESearchViewport::PlacementInput input = { scrollTop, 5000, clientHeight, matchTop,
		FBESearchViewport::PreferredMatchTop(clientHeight, 96), 0, 0, 0, false };
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

	Need(FBESearchViewport::ScrollTopForMatch(Input(1000, -200)) == 692, "match above viewport");
	Need(FBESearchViewport::ScrollTopForMatch(Input(1000, 900)) == 1792, "match below viewport");
	Need(FBESearchViewport::ScrollTopForMatch(Input(1000, 300)) == 1192, "match within viewport");
	Need(FBESearchViewport::ScrollTopForMatch(Input(0, 0)) == 0, "document start");
	Need(FBESearchViewport::ScrollTopForMatch(Input(4400, 599)) == 4400, "document end");

	FBESearchViewport::PlacementInput obscured = Input(1000, 300);
	obscured.obstructionOverlapsViewport = true;
	obscured.obstructionTop = 0;
	obscured.obstructionBottom = 180;
	obscured.obstructionMargin = 12;
	Need(FBESearchViewport::ScrollTopForMatch(obscured) == 1108, "find dialog obstruction");
	obscured.obstructionTop = 250;
	obscured.obstructionBottom = 350;
	Need(FBESearchViewport::ScrollTopForMatch(obscured) == 1192, "non-overlapping dialog");
	return 0;
}
