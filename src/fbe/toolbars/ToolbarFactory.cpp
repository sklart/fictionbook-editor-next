#include "stdafx.h"
#include "ToolbarFactory.h"
#include "..\\StartupTrace.h"
#include "..\\UiMetrics.h"
namespace { struct ToolbarResourceData { WORD version; WORD width; WORD height; WORD itemCount; WORD* Items() { return reinterpret_cast<WORD*>(this + 1); } }; BOOL CALLBACK SetDialogFontForToolbarChild(HWND window, LPARAM) { ::SendMessage(window, WM_SETFONT, reinterpret_cast<WPARAM>(UiMetrics::DialogFont()), TRUE); return TRUE; } bool CopyToolbarImages(HIMAGELIST destination, HIMAGELIST source, int imageCount) { for(int index = 0; index < imageCount; ++index) { HICON icon = ::ImageList_GetIcon(source, index, ILD_NORMAL); const int copiedIndex = icon != NULL ? ::ImageList_AddIcon(destination, icon) : -1; if(icon != NULL) ::DestroyIcon(icon); if(copiedIndex != index) return false; } return true; } }
bool ToolbarFactory::ImageListHasMaskPlane(HIMAGELIST imageList) { IMAGEINFO imageInfo = {}; return imageList != NULL && ::ImageList_GetImageInfo(imageList, 0, &imageInfo) != FALSE && imageInfo.hbmMask != NULL; }
void ToolbarFactory::SetDialogFontForToolbarRow(HWND window, bool includeChildren) { if(window == NULL) return; ::SendMessage(window, WM_SETFONT, reinterpret_cast<WPARAM>(UiMetrics::DialogFont()), TRUE); if(includeChildren) ::EnumChildWindows(window, SetDialogFontForToolbarChild, 0); }
void ToolbarFactory::AutoSizeToolbar(HWND window) { if(window != NULL) ::SendMessage(window, TB_AUTOSIZE, 0, 0); }
HWND ToolbarFactory::CreateCommandToolbarCtrl(HWND parent, CImageList& ownedImages, UINT toolbarResourceId, UINT dpi, DWORD style, UINT controlId)
{
	HINSTANCE module = _Module.GetResourceInstance(); HRSRC resource = ::FindResource(module, MAKEINTRESOURCE(toolbarResourceId), RT_TOOLBAR);
	HGLOBAL resourceData = resource != NULL ? ::LoadResource(module, resource) : NULL;
	ToolbarResourceData* toolbarData = resourceData != NULL ? static_cast<ToolbarResourceData*>(::LockResource(resourceData)) : NULL;
	if(toolbarData == NULL || toolbarData->version != 1 || toolbarData->width != 24 || toolbarData->height != 24) return NULL;
	ATL::CTempBuffer<TBBUTTON, _WTL_STACK_ALLOC_THRESHOLD> buttonsBuffer; TBBUTTON* buttons = buttonsBuffer.Allocate(toolbarData->itemCount); if(buttons == NULL) return NULL;
	int standardImageCount = 0;
	for(int index = 0; index < toolbarData->itemCount; ++index) { TBBUTTON& button = buttons[index]; ::ZeroMemory(&button, sizeof(button)); const WORD commandId = toolbarData->Items()[index]; if(commandId != 0) { button.iBitmap = standardImageCount++; button.idCommand = commandId; button.fsState = TBSTATE_ENABLED; button.fsStyle = BTNS_BUTTON; } else { button.iBitmap = 8; button.fsStyle = BTNS_SEP; } }
	HWND window = ::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, style, 0, 0, 100, 100, parent, (HMENU)LongToHandle(controlId), module, NULL); if(window == NULL) return NULL;
	::SendMessage(window, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	if(!CreateCommandToolbarImages(ownedImages, toolbarResourceId, dpi)) { ::DestroyWindow(window); return NULL; }
	const HIMAGELIST previousImages = reinterpret_cast<HIMAGELIST>(::SendMessage(window, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(static_cast<HIMAGELIST>(ownedImages))));
	if(previousImages != NULL || ::SendMessage(window, TB_ADDBUTTONS, toolbarData->itemCount, reinterpret_cast<LPARAM>(buttons)) == FALSE) { ::SendMessage(window, TB_SETIMAGELIST, 0, 0); ownedImages.Destroy(); ::DestroyWindow(window); return NULL; }
	SetDialogFontForToolbarRow(window); ApplyCommandToolbarMetrics(window, toolbarResourceId, dpi);
	StartupTrace::Event(L"toolbar", L"TB210", L"command-toolbar image list created at current DPI; ILC_COLOR32|ILC_MASK"); return window;
}
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

