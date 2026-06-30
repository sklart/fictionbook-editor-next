#include "stdafx.h"
#include <windows.h>
#include <thumbcache.h>
#include <atlbase.h>
#include <atlstr.h>
#include <comdef.h>
#include <stdio.h>
#include "Fb2ThumbnailProvider.h"
#include "..\fbe\Fb2CoverImage.h"
#include "..\fbe\Fb2CoverThumbnail.h"
#include <initguid.h>
// {4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}
DEFINE_GUID(CLSID_Fb2ThumbnailProvider,
0x4f99d1f0, 0x5d76, 0x4b9c, 0x9d, 0x3d, 0x9e, 0x6b, 0x8b, 0x4c, 0x7e, 0x31);
namespace {
void AppendThumbnailTraceLine(const wchar_t* line)
{
    if (line == nullptr)
        return;

    wchar_t localAppData[MAX_PATH] = { 0 };
    const DWORD localAppDataLength = ::GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, _countof(localAppData));
    if (localAppDataLength == 0 || localAppDataLength >= _countof(localAppData))
        return;

    ATL::CString directoryPath(localAppData);
    directoryPath += L"\\FBE";
    ::CreateDirectoryW(directoryPath, nullptr);

    ATL::CString logPath(directoryPath);
    logPath += L"\\thumbnail-provider-trace.log";

    HANDLE fileHandle = ::CreateFileW(
        logPath,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE)
        return;

    SYSTEMTIME systemTime = {};
    ::GetLocalTime(&systemTime);
    wchar_t wideBuffer[1200] = { 0 };
    _snwprintf_s(
        wideBuffer,
        _countof(wideBuffer),
        _TRUNCATE,
        L"[%04u-%02u-%02u %02u:%02u:%02u.%03u] %ls\r\n",
        systemTime.wYear,
        systemTime.wMonth,
        systemTime.wDay,
        systemTime.wHour,
        systemTime.wMinute,
        systemTime.wSecond,
        systemTime.wMilliseconds,
        line);

    const int utf8Length = ::WideCharToMultiByte(CP_UTF8, 0, wideBuffer, -1, nullptr, 0, nullptr, nullptr);
    if (utf8Length <= 1) {
        ::CloseHandle(fileHandle);
        return;
    }

    char utf8Buffer[4096] = { 0 };
    const int converted = ::WideCharToMultiByte(CP_UTF8, 0, wideBuffer, -1, utf8Buffer, _countof(utf8Buffer), nullptr, nullptr);
    if (converted <= 1) {
        ::CloseHandle(fileHandle);
        return;
    }

    DWORD bytesWritten = 0;
    ::WriteFile(fileHandle, utf8Buffer, static_cast<DWORD>(converted - 1), &bytesWritten, nullptr);
    ::CloseHandle(fileHandle);
}

void AppendThumbnailTraceFormat(const wchar_t* format, ...)
{
    if (format == nullptr)
        return;

    wchar_t buffer[1024] = { 0 };
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
    va_end(args);
    AppendThumbnailTraceLine(buffer);
}

bool IsMissingCoverError(const ATL::CString& errorMessage)
{
    return
        errorMessage == L"В FB2 не найдена ссылка на обложку." ||
        errorMessage == L"Ссылка на обложку найдена, но идентификатор binary пустой." ||
        errorMessage == L"В FB2 нет раздела binary для обложки." ||
        errorMessage.Find(L"Не найден binary") == 0;
}

