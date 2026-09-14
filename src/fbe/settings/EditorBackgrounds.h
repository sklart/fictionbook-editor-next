#pragma once

#include <vector>

struct EditorBackgroundDescriptor
{
	CString id;
	CString name;
	CString localizationKey;
	CString fileName;
	CString theme;
	CString fallbackColor;
	CString recommendedTextColor;
};

// The runtime catalogue is deliberately optional: a bad or missing manifest only
// disables built-in images and never affects the editor's normal colour background.
class EditorBackgrounds
{
public:
	static void Load(std::vector<EditorBackgroundDescriptor>& backgrounds);
	static bool ResolveBuiltIn(const CString& id, CString& filePath);
	// Returns the manifest's contrast-safe fallback and text colours for a
	// built-in background.  User-supplied images deliberately have no such
	// policy and remain entirely user-controlled.
	static bool GetBuiltInRecommendedColors(const CString& id, COLORREF& fallbackColor, COLORREF& textColor);
	static bool IsSupportedLocalImage(const CString& path);
};