HBITMAP ToolbarFactory::CreateScaledAlphaBitmap(HBITMAP source, int sourceSize, int targetSize)
{
	HBITMAP alpha = CreateAlphaBitmap(source, sourceSize, sourceSize);
	if(alpha == NULL || targetSize <= 0) return NULL;
	if(targetSize == sourceSize) return alpha;
	BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(info.bmiHeader); info.bmiHeader.biWidth = targetSize; info.bmiHeader.biHeight = -targetSize; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
	DIBSECTION alphaInfo = {};
	if(::GetObject(alpha, sizeof(alphaInfo), &alphaInfo) != sizeof(alphaInfo) || alphaInfo.dsBm.bmBits == NULL) { ::DeleteObject(alpha); return NULL; }
	void* targetBits = NULL; HBITMAP scaled = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &targetBits, NULL, 0);
	if(scaled == NULL || targetBits == NULL) { if(scaled) ::DeleteObject(scaled); ::DeleteObject(alpha); return NULL; }
	const DWORD* sourcePixels = static_cast<const DWORD*>(alphaInfo.dsBm.bmBits);
	DWORD* targetPixels = static_cast<DWORD*>(targetBits);
	// Interpolate premultiplied ARGB. StretchBlt treats this DIB as ordinary RGB,
	// losing alpha and turning the transparent magenta canvas into an opaque black halo.
	for(int y = 0; y < targetSize; ++y) for(int x = 0; x < targetSize; ++x)
	{
		const double sourceX = (static_cast<double>(x) + 0.5) * sourceSize / targetSize - 0.5;
		const double sourceY = (static_cast<double>(y) + 0.5) * sourceSize / targetSize - 0.5;
		const double clampedX = max(0.0, min(static_cast<double>(sourceSize - 1), sourceX));
		const double clampedY = max(0.0, min(static_cast<double>(sourceSize - 1), sourceY));
		const int left = static_cast<int>(floor(clampedX)), top = static_cast<int>(floor(clampedY));
		const int right = min(sourceSize - 1, left + 1), bottom = min(sourceSize - 1, top + 1);
		const double fx = clampedX - left, fy = clampedY - top;
		const DWORD samples[] = { sourcePixels[top * sourceSize + left], sourcePixels[top * sourceSize + right], sourcePixels[bottom * sourceSize + left], sourcePixels[bottom * sourceSize + right] };
		const double weights[] = { (1.0 - fx) * (1.0 - fy), fx * (1.0 - fy), (1.0 - fx) * fy, fx * fy };
		double a = 0.0, r = 0.0, g = 0.0, b = 0.0;
		for(int sample = 0; sample < 4; ++sample) { const double sampleAlpha = (samples[sample] >> 24) & 0xFF; a += sampleAlpha * weights[sample]; r += ((samples[sample] >> 16) & 0xFF) * sampleAlpha * weights[sample]; g += ((samples[sample] >> 8) & 0xFF) * sampleAlpha * weights[sample]; b += (samples[sample] & 0xFF) * sampleAlpha * weights[sample]; }
		const int alphaValue = max(0, min(255, static_cast<int>(a + 0.5)));
		if(alphaValue == 0) targetPixels[y * targetSize + x] = 0;
		else
		{
			const int red = max(0, min(alphaValue, static_cast<int>(r + 0.5)));
			const int green = max(0, min(alphaValue, static_cast<int>(g + 0.5)));
			const int blue = max(0, min(alphaValue, static_cast<int>(b + 0.5)));
			targetPixels[y * targetSize + x] = (static_cast<DWORD>(alphaValue) << 24) | (static_cast<DWORD>(red) << 16) | (static_cast<DWORD>(green) << 8) | static_cast<DWORD>(blue);
		}
	}
	::DeleteObject(alpha);
	return scaled;
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

int ToolbarFactory::CommandToolbarImageSize(UINT dpi)
{
	return UiMetrics::ScaleForDpi(24, dpi ? dpi : 96);
}

bool ToolbarFactory::CreateCommandToolbarImages(CImageList& ownedImages, UINT toolbarResourceId, UINT dpi)
{
	HINSTANCE module = _Module.GetResourceInstance();
	HRSRC resource = ::FindResource(module, MAKEINTRESOURCE(toolbarResourceId), RT_TOOLBAR);
	HGLOBAL resourceData = resource != NULL ? ::LoadResource(module, resource) : NULL;
	ToolbarResourceData* toolbarData = resourceData != NULL ? static_cast<ToolbarResourceData*>(::LockResource(resourceData)) : NULL;
	if(toolbarData == NULL || toolbarData->version != 1 || toolbarData->width != 24 || toolbarData->height != 24) return false;
	int standardImageCount = 0;
	for(int index = 0; index < toolbarData->itemCount; ++index) if(toolbarData->Items()[index] != 0) ++standardImageCount;
	const int imageSize = CommandToolbarImageSize(dpi);
	if(!ownedImages.Create(imageSize, imageSize, ILC_COLOR32 | ILC_MASK, standardImageCount + 8, 8)) return false;
	HIMAGELIST sourceImages = ::ImageList_LoadImage(module, MAKEINTRESOURCE(toolbarResourceId), 24, 1, CLR_DEFAULT, IMAGE_BITMAP, LR_CREATEDIBSECTION | LR_DEFAULTSIZE);
	const bool copied = sourceImages != NULL && ::ImageList_GetImageCount(sourceImages) >= standardImageCount && CopyToolbarImages(ownedImages, sourceImages, standardImageCount);
	if(sourceImages != NULL) ::ImageList_Destroy(sourceImages);
	if(!copied) { ownedImages.Destroy(); return false; }
	return true;
}

void ToolbarFactory::ApplyCommandToolbarMetrics(HWND toolbar, UINT toolbarResourceId, UINT dpi)
{
	HINSTANCE module = _Module.GetResourceInstance();
	HRSRC resource = ::FindResource(module, MAKEINTRESOURCE(toolbarResourceId), RT_TOOLBAR);
	HGLOBAL resourceData = resource != NULL ? ::LoadResource(module, resource) : NULL;
	ToolbarResourceData* toolbarData = resourceData != NULL ? static_cast<ToolbarResourceData*>(::LockResource(resourceData)) : NULL;
	if(toolbar == NULL || toolbarData == NULL) return;
	const int imageSize = CommandToolbarImageSize(dpi);
	::SendMessage(toolbar, TB_SETBITMAPSIZE, 0, MAKELONG(imageSize, imageSize));
	::SendMessage(toolbar, TB_SETBUTTONSIZE, 0, MAKELONG(UiMetrics::ScaleForDpi(toolbarData->width + 7, dpi), UiMetrics::ScaleForDpi(toolbarData->height + 7, dpi)));
	AutoSizeToolbar(toolbar);
}

int ToolbarFactory::AddBitmapFromModule(HIMAGELIST imageList, HINSTANCE module, UINT bitmapResourceId, int imageSize)
{
	HBITMAP source = static_cast<HBITMAP>(::LoadImage(module, MAKEINTRESOURCE(bitmapResourceId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
	if(source == NULL || imageList == NULL) return -1;
	HBITMAP scaled = CreateScaledAlphaBitmap(source, 24, imageSize); ::DeleteObject(source);
	if(scaled == NULL) return -1;
	const int imageIndex = ::ImageList_Add(imageList, scaled, NULL); ::DeleteObject(scaled); return imageIndex;
}
