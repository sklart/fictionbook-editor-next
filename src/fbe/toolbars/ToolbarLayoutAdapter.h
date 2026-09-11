#pragma once
#include "PortableToolbarLayout.h"
namespace ToolbarLayoutAdapter { void Capture(HWND toolbar, std::vector<PortableToolbarItem>& items); void Apply(HWND toolbar, const std::vector<PortableToolbarItem>& items, const std::vector<TBBUTTON>& catalog); }
