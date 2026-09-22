#include "stdafx.h"
#include "ToolbarFactory.h"
#include "..\\StartupTrace.h"
#include "..\\UiMetrics.h"
namespace { struct ToolbarResourceData { WORD version; WORD width; WORD height; WORD itemCount; WORD* Items() { return reinterpret_cast<WORD*>(this + 1); } }; BOOL CALLBACK SetDialogFontForToolbarChild(HWND window, LPARAM) { ::SendMessage(window, WM_SETFONT, reinterpret_cast<WPARAM>(UiMetrics::DialogFont()), TRUE); return TRUE; } bool CopyToolbarImages(HIMAGELIST destination, HIMAGELIST source, int imageCount) { for(int index = 0; index < imageCount; ++index) { HICON icon = ::ImageList_GetIcon(source, index, ILD_NORMAL); const int copiedIndex = icon != NULL ? ::ImageList_AddIcon(destination, icon) : -1; if(icon != NULL) ::DestroyIcon(icon); if(copiedIndex != index) return false; } return true; } }
bool ToolbarFactory::ImageListHasMaskPlane(HIMAGELIST imageList) { IMAGEINFO imageInfo = {}; return imageList != NULL && ::ImageList_GetImageInfo(imageList, 0, &imageInfo) != FALSE && imageInfo.hbmMask != NULL; }
void ToolbarFactory::SetDialogFontForToolbarRow(HWND window, bool includeChildren) { if(window == NULL) return; ::SendMessage(window, WM_SETFONT, reinterpret_cast<WPARAM>(UiMetrics::DialogFont()), TRUE); if(includeChildren) ::EnumChildWindows(window, SetDialogFontForToolbarChild, 0); }
void ToolbarFactory::AutoSizeToolbar(HWND window) { if(window != NULL) ::SendMessage(window, TB_AUTOSIZE, 0, 0); }
HWND ToolbarFactory::CreateCommandToolbarCtrl(HWND parent, CImageList& ownedImages, UINT toolbarResourceId, DWORD style, UINT controlId) { HINSTANCE module = _Module.GetResourceInstance(); HRSRC resource = ::FindResource(module, MAKEINTRESOURCE(toolbarResourceId), RT_TOOLBAR); HGLOBAL resourceData = resource != NULL ? ::LoadResource(module, resource) : NULL; ToolbarResourceData* toolbarData = resourceData != NULL ? static_cast<ToolbarResourceData*>(::LockResource(resourceData)) : NULL; if(toolbarData == NULL || toolbarData->version != 1 || toolbarData->width != 24 || toolbarData->height != 24) return NULL; ATL::CTempBuffer<TBBUTTON, _WTL_STACK_ALLOC_THRESHOLD> buttonsBuffer; TBBUTTON* buttons = buttonsBuffer.Allocate(toolbarData->itemCount); if(buttons == NULL) return NULL; int standardImageCount = 0; for(int index = 0; index < toolbarData->itemCount; ++index) { TBBUTTON& button = buttons[index]; ::ZeroMemory(&button, sizeof(button)); const WORD commandId = toolbarData->Items()[index]; if(commandId != 0) { button.iBitmap = standardImageCount++; button.idCommand = commandId; button.fsState = TBSTATE_ENABLED; button.fsStyle = BTNS_BUTTON; } else { button.iBitmap = 8; button.fsStyle = BTNS_SEP; } } HWND window = ::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, style, 0, 0, 100, 100, parent, (HMENU)LongToHandle(controlId), module, NULL); if(window == NULL) return NULL; ::SendMessage(window, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0); if(!ownedImages.Create(24, 24, ILC_COLOR32 | ILC_MASK, standardImageCount + 8, 8)) { ::DestroyWindow(window); return NULL; } HIMAGELIST sourceImages = ::ImageList_LoadImage(module, MAKEINTRESOURCE(toolbarResourceId), 24, 1, CLR_DEFAULT, IMAGE_BITMAP, LR_CREATEDIBSECTION | LR_DEFAULTSIZE); const bool copied = sourceImages != NULL && ::ImageList_GetImageCount(sourceImages) >= standardImageCount && CopyToolbarImages(ownedImages, sourceImages, standardImageCount); if(sourceImages != NULL) ::ImageList_Destroy(sourceImages); if(!copied) { ownedImages.Destroy(); ::DestroyWindow(window); return NULL; } const HIMAGELIST previousImages = reinterpret_cast<HIMAGELIST>(::SendMessage(window, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(static_cast<HIMAGELIST>(ownedImages)))); if(previousImages != NULL || ::SendMessage(window, TB_ADDBUTTONS, toolbarData->itemCount, reinterpret_cast<LPARAM>(buttons)) == FALSE) { ::SendMessage(window, TB_SETIMAGELIST, 0, 0); ownedImages.Destroy(); ::DestroyWindow(window); return NULL; } SetDialogFontForToolbarRow(window); ::SendMessage(window, TB_SETBITMAPSIZE, 0, MAKELONG(24, 24)); ::SendMessage(window, TB_SETBUTTONSIZE, 0, MAKELONG(toolbarData->width + 7, toolbarData->height + 7)); AutoSizeToolbar(window); StartupTrace::Event(L"toolbar", L"TB210", L"command-toolbar image list created; 24x24; ILC_COLOR32|ILC_MASK"); return window; }
HBITMAP ToolbarFactory::CreateAlphaBitmap(HBITMAP source, int width, int height)
{
	DIBSECTION sourceInfo = {};
	if(source == NULL || ::GetObject(source, sizeof(sourceInfo), &sourceInfo) != sizeof(sourceInfo) ||
		sourceInfo.dsBm.bmWidth != width || sourceInfo.dsBmih.biHeight == 0 ||
		(sourceInfo.dsBmih.biHeight < 0 ? -sourceInfo.dsBmih.biHeight : sourceInfo.dsBmih.biHeight) != height ||
		sourceInfo.dsBm.bmBitsPixel != 24 || sourceInfo.dsBm.bmBits == NULL || sourceInfo.dsBm.bmWidthBytes < width * 3)
		return NULL;

	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	void* targetBits = NULL;
	HBITMAP target = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &targetBits, NULL, 0);
	if(target == NULL || targetBits == NULL) { if(target != NULL) ::DeleteObject(target); return NULL; }

	const BYTE* sourceBits = static_cast<const BYTE*>(sourceInfo.dsBm.bmBits);
	DWORD* pixels = static_cast<DWORD*>(targetBits);
	const bool sourceBottomUp = sourceInfo.dsBmih.biHeight > 0;
	// Table toolbar source bitmaps reserve exact magenta for transparent canvas.
	// It never occurs in a glyph, so white and grey table cells remain opaque.
	const BYTE keyBlue = 0xFF, keyGreen = 0x00, keyRed = 0xFF;
	for(int y = 0; y < height; ++y)
	{
		const int sourceY = sourceBottomUp ? height - 1 - y : y;
		const BYTE* row = sourceBits + sourceY * sourceInfo.dsBm.bmWidthBytes;
		for(int x = 0; x < width; ++x)
		{
			const BYTE blue = row[x * 3];
			const BYTE green = row[x * 3 + 1];
			const BYTE red = row[x * 3 + 2];
			const int index = y * width + x;
			pixels[index] = blue == keyBlue && green == keyGreen && red == keyRed ? 0 :
				0xFF000000 | (static_cast<DWORD>(red) << 16) | (static_cast<DWORD>(green) << 8) | blue;
		}
	}
	return target;
}

int ToolbarFactory::AddBitmapFromModule(CToolBarCtrl& toolbar, HINSTANCE module, UINT bitmapResourceId)
{
	HBITMAP source = static_cast<HBITMAP>(::LoadImage(module, MAKEINTRESOURCE(bitmapResourceId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
	if(source == NULL) return -1;
	HBITMAP alpha = CreateAlphaBitmap(source, 24, 24);
	::DeleteObject(source);
	if(alpha == NULL) return -1;
	const int imageIndex = ::ImageList_Add(toolbar.GetImageList(), alpha, NULL);
	::DeleteObject(alpha);
	return imageIndex;
}
