#include "stdafx.h"
#include "ScriptVisualResources.h"
#include "ScriptMenuBitmap.h"

namespace FbeScripts
{

VisualResource VisualResources::Load(const CString& directory, const CString& baseName, bool folder) const
{
	VisualResource result;
	const CString base = directory + baseName;
	const CString bitmapPath = base + L".bmp";
	const DWORD bitmapAttributes = ::GetFileAttributes(bitmapPath);
	if (bitmapAttributes != INVALID_FILE_ATTRIBUTES && (bitmapAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		result.bitmap = static_cast<HBITMAP>(::LoadImage(NULL, bitmapPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
	if(result.bitmap != NULL)
	{
		// Match WTL AddBitmap's default mask and turn it into a native alpha
		// bitmap; passing the original BMP to MIIM_BITMAP loses that mask.
		HIMAGELIST images = ::ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);
		if(images != NULL) {
			const int index = ::ImageList_AddMasked(images, result.bitmap, RGB(192, 192, 192));
			HICON icon = index < 0 ? NULL : ::ImageList_GetIcon(images, index, ILD_TRANSPARENT);
			if(icon != NULL) { result.menuBitmap = Detail::CreateMenuBitmap(icon); ::DestroyIcon(icon); }
			::ImageList_Destroy(images);
		}
	}
	if (result.bitmap == NULL)
	{
		const CString iconPath = base + L".ico";
		const DWORD iconAttributes = ::GetFileAttributes(iconPath);
		if (iconAttributes != INVALID_FILE_ATTRIBUTES && (iconAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			result.icon = static_cast<HICON>(::LoadImage(NULL, iconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE));
		if(result.icon == NULL)
		{
			SHFILEINFOW info = {};
			const DWORD attributes = folder ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
			::SHGetFileInfo(folder ? L"folder" : L"script.js", attributes, &info, sizeof(info),
				SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
			result.icon = info.hIcon;
		}
		result.menuBitmap = Detail::CreateMenuBitmap(result.icon);
	}
	return result;
}
}
