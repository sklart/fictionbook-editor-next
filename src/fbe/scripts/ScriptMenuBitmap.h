#pragma once
#include <windows.h>
#include <commctrl.h>

namespace FbeScripts
{
namespace Detail
{
inline HBITMAP CreateMenuBitmap(HICON icon, int size = 16)
{
	if(icon == NULL || size <= 0) return NULL;
	HIMAGELIST images = ::ImageList_Create(size, size, ILC_COLOR32 | ILC_MASK, 1, 1);
	if(images == NULL || ::ImageList_AddIcon(images, icon) == -1)
	{
		if(images != NULL) ::ImageList_Destroy(images);
		return NULL;
	}
	HDC screen = ::GetDC(NULL);
	HDC memory = screen != NULL ? ::CreateCompatibleDC(screen) : NULL;
	BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(info.bmiHeader); info.bmiHeader.biWidth = size;
	info.bmiHeader.biHeight = size; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
	void* bits = NULL;
	HBITMAP bitmap = memory != NULL ? ::CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0) : NULL;
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

}
