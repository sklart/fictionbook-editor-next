#include "../../src/fbe/stdafx.h"
#include "../../src/fbe/ImageImport.h"
#include "../../src/fbe/RuntimeLocalization.h"

#include <fstream>
#include <iostream>
#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/mux.h>
#define OPJ_STATIC
#include <openjpeg-2.5/openjpeg.h>
#include <libheif/heif.h>

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

static bool OutputHasTransparentPixel(const std::vector<BYTE>& bytes)
{
	HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
	if (!memory) return false;
	void* raw = GlobalLock(memory);
	if (!raw) { GlobalFree(memory); return false; }
	memcpy(raw, bytes.data(), bytes.size()); GlobalUnlock(memory);
	CComPtr<IStream> stream;
	if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) { GlobalFree(memory); return false; }
	Gdiplus::Image image(stream);
	if (image.GetLastStatus() != Gdiplus::Ok) return false;
	Gdiplus::Bitmap bitmap(image.GetWidth(), image.GetHeight(), PixelFormat32bppARGB);
	Gdiplus::Graphics graphics(&bitmap);
	if (graphics.DrawImage(&image, 0, 0, image.GetWidth(), image.GetHeight()) != Gdiplus::Ok) return false;
	Gdiplus::Rect area(0, 0, bitmap.GetWidth(), bitmap.GetHeight()); Gdiplus::BitmapData locked = {};
	if (bitmap.LockBits(&area, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &locked) != Gdiplus::Ok) return false;
	bool transparent = false;
	for (UINT y = 0; y < bitmap.GetHeight() && !transparent; ++y) {
		const BYTE* row = static_cast<const BYTE*>(locked.Scan0) + static_cast<ptrdiff_t>(y) * static_cast<ptrdiff_t>(locked.Stride);
		for (UINT x = 0; x < bitmap.GetWidth(); ++x) if (row[x * 4 + 3] != 255) { transparent = true; break; }
	}
	bitmap.UnlockBits(&locked);
	return transparent;
}

static bool OutputHasColorPixel(const std::vector<BYTE>& bytes)
{
	HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes.size()); if (!memory) return false;
	void* raw = GlobalLock(memory); if (!raw) { GlobalFree(memory); return false; }
	memcpy(raw, bytes.data(), bytes.size()); GlobalUnlock(memory); CComPtr<IStream> stream;
	if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) { GlobalFree(memory); return false; }
	Gdiplus::Image image(stream); if (image.GetLastStatus() != Gdiplus::Ok) return false;
	Gdiplus::Bitmap bitmap(image.GetWidth(), image.GetHeight(), PixelFormat32bppARGB); Gdiplus::Graphics graphics(&bitmap);
	if (graphics.DrawImage(&image, 0, 0, image.GetWidth(), image.GetHeight()) != Gdiplus::Ok) return false;
	Gdiplus::Rect area(0, 0, bitmap.GetWidth(), bitmap.GetHeight()); Gdiplus::BitmapData locked = {};
	if (bitmap.LockBits(&area, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &locked) != Gdiplus::Ok) return false;
	bool color = false; for (UINT y = 0; y < bitmap.GetHeight() && !color; ++y) { const BYTE* row = static_cast<const BYTE*>(locked.Scan0) + static_cast<ptrdiff_t>(y) * static_cast<ptrdiff_t>(locked.Stride); for (UINT x = 0; x < bitmap.GetWidth(); ++x) if (abs(int(row[x*4])-int(row[x*4+1])) > 12 || abs(int(row[x*4+1])-int(row[x*4+2])) > 12) { color=true; break; } }
	bitmap.UnlockBits(&locked); return color;
}

static bool TestAnimatedWebpRejection()
{
	const BYTE pixels[] = { 0, 0, 255, 255 };
	uint8_t* encoded = NULL;
	const size_t encodedSize = WebPEncodeRGBA(pixels, 1, 1, 4, 75.0f, &encoded);
	if (!encodedSize || !encoded) return false;
	WebPMux* mux = WebPMuxNew();
	if (!mux) { WebPFree(encoded); return false; }
	WebPMuxAnimParams animation = {};
	const WebPMuxError setup = WebPMuxSetAnimationParams(mux, &animation);
	WebPMuxFrameInfo frame = {};
	frame.bitstream.bytes = encoded; frame.bitstream.size = encodedSize;
	frame.id = WEBP_CHUNK_ANMF; frame.duration = 100;
	frame.dispose_method = WEBP_MUX_DISPOSE_NONE; frame.blend_method = WEBP_MUX_BLEND;
	const WebPMuxError first = setup == WEBP_MUX_OK ? WebPMuxPushFrame(mux, &frame, 1) : setup;
	const WebPMuxError second = first == WEBP_MUX_OK ? WebPMuxPushFrame(mux, &frame, 1) : first;
	WebPFree(encoded);
	WebPData data = {};
	const WebPMuxError assembled = second == WEBP_MUX_OK ? WebPMuxAssemble(mux, &data) : second;
	WebPMuxDelete(mux);
	WebPBitstreamFeatures features = {};
	if (assembled != WEBP_MUX_OK || !data.bytes || !data.size || WebPGetFeatures(data.bytes, data.size, &features) != VP8_STATUS_OK || !features.has_animation) { WebPDataClear(&data); return false; }
	wchar_t temporaryPath[MAX_PATH] = {};
	if (!GetTempPathW(_countof(temporaryPath), temporaryPath) || !GetTempFileNameW(temporaryPath, L"fbe", 0, temporaryPath)) { WebPDataClear(&data); return false; }
	const CString path(temporaryPath);
	std::ofstream stream(path, std::ios::binary);
	stream.write(reinterpret_cast<const char*>(data.bytes), static_cast<std::streamsize>(data.size));
	stream.close(); WebPDataClear(&data);
	ImageImportOptions options;
	ImageImportResult result;
	CString error;
	const HRESULT hr = ImportImageForFb2(path, options, result, error);
	DeleteFileW(path);
	return hr == E_NOTIMPL;
}

static bool TestTransparentWebp()
{
	const BYTE pixels[] = { 0, 0, 255, 64 };
	uint8_t* encoded = NULL;
	const size_t encodedSize = WebPEncodeRGBA(pixels, 1, 1, 4, 75.0f, &encoded);
	if (!encodedSize || !encoded) return false;
	wchar_t temporaryPath[MAX_PATH] = {};
	if (!GetTempPathW(_countof(temporaryPath), temporaryPath) || !GetTempFileNameW(temporaryPath, L"fbe", 0, temporaryPath)) { WebPFree(encoded); return false; }
	const CString path(temporaryPath);
	std::ofstream stream(path, std::ios::binary); stream.write(reinterpret_cast<const char*>(encoded), static_cast<std::streamsize>(encodedSize)); stream.close(); WebPFree(encoded);
	ImageImportOptions options; ImageImportResult result; CString error;
	if (FAILED(ImportImageForFb2(path, options, result, error)) || !result.converted || !result.hasTransparency || result.mimeType != L"image/png" || !HasPngMagic(result.data)) { DeleteFileW(path); return false; }
	options.outputFormat = ImageOutputFormat::Jpeg;
	if (ImportImageForFb2(path, options, result, error) != E_ABORT) { DeleteFileW(path); return false; }
	options.flattenTransparentJpeg = true;
	const bool ok = SUCCEEDED(ImportImageForFb2(path, options, result, error)) && result.mimeType == L"image/jpeg" && HasJpegMagic(result.data);
	DeleteFileW(path);
	return ok;
}

static bool TestJpeg2000Fixture(bool jp2, bool alpha = false)
{
	opj_image_cmptparm_t components[2] = {};
	for (size_t index = 0; index < (alpha ? 2u : 1u); ++index) { components[index].dx = components[index].dy = 1; components[index].w = components[index].h = 2; components[index].prec = components[index].bpp = 8; }
	std::unique_ptr<opj_image_t, void(*)(opj_image_t*)> image(opj_image_create(alpha ? 2 : 1, components, OPJ_CLRSPC_GRAY), opj_image_destroy);
	if (!image) return false;
	image->x1 = image->y1 = 2;
	const int pixels[] = { 0, 96, 160, 255 }; for (size_t index = 0; index < _countof(pixels); ++index) image->comps[0].data[index] = pixels[index];
	if (alpha) { image->comps[1].alpha = 1; const int opacity[] = { 0, 255, 255, 255 }; for (size_t index = 0; index < _countof(opacity); ++index) image->comps[1].data[index] = opacity[index]; }
	wchar_t temporaryPath[MAX_PATH] = {};
	if (!GetTempPathW(_countof(temporaryPath), temporaryPath) || !GetTempFileNameW(temporaryPath, L"fbe", 0, temporaryPath)) return false;
	DeleteFileW(temporaryPath); CStringA outputPath(temporaryPath);
	opj_cparameters_t parameters; opj_set_default_encoder_parameters(&parameters);
	parameters.cod_format = jp2 ? 1 : 0; parameters.tcp_numlayers = 1; parameters.cp_disto_alloc = 1; parameters.tcp_rates[0] = 0; parameters.numresolution = 1;
	std::unique_ptr<opj_codec_t, void(*)(opj_codec_t*)> codec(opj_create_compress(jp2 ? OPJ_CODEC_JP2 : OPJ_CODEC_J2K), opj_destroy_codec);
	std::unique_ptr<opj_stream_t, void(*)(opj_stream_t*)> stream(opj_stream_create_default_file_stream(outputPath, OPJ_FALSE), opj_stream_destroy);
	if (!codec || !stream || !opj_setup_encoder(codec.get(), &parameters, image.get()) || !opj_start_compress(codec.get(), image.get(), stream.get()) || !opj_encode(codec.get(), stream.get()) || !opj_end_compress(codec.get(), stream.get())) { DeleteFileW(temporaryPath); return false; }
	stream.reset();
	ImageImportOptions options; ImageImportResult result; CString error;
	const HRESULT hr = ImportImageForFb2(temporaryPath, options, result, error); DeleteFileW(temporaryPath);
	return SUCCEEDED(hr) && result.converted && result.mimeType == (alpha ? L"image/png" : L"image/jpeg") && result.width == 2 && result.height == 2 && result.hasTransparency == alpha && (alpha ? HasPngMagic(result.data) : HasJpegMagic(result.data));
}

static bool TestJpeg2000SyccSubsampled()
{
	opj_image_cmptparm_t components[3] = {};
	components[0].dx = components[0].dy = 1; components[0].w = components[0].h = 4; components[0].prec = components[0].bpp = 8;
	for (size_t index = 1; index < _countof(components); ++index) { components[index].dx = components[index].dy = 2; components[index].w = components[index].h = 2; components[index].prec = components[index].bpp = 8; }
	std::unique_ptr<opj_image_t, void(*)(opj_image_t*)> image(opj_image_create(3, components, OPJ_CLRSPC_SYCC), opj_image_destroy);
	if (!image) return false;
	image->x1 = image->y1 = 4;
	const int luma[] = { 32, 64, 96, 128, 48, 80, 112, 144, 64, 96, 128, 160, 80, 112, 144, 176 };
	const int cb[] = { 96, 160, 112, 144 }, cr[] = { 144, 96, 128, 176 };
	for (size_t index = 0; index < _countof(luma); ++index) image->comps[0].data[index] = luma[index];
	for (size_t index = 0; index < _countof(cb); ++index) { image->comps[1].data[index] = cb[index]; image->comps[2].data[index] = cr[index]; }
	wchar_t temporaryPath[MAX_PATH] = {};
	if (!GetTempPathW(_countof(temporaryPath), temporaryPath) || !GetTempFileNameW(temporaryPath, L"fbe", 0, temporaryPath)) return false;
	DeleteFileW(temporaryPath); CStringA outputPath(temporaryPath);
	opj_cparameters_t parameters; opj_set_default_encoder_parameters(&parameters);
	parameters.cod_format = 1; parameters.tcp_numlayers = 1; parameters.cp_disto_alloc = 1; parameters.tcp_rates[0] = 0; parameters.numresolution = 1;
	std::unique_ptr<opj_codec_t, void(*)(opj_codec_t*)> codec(opj_create_compress(OPJ_CODEC_JP2), opj_destroy_codec);
	std::unique_ptr<opj_stream_t, void(*)(opj_stream_t*)> stream(opj_stream_create_default_file_stream(outputPath, OPJ_FALSE), opj_stream_destroy);
	if (!codec || !stream || !opj_setup_encoder(codec.get(), &parameters, image.get()) || !opj_start_compress(codec.get(), image.get(), stream.get()) || !opj_encode(codec.get(), stream.get()) || !opj_end_compress(codec.get(), stream.get())) { DeleteFileW(temporaryPath); return false; }
	stream.reset();
	ImageImportOptions options; ImageImportResult result; CString error;
	const HRESULT hr = ImportImageForFb2(temporaryPath, options, result, error); DeleteFileW(temporaryPath);
	return SUCCEEDED(hr) && result.converted && result.mimeType == L"image/jpeg" && result.width == 4 && result.height == 4 && HasJpegMagic(result.data);
}

static bool TestCorruptImages()
{
	static const BYTE jpeg[] = { 0xff, 0xd8, 0xff };
	static const BYTE png[] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	static const BYTE webp[] = { 'R', 'I', 'F', 'F', 4, 0, 0, 0, 'W', 'E', 'B', 'P' };
	static const BYTE jp2[] = { 0, 0, 0, 12, 'j', 'P', ' ', ' ', 0x0d, 0x0a, 0x87, 0x0a };
	static const BYTE j2k[] = { 0xff, 0x4f, 0xff, 0x51 };
	static const BYTE tiff[] = { 'I', 'I', 42, 0 };
	static const BYTE bmp[] = { 'B', 'M' };
	static const BYTE gif[] = { 'G', 'I', 'F', '8', '9', 'a' };
	static const BYTE avif[] = { 0, 0, 0, 16, 'f', 't', 'y', 'p', 'a', 'v', 'i', 'f', 0, 0, 0, 0 };
	static const BYTE heif[] = { 0, 0, 0, 16, 'f', 't', 'y', 'p', 'h', 'e', 'i', 'c', 0, 0, 0, 0 };
	struct Fixture { const BYTE* bytes; size_t size; } fixtures[] = {
		{ jpeg, sizeof(jpeg) }, { png, sizeof(png) }, { webp, sizeof(webp) }, { jp2, sizeof(jp2) }, { j2k, sizeof(j2k) },
		{ tiff, sizeof(tiff) }, { bmp, sizeof(bmp) }, { gif, sizeof(gif) }, { avif, sizeof(avif) }, { heif, sizeof(heif) }
	};
	for (const Fixture& fixture : fixtures) {
		wchar_t temporaryPath[MAX_PATH] = {};
		if (!GetTempPathW(_countof(temporaryPath), temporaryPath) || !GetTempFileNameW(temporaryPath, L"fbe", 0, temporaryPath)) return false;
		std::ofstream stream(temporaryPath, std::ios::binary); stream.write(reinterpret_cast<const char*>(fixture.bytes), static_cast<std::streamsize>(fixture.size)); stream.close();
		ImageImportOptions options; ImageImportResult result; CString error;
		const HRESULT hr = ImportImageForFb2(temporaryPath, options, result, error); DeleteFileW(temporaryPath);
		if (SUCCEEDED(hr) || !result.data.empty()) return false;
	}
	return true;
}

static bool TestBmffFiletypeClassification()
{
	const BYTE compatibleHeif[] = { 0,0,0,24, 'f','t','y','p', 'm','i','f','3', 0,0,0,0, 'a','v','i','f', 'm','i','f','1' };
	const BYTE video[] = { 0,0,0,24, 'f','t','y','p', 'm','p','4','2', 0,0,0,0, 'a','v','i','f', 'm','i','f','1' };
	struct Fixture { const BYTE* bytes; size_t size; bool heif; } fixtures[] = { { compatibleHeif, sizeof(compatibleHeif), true }, { video, sizeof(video), false } };
	for (const Fixture& fixture : fixtures) {
		wchar_t path[MAX_PATH] = {}; if (!GetTempPathW(_countof(path), path) || !GetTempFileNameW(path, L"fbe", 0, path)) return false;
		std::ofstream stream(path, std::ios::binary); stream.write(reinterpret_cast<const char*>(fixture.bytes), static_cast<std::streamsize>(fixture.size)); stream.close();
		ImageImportOptions options; ImageImportResult result; CString error;
		const HRESULT hr = ImportImageForFb2(path, options, result, error); DeleteFileW(path);
		if (fixture.heif) { if (SUCCEEDED(hr) || error != L"Corrupt or unsupported AVIF/HEIF image.") return false; }
		else if (SUCCEEDED(hr) || error != L"Unsupported or corrupt image format.") return false;
	}
	return true;
}

static bool TestHeif(const wchar_t* path, UINT expectedWidth = 0, UINT expectedHeight = 0, bool requireColor = false)
{
	ImageImportOptions options;
	ImageImportResult result;
	CString error;
	if (FAILED(ImportImageForFb2(path, options, result, error))) { std::wcerr << error.GetString() << std::endl; return false; }
	return result.converted && (result.mimeType == L"image/jpeg" || result.mimeType == L"image/png") && !result.data.empty() && result.width && result.height && (!expectedWidth || (result.width == expectedWidth && result.height == expectedHeight)) && (!requireColor || OutputHasColorPixel(result.data));
}

static bool TestHeif10Bit(const wchar_t* path)
{
	std::unique_ptr<heif_context, void(*)(heif_context*)> context(heif_context_alloc(), heif_context_free); if (!context) return false;
	if (heif_context_read_from_file(context.get(), CStringA(path), NULL).code != heif_error_Ok) return false;
	heif_image_handle* raw = NULL; if (heif_context_get_primary_image_handle(context.get(), &raw).code != heif_error_Ok || !raw) return false;
	std::unique_ptr<heif_image_handle, void(*)(const heif_image_handle*)> handle(raw, heif_image_handle_release);
	return heif_image_handle_get_luma_bits_per_pixel(handle.get()) >= 10 && TestHeif(path);
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
	if (argc != 22) return 2;
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
	if (FAILED(ImportImageForFb2(argv[5], passThrough, result, error)) || !result.converted || !result.hasTransparency || result.mimeType != L"image/png" || !HasPngMagic(result.data) || !OutputHasTransparentPixel(result.data)) return 11;
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
	ImageImportOptions forceConvertedPng;
	forceConvertedPng.outputFormat = ImageOutputFormat::Png;
	if (FAILED(ImportImageForFb2(argv[9], forceConvertedPng, result, error)) || !result.converted || result.mimeType != L"image/png" || !HasPngMagic(result.data)) return 19;
	ImageImportOptions jpegPipeline;
	jpegPipeline.keepSupportedImages = false;
	if (FAILED(ImportImageForFb2(argv[10], jpegPipeline, result, error)) || !result.converted || result.mimeType != L"image/jpeg" || !HasJpegMagic(result.data) || result.width != 2 || result.height != 2) return 20;
	if (!TestHeif(argv[11], 2, 2)) return 21;
	if (ImportImageForFb2(argv[12], passThrough, result, error) != E_NOTIMPL) return 22;
	std::vector<BYTE> transparentPng;
	if (!ReadFile(argv[13], transparentPng) || FAILED(ImportImageForFb2(argv[13], passThrough, result, error)) || result.converted || !result.hasTransparency || result.mimeType != L"image/png" || result.data != transparentPng) return 23;
	forceJpeg.keepSupportedImages = false;
	forceJpeg.flattenTransparentJpeg = false;
	if (ImportImageForFb2(argv[13], forceJpeg, result, error) != E_ABORT) return 24;
	forceJpeg.flattenTransparentJpeg = true;
	if (FAILED(ImportImageForFb2(argv[13], forceJpeg, result, error)) || result.mimeType != L"image/jpeg" || !HasJpegMagic(result.data) || result.width != 2 || result.height != 2) return 25;
	if (ImportImageForFb2(argv[14], passThrough, result, error) != E_NOTIMPL) return 26;
	std::vector<BYTE> pngWithWrongExtension;
	if (!ReadFile(argv[15], pngWithWrongExtension) || FAILED(ImportImageForFb2(argv[15], passThrough, result, error)) || result.mimeType != L"image/png" || result.data != pngWithWrongExtension || result.logicalFileName.Right(4).CompareNoCase(L".png") != 0) return 27;
	if (ImportImageForFb2(argv[16], passThrough, result, error) != E_NOTIMPL) return 28;
	if (!TestAnimatedWebpRejection()) return 29;
	std::vector<BYTE> jpegPassThrough;
	if (!ReadFile(argv[17], jpegPassThrough) || FAILED(ImportImageForFb2(argv[17], passThrough, result, error)) || result.converted || result.mimeType != L"image/jpeg" || result.data != jpegPassThrough || result.logicalFileName.CompareNoCase(L"original.jpeg") != 0) return 30;
	if (!TestTransparentWebp()) return 31;
	if (!TestCorruptImages()) return 32;
	if (!TestBmffFiletypeClassification()) return 36;
	if (!TestJpeg2000Fixture(true) || !TestJpeg2000Fixture(false) || !TestJpeg2000Fixture(true, true) || !TestJpeg2000SyccSubsampled()) return 33;
	if (ImportImageForFb2(argv[18], passThrough, result, error) != HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE)) return 34;
	if (FAILED(ImportImageForFb2(argv[19], passThrough, result, error)) || !result.converted || result.mimeType != L"image/jpeg" || !HasJpegMagic(result.data) || result.width != 2 || result.height != 2) return 35;
	if (!TestHeif(argv[20], 0, 0, true)) return 37;
	if (!TestHeif10Bit(argv[21])) return 38;
	return 0;
}
