#pragma once

namespace FbeScripts
{
struct VisualResource
{
	HBITMAP bitmap;
	HICON icon;
	HBITMAP menuBitmap;
	VisualResource() : bitmap(NULL), icon(NULL), menuBitmap(NULL) {}
	VisualResource(const VisualResource&) = delete;
	VisualResource& operator=(const VisualResource&) = delete;
	VisualResource(VisualResource&& other) : bitmap(other.bitmap), icon(other.icon), menuBitmap(other.menuBitmap) { other.bitmap = NULL; other.icon = NULL; other.menuBitmap = NULL; }
	VisualResource& operator=(VisualResource&& other) { if (this != &other) { Reset(); bitmap = other.bitmap; icon = other.icon; menuBitmap = other.menuBitmap; other.bitmap = NULL; other.icon = NULL; other.menuBitmap = NULL; } return *this; }
	~VisualResource() { Reset(); }
	void Reset() { if (bitmap != NULL) ::DeleteObject(bitmap); if (icon != NULL) ::DestroyIcon(icon); if (menuBitmap != NULL) ::DeleteObject(menuBitmap); bitmap = NULL; icon = NULL; menuBitmap = NULL; }
	HBITMAP NativeMenuBitmap() const { return menuBitmap != NULL ? menuBitmap : bitmap; }
};

class VisualResources
{
public:
	VisualResource Load(const CString& directory, const CString& baseName) const;
};
}
