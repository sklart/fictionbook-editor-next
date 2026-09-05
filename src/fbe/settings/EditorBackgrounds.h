#pragma once

#include <vector>

struct EditorBackgroundDescriptor
{
	CString id;
	CString name;
	CString localizationKey;
	CString fileName;
	CString theme;
};

// The runtime catalogue is deliberately optional: a bad or missing manifest only
// disables built-in images and never affects the editor's normal colour background.
class EditorBackgrounds
{
public:
	static void Load(std::vector<EditorBackgroundDescriptor>& backgrounds);
	static bool ResolveBuiltIn(const CString& id, CString& filePath);
	static bool IsSupportedLocalImage(const CString& path);
};
