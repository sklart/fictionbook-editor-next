#include "stdafx.h"
#include "Fb2CoverThumbnail.h"

#include <windows.h>
#include <objidl.h>
#include <atlbase.h>
#include <atlstr.h>
#include <cstring>

#include "atlimage.h"

namespace {

HRESULT CreateStreamFromBytes(const std::vector<unsigned char>& bytes, IStream** stream)
{
    if (stream == nullptr)
        return E_POINTER;

    *stream = nullptr;

    if (bytes.empty())
        return E_INVALIDARG;

    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (memory == nullptr)
        return E_OUTOFMEMORY;

    void* buffer = ::GlobalLock(memory);
    if (buffer == nullptr) {
        const HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
        ::GlobalFree(memory);
        return hr;
    }

    std::memcpy(buffer, bytes.data(), bytes.size());
    ::GlobalUnlock(memory);

    const HRESULT hr = ::CreateStreamOnHGlobal(memory, TRUE, stream);
    if (FAILED(hr))
        ::GlobalFree(memory);

    return hr;
}

int ScaleDimension(int value, int sourceMaxEdge, unsigned int targetMaxEdge)
{
    if (value <= 0 || sourceMaxEdge <= 0 || targetMaxEdge == 0)
        return 0;

    const long long scaled = static_cast<long long>(value) * static_cast<long long>(targetMaxEdge);
    const int result = static_cast<int>(scaled / sourceMaxEdge);
    return result > 0 ? result : 1;
}

void ComputeFitSize(
    int sourceWidth,
    int sourceHeight,
    unsigned int maxEdge,
    int& targetWidth,
    int& targetHeight)
{
    targetWidth = 0;
    targetHeight = 0;

    if (sourceWidth <= 0 || sourceHeight <= 0 || maxEdge == 0)
        return;

    const int sourceMaxEdge = sourceWidth > sourceHeight ? sourceWidth : sourceHeight;
    targetWidth = ScaleDimension(sourceWidth, sourceMaxEdge, maxEdge);
    targetHeight = ScaleDimension(sourceHeight, sourceMaxEdge, maxEdge);
}

} // namespace

namespace FB2CoverThumbnail {

DecodedImage::DecodedImage() :
    bitmap(nullptr),
    width(0),
    height(0)
{
}

DecodedImage::~DecodedImage()
{
    Reset();
}

void DecodedImage::Reset()
{
    if (bitmap != nullptr) {
        ::DeleteObject(bitmap);
        bitmap = nullptr;
    }

    width = 0;
    height = 0;
}

bool DecodedImage::IsEmpty() const
{
    return bitmap == nullptr;
}

bool TryDecode(const std::vector<unsigned char>& bytes, DecodedImage& image, ATL::CString* errorMessage)
{
    image.Reset();

    if (errorMessage != nullptr)
        errorMessage->Empty();

    if (bytes.empty()) {
        if (errorMessage != nullptr)
            *errorMessage = L"Не переданы байты изображения обложки.";
        return false;
    }

    CComPtr<IStream> stream;
    HRESULT hr = CreateStreamFromBytes(bytes, &stream);
    if (FAILED(hr)) {
        if (errorMessage != nullptr)
            errorMessage->Format(L"Не удалось создать поток из байтов обложки: 0x%08X", static_cast<unsigned int>(hr));
        return false;
    }

    ATL::CImage decodedImage;
    hr = decodedImage.Load(stream);
    if (FAILED(hr) || decodedImage.IsNull()) {
        if (errorMessage != nullptr)
            errorMessage->Format(L"Системный image-стек Windows не смог декодировать обложку: 0x%08X", static_cast<unsigned int>(hr));
        return false;
    }

    image.width = decodedImage.GetWidth();
    image.height = decodedImage.GetHeight();
    image.bitmap = decodedImage.Detach();

    if (image.bitmap == nullptr || image.width <= 0 || image.height <= 0) {
        image.Reset();
        if (errorMessage != nullptr)
            *errorMessage = L"Изображение обложки декодировано, но bitmap или размеры некорректны.";
        return false;
    }

    return true;
}

bool TryResizeToFit(const DecodedImage& sourceImage, unsigned int maxEdge, DecodedImage& resizedImage, ATL::CString* errorMessage)
{
    resizedImage.Reset();

    if (errorMessage != nullptr)
        errorMessage->Empty();

    if (sourceImage.bitmap == nullptr || sourceImage.width <= 0 || sourceImage.height <= 0) {
        if (errorMessage != nullptr)
            *errorMessage = L"Не передано корректное декодированное изображение для масштабирования.";
        return false;
    }

    if (maxEdge == 0) {
        if (errorMessage != nullptr)
            *errorMessage = L"Запрошен нулевой размер thumbnail.";
        return false;
    }

    int targetWidth = 0;
    int targetHeight = 0;
    ComputeFitSize(
        sourceImage.width,
        sourceImage.height,
        maxEdge,
        targetWidth,
        targetHeight);

    if (targetWidth <= 0 || targetHeight <= 0) {
        if (errorMessage != nullptr)
            *errorMessage = L"Не удалось вычислить размеры thumbnail.";
        return false;
    }

    ATL::CImage sourceCImage;
    sourceCImage.Attach(sourceImage.bitmap);

    ATL::CImage targetCImage;
    const BOOL created = targetCImage.Create(targetWidth, targetHeight, 32);
    if (!created || targetCImage.IsNull()) {
        sourceCImage.Detach();
        if (errorMessage != nullptr)
            *errorMessage = L"Не удалось создать target bitmap для thumbnail.";
        return false;
    }

    HDC targetDc = targetCImage.GetDC();
    if (targetDc == nullptr) {
        sourceCImage.Detach();
        targetCImage.Destroy();
        if (errorMessage != nullptr)
            *errorMessage = L"Не удалось получить DC для target thumbnail.";
        return false;
    }

    HDC sourceDc = sourceCImage.GetDC();
    if (sourceDc == nullptr) {
        targetCImage.ReleaseDC();
        sourceCImage.Detach();
        targetCImage.Destroy();
        if (errorMessage != nullptr)
            *errorMessage = L"Не удалось получить DC для исходной обложки.";
        return false;
    }

    const BOOL stretchModeOk = ::SetStretchBltMode(targetDc, HALFTONE) != 0;
    if (stretchModeOk)
        ::SetBrushOrgEx(targetDc, 0, 0, nullptr);

    const BOOL stretchOk = ::StretchBlt(
        targetDc,
        0,
        0,
        targetWidth,
        targetHeight,
        sourceDc,
        0,
        0,
        sourceImage.width,
        sourceImage.height,
        SRCCOPY);

    sourceCImage.ReleaseDC();
    targetCImage.ReleaseDC();
    sourceCImage.Detach();

    if (!stretchOk) {
        targetCImage.Destroy();
        if (errorMessage != nullptr)
            errorMessage->Format(L"Не удалось масштабировать bitmap обложки: Win32 0x%08X", static_cast<unsigned int>(::GetLastError()));
        return false;
    }

    resizedImage.width = targetWidth;
    resizedImage.height = targetHeight;
    resizedImage.bitmap = targetCImage.Detach();

    if (resizedImage.bitmap == nullptr) {
        resizedImage.Reset();
        if (errorMessage != nullptr)
            *errorMessage = L"Масштабирование завершилось без итогового bitmap.";
        return false;
    }

    return true;
}

} // namespace FB2CoverThumbnail