class TempFileScope {
public:
    TempFileScope()
    {
        m_path.Empty();
    }
    ~TempFileScope()
    {
        Cleanup();
    }
    void SetPath(const wchar_t* path)
    {
        m_path = path;
    }
    const wchar_t* GetPath() const
    {
        return m_path;
    }
    void Cleanup()
    {
        if (!m_path.IsEmpty()) {
            ::DeleteFileW(m_path);
            m_path.Empty();
        }
    }
private:
    ATL::CString m_path;
};
HRESULT SeekStreamToStart(IStream* stream)
{
    if (stream == nullptr)
        return E_POINTER;
    LARGE_INTEGER offset;
    offset.QuadPart = 0;
    return stream->Seek(offset, STREAM_SEEK_SET, nullptr);
}
ULONGLONG GetStreamSizeOrZero(IStream* stream)
{
    if (stream == nullptr)
        return 0;

    STATSTG stat = {};
    if (FAILED(stream->Stat(&stat, STATFLAG_NONAME)))
        return 0;

    return static_cast<ULONGLONG>(stat.cbSize.QuadPart);
}
HRESULT CopyStreamToTempFile(IStream* stream, TempFileScope& tempFile)
{
    if (stream == nullptr)
        return E_POINTER;
    wchar_t tempDirectory[MAX_PATH] = { 0 };
    const DWORD tempDirectoryLength = ::GetTempPathW(_countof(tempDirectory), tempDirectory);
    if (tempDirectoryLength == 0 || tempDirectoryLength >= _countof(tempDirectory)) {
        return HRESULT_FROM_WIN32(::GetLastError());
    }
    wchar_t tempFilePath[MAX_PATH] = { 0 };
    if (::GetTempFileNameW(tempDirectory, L"FBE", 0, tempFilePath) == 0) {
        return HRESULT_FROM_WIN32(::GetLastError());
    }
    AppendThumbnailTraceFormat(L"Создан временный файл для thumbnail provider: %ls", tempFilePath);
    HANDLE fileHandle = ::CreateFileW(
        tempFilePath,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY,
        nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        const HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
        ::DeleteFileW(tempFilePath);
        return hr;
    }
    tempFile.SetPath(tempFilePath);
    const ULONGLONG inputStreamSize = GetStreamSizeOrZero(stream);
    AppendThumbnailTraceFormat(L"Размер входного FB2-потока: %llu байт", inputStreamSize);
    const HRESULT seekHr = SeekStreamToStart(stream);
    if (FAILED(seekHr)) {
        ::CloseHandle(fileHandle);
        return seekHr;
    }
    BYTE buffer[64 * 1024];
    ULONGLONG totalCopiedBytes = 0;
    while (true) {
        ULONG bytesRead = 0;
        const HRESULT readHr = stream->Read(buffer, sizeof(buffer), &bytesRead);
        if (FAILED(readHr)) {
            ::CloseHandle(fileHandle);
            return readHr;
        }
        if (bytesRead == 0)
            break;
        DWORD bytesWritten = 0;
        if (!::WriteFile(fileHandle, buffer, bytesRead, &bytesWritten, nullptr) || bytesWritten != bytesRead) {
            const HRESULT writeHr = HRESULT_FROM_WIN32(::GetLastError());
            ::CloseHandle(fileHandle);
            return writeHr;
        }
        totalCopiedBytes += bytesWritten;
    }
    ::CloseHandle(fileHandle);
    AppendThumbnailTraceFormat(L"Во временный FB2-файл скопировано: %llu байт", totalCopiedBytes);
    return S_OK;
}
HRESULT BuildThumbnailFromStream(IStream* stream, UINT requestedEdge, HBITMAP* bitmap, WTS_ALPHATYPE* alphaType)
{
    if (bitmap == nullptr || alphaType == nullptr)
        return E_POINTER;
    *bitmap = nullptr;
    *alphaType = WTSAT_UNKNOWN;
    TempFileScope tempFile;
    const HRESULT copyHr = CopyStreamToTempFile(stream, tempFile);
    if (FAILED(copyHr)) {
        AppendThumbnailTraceFormat(L"CopyStreamToTempFile завершился ошибкой: 0x%08X", static_cast<unsigned int>(copyHr));
        return copyHr;
    }

    AppendThumbnailTraceFormat(L"Начато чтение обложки из временного FB2: %ls", tempFile.GetPath());

    FB2CoverImage::CoverImage coverImage;
    ATL::CString coverError;
    if (!FB2CoverImage::TryRead(tempFile.GetPath(), coverImage, &coverError)) {
        AppendThumbnailTraceFormat(L"FB2CoverImage::TryRead завершился ошибкой: %ls", static_cast<const wchar_t*>(coverError));
        return IsMissingCoverError(coverError)
            ? HRESULT_FROM_WIN32(ERROR_NOT_FOUND)
            : HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
    }

    AppendThumbnailTraceFormat(
        L"Обложка прочитана: bytes=%u, content-type=%ls, binary-id=%ls",
        static_cast<unsigned int>(coverImage.bytes.size()),
        static_cast<const wchar_t*>(coverImage.contentType),
        static_cast<const wchar_t*>(coverImage.binaryId));

    FB2CoverThumbnail::DecodedImage decodedImage;
    ATL::CString decodeError;
    if (!FB2CoverThumbnail::TryDecode(coverImage.bytes, decodedImage, &decodeError)) {
        AppendThumbnailTraceFormat(L"FB2CoverThumbnail::TryDecode завершился ошибкой: %ls", static_cast<const wchar_t*>(decodeError));
        return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
    }

    AppendThumbnailTraceFormat(L"Обложка декодирована успешно: %dx%d", decodedImage.width, decodedImage.height);

    FB2CoverThumbnail::DecodedImage thumbnailImage;
    ATL::CString resizeError;
    if (!FB2CoverThumbnail::TryResizeToFit(decodedImage, requestedEdge, thumbnailImage, &resizeError)) {
        AppendThumbnailTraceFormat(L"FB2CoverThumbnail::TryResizeToFit завершился ошибкой: %ls", static_cast<const wchar_t*>(resizeError));
        return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
    }

    AppendThumbnailTraceFormat(L"Thumbnail подготовлен под shell-контракт: %dx%d, requested=%u", thumbnailImage.width, thumbnailImage.height, static_cast<unsigned int>(requestedEdge));

    *alphaType = WTSAT_RGB;
    *bitmap = thumbnailImage.bitmap;
    thumbnailImage.bitmap = nullptr;
    thumbnailImage.width = 0;
    thumbnailImage.height = 0;
    return S_OK;
}
} // namespace
Fb2ThumbnailProvider::Fb2ThumbnailProvider() :
    m_mode(0)
{
}
STDMETHODIMP Fb2ThumbnailProvider::Initialize(IStream* stream, DWORD mode)
{
    if (stream == nullptr)
        return E_POINTER;
    if (m_stream != nullptr)
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    AppendThumbnailTraceFormat(L"Initialize вызван, mode=0x%08X", static_cast<unsigned int>(mode));
    const HRESULT seekHr = SeekStreamToStart(stream);
    if (FAILED(seekHr)) {
        AppendThumbnailTraceFormat(L"SeekStreamToStart в Initialize завершился ошибкой: 0x%08X", static_cast<unsigned int>(seekHr));
        return seekHr;
    }
    m_stream = stream;
    m_mode = mode;
    return S_OK;
}
STDMETHODIMP Fb2ThumbnailProvider::GetThumbnail(UINT cx, HBITMAP* bitmap, WTS_ALPHATYPE* alphaType)
{
    if (bitmap == nullptr || alphaType == nullptr)
        return E_POINTER;
    if (m_stream == nullptr)
        return E_UNEXPECTED;
    AppendThumbnailTraceFormat(L"GetThumbnail вызван, requested=%u.", static_cast<unsigned int>(cx));
    const HRESULT seekHr = SeekStreamToStart(m_stream);
    if (FAILED(seekHr)) {
        AppendThumbnailTraceFormat(L"SeekStreamToStart в GetThumbnail завершился ошибкой: 0x%08X", static_cast<unsigned int>(seekHr));
        return seekHr;
    }
    const HRESULT thumbnailHr = BuildThumbnailFromStream(m_stream, cx, bitmap, alphaType);
    AppendThumbnailTraceFormat(L"GetThumbnail завершился с HRESULT: 0x%08X", static_cast<unsigned int>(thumbnailHr));
    return thumbnailHr;
}



