#pragma once

#include <windows.h>
#include <cmath>
#include <cstdlib>

namespace ToolbarBitmapScaling {
inline HBITMAP CreateAlphaBitmapCell(HBITMAP source, int cellSize, int cellIndex)
{
	DIBSECTION sourceInfo = {};
	if(source == NULL || cellSize <= 0 || cellIndex < 0 || ::GetObject(source, sizeof(sourceInfo), &sourceInfo) != sizeof(sourceInfo) ||
		sourceInfo.dsBm.bmWidth < (cellIndex + 1) * cellSize || sourceInfo.dsBmih.biHeight == 0 ||
		(sourceInfo.dsBmih.biHeight < 0 ? -sourceInfo.dsBmih.biHeight : sourceInfo.dsBmih.biHeight) != cellSize ||
		((sourceInfo.dsBm.bmBitsPixel != 24 && sourceInfo.dsBm.bmBitsPixel != 32) || sourceInfo.dsBm.bmBits == NULL || sourceInfo.dsBm.bmWidthBytes < sourceInfo.dsBm.bmWidth * (sourceInfo.dsBm.bmBitsPixel / 8)))
		return NULL;

	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(info.bmiHeader); info.bmiHeader.biWidth = cellSize; info.bmiHeader.biHeight = -cellSize;
	info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
	void* targetBits = NULL;
	HBITMAP target = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &targetBits, NULL, 0);
	if(target == NULL || targetBits == NULL) { if(target != NULL) ::DeleteObject(target); return NULL; }

	const BYTE* sourceBits = static_cast<const BYTE*>(sourceInfo.dsBm.bmBits);
	DWORD* pixels = static_cast<DWORD*>(targetBits);
	const bool sourceBottomUp = sourceInfo.dsBmih.biHeight > 0;
	bool hasSourceAlpha = false;
	if(sourceInfo.dsBm.bmBitsPixel == 32)
		for(int y = 0; y < cellSize && !hasSourceAlpha; ++y)
		{
			const BYTE* row = sourceBits + (sourceBottomUp ? cellSize - 1 - y : y) * sourceInfo.dsBm.bmWidthBytes + cellIndex * cellSize * 4;
			for(int x = 0; x < cellSize; ++x)
				if(row[x * 4 + 3] != 0) { hasSourceAlpha = true; break; }
		}
	for(int y = 0; y < cellSize; ++y)
	{
		const int sourceY = sourceBottomUp ? cellSize - 1 - y : y;
		const int bytesPerPixel = sourceInfo.dsBm.bmBitsPixel / 8;
		const BYTE* row = sourceBits + sourceY * sourceInfo.dsBm.bmWidthBytes + cellIndex * cellSize * bytesPerPixel;
		for(int x = 0; x < cellSize; ++x)
		{
			const BYTE blue = row[x * bytesPerPixel], green = row[x * bytesPerPixel + 1], red = row[x * bytesPerPixel + 2];
			const BYTE alpha = hasSourceAlpha ? row[x * bytesPerPixel + 3] : 0xFF;
			if(alpha == 0 || (blue == 0xFF && green == 0x00 && red == 0xFF)) pixels[y * cellSize + x] = 0;
			else
			{
				const BYTE premultipliedRed = static_cast<BYTE>((red * alpha + 127) / 255);
				const BYTE premultipliedGreen = static_cast<BYTE>((green * alpha + 127) / 255);
				const BYTE premultipliedBlue = static_cast<BYTE>((blue * alpha + 127) / 255);
				pixels[y * cellSize + x] = (static_cast<DWORD>(alpha) << 24) | (static_cast<DWORD>(premultipliedRed) << 16) | (static_cast<DWORD>(premultipliedGreen) << 8) | premultipliedBlue;
			}
		}
	}
	return target;
}

inline HBITMAP ScaleHighResolutionAlphaBitmap(HBITMAP alpha, int sourceSize, int targetSize)
{
	if(alpha == NULL || sourceSize <= 0 || targetSize <= 0) return NULL;
	if(targetSize == sourceSize) return alpha;
	DIBSECTION sourceInfo = {};
	if(::GetObject(alpha, sizeof(sourceInfo), &sourceInfo) != sizeof(sourceInfo) || sourceInfo.dsBm.bmBits == NULL ||
		sourceInfo.dsBm.bmWidth != sourceSize || abs(sourceInfo.dsBmih.biHeight) != sourceSize)
		{ ::DeleteObject(alpha); return NULL; }
	BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(info.bmiHeader); info.bmiHeader.biWidth = targetSize;
	info.bmiHeader.biHeight = -targetSize; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
	void* targetBits = NULL;
	HBITMAP scaled = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &targetBits, NULL, 0);
	if(scaled == NULL || targetBits == NULL) { if(scaled) ::DeleteObject(scaled); ::DeleteObject(alpha); return NULL; }
	const DWORD* sourcePixels = static_cast<const DWORD*>(sourceInfo.dsBm.bmBits);
	DWORD* targetPixels = static_cast<DWORD*>(targetBits);
	// The source and destination both contain premultiplied ARGB. Average each
	// channel directly to keep antialiased edges clear against both themes.
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
		double channels[4] = {};
		for(int sample = 0; sample < 4; ++sample)
			for(int channel = 0; channel < 4; ++channel)
				channels[channel] += ((samples[sample] >> (channel * 8)) & 0xFF) * weights[sample];
		DWORD pixel = 0;
		for(int channel = 0; channel < 4; ++channel)
			pixel |= static_cast<DWORD>(min(255, max(0, static_cast<int>(channels[channel] + 0.5)))) << (channel * 8);
		targetPixels[y * targetSize + x] = pixel;
	}
	::DeleteObject(alpha);
	return scaled;
}
}
