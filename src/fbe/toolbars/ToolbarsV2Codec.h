#pragma once

#include "PortableToolbarLayout.h"

namespace ToolbarsV2Codec
{
bool Parse(const CString& xml, PortableToolbarLayout& layout);
CString Serialize(const PortableToolbarLayout& layout);
}
