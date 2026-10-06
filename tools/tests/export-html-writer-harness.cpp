#include "../../src/export-html/stdafx.h"
#include "../../src/export-html/HtmlExportWriter.h"
#include "../../src/export-html/utils.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

CComModule _Module;
CRegKey _Settings;
CString _SettingsPath;

namespace U {
HandleStreamPtr NewStream(HANDLE& handle, bool closeHandle)
{
	HandleStream* stream = NULL;
	CheckError(HandleStream::CreateInstance(&stream));
	stream->SetHandle(handle, closeHandle);
	if (closeHandle) handle = INVALID_HANDLE_VALUE;
	return stream;
}

void NormalizeInplace(CString&) {}
CString QuerySV(HKEY, const TCHAR*, const TCHAR*) { return CString(); }
DWORD QueryIV(HKEY, const TCHAR*, DWORD value) { return value; }
}

namespace {

std::wstring TemporaryPath(const wchar_t* name)
{
	wchar_t directory[MAX_PATH] = {};
	::GetTempPath(_countof(directory), directory);
	std::wstring path(directory);
	path += L"fbe-export-html-writer-";
	path += std::to_wstring(::GetCurrentProcessId());
	path += L"-";
	path += std::to_wstring(::GetTickCount64());
	path += L"-";
	path += name;
	return path;
}

bool ReadFileText(const std::wstring& path, std::string& text)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) return false;
	text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	return true;
}

int WriteText(IStream* stream, const char* text)
{
	ULONG written = 0;
	const ULONG length = static_cast<ULONG>(strlen(text));
	return SUCCEEDED(stream->Write(text, length, &written)) && written == length ? 0 : 1;
}

HtmlExportWriter::Callbacks Callbacks(int& failures)
{
	HtmlExportWriter::Callbacks callbacks;
	callbacks.reportFailure = [&failures](HtmlExportWriter::Failure, const std::wstring&, DWORD) { ++failures; };
	callbacks.confirmImageOverwrite = [](const std::wstring&) { return false; };
	return callbacks;
}

int ExpectNormalWrite()
{
	const std::wstring path = TemporaryPath(L"normal.html");
	int reported = 0;
	{
		HtmlExportWriter::Options options;
		options.targetPath = path;
		HtmlExportWriter::Writer writer(options, Callbacks(reported));
		if (FAILED(writer.Prepare())) return 1;
		CComPtr<IStream> output;
		if (FAILED(writer.GetTransformOutput(&output)) || WriteText(output, "normal")) return 1;
		if (FAILED(writer.Finalize())) return 1;
		writer.Commit();
	}
	std::string text;
	const int failure = reported != 0 || !ReadFileText(path, text) || text != "normal";
	::DeleteFile(path.c_str());
	return failure;
}

int ExpectOpenFailure()
{
	HtmlExportWriter::Options options;
	options.targetPath = TemporaryPath(L"missing\\target.html");
	int reported = 0;
	HtmlExportWriter::Writer writer(options, Callbacks(reported));
	return FAILED(writer.Prepare()) && reported == 1 ? 0 : 1;
}

IXMLDOMDocument2Ptr CreateBinaryDocument(const std::wstring& id, const std::wstring& contentType = L"application/octet-stream", const std::wstring& data = L"AA==")
{
	IXMLDOMDocument2Ptr document;
	CheckError(document.CreateInstance(CLSID_DOMDocument60));
	const std::wstring xml = L"<FictionBook xmlns='http://www.gribuser.ru/xml/fictionbook/2.0'><binary id='" + id + L"' content-type='" + contentType + L"'>" + data + L"</binary></FictionBook>";
	VARIANT_BOOL loaded = VARIANT_FALSE;
	CheckError(document->loadXML(CComBSTR(xml.c_str()), &loaded));
	if (loaded != VARIANT_TRUE) throw _com_error(E_FAIL);
	CheckError(document->setProperty(CComBSTR(L"SelectionLanguage"), _variant_t(L"XPath")));
	CheckError(document->setProperty(CComBSTR(L"SelectionNamespaces"), _variant_t(L"xmlns:fb='http://www.gribuser.ru/xml/fictionbook/2.0'")));
	return document;
}

bool HasBinaryContentType(IXMLDOMDocument2* document, const wchar_t* expected)
{
	IXMLDOMNodePtr node;
	if (FAILED(document->selectSingleNode(bstr_t(L"/fb:FictionBook/fb:binary"), &node)) || !node) return false;
	IXMLDOMElementPtr element(node);
	_variant_t actual;
	return SUCCEEDED(element->getAttribute(bstr_t(L"content-type"), &actual)) &&
		V_VT(&actual) == VT_BSTR && wcscmp(V_BSTR(&actual), expected) == 0;
}

