#pragma once

namespace FB { class Doc; }
struct DocumentOpenSource;

class DocumentLoader
{
public:
	static bool Load(FB::Doc& document, HWND parent, const DocumentOpenSource& source);
};
