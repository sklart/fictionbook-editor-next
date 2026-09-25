#include "stdafx.h"
#include "ScriptVisualResources.h"

namespace FbeScripts
{
namespace
{
HBITMAP CreateMenuBitmap(HICON icon)
{
	if(icon == NULL) return NULL;
	HIMAGELIST images = ::ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);
	if(images == NULL || ::ImageList_AddIcon(images, icon) == -1)
	{
		if(images != NULL) ::ImageList_Destroy(images);
		return NULL;
	}
	HDC screen = ::GetDC(NULL);
	HDC memory = screen != NULL ? ::CreateCompatibleDC(screen) : NULL;
	BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(info.bmiHeader); info.bmiHeader.biWidth = 16;
	info.bmiHeader.biHeight = 16; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
	HBITMAP bitmap = memory != NULL ? ::CreateDIBSection(screen, &info, DIB_RGB_COLORS, NULL, NULL, 0) : NULL;
	if(bitmap != NULL)
	{
		HGDIOBJ previous = ::SelectObject(memory, bitmap);
		IMAGELISTDRAWPARAMS draw = {}; draw.cbSize = sizeof(draw); draw.himl = images; draw.i = 0;
		draw.hdcDst = memory; draw.fStyle = ILD_TRANSPARENT; draw.fState = ILS_ALPHA; draw.Frame = 255;
		if(!::ImageList_DrawIndirect(&draw)) { ::SelectObject(memory, previous); ::DeleteObject(bitmap); bitmap = NULL; }
		else ::SelectObject(memory, previous);
	}
	if(memory != NULL) ::DeleteDC(memory);
	if(screen != NULL) ::ReleaseDC(NULL, screen);
	::ImageList_Destroy(images);
	return bitmap;
}
}

VisualResource VisualResources::Load(const CString& directory, const CString& baseName, bool folder) const
{
	VisualResource result;
	const CString base = directory + baseName;
	const CString bitmapPath = base + L".bmp";
	const DWORD bitmapAttributes = ::GetFileAttributes(bitmapPath);
	if (bitmapAttributes != INVALID_FILE_ATTRIBUTES && (bitmapAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		result.bitmap = static_cast<HBITMAP>(::LoadImage(NULL, bitmapPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
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
		result.menuBitmap = CreateMenuBitmap(result.icon);
	}
	return result;
}
}
