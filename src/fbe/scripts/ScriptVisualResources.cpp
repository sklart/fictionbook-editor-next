#include "stdafx.h"
#include "ScriptVisualResources.h"

namespace FbeScripts
{
VisualResource VisualResources::Load(const CString& directory, const CString& baseName) const
{
	VisualResource result;
	const CString base = directory + baseName;
	const CString bitmapPath = base + L".bmp";
	if ((::GetFileAttributes(bitmapPath) & FILE_ATTRIBUTE_DIRECTORY) == 0)
		result.bitmap = static_cast<HBITMAP>(::LoadImage(NULL, bitmapPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
	if (result.bitmap == NULL)
	{
		const CString iconPath = base + L".ico";
		if ((::GetFileAttributes(iconPath) & FILE_ATTRIBUTE_DIRECTORY) == 0)
			result.icon = static_cast<HICON>(::LoadImage(NULL, iconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE));
	}
	return result;
}
}
