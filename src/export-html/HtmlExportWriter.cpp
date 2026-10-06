#include "stdafx.h"
#include "HtmlExportWriter.h"

#include "utils.h"
#include "..\\common\\fb2\\Fb2BinaryInspector.h"

#include <vector>

namespace {

class ScopedHandle {
public:
	explicit ScopedHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
	~ScopedHandle() { if (m_handle != INVALID_HANDLE_VALUE) ::CloseHandle(m_handle); }
	HANDLE Get() const { return m_handle; }
	void Reset(HANDLE handle = INVALID_HANDLE_VALUE) {
		if (m_handle != INVALID_HANDLE_VALUE) ::CloseHandle(m_handle);
		m_handle = handle;
	}
private:
	HANDLE m_handle;
};

}

namespace HtmlExportWriter {

HRESULT NormalizeBinaryMimeTypes(IXMLDOMDocument2* source)
{
    if (source == NULL) return E_POINTER;
    IXMLDOMNodeListPtr binaries;
    HRESULT hr = source->selectNodes(bstr_t(L"/fb:FictionBook/fb:binary"), &binaries);
    if (FAILED(hr) || !binaries) return hr;
    long count = 0;
    if (FAILED(hr = binaries->get_length(&count))) return hr;
    for (long index = 0; index < count; ++index) {
        IXMLDOMNodePtr node;
        if (FAILED(binaries->get_item(index, &node)) || !node) continue;
        IXMLDOMElementPtr element;
        if (FAILED(node->QueryInterface(IID_PPV_ARGS(&element))) || !element) continue;
        _variant_t declared;
        if (FAILED(element->getAttribute(bstr_t(L"content-type"), &declared))) continue;
        CComBSTR data;
        if (FAILED(node->get_text(&data))) continue;
        const std::wstring mime = V_VT(&declared) == VT_BSTR ? std::wstring(V_BSTR(&declared), ::SysStringLen(V_BSTR(&declared))) : std::wstring();
        const FbeFb2Binary::Inspection inspection = FbeFb2Binary::InspectBinary(
            mime, std::wstring(static_cast<const wchar_t*>(data), data.Length()));
        if (!inspection.detectedMime.empty())
            element->setAttribute(bstr_t(L"content-type"), _variant_t(inspection.detectedMime.c_str()));
        else if (!inspection.normalizedDeclaredMime.empty())
            element->setAttribute(bstr_t(L"content-type"), _variant_t(inspection.normalizedDeclaredMime.c_str()));
    }
    return S_OK;
}
Writer::Writer(const Options& options, const Callbacks& callbacks) :
	m_options(options), m_callbacks(callbacks), m_target(INVALID_HANDLE_VALUE),
	m_createdImagesDirectory(false), m_targetOpened(false), m_prepared(false), m_committed(false)
{
}

Writer::~Writer()
{
	if (!m_committed) Abort();
}

void Writer::Report(Failure failure, const std::wstring& path, DWORD error) const
{
	if (m_callbacks.reportFailure) m_callbacks.reportFailure(failure, path, error);
}

HRESULT Writer::OpenTarget()
{
	if (m_target != INVALID_HANDLE_VALUE) return S_OK;
	m_target = ::CreateFile(m_options.targetPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (m_target != INVALID_HANDLE_VALUE) { m_targetOpened = true; return S_OK; }
	const DWORD error = ::GetLastError();
	Report(Failure::OpenTarget, m_options.targetPath, error);
	return HRESULT_FROM_WIN32(error);
}

HRESULT Writer::WriteTargetBytes(const void* bytes, DWORD length)
{
	DWORD written = 0;
	const BOOL writeSucceeded = m_callbacks.writeTarget ? m_callbacks.writeTarget(m_target, bytes, length, &written) : ::WriteFile(m_target, bytes, length, &written, NULL);
	if (writeSucceeded && written == length) return S_OK;
	const DWORD error = ::GetLastError();
	Report(!writeSucceeded ? Failure::WriteTarget : Failure::ShortWrite, m_options.targetPath, error);
	return HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_WRITE_FAULT : error);
}

HRESULT Writer::WriteMimePreamble()
{
	HtmlExportWriterHelpers::MimePreamble preamble;
	if (!HtmlExportWriterHelpers::BuildMimePreamble(time(NULL), static_cast<unsigned int>(rand()), preamble)) return E_FAIL;
	m_mimeBoundary = preamble.boundary;
	return WriteTargetBytes(preamble.header.data(), static_cast<DWORD>(preamble.header.size()));
}

HRESULT Writer::Prepare()
{
	if (m_prepared) return S_OK;
	if (!HtmlExportWriterHelpers::BuildImagePaths(
		m_options.targetPath, m_options.externalImages ? m_options.externalImagesFolderMode : 0,
		m_options.externalImages ? m_options.externalImagesFolderName : std::wstring(), m_imagePaths)) return E_INVALIDARG;

	if (m_options.standalone) {
		m_prepared = true;
		return S_OK;
	}
    HRESULT hr = S_OK;
    if (!m_options.split) {
        hr = OpenTarget();
        if (FAILED(hr)) return hr;
    }
	if (m_options.externalImages && !m_options.mime) {
		if (!::CreateDirectory(m_imagePaths.directory.c_str(), NULL)) {
			const DWORD error = ::GetLastError();
			if (error != ERROR_ALREADY_EXISTS) {
				Report(Failure::CreateImagesDirectory, m_imagePaths.directory, error);
				return HRESULT_FROM_WIN32(error);
			}
		} else {
			m_createdImagesDirectory = true;
		}
	}
	if (m_options.mime) {
		hr = WriteMimePreamble();
		if (FAILED(hr)) return hr;
	}
	m_prepared = true;
	return S_OK;
}

const HtmlExportWriterHelpers::ImagePaths& Writer::ImagePaths() const
{
	return m_imagePaths;
}

HRESULT Writer::GetTransformOutput(IStream** output)
{
	if (output == NULL) return E_POINTER;
	*output = NULL;
	if (!m_prepared) return E_UNEXPECTED;
	if (m_options.standalone) {
		if (m_standaloneOutput == NULL) {
			HRESULT hr = ::CreateStreamOnHGlobal(NULL, TRUE, &m_standaloneOutput);
			if (FAILED(hr)) return hr;
		}
		*output = m_standaloneOutput;
	} else {
		if (m_transformOutput == NULL) {
			try { m_transformOutput = U::NewStream(m_target, false); }
			catch (const _com_error& error) { return error.Error(); }
		}
		*output = m_transformOutput;
	}
	(*output)->AddRef();
	return S_OK;
}

IStream* Writer::StandaloneOutput() const
{
	return m_standaloneOutput;
}

HRESULT Writer::WriteStandaloneToTarget()
{
	if (!m_options.standalone || m_standaloneOutput == NULL) return E_UNEXPECTED;
    HRESULT hr = S_OK;
    if (!m_options.split) {
        hr = OpenTarget();
        if (FAILED(hr)) return hr;
    }
	LARGE_INTEGER start = {};
	hr = m_standaloneOutput->Seek(start, STREAM_SEEK_SET, NULL);
	if (FAILED(hr)) return hr;
	CComPtr<IStream> targetStream;
	try { targetStream = U::NewStream(m_target, false); }
	catch (const _com_error& error) { return error.Error(); }
	ULARGE_INTEGER written = {};
	hr = m_standaloneOutput->CopyTo(targetStream, ULARGE_INTEGER{ 0xFFFFFFFF, 0x7FFFFFFF }, NULL, &written);
	if (FAILED(hr)) {
		Report(Failure::WriteTarget, m_options.targetPath, HRESULT_FACILITY(hr) == FACILITY_WIN32 ? HRESULT_CODE(hr) : ERROR_WRITE_FAULT);
		return hr;
	}
	return S_OK;
}

HRESULT Writer::WriteSplitDocument(const std::wstring& fileName, const std::wstring& html)
{
    if (!m_prepared || fileName.empty() || fileName.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos || fileName == L"." || fileName == L"..") return E_INVALIDARG;
    const std::wstring::size_type slash = m_options.targetPath.find_last_of(L"\\/");
    const std::wstring path = (slash == std::wstring::npos ? std::wstring() : m_options.targetPath.substr(0, slash + 1)) + fileName;
    const int bytesNeeded = html.empty() ? 0 : ::WideCharToMultiByte(CP_UTF8, 0, html.data(), static_cast<int>(html.size()), NULL, 0, NULL, NULL);
    if (!html.empty() && bytesNeeded == 0) return HRESULT_FROM_WIN32(::GetLastError());
    std::vector<char> bytes(static_cast<size_t>(bytesNeeded));
    if (bytesNeeded > 0 && ::WideCharToMultiByte(CP_UTF8, 0, html.data(), static_cast<int>(html.size()), bytes.data(), bytesNeeded, NULL, NULL) == 0) return HRESULT_FROM_WIN32(::GetLastError());
    const bool existed = ::GetFileAttributes(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    if (existed && (!m_callbacks.confirmImageOverwrite || !m_callbacks.confirmImageOverwrite(path)))
        return S_FALSE;
    ScopedHandle output(::CreateFile(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
    if (output.Get() == INVALID_HANDLE_VALUE) { const DWORD error = ::GetLastError(); Report(Failure::OpenTarget, path, error); return HRESULT_FROM_WIN32(error); }
    DWORD written = 0;
    const BOOL success = bytes.empty() || ::WriteFile(output.Get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL);
    if (!success || written != bytes.size()) { const DWORD error = ::GetLastError(); Report(success ? Failure::ShortWrite : Failure::WriteTarget, path, error); return HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_WRITE_FAULT : error); }
    if (!existed) m_createdDocuments.push_back(path);
    return S_OK;
}
HRESULT Writer::WriteImages(IXMLDOMDocument2* source)
{
    if (source == NULL || (!m_options.externalImages && !m_options.mime)) return S_OK;
    IXMLDOMNodeListPtr binaries;
    HRESULT hr = source->selectNodes(bstr_t(L"/fb:FictionBook/fb:binary"), &binaries);
    if (FAILED(hr)) return hr;
    long count = 0;
    if (FAILED(hr = binaries->get_length(&count))) return hr;
    for (long index = 0; index < count; ++index) {
        try {
            IXMLDOMNodePtr node;
            CheckError(binaries->get_item(index, &node));
            IXMLDOMElementPtr element;
            CheckError(node->QueryInterface(IID_PPV_ARGS(&element)));
            _variant_t id, contentType;
            CheckError(element->getAttribute(bstr_t(L"id"), &id));
            CheckError(element->getAttribute(bstr_t(L"content-type"), &contentType));
            if (V_VT(&id) != VT_BSTR) continue;

            CComBSTR data;
            CheckError(node->get_text(&data));
            const std::wstring declared = V_VT(&contentType) == VT_BSTR
                ? std::wstring(V_BSTR(&contentType), ::SysStringLen(V_BSTR(&contentType))) : std::wstring();
            const std::wstring binaryText(static_cast<const wchar_t*>(data), data.Length());

            if (m_options.mime) {
                // NormalizeBinaryMimeTypes prepared this private DOM before the XSL transform.
                // Keep its existing base64 spelling in MHTML so no second bulk decode is needed.
                const std::wstring mime = declared.empty() ? L"application/octet-stream" : declared;
                std::vector<char> buffer(data.Length() + 1024);
                const int headerLength = _snprintf_s(buffer.data(), 1024, _TRUNCATE,
                    "\r\n%s\r\nContent-Type: %S\r\nContent-Transfer-Encoding: base64\r\nContent-Location: %S\r\n\r\n",
                    m_mimeBoundary.c_str(), mime.c_str(), V_BSTR(&id));
                if (headerLength < 0) return E_FAIL;
                const DWORD dataLength = ::WideCharToMultiByte(CP_ACP, 0, data, data.Length(),
                    buffer.data() + headerLength, data.Length(), NULL, NULL);
                if (data.Length() != 0 && dataLength == 0) return HRESULT_FROM_WIN32(::GetLastError());
                hr = WriteTargetBytes(buffer.data(), static_cast<DWORD>(headerLength) + dataLength);
                if (FAILED(hr)) return hr;
                continue;
            }

            const FbeFb2Binary::Inspection inspection = FbeFb2Binary::InspectBinary(declared, binaryText);
            if (inspection.status == FbeFb2Binary::BinaryStatus::InvalidBase64 ||
                inspection.status == FbeFb2Binary::BinaryStatus::Empty) continue;
            std::wstring imagePath;
            if (!HtmlExportWriterHelpers::BuildExternalImagePath(m_imagePaths,
                std::wstring(V_BSTR(&id), ::SysStringLen(V_BSTR(&id))), imagePath)) continue;
            HANDLE image = ::CreateFile(imagePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
            bool created = image != INVALID_HANDLE_VALUE;
            if (image == INVALID_HANDLE_VALUE && ::GetLastError() == ERROR_FILE_EXISTS) {
                if (!m_callbacks.confirmImageOverwrite || !m_callbacks.confirmImageOverwrite(imagePath)) continue;
                image = ::CreateFile(imagePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
            }
            if (image == INVALID_HANDLE_VALUE) {
                Report(Failure::OpenImage, imagePath, ::GetLastError());
                continue;
            }
            ScopedHandle imageHandle(image);
            DWORD written = 0;
            const DWORD length = static_cast<DWORD>(inspection.bytes.size());
            const BOOL wrote = ::WriteFile(image, inspection.bytes.data(), length, &written, NULL);
            const DWORD error = ::GetLastError();
            if (!wrote || written != length) {
                Report(wrote ? Failure::ShortImageWrite : Failure::WriteImage, imagePath, error);
                imageHandle.Reset();
                ::DeleteFile(imagePath.c_str());
                continue;
            }
            if (created) m_createdImages.push_back(imagePath);
        }
        catch (const _com_error&) {
            // A malformed individual binary remains non-fatal, as in the legacy writer.
            continue;
        }
    }
    return S_OK;
}
HRESULT Writer::WriteMimeFinalBoundary()
{
	const std::string finalBoundary = "\r\n" + m_mimeBoundary + "\r\n";
	return WriteTargetBytes(finalBoundary.data(), static_cast<DWORD>(finalBoundary.size()));
}

HRESULT Writer::CloseTarget()
{
	m_transformOutput.Release();
	if (m_target == INVALID_HANDLE_VALUE) return S_OK;
	const HANDLE target = m_target;
	m_target = INVALID_HANDLE_VALUE;
	return ::CloseHandle(target) ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
}

HRESULT Writer::Finalize()
{
	if (!m_prepared) return E_UNEXPECTED;
	if (m_options.mime) {
		const HRESULT hr = WriteMimeFinalBoundary();
		if (FAILED(hr)) return hr;
	}
	return CloseTarget();
}

void Writer::Abort()
{
	m_transformOutput.Release();
	m_standaloneOutput.Release();
	if (m_target != INVALID_HANDLE_VALUE) {
		::CloseHandle(m_target);
		m_target = INVALID_HANDLE_VALUE;
	}
	for (size_t index = 0; index < m_createdImages.size(); ++index)
		::DeleteFile(m_createdImages[index].c_str());
	m_createdImages.clear();
	for (size_t index = 0; index < m_createdDocuments.size(); ++index)
		::DeleteFile(m_createdDocuments[index].c_str());
	m_createdDocuments.clear();
	if (m_createdImagesDirectory) ::RemoveDirectory(m_imagePaths.directory.c_str());
	m_createdImagesDirectory = false;
	if (m_targetOpened && !m_options.targetPath.empty()) ::DeleteFile(m_options.targetPath.c_str());
}

void Writer::Commit()
{
	m_committed = true;
	m_standaloneOutput.Release();
}

}