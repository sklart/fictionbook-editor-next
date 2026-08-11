#include "../../src/fbe/stdafx.h"
#include "../../src/fbe/ImageImport.h"
#include "../../src/fbe/RuntimeLocalization.h"

#include <fstream>
#include <iostream>

CAppModule _Module;

// The importer only uses this API to obtain a translated diagnostic string.
// The smoke binary intentionally exercises the English fallback path.
int FbeLoadRuntimeString(UINT, wchar_t*, int) { return 0; }
CString FbeLoadRuntimeString(UINT, LPCWSTR fallback) { return CString(fallback ? fallback : L""); }
CString FbeLoadRuntimeStringByKey(LPCWSTR, LPCWSTR fallback) { return CString(fallback ? fallback : L""); }
bool FbeIsRuntimeLocaleInstalled(LPCWSTR) { return false; }
void FbePublishRuntimeLocaleName(LPCWSTR) {}
void FbeResetRuntimeLocalization() {}

namespace StartupTrace {
void Event(const wchar_t*, const wchar_t*, const wchar_t*) {}
void HResult(const wchar_t*, const wchar_t*, HRESULT, const wchar_t*) {}
}

static bool ReadFile(const wchar_t* path, std::vector<BYTE>& bytes)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream) return false;
	bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
	return true;
}

static bool HasPngMagic(const std::vector<BYTE>& bytes)
{
	static const BYTE magic[] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	return bytes.size() >= sizeof(magic) && memcmp(bytes.data(), magic, sizeof(magic)) == 0;
}

static bool HasJpegMagic(const std::vector<BYTE>& bytes)
{
	return bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff;
}

static bool TestHeif(const wchar_t* path)
{
	ImageImportOptions options;
	ImageImportResult result;
	CString error;
	if (FAILED(ImportImageForFb2(path, options, result, error))) { std::wcerr << error.GetString() << std::endl; return false; }
	return result.converted && (result.mimeType == L"image/jpeg" || result.mimeType == L"image/png") && !result.data.empty() && result.width && result.height;
}

static bool TestFb2BinaryRoundTrip(const wchar_t* path)
{
	ImageImportOptions options;
	ImageImportResult imported;
	CString error;
	if (FAILED(ImportImageForFb2(path, options, imported, error)) || imported.data.empty()) return false;

	const HRESULT initialized = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return false;
	bool ok = false;
	try {
		MSXML2::IXMLDOMDocument2Ptr document;
		if (FAILED(document.CreateInstance(__uuidof(MSXML2::DOMDocument60)))) throw _com_error(E_FAIL);
		MSXML2::IXMLDOMElementPtr root = document->createElement(_bstr_t(L"FictionBook"));
		if (!root) throw _com_error(E_FAIL);
		document->appendChild(root);
		MSXML2::IXMLDOMElementPtr binary = document->createElement(_bstr_t(L"binary"));
		if (!binary) throw _com_error(E_FAIL);
		binary->setAttribute(_bstr_t(L"id"), _variant_t(L"cover-image"));
		binary->setAttribute(_bstr_t(L"content-type"), _variant_t((LPCWSTR)imported.mimeType));
		SAFEARRAY* bytes = SafeArrayCreateVector(VT_UI1, 0, static_cast<ULONG>(imported.data.size()));
		if (!bytes) throw _com_error(E_OUTOFMEMORY);
		void* raw = NULL;
		if (FAILED(SafeArrayAccessData(bytes, &raw))) { SafeArrayDestroy(bytes); throw _com_error(E_FAIL); }
		memcpy(raw, imported.data.data(), imported.data.size());
		SafeArrayUnaccessData(bytes);
		_variant_t value;
		V_VT(&value) = VT_ARRAY | VT_UI1;
		V_ARRAY(&value) = bytes;
		MSXML2::IXMLDOMNodePtr binaryNode = binary;
		binaryNode->PutdataType(_bstr_t(L"bin.base64"));
		binaryNode->PutnodeTypedValue(value);
		root->appendChild(binary);

		wchar_t temporaryPath[MAX_PATH] = {};
		if (!GetTempPathW(_countof(temporaryPath), temporaryPath) || !GetTempFileNameW(temporaryPath, L"fbe", 0, temporaryPath)) throw _com_error(E_FAIL);
		const CString savedPath(temporaryPath);
		document->save(_variant_t((LPCWSTR)savedPath));
		MSXML2::IXMLDOMDocument2Ptr reopened;
		if (FAILED(reopened.CreateInstance(__uuidof(MSXML2::DOMDocument60))) || reopened->load(_variant_t((LPCWSTR)savedPath)) != VARIANT_TRUE) { DeleteFileW(savedPath); throw _com_error(E_FAIL); }
		MSXML2::IXMLDOMNodePtr stored = reopened->selectSingleNode(_bstr_t(L"/FictionBook/binary"));
		if (!stored || _bstr_t(stored->attributes->getNamedItem(_bstr_t(L"id"))->text) != _bstr_t(L"cover-image") || _bstr_t(stored->attributes->getNamedItem(_bstr_t(L"content-type"))->text) != _bstr_t((LPCWSTR)imported.mimeType)) { DeleteFileW(savedPath); throw _com_error(E_FAIL); }
		stored->PutdataType(_bstr_t(L"bin.base64"));
		_variant_t restored = stored->GetnodeTypedValue();
		if ((V_VT(&restored) & VT_ARRAY) == 0 || (V_VT(&restored) & VT_TYPEMASK) != VT_UI1) { DeleteFileW(savedPath); throw _com_error(E_FAIL); }
		LONG lower = 0, upper = -1;
		SafeArrayGetLBound(V_ARRAY(&restored), 1, &lower); SafeArrayGetUBound(V_ARRAY(&restored), 1, &upper);
		void* restoredRaw = NULL;
		if (upper - lower + 1 != static_cast<LONG>(imported.data.size()) || FAILED(SafeArrayAccessData(V_ARRAY(&restored), &restoredRaw)) || memcmp(restoredRaw, imported.data.data(), imported.data.size()) != 0) { if (restoredRaw) SafeArrayUnaccessData(V_ARRAY(&restored)); DeleteFileW(savedPath); throw _com_error(E_FAIL); }
		SafeArrayUnaccessData(V_ARRAY(&restored));
		DeleteFileW(savedPath);
		ok = true;
	}
	catch (const _com_error&) { ok = false; }
	if (SUCCEEDED(initialized)) CoUninitialize();
	return ok;
}

int wmain(int argc, wchar_t** argv)
{
	if (argc != 14) return 2;
	std::vector<BYTE> input;
	if (!ReadFile(argv[1], input)) return 3;

	ImageImportOptions passThrough;
	ImageImportResult result;
	CString error;
	if (FAILED(ImportImageForFb2(argv[1], passThrough, result, error))) return 4;
	if (result.converted || result.mimeType != L"image/png" || result.data != input || !result.width || !result.height) return 5;

	ImageImportOptions forcePng;
	forcePng.keepSupportedImages = false;
	forcePng.outputFormat = ImageOutputFormat::Png;
	if (FAILED(ImportImageForFb2(argv[1], forcePng, result, error))) return 6;
	if (!result.converted || result.mimeType != L"image/png" || !HasPngMagic(result.data) || result.logicalFileName.Right(4).CompareNoCase(L".png") != 0) return 7;

	if (SUCCEEDED(ImportImageForFb2(argv[2], passThrough, result, error))) return 8;
	if (!TestHeif(argv[3])) return 9;
	if (!TestHeif(argv[4])) return 10;

	// Actual alpha, rather than an alpha-channel flag, must select PNG in Auto.
	if (FAILED(ImportImageForFb2(argv[5], passThrough, result, error)) || !result.converted || !result.hasTransparency || result.mimeType != L"image/png" || !HasPngMagic(result.data)) return 11;
	ImageImportOptions forceJpeg;
	forceJpeg.outputFormat = ImageOutputFormat::Jpeg;
	if (ImportImageForFb2(argv[5], forceJpeg, result, error) != E_ABORT) return 12;
	forceJpeg.flattenTransparentJpeg = true;
	if (FAILED(ImportImageForFb2(argv[5], forceJpeg, result, error)) || result.mimeType != L"image/jpeg" || !HasJpegMagic(result.data)) return 13;

	// GDI source formats must also use the common JPEG/PNG output pipeline.
	if (FAILED(ImportImageForFb2(argv[6], passThrough, result, error)) || !result.converted || result.mimeType != L"image/jpeg" || !HasJpegMagic(result.data)) return 14;
	// Detection uses actual bytes, never only the extension.
	if (!TestHeif(argv[7])) return 15;
	if (SUCCEEDED(ImportImageForFb2(argv[8], passThrough, result, error))) return 16;
	if (!TestFb2BinaryRoundTrip(argv[3])) return 17;
	if (!TestHeif(argv[9])) return 18;
	ImageImportOptions jpegPipeline;
	jpegPipeline.keepSupportedImages = false;
	if (FAILED(ImportImageForFb2(argv[10], jpegPipeline, result, error)) || !result.converted || result.mimeType != L"image/jpeg" || !HasJpegMagic(result.data)) return 19;
	if (!TestHeif(argv[11])) return 20;
	if (!TestHeif(argv[12])) return 21;
	std::vector<BYTE> transparentPng;
	if (!ReadFile(argv[13], transparentPng) || FAILED(ImportImageForFb2(argv[13], passThrough, result, error)) || result.converted || !result.hasTransparency || result.mimeType != L"image/png" || result.data != transparentPng) return 22;
	forceJpeg.keepSupportedImages = false;
	forceJpeg.flattenTransparentJpeg = false;
	if (ImportImageForFb2(argv[13], forceJpeg, result, error) != E_ABORT) return 23;
	forceJpeg.flattenTransparentJpeg = true;
	if (FAILED(ImportImageForFb2(argv[13], forceJpeg, result, error)) || result.mimeType != L"image/jpeg" || !HasJpegMagic(result.data)) return 24;
	return 0;
}