int ExpectWriteFailureAndRollback(bool shortWrite)
{
	const std::wstring path = TemporaryPath(shortWrite ? L"short-write.mht" : L"write-failure.mht");
	int reported = 0;
	HRESULT result = S_OK;
	{
		HtmlExportWriter::Options options;
		options.targetPath = path;
		options.mime = true;
		HtmlExportWriter::Callbacks callbacks = Callbacks(reported);
		callbacks.writeTarget = [shortWrite](HANDLE, const void*, DWORD length, DWORD* written) {
			*written = shortWrite && length > 0 ? length - 1 : 0;
			::SetLastError(shortWrite ? ERROR_SUCCESS : ERROR_DISK_FULL);
			return shortWrite ? TRUE : FALSE;
		};
		HtmlExportWriter::Writer writer(options, callbacks);
		result = writer.Prepare();
	}
	return FAILED(result) && reported == 1 && ::GetFileAttributes(path.c_str()) == INVALID_FILE_ATTRIBUTES ? 0 : 1;
}

int ExpectNoTargetHandleLeak()
{
	const std::wstring path = TemporaryPath(L"handle.html");
	int reported = 0;
	{
		HtmlExportWriter::Options options;
		options.targetPath = path;
		HtmlExportWriter::Writer writer(options, Callbacks(reported));
		if (FAILED(writer.Prepare())) return 1;
		CComPtr<IStream> output;
		if (FAILED(writer.GetTransformOutput(&output)) || WriteText(output, "handle") || FAILED(writer.Finalize())) return 1;
		writer.Commit();
	}
	HANDLE reopened = ::CreateFile(path.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	const int failure = reported != 0 || reopened == INVALID_HANDLE_VALUE;
	if (reopened != INVALID_HANDLE_VALUE) ::CloseHandle(reopened);
	::DeleteFile(path.c_str());
	return failure;
}

int ExpectExistingImage(bool overwrite)
{
	const std::wstring path = TemporaryPath(overwrite ? L"overwrite.html" : L"keep-existing.html");
	int reported = 0;
	const std::wstring imageName = L"pixel.bin";
	{
		HtmlExportWriter::Options options;
		options.targetPath = path;
		options.externalImages = true;
		HtmlExportWriter::Callbacks callbacks = Callbacks(reported);
		callbacks.confirmImageOverwrite = [overwrite](const std::wstring&) { return overwrite; };
		HtmlExportWriter::Writer writer(options, callbacks);
		if (FAILED(writer.Prepare())) return 1;
		std::wstring imagePath;
		if (!HtmlExportWriterHelpers::BuildExternalImagePath(writer.ImagePaths(), imageName, imagePath)) return 1;
		HANDLE existing = ::CreateFile(imagePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
		const BYTE original = 0x7F;
		DWORD written = 0;
		if (existing == INVALID_HANDLE_VALUE || !::WriteFile(existing, &original, 1, &written, NULL) || written != 1) return 1;
		::CloseHandle(existing);
		if (FAILED(writer.WriteImages(CreateBinaryDocument(imageName))) || FAILED(writer.Finalize())) return 1;
		writer.Commit();
		HANDLE image = ::CreateFile(imagePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		BYTE actual = 0xFF; DWORD read = 0;
		if (image == INVALID_HANDLE_VALUE || !::ReadFile(image, &actual, 1, &read, NULL) || read != 1) return 1;
		::CloseHandle(image);
		const int expected = overwrite ? 0x00 : 0x7F;
		if (actual != expected || reported != 0) return 1;
		::DeleteFile(imagePath.c_str());
		::RemoveDirectory(writer.ImagePaths().directory.c_str());
	}
	::DeleteFile(path.c_str());
	return 0;
}

int ExpectUnsafeImageIdsRejected()
{
	const std::wstring path = TemporaryPath(L"safe-images.html");
	int reported = 0;
	{
		HtmlExportWriter::Options options;
		options.targetPath = path;
		options.externalImages = true;
		HtmlExportWriter::Writer writer(options, Callbacks(reported));
		if (FAILED(writer.Prepare())) return 1;
		const std::vector<std::wstring> unsafe = { L"../evil.png", L"C:\\evil.png", L"nested/evil.png", L"nested\\evil.png" };
		for (size_t index = 0; index < unsafe.size(); ++index)
			if (FAILED(writer.WriteImages(CreateBinaryDocument(unsafe[index])))) return 1;
		const std::wstring unicodeName = L"Иллюстрация.png";
		if (FAILED(writer.WriteImages(CreateBinaryDocument(unicodeName))) || FAILED(writer.Finalize())) return 1;
		writer.Commit();
		std::wstring unicodePath;
		if (!HtmlExportWriterHelpers::BuildExternalImagePath(writer.ImagePaths(), unicodeName, unicodePath)) return 1;
		const int failure = reported != 0 || ::GetFileAttributes(unicodePath.c_str()) == INVALID_FILE_ATTRIBUTES;
		::DeleteFile(unicodePath.c_str());
		::RemoveDirectory(writer.ImagePaths().directory.c_str());
		::DeleteFile(path.c_str());
		return failure;
	}
}
int ExpectStandaloneWrite()
{
	const std::wstring path = TemporaryPath(L"standalone.html");
	int reported = 0;
	{
		HtmlExportWriter::Options options;
		options.targetPath = path;
		options.standalone = true;
		HtmlExportWriter::Writer writer(options, Callbacks(reported));
		if (FAILED(writer.Prepare())) return 1;
		CComPtr<IStream> output;
		if (FAILED(writer.GetTransformOutput(&output)) || WriteText(output, "standalone")) return 1;
		if (FAILED(writer.WriteStandaloneToTarget()) || FAILED(writer.Finalize())) return 1;
		writer.Commit();
	}
	std::string text;
	const int failure = reported != 0 || !ReadFileText(path, text) || text != "standalone";
	::DeleteFile(path.c_str());
	return failure;
}

int ExpectMimeFinalBoundary()
{
	const std::wstring path = TemporaryPath(L"document.mht");
	int reported = 0;
	{
		HtmlExportWriter::Options options;
		options.targetPath = path;
		options.mime = true;
		HtmlExportWriter::Writer writer(options, Callbacks(reported));
		if (FAILED(writer.Prepare())) return 1;
		CComPtr<IStream> output;
		if (FAILED(writer.GetTransformOutput(&output)) || WriteText(output, "mime-body")) return 1;
		if (FAILED(writer.Finalize())) return 1;
		writer.Commit();
	}
	std::string text;
	const int failure = reported != 0 || !ReadFileText(path, text) ||
		text.find("Content-Type: multipart/related") == std::string::npos ||
		text.find("mime-body") == std::string::npos ||
		text.size() < 4 || text.compare(text.size() - 2, 2, "\r\n") != 0;
	::DeleteFile(path.c_str());
	return failure;
}

int ExpectExternalImageWrite()
{
	const std::wstring path = TemporaryPath(L"images.html");
	const std::wstring images = path.substr(0, path.size() - 5) + L"_files";
	const std::wstring imagePath = images + L"\\pixel.bin";
	int reported = 0;
	IXMLDOMDocument2Ptr document;
	if (FAILED(document.CreateInstance(CLSID_DOMDocument60))) return 1;
	VARIANT_BOOL loaded = VARIANT_FALSE;
	if (FAILED(document->loadXML(CComBSTR(L"<FictionBook xmlns='http://www.gribuser.ru/xml/fictionbook/2.0'><binary id='pixel.bin' content-type='application/octet-stream'>AA==</binary></FictionBook>"), &loaded)) || loaded != VARIANT_TRUE) return 1;
	if (FAILED(document->setProperty(CComBSTR(L"SelectionLanguage"), _variant_t(L"XPath"))) ||
		FAILED(document->setProperty(CComBSTR(L"SelectionNamespaces"), _variant_t(L"xmlns:fb='http://www.gribuser.ru/xml/fictionbook/2.0'")))) return 1;
	{
		HtmlExportWriter::Options options;
		options.targetPath = path;
		options.externalImages = true;
		HtmlExportWriter::Writer writer(options, Callbacks(reported));
		if (FAILED(writer.Prepare()) || FAILED(writer.WriteImages(document)) || FAILED(writer.Finalize())) return 1;
		writer.Commit();
	}
	HANDLE image = ::CreateFile(imagePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	DWORD size = image == INVALID_HANDLE_VALUE ? 0 : ::GetFileSize(image, NULL);
	if (image != INVALID_HANDLE_VALUE) ::CloseHandle(image);
	const int failure = reported != 0 || size != 1;
	::DeleteFile(imagePath.c_str());
	::RemoveDirectory(images.c_str());
	::DeleteFile(path.c_str());
	return failure;
}

int ExpectDetectedBinaryMimeTypes()
{
	const std::wstring png = L"iVBORw0KGgo=";
	IXMLDOMDocument2Ptr normalized = CreateBinaryDocument(L"actual.png", L"image/jpeg", png);
	if (FAILED(HtmlExportWriter::NormalizeBinaryMimeTypes(normalized)) || !HasBinaryContentType(normalized, L"image/png")) return 1;

	const std::wstring mhtPath = TemporaryPath(L"detected-image.mht");
	int reported = 0;
	{
		HtmlExportWriter::Options options;
		options.targetPath = mhtPath;
		options.mime = true;
		HtmlExportWriter::Writer writer(options, Callbacks(reported));
		if (FAILED(writer.Prepare())) return 1;
		CComPtr<IStream> output;
		if (FAILED(writer.GetTransformOutput(&output)) || WriteText(output, "mime-body") ||
			FAILED(writer.WriteImages(normalized)) || FAILED(writer.Finalize())) return 1;
		writer.Commit();
	}
	std::string mht;
	if (reported != 0 || !ReadFileText(mhtPath, mht) || mht.find("Content-Type: image/png") == std::string::npos) {
		::DeleteFile(mhtPath.c_str());
		return 1;
	}
	::DeleteFile(mhtPath.c_str());

	const std::wstring htmlPath = TemporaryPath(L"detected-image.html");
	const std::wstring imagePath = htmlPath.substr(0, htmlPath.size() - 5) + L"_files\\actual.png";
	IXMLDOMDocument2Ptr external = CreateBinaryDocument(L"actual.png", L"application/octet-stream", png);
	{
		HtmlExportWriter::Options options;
		options.targetPath = htmlPath;
		options.externalImages = true;
		HtmlExportWriter::Writer writer(options, Callbacks(reported));
		if (FAILED(writer.Prepare()) || FAILED(writer.WriteImages(external)) || FAILED(writer.Finalize())) return 1;
		writer.Commit();
	}
	HANDLE image = ::CreateFile(imagePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	BYTE bytes[8] = {}; DWORD read = 0;
	const bool imageOk = image != INVALID_HANDLE_VALUE && ::ReadFile(image, bytes, sizeof(bytes), &read, NULL) && read == sizeof(bytes) &&
		bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G';
	if (image != INVALID_HANDLE_VALUE) ::CloseHandle(image);
	::DeleteFile(imagePath.c_str());
	::RemoveDirectory((htmlPath.substr(0, htmlPath.size() - 5) + L"_files").c_str());
	::DeleteFile(htmlPath.c_str());
	return reported == 0 && imageOk ? 0 : 1;
}
int ExpectExistingSplitDocument()
{
    const std::wstring target = TemporaryPath(L"split-target.html");
    const std::wstring root = target.substr(0, target.find_last_of(L"\\/") + 1);
    const std::wstring index = root + L"index.html";
    { std::ofstream initial(index, std::ios::binary); initial << "old"; }
    int reported = 0;
    {
        HtmlExportWriter::Options options; options.targetPath = target; options.split = true;
        HtmlExportWriter::Writer writer(options, Callbacks(reported));
        if (FAILED(writer.Prepare()) || writer.WriteSplitDocument(L"index.html", L"new") != S_FALSE) return 1;
    }
    std::string text;
    if (!ReadFileText(index, text) || text != "old") return 1;
    {
        HtmlExportWriter::Options options; options.targetPath = target; options.split = true;
        HtmlExportWriter::Callbacks callbacks = Callbacks(reported); callbacks.confirmImageOverwrite = [](const std::wstring&) { return true; };
        HtmlExportWriter::Writer writer(options, callbacks);
        if (FAILED(writer.Prepare()) || FAILED(writer.WriteSplitDocument(L"index.html", L"new"))) return 1;
        // Existing files are never owned by rollback.
    }
    const int failure = !ReadFileText(index, text) || text != "new";
    ::DeleteFile(index.c_str());
    return failure;
}
int ExpectRollback()
{
	const std::wstring path = TemporaryPath(L"rollback.html");
	int reported = 0;
	{
		HtmlExportWriter::Options options;
		options.targetPath = path;
		HtmlExportWriter::Writer writer(options, Callbacks(reported));
		if (FAILED(writer.Prepare())) return 1;
		CComPtr<IStream> output;
		if (FAILED(writer.GetTransformOutput(&output)) || WriteText(output, "partial")) return 1;
		// No Commit(): destruction must close and remove the partial target.
	}
	return ::GetFileAttributes(path.c_str()) == INVALID_FILE_ATTRIBUTES ? 0 : 1;
}

}

int main()
{
	::CoInitialize(NULL);
	const int failures = ExpectNormalWrite() + ExpectOpenFailure() +
		ExpectWriteFailureAndRollback(false) + ExpectWriteFailureAndRollback(true) + ExpectNoTargetHandleLeak() +
		ExpectStandaloneWrite() + ExpectMimeFinalBoundary() + ExpectExternalImageWrite() +
		ExpectExistingImage(false) + ExpectExistingImage(true) + ExpectUnsafeImageIdsRejected() + ExpectDetectedBinaryMimeTypes() + ExpectExistingSplitDocument() + ExpectRollback();
	::CoUninitialize();
	if (failures != 0) std::cerr << "writer harness failures: " << failures << std::endl;
	return failures == 0 ? 0 : 1;
}