#pragma once

namespace FbeScripts
{
struct VisualResource
{
	HBITMAP bitmap;
	HICON icon;
	VisualResource() : bitmap(NULL), icon(NULL) {}
	VisualResource(const VisualResource&) = delete;
	VisualResource& operator=(const VisualResource&) = delete;
	VisualResource(VisualResource&& other) : bitmap(other.bitmap), icon(other.icon) { other.bitmap = NULL; other.icon = NULL; }
	VisualResource& operator=(VisualResource&& other) { if (this != &other) { Reset(); bitmap = other.bitmap; icon = other.icon; other.bitmap = NULL; other.icon = NULL; } return *this; }
	~VisualResource() { Reset(); }
	void Reset() { if (bitmap != NULL) ::DeleteObject(bitmap); if (icon != NULL) ::DestroyIcon(icon); bitmap = NULL; icon = NULL; }
};

class VisualResources
{
public:
	VisualResource Load(const CString& directory, const CString& baseName) const;
};
}
