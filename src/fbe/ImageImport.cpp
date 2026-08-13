#include "stdafx.h"
#include "ImageImport.h"
#include "RuntimeLocalization.h"
#include "StartupTrace.h"

#include <webp/decode.h>
#include <memory>
#define OPJ_STATIC
#include <openjpeg-2.5/openjpeg.h>
#define LIBHEIF_STATIC_BUILD
#include <libheif/heif.h>

namespace {
CString ImageMessage(LPCWSTR key, LPCWSTR fallback) { return FbeLoadRuntimeStringByKey(key, fallback); }
enum class SourceFormat { Unknown, Jpeg, Png, Webp, Jp2, J2k, Tiff, Bmp, Gif, Heif };
// Conservative limits for the 32-bit editor: the importer can otherwise hold
// source bytes, a BGRA raster, encoder buffers, a SAFEARRAY and DOM data at once.
const ULONGLONG kMaxSourceBytes = 64ULL * 1024 * 1024;
const UINT kMaxImageDimension = 16384;
const size_t kMaxImagePixels = 32000000;
const size_t kMaxRasterBytes = 128 * 1024 * 1024;
// The encoded image is copied again into a SAFEARRAY and MSHTML/base64 DOM.
const size_t kMaxOutputBytes = 64 * 1024 * 1024;

class GdiplusSession {
	ULONG_PTR token;
public:
	GdiplusSession() : token(0) { Gdiplus::GdiplusStartupInput input; if (Gdiplus::GdiplusStartup(&token, &input, NULL) != Gdiplus::Ok) token = 0; }
	~GdiplusSession() { if (token) Gdiplus::GdiplusShutdown(token); }
	bool Ready() const { return token != 0; }
};
GdiplusSession& GetGdiplusSession() { static GdiplusSession session; return session; }

bool CheckedRasterSize(UINT width, UINT height, size_t channels, size_t& bytes) {
	if (!width || !height || width > kMaxImageDimension || height > kMaxImageDimension) return false;
	if (width > kMaxImagePixels / height) return false;
	const size_t pixels = static_cast<size_t>(width) * height;
	if (channels && pixels > SIZE_MAX / channels) return false;
	bytes = pixels * channels;
	return bytes <= kMaxRasterBytes;
}

bool StartsWith(const std::vector<BYTE>& b, const BYTE* s, size_t n) { return b.size() >= n && memcmp(b.data(), s, n) == 0; }
SourceFormat Detect(const std::vector<BYTE>& b) {
	static const BYTE jpeg[] = { 0xff, 0xd8, 0xff }, png[] = { 0x89, 'P','N','G',0x0d,0x0a,0x1a,0x0a };
	static const BYTE gif87[] = { 'G','I','F','8','7','a' }, gif89[] = { 'G','I','F','8','9','a' };
	static const BYTE j2k[] = { 0xff,0x4f,0xff,0x51 };
	if (StartsWith(b,jpeg,sizeof(jpeg))) return SourceFormat::Jpeg;
	if (StartsWith(b,png,sizeof(png))) return SourceFormat::Png;
	if (b.size() >= 12 && memcmp(b.data(), "RIFF", 4) == 0 && memcmp(b.data()+8, "WEBP", 4) == 0) return SourceFormat::Webp;
	if (b.size() >= 12 && b[4]=='j' && b[5]=='P' && b[6]==' ' && b[7]==' ' && b[8]==0x0d && b[9]==0x0a && b[10]==0x87 && b[11]==0x0a) return SourceFormat::Jp2;
	if (StartsWith(b,j2k,sizeof(j2k))) return SourceFormat::J2k;
	if (b.size() >= 12 && memcmp(b.data()+4, "ftyp", 4) == 0) {
		const int length = static_cast<int>(min(b.size(), static_cast<size_t>(INT_MAX)));
		const heif_filetype_result filetype = heif_check_filetype(b.data(), length);
		// The ftyp box alone cannot establish that an ISO-BMFF file is a still
		// image.  Let libheif parse the complete container and require its primary
		// image in DecodeHeif; compatible image brands must work with generic majors.
		if (filetype == heif_filetype_yes_supported || heif_has_compatible_filetype(b.data(), length).code == heif_error_Ok) return SourceFormat::Heif;
	}
	if (b.size() >= 4 && ((b[0]=='I' && b[1]=='I' && b[2]==42 && b[3]==0) || (b[0]=='M' && b[1]=='M' && b[2]==0 && b[3]==42))) return SourceFormat::Tiff;
	if (b.size() >= 2 && b[0]=='B' && b[1]=='M') return SourceFormat::Bmp;
	if (StartsWith(b,gif87,sizeof(gif87)) || StartsWith(b,gif89,sizeof(gif89))) return SourceFormat::Gif;
	return SourceFormat::Unknown;
}
HRESULT ReadBytes(const CString& file, std::vector<BYTE>& data) {
	CAtlFile f; HRESULT hr = f.Create(file, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING); if (FAILED(hr)) return hr;
	ULONGLONG n=0; hr = f.GetSize(n); if (FAILED(hr)) return hr;
	if (n > ULONG_MAX || n > kMaxSourceBytes) return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
	try { data.resize(static_cast<size_t>(n)); } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
	DWORD read=0; if (n == 0) return S_OK;
	hr = f.Read(data.data(), static_cast<DWORD>(n), read); return SUCCEEDED(hr) && read == n ? S_OK : FAILED(hr) ? hr : HRESULT_FROM_WIN32(ERROR_READ_FAULT);
}
CString TargetName(const CString& file, bool png) { CString r(file); int p=r.ReverseFind(L'\\'); if(p>=0) r=r.Mid(p+1); int dot=r.ReverseFind(L'.'); if(dot>=0) r=r.Left(dot); return r + (png ? L".png" : L".jpg"); }
CString PassThroughName(const CString& file, SourceFormat type) {
	CString name(file); int slash = name.ReverseFind(L'\\'); if (slash >= 0) name = name.Mid(slash + 1);
	int dot = name.ReverseFind(L'.'); CString extension = dot >= 0 ? name.Mid(dot) : L"";
	if ((type == SourceFormat::Png && extension.CompareNoCase(L".png") == 0) ||
		(type == SourceFormat::Jpeg && (extension.CompareNoCase(L".jpg") == 0 || extension.CompareNoCase(L".jpeg") == 0))) return name;
	return TargetName(name, type == SourceFormat::Png);
}
bool GetEncoder(const WCHAR* mime, CLSID& clsid) {
	UINT n=0, bytes=0; if (Gdiplus::GetImageEncodersSize(&n,&bytes)!=Gdiplus::Ok || !bytes) return false;
	std::vector<BYTE> p(bytes); auto e=reinterpret_cast<Gdiplus::ImageCodecInfo*>(p.data()); if (Gdiplus::GetImageEncoders(n,bytes,e)!=Gdiplus::Ok) return false;
	for(UINT i=0;i<n;++i) if(wcscmp(e[i].MimeType,mime)==0) { clsid=e[i].Clsid; return true; } return false;
}
HRESULT SaveBitmap(Gdiplus::Image& image, bool png, int quality, std::vector<BYTE>& out, bool flattenWhite = false) {
	CComPtr<IStream> stream; HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, &stream); if(FAILED(hr)) return hr;
	CLSID clsid; if(!GetEncoder(png ? L"image/png" : L"image/jpeg",clsid)) return E_FAIL;
	Gdiplus::EncoderParameters ep = {}; ep.Count=1; ULONG q=(ULONG)max(0,min(100,quality)); ep.Parameter[0].Guid=Gdiplus::EncoderQuality; ep.Parameter[0].Type=Gdiplus::EncoderParameterValueTypeLong; ep.Parameter[0].NumberOfValues=1; ep.Parameter[0].Value=&q;
	if (flattenWhite) {
		Gdiplus::Bitmap flattened(image.GetWidth(), image.GetHeight(), PixelFormat24bppRGB);
		Gdiplus::Graphics graphics(&flattened); graphics.Clear(Gdiplus::Color::White);
		if (graphics.DrawImage(&image, 0, 0, image.GetWidth(), image.GetHeight()) != Gdiplus::Ok || flattened.Save(stream, &clsid, &ep) != Gdiplus::Ok) return E_FAIL;
	} else if(image.Save(stream, &clsid, png ? NULL : &ep) != Gdiplus::Ok) return E_FAIL;
	HGLOBAL h=NULL; if(FAILED(GetHGlobalFromStream(stream,&h))) return E_FAIL; SIZE_T n=GlobalSize(h); void* p=GlobalLock(h); if(!p || !n || n>ULONG_MAX) { if(p) GlobalUnlock(h); return E_FAIL; } if (n > kMaxOutputBytes) { GlobalUnlock(h); return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE); } out.assign((BYTE*)p,(BYTE*)p+n); GlobalUnlock(h); return S_OK;
}
HRESULT BitmapHasTransparency(Gdiplus::Image& image, bool& hasTransparency) {
	hasTransparency = false;
	const UINT w=image.GetWidth(), h=image.GetHeight(); size_t bytes = 0; if(!CheckedRasterSize(w, h, 4, bytes)) return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
	Gdiplus::Bitmap b(w,h,PixelFormat32bppARGB); Gdiplus::Graphics g(&b); if(g.DrawImage(&image,0,0,w,h)!=Gdiplus::Ok) return E_FAIL;
	Gdiplus::Rect r(0,0,w,h); Gdiplus::BitmapData d={}; if(b.LockBits(&r,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&d)!=Gdiplus::Ok) return E_FAIL;
	for(UINT y=0;y<h&&!hasTransparency;++y) { BYTE* row=static_cast<BYTE*>(d.Scan0)+static_cast<ptrdiff_t>(y)*static_cast<ptrdiff_t>(d.Stride); for(UINT x=0;x<w;++x) if(row[x*4+3]!=255) { hasTransparency=true; break; } } b.UnlockBits(&d); return S_OK;
}
HRESULT ApplyGdiOrientation(Gdiplus::Image& image) {
	const UINT size = image.GetPropertyItemSize(PropertyTagOrientation);
	if (!size) return S_OK;
	std::vector<BYTE> propertyData(size);
	Gdiplus::PropertyItem* property = reinterpret_cast<Gdiplus::PropertyItem*>(propertyData.data());
	if (image.GetPropertyItem(PropertyTagOrientation, size, property) != Gdiplus::Ok || property->length < sizeof(USHORT)) return E_FAIL;
	const USHORT orientation = *reinterpret_cast<const USHORT*>(property->value);
	Gdiplus::RotateFlipType transform = Gdiplus::RotateNoneFlipNone;
	switch (orientation) {
	case 1: return S_OK;
	case 2: transform = Gdiplus::RotateNoneFlipX; break;
	case 3: transform = Gdiplus::Rotate180FlipNone; break;
	case 4: transform = Gdiplus::Rotate180FlipX; break;
	case 5: transform = Gdiplus::Rotate90FlipX; break;
	case 6: transform = Gdiplus::Rotate90FlipNone; break;
	case 7: transform = Gdiplus::Rotate270FlipX; break;
	case 8: transform = Gdiplus::Rotate270FlipNone; break;
	default: return S_OK;
	}
	return image.RotateFlip(transform) == Gdiplus::Ok ? S_OK : E_FAIL;
}
HRESULT DecodeGdi(const std::vector<BYTE>& data, SourceFormat type, const ImageImportOptions& o, ImageImportResult& r, CString& err) {
	if (!GetGdiplusSession().Ready()) return E_FAIL;
	HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,data.size()); if(!h) return E_OUTOFMEMORY;
	void* p=GlobalLock(h); if(!p) { GlobalFree(h); return E_OUTOFMEMORY; }
	memcpy(p,data.data(),data.size()); GlobalUnlock(h);
	CComPtr<IStream> s; HRESULT hr=CreateStreamOnHGlobal(h,TRUE,&s); if(FAILED(hr)) { GlobalFree(h); return hr; }
	Gdiplus::Image image(s); if(image.GetLastStatus()!=Gdiplus::Ok) { err=ImageMessage(L"fbe.image_import.invalid_image", L"Corrupt or unsupported image."); return E_FAIL; }
	size_t rasterBytes = 0; if(!CheckedRasterSize(image.GetWidth(), image.GetHeight(), 4, rasterBytes)) { err=ImageMessage(L"fbe.image_import.too_large", L"Image is too large."); return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE); }
	if (type == SourceFormat::Gif || type == SourceFormat::Tiff) {
		const GUID dimension = type == SourceFormat::Gif ? Gdiplus::FrameDimensionTime : Gdiplus::FrameDimensionPage;
		if (image.GetFrameCount(&dimension) > 1) { err=ImageMessage(type == SourceFormat::Gif ? L"fbe.image_import.gif_animated" : L"fbe.image_import.tiff_multipage", type == SourceFormat::Gif ? L"Animated GIF is not supported yet." : L"Multi-page TIFF is not supported yet."); return E_NOTIMPL; }
	}
	hr = ApplyGdiOrientation(image); if (FAILED(hr)) { err=ImageMessage(L"fbe.image_import.orientation_failed", L"Could not apply image orientation."); return hr; }
	r.width = image.GetWidth(); r.height = image.GetHeight(); hr = BitmapHasTransparency(image, r.hasTransparency); if (FAILED(hr)) { err=ImageMessage(L"fbe.image_import.transparency_failed", L"Could not determine image transparency."); return hr; } bool png=o.outputFormat==ImageOutputFormat::Png || (o.outputFormat==ImageOutputFormat::Auto && r.hasTransparency);
	if(o.outputFormat==ImageOutputFormat::Jpeg && r.hasTransparency && !o.flattenTransparentJpeg) { err=ImageMessage(L"fbe.image_import.flatten_required", L"This image has transparency and needs confirmation before JPEG conversion."); return E_ABORT; }
	hr = SaveBitmap(image,png,o.jpegQuality,r.data,o.flattenTransparentJpeg && !png); if(FAILED(hr)) { err=ImageMessage(hr == HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE) ? L"fbe.image_import.too_large" : L"fbe.image_import.encode_failed", hr == HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE) ? L"Image is too large." : L"Could not encode image."); return hr; }
	r.logicalFileName=TargetName(r.logicalFileName,png); r.mimeType=png?L"image/png":L"image/jpeg"; r.converted=true; return S_OK;
}
HRESULT ValidatePassThroughGdi(const std::vector<BYTE>& data, ImageImportResult& r, CString& err) {
	if (!GetGdiplusSession().Ready()) return E_FAIL;
	HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,data.size()); if(!h) return E_OUTOFMEMORY;
	void* p=GlobalLock(h); if (!p) { GlobalFree(h); return E_OUTOFMEMORY; } memcpy(p,data.data(),data.size()); GlobalUnlock(h);
	CComPtr<IStream> s; HRESULT hr=CreateStreamOnHGlobal(h,TRUE,&s); if(FAILED(hr)) { GlobalFree(h); return hr; }
	Gdiplus::Image image(s); if(image.GetLastStatus()!=Gdiplus::Ok) { err=ImageMessage(L"fbe.image_import.invalid_image", L"Corrupt or unsupported image."); return E_FAIL; }
	size_t rasterBytes = 0; if(!CheckedRasterSize(image.GetWidth(), image.GetHeight(), 4, rasterBytes)) { err=ImageMessage(L"fbe.image_import.too_large", L"Image is too large."); return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE); }
	r.width = image.GetWidth(); r.height = image.GetHeight(); return BitmapHasTransparency(image, r.hasTransparency);
}
HRESULT DecodeWebp(const std::vector<BYTE>& data, const ImageImportOptions& o, ImageImportResult& r, CString& err) {
	WebPBitstreamFeatures f={}; VP8StatusCode st=WebPGetFeatures(data.data(),data.size(),&f); if(st!=VP8_STATUS_OK) { err=ImageMessage(L"fbe.image_import.webp_invalid", L"Corrupt WebP image."); return E_FAIL; }
	if(f.has_animation) { err=ImageMessage(L"fbe.image_import.webp_animated", L"Animated WebP is not supported yet."); return E_NOTIMPL; }
	size_t rasterBytes = 0; if(f.width<=0||f.height<=0||!CheckedRasterSize(static_cast<UINT>(f.width), static_cast<UINT>(f.height), 4, rasterBytes)) { err=ImageMessage(L"fbe.image_import.too_large", L"Image is too large."); return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE); }
	r.width=static_cast<UINT>(f.width); r.height=static_cast<UINT>(f.height);
	if (!GetGdiplusSession().Ready()) return E_FAIL;
	std::vector<BYTE> pixels(rasterBytes); if(!WebPDecodeBGRAInto(data.data(),data.size(),pixels.data(),pixels.size(),f.width*4)) { err=ImageMessage(L"fbe.image_import.webp_decode_failed", L"Could not decode WebP image."); return E_FAIL; }
	r.hasTransparency=false; if (f.has_alpha) for (size_t offset=3; offset<pixels.size(); offset+=4) if (pixels[offset] != 255) { r.hasTransparency=true; break; }
	bool png=o.outputFormat==ImageOutputFormat::Png || (o.outputFormat==ImageOutputFormat::Auto&&r.hasTransparency); if(o.outputFormat==ImageOutputFormat::Jpeg&&r.hasTransparency&&!o.flattenTransparentJpeg) { err=ImageMessage(L"fbe.image_import.flatten_required", L"This image has transparency and needs confirmation before JPEG conversion."); return E_ABORT; }
	Gdiplus::Bitmap b(f.width,f.height,f.width*4,PixelFormat32bppARGB,pixels.data()); HRESULT hr = SaveBitmap(b,png,o.jpegQuality,r.data,o.flattenTransparentJpeg && !png); if(FAILED(hr)) { err=ImageMessage(hr == HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE) ? L"fbe.image_import.too_large" : L"fbe.image_import.webp_encode_failed", hr == HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE) ? L"Image is too large." : L"Could not encode WebP image."); return hr; }
	r.logicalFileName=TargetName(r.logicalFileName,png); r.mimeType=png?L"image/png":L"image/jpeg"; r.converted=true; return S_OK;
}
HRESULT DecodeHeif(const std::vector<BYTE>& data, const ImageImportOptions& o, ImageImportResult& r, CString& err) {
	std::unique_ptr<heif_context, void(*)(heif_context*)> context(heif_context_alloc(), heif_context_free); if(!context) return E_OUTOFMEMORY;
	heif_error status=heif_context_read_from_memory_without_copy(context.get(),data.data(),data.size(),NULL); if(status.code!=heif_error_Ok) { err=ImageMessage(L"fbe.image_import.heif_invalid",L"Corrupt or unsupported AVIF/HEIF image."); return E_FAIL; }
	if(heif_context_get_number_of_top_level_images(context.get())!=1) { err=ImageMessage(L"fbe.image_import.heif_multiple",L"HEIF image sequences are not supported."); return E_NOTIMPL; }
	heif_image_handle* rawHandle=NULL; status=heif_context_get_primary_image_handle(context.get(),&rawHandle); std::unique_ptr<heif_image_handle,void(*)(const heif_image_handle*)> handle(rawHandle,heif_image_handle_release); if(status.code!=heif_error_Ok||!handle) { err=ImageMessage(L"fbe.image_import.heif_invalid",L"Corrupt or unsupported AVIF/HEIF image."); return E_FAIL; }
	int width=heif_image_handle_get_width(handle.get()),height=heif_image_handle_get_height(handle.get()); size_t rasterBytes=0; if(width<=0||height<=0||!CheckedRasterSize((UINT)width,(UINT)height,4,rasterBytes)) { err=ImageMessage(L"fbe.image_import.too_large",L"Image is too large."); return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE); }
	heif_decoding_options* rawOptions=heif_decoding_options_alloc(); std::unique_ptr<heif_decoding_options,void(*)(heif_decoding_options*)> options(rawOptions,heif_decoding_options_free); if(!options) return E_OUTOFMEMORY; options->strict_decoding=1; options->convert_hdr_to_8bit=1; options->ignore_transformations=0;
	heif_image* rawImage=NULL; status=heif_decode_image(handle.get(),&rawImage,heif_colorspace_RGB,heif_chroma_interleaved_RGBA,options.get()); std::unique_ptr<heif_image,void(*)(const heif_image*)> image(rawImage,heif_image_release); if(status.code!=heif_error_Ok||!image) { err=ImageMessage(L"fbe.image_import.heif_decode_failed",L"Could not decode AVIF/HEIF image."); return E_FAIL; }
	width=heif_image_get_primary_width(image.get()); height=heif_image_get_primary_height(image.get()); if(width<=0||height<=0||!CheckedRasterSize((UINT)width,(UINT)height,4,rasterBytes)) { err=ImageMessage(L"fbe.image_import.too_large",L"Image is too large."); return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE); }
	int stride=0; const uint8_t* plane=heif_image_get_plane_readonly(image.get(),heif_channel_interleaved,&stride); if(!plane||stride<width*4||size_t(stride)>kMaxRasterBytes/size_t(height)) { err=ImageMessage(L"fbe.image_import.heif_decode_failed",L"Could not decode AVIF/HEIF image."); return E_FAIL; }
	// libheif returns RGBA; GDI+ PixelFormat32bppARGB is stored as BGRA on little-endian Win32.
	std::vector<BYTE> pixels(rasterBytes); for(int y=0;y<height;++y) { const BYTE* source=plane+size_t(y)*stride; BYTE* target=pixels.data()+size_t(y)*width*4; for(int x=0;x<width;++x) { target[x*4]=source[x*4+2]; target[x*4+1]=source[x*4+1]; target[x*4+2]=source[x*4]; target[x*4+3]=source[x*4+3]; } }
	r.width=width; r.height=height; r.hasTransparency=false; if(heif_image_handle_has_alpha_channel(handle.get())) for(size_t i=3;i<pixels.size();i+=4) if(pixels[i]!=255) { r.hasTransparency=true; break; }
	bool png=o.outputFormat==ImageOutputFormat::Png||(o.outputFormat==ImageOutputFormat::Auto&&r.hasTransparency); if(o.outputFormat==ImageOutputFormat::Jpeg&&r.hasTransparency&&!o.flattenTransparentJpeg) { err=ImageMessage(L"fbe.image_import.flatten_required",L"This image has transparency and needs confirmation before JPEG conversion."); return E_ABORT; }
	if(!GetGdiplusSession().Ready()) return E_FAIL; Gdiplus::Bitmap bitmap(width,height,width*4,PixelFormat32bppARGB,pixels.data()); HRESULT hr = SaveBitmap(bitmap,png,o.jpegQuality,r.data,o.flattenTransparentJpeg&&!png); if(FAILED(hr)) { err=ImageMessage(hr == HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE) ? L"fbe.image_import.too_large" : L"fbe.image_import.encode_failed",hr == HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE) ? L"Image is too large." : L"Could not encode image."); return hr; }
	r.logicalFileName=TargetName(r.logicalFileName,png); r.mimeType=png?L"image/png":L"image/jpeg"; r.converted=true; return S_OK;
}
struct J2kMemory { const BYTE* data; size_t size; size_t pos; };
OPJ_SIZE_T J2kRead(void* buffer, OPJ_SIZE_T bytes, void* user) { J2kMemory* m=(J2kMemory*)user; size_t n=min((size_t)bytes,m->size-m->pos); if(!n) return (OPJ_SIZE_T)-1; memcpy(buffer,m->data+m->pos,n); m->pos+=n; return n; }
OPJ_OFF_T J2kSkip(OPJ_OFF_T bytes, void* user) { J2kMemory* m=(J2kMemory*)user; if(bytes<0 || (size_t)bytes>m->size-m->pos) return -1; m->pos+=(size_t)bytes; return bytes; }
OPJ_BOOL J2kSeek(OPJ_OFF_T pos, void* user) { J2kMemory* m=(J2kMemory*)user; if(pos<0 || (size_t)pos>m->size) return OPJ_FALSE; m->pos=(size_t)pos; return OPJ_TRUE; }
struct J2kDiagnostics { char message[256]; J2kDiagnostics() { message[0] = 0; } };
void J2kMessage(const char* text, void* user) { J2kDiagnostics* diagnostics = static_cast<J2kDiagnostics*>(user); if (!diagnostics || diagnostics->message[0] || !text) return; strncpy_s(diagnostics->message, text, _TRUNCATE); }
BYTE ComponentByte(const opj_image_comp_t& c, size_t offset) { int value=c.data[offset]; if(c.sgnd) value += 1 << (c.prec-1); if(c.prec>8) value >>= c.prec-8; else if(c.prec<8) value <<= 8-c.prec; return (BYTE)max(0,min(255,value)); }
// JPEG 2000 commonly stores Cb/Cr at a lower resolution than Y (sYCC 4:2:0
// and 4:2:2).  OpenJPEG keeps that sampling geometry in each component rather
// than upsampling it for callers, so map output pixels back to the component.
bool ComponentByteAt(const opj_image_comp_t& c, UINT x, UINT y, OPJ_UINT32 imageX0, OPJ_UINT32 imageY0, BYTE& value) {
	// Component samples live on their own reference grid.  Mapping by raster
	// proportions loses component origins and is wrong for non-default dx/dy.
	auto SampleAt = [&](ULONGLONG referenceX, ULONGLONG referenceY) -> bool {
		if (!c.dx || !c.dy || referenceX < c.x0 || referenceY < c.y0) return false;
		const ULONGLONG componentX = (referenceX - c.x0) / c.dx;
		const ULONGLONG componentY = (referenceY - c.y0) / c.dy;
		if (componentX >= c.w || componentY >= c.h) return false;
		value = ComponentByte(c, static_cast<size_t>(componentY * c.w + componentX));
		return true;
	};
	if (SampleAt(static_cast<ULONGLONG>(imageX0) + x, static_cast<ULONGLONG>(imageY0) + y)) return true;
	// OpenJPEG can normalize component coordinates to the decoded raster while
	// retaining the non-zero SIZ image origin separately.  Accept that form only
	// after the absolute-grid mapping above has proved outside the component.
	if (!(imageX0 || imageY0)) return false;
	if (SampleAt(x, y)) return true;
	return SampleAt(static_cast<ULONGLONG>(c.x0) + x, static_cast<ULONGLONG>(c.y0) + y);
}
BYTE ClampByte(int value) { return static_cast<BYTE>(max(0, min(255, value))); }
HRESULT DecodeJ2k(const std::vector<BYTE>& data, SourceFormat type, const ImageImportOptions& o, ImageImportResult& r, CString& err) {
	J2kMemory memory={data.data(),data.size(),0}; std::unique_ptr<opj_stream_t, void(*)(opj_stream_t*)> stream(opj_stream_create(1024*1024,OPJ_TRUE), opj_stream_destroy); if(!stream) return E_OUTOFMEMORY;
	opj_stream_set_user_data(stream.get(),&memory,NULL); opj_stream_set_user_data_length(stream.get(),data.size()); opj_stream_set_read_function(stream.get(),J2kRead); opj_stream_set_skip_function(stream.get(),J2kSkip); opj_stream_set_seek_function(stream.get(),J2kSeek);
	opj_dparameters_t params; opj_set_default_decoder_parameters(&params); std::unique_ptr<opj_codec_t, void(*)(opj_codec_t*)> codec(opj_create_decompress(type==SourceFormat::Jp2?OPJ_CODEC_JP2:OPJ_CODEC_J2K), opj_destroy_codec);
	if(!codec) return E_OUTOFMEMORY;
	J2kDiagnostics diagnostics; opj_set_error_handler(codec.get(),J2kMessage,&diagnostics); opj_set_warning_handler(codec.get(),J2kMessage,&diagnostics);
	opj_image_t* rawImage = NULL; bool ok=opj_setup_decoder(codec.get(),&params)&&opj_read_header(stream.get(),codec.get(),&rawImage)&&opj_decode(codec.get(),stream.get(),rawImage)&&opj_end_decompress(codec.get(),stream.get());
	std::unique_ptr<opj_image_t, void(*)(opj_image_t*)> image(rawImage, opj_image_destroy);
	if(!ok||!image||image->numcomps<1||image->numcomps>4) { err=ImageMessage(L"fbe.image_import.jp2_invalid", L"Corrupt or unsupported JPEG 2000 image."); return E_FAIL; }
	UINT w=image->comps[0].w,h=image->comps[0].h; size_t rasterBytes = 0; if(!CheckedRasterSize(w,h,4,rasterBytes)) { err=ImageMessage(L"fbe.image_import.too_large", L"Image is too large."); return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE); }
	for(UINT i=0;i<image->numcomps;++i) if(!image->comps[i].data||!image->comps[i].w||!image->comps[i].h||!image->comps[i].dx||!image->comps[i].dy||image->comps[i].prec==0||image->comps[i].prec>16) { err=ImageMessage(L"fbe.image_import.jp2_structure", L"Unsupported JPEG 2000 component structure."); return E_FAIL; }
	if (image->color_space == OPJ_CLRSPC_CMYK) { err=ImageMessage(L"fbe.image_import.jp2_cmyk", L"CMYK JPEG 2000 images are not supported."); return E_NOTIMPL; }
	const bool unspecifiedGray = image->color_space == OPJ_CLRSPC_UNSPECIFIED && image->numcomps <= 2;
	if (image->color_space != OPJ_CLRSPC_GRAY && image->color_space != OPJ_CLRSPC_SRGB && image->color_space != OPJ_CLRSPC_SYCC && !unspecifiedGray) { err=ImageMessage(L"fbe.image_import.jp2_structure", L"Unsupported JPEG 2000 component structure."); return E_NOTIMPL; }
	const bool color = image->color_space == OPJ_CLRSPC_SRGB || image->color_space == OPJ_CLRSPC_SYCC; const bool sycc = image->color_space == OPJ_CLRSPC_SYCC;
	int alphaIndex = -1; for (UINT i=0;i<image->numcomps;++i) if (image->comps[i].alpha) { if (alphaIndex >= 0) { err=ImageMessage(L"fbe.image_import.jp2_structure", L"Unsupported JPEG 2000 component structure."); return E_FAIL; } alphaIndex = static_cast<int>(i); }
	if ((!color && (image->numcomps != 1 && !(image->numcomps == 2 && alphaIndex == 1))) || (color && (image->numcomps != 3 && !(image->numcomps == 4 && alphaIndex == 3)))) { err=ImageMessage(L"fbe.image_import.jp2_structure", L"Unsupported JPEG 2000 component structure."); return E_FAIL; }
	if ((color && alphaIndex >= 0 && alphaIndex != 3) || (!color && alphaIndex >= 0 && alphaIndex != 1) || (alphaIndex >= 0 && (image->comps[alphaIndex].w != w || image->comps[alphaIndex].h != h))) { err=ImageMessage(L"fbe.image_import.jp2_structure", L"Unsupported JPEG 2000 component structure."); return E_FAIL; }
	if (!sycc) for (UINT i=0; i<(color ? 3u : 1u); ++i) if (image->comps[i].w != w || image->comps[i].h != h) { err=ImageMessage(L"fbe.image_import.jp2_structure", L"Unsupported JPEG 2000 component structure."); return E_FAIL; }
	r.width=w; r.height=h; r.hasTransparency=false; if (alphaIndex >= 0) for (size_t i=0; i<size_t(w)*h; ++i) if (ComponentByte(image->comps[alphaIndex],i) != 255) { r.hasTransparency=true; break; } bool png=o.outputFormat==ImageOutputFormat::Png || (o.outputFormat==ImageOutputFormat::Auto&&r.hasTransparency); if(o.outputFormat==ImageOutputFormat::Jpeg&&r.hasTransparency&&!o.flattenTransparentJpeg) { err=ImageMessage(L"fbe.image_import.flatten_required", L"This image has transparency and needs confirmation before JPEG conversion."); return E_ABORT; }
	std::vector<BYTE> pixels(rasterBytes); for(UINT y=0;y<h;++y) for(UINT x=0;x<w;++x) { const size_t i=size_t(y)*w+x; BYTE red=0, green=0, blue=0, alpha=255; if (!ComponentByteAt(image->comps[0],x,y,image->x0,image->y0,red)) { err=ImageMessage(L"fbe.image_import.jp2_structure", L"Unsupported JPEG 2000 component structure."); return E_FAIL; } if (color && (!ComponentByteAt(image->comps[1],x,y,image->x0,image->y0,green) || !ComponentByteAt(image->comps[2],x,y,image->x0,image->y0,blue))) { err=ImageMessage(L"fbe.image_import.jp2_structure", L"Unsupported JPEG 2000 component structure."); return E_FAIL; } if (!color) green=blue=red; if (sycc) { const int luma = red, cb = green - 128, cr = blue - 128; red = ClampByte(luma + (359 * cr) / 256); green = ClampByte(luma - (88 * cb + 183 * cr) / 256); blue = ClampByte(luma + (454 * cb) / 256); } if (r.hasTransparency && !ComponentByteAt(image->comps[alphaIndex],x,y,image->x0,image->y0,alpha)) { err=ImageMessage(L"fbe.image_import.jp2_structure", L"Unsupported JPEG 2000 component structure."); return E_FAIL; } pixels[i*4]=blue; pixels[i*4+1]=green; pixels[i*4+2]=red; pixels[i*4+3]=alpha; }
	if (!GetGdiplusSession().Ready()) return E_FAIL;
	Gdiplus::Bitmap b(w,h,w*4,PixelFormat32bppARGB,pixels.data()); HRESULT hr = SaveBitmap(b,png,o.jpegQuality,r.data,o.flattenTransparentJpeg && !png); if(FAILED(hr)) { err=ImageMessage(hr == HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE) ? L"fbe.image_import.too_large" : L"fbe.image_import.encode_failed", hr == HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE) ? L"Image is too large." : L"Could not encode image."); return hr; }
	r.logicalFileName=TargetName(r.logicalFileName,png); r.mimeType=png?L"image/png":L"image/jpeg"; r.converted=true; return S_OK;
}
}

HRESULT ImportImageForFb2(const CString& sourceFile, const ImageImportOptions& options, ImageImportResult& result, CString& errorMessage) {
	try {
	result=ImageImportResult(); errorMessage.Empty(); result.logicalFileName=sourceFile;
	std::vector<BYTE> source; HRESULT readResult = ReadBytes(sourceFile,source); if(FAILED(readResult)) { errorMessage = readResult == HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE) ? ImageMessage(L"fbe.image_import.too_large", L"Image is too large.") : ImageMessage(L"fbe.image_import.read_failed", L"Could not read image file."); StartupTrace::HResult(L"image-import", L"I100", readResult, L"stage=read"); return readResult; }
	SourceFormat type=Detect(source); if(type==SourceFormat::Unknown) { errorMessage=ImageMessage(L"fbe.image_import.unsupported", L"Unsupported or corrupt image format."); StartupTrace::HResult(L"image-import", L"I101", E_FAIL, L"source-format=unknown; stage=detect"); return E_FAIL; }
	const wchar_t* format = type == SourceFormat::Jpeg ? L"jpeg" : type == SourceFormat::Png ? L"png" : type == SourceFormat::Webp ? L"webp" : type == SourceFormat::Jp2 ? L"jp2" : type == SourceFormat::J2k ? L"j2k" : type == SourceFormat::Tiff ? L"tiff" : type == SourceFormat::Bmp ? L"bmp" : type == SourceFormat::Gif ? L"gif" : L"heif";
	auto FinishImport = [&](HRESULT hr) -> HRESULT { CString details; details.Format(L"source-format=%s; width=%u; height=%u; alpha=%d; output=%s; converted=%d", format, result.width, result.height, result.hasTransparency ? 1 : 0, (LPCWSTR)result.mimeType, result.converted ? 1 : 0); if (SUCCEEDED(hr)) StartupTrace::Event(L"image-import", L"I200", details); else StartupTrace::HResult(L"image-import", L"I201", hr, details); return hr; };
	switch (type) {
	case SourceFormat::Jpeg:
	case SourceFormat::Png:
		if (options.keepSupportedImages) {
			HRESULT hr = ValidatePassThroughGdi(source, result, errorMessage);
			if (FAILED(hr)) return FinishImport(hr);
			result.data.swap(source); result.mimeType=type==SourceFormat::Jpeg?L"image/jpeg":L"image/png";
			result.logicalFileName=PassThroughName(result.logicalFileName, type);
			return FinishImport(S_OK);
		}
		return FinishImport(DecodeGdi(source,type,options,result,errorMessage));
	case SourceFormat::Webp: return FinishImport(DecodeWebp(source,options,result,errorMessage));
	case SourceFormat::Jp2:
	case SourceFormat::J2k: return FinishImport(DecodeJ2k(source,type,options,result,errorMessage));
	case SourceFormat::Heif: return FinishImport(DecodeHeif(source,options,result,errorMessage));
	case SourceFormat::Bmp:
	case SourceFormat::Gif:
	case SourceFormat::Tiff: return FinishImport(DecodeGdi(source,type,options,result,errorMessage));
	default: errorMessage=ImageMessage(L"fbe.image_import.unsupported", L"Unsupported or corrupt image format."); return FinishImport(E_FAIL);
	}
	} catch (const std::bad_alloc&) {
		result=ImageImportResult(); errorMessage=ImageMessage(L"fbe.error.out_of_memory", L"Out of memory"); StartupTrace::HResult(L"image-import", L"I102", E_OUTOFMEMORY, L"stage=allocation"); return E_OUTOFMEMORY;
	}
}

CString ImageImportFileFilter() {
	struct Filter { LPCWSTR key; LPCWSTR fallback; LPCWSTR pattern; };
	static const Filter filters[] = {
		{ L"fbe.image_import.filter_supported", L"Supported images", L"*.jpg;*.jpeg;*.png;*.webp;*.jp2;*.j2k;*.bmp;*.gif;*.tif;*.tiff;*.avif;*.heic;*.heif" },
		{ L"fbe.image_import.filter_all", L"All files", L"*.*" },
		{ L"fbe.image_import.filter_jpeg", L"JPEG", L"*.jpg;*.jpeg" },
		{ L"fbe.image_import.filter_png", L"PNG", L"*.png" },
		{ L"fbe.image_import.filter_webp", L"WebP", L"*.webp" },
		{ L"fbe.image_import.filter_jpeg2000", L"JPEG 2000", L"*.jp2;*.j2k" },
		{ L"fbe.image_import.filter_bmp", L"Bitmap", L"*.bmp" },
		{ L"fbe.image_import.filter_gif", L"GIF", L"*.gif" },
		{ L"fbe.image_import.filter_tiff", L"TIFF", L"*.tif;*.tiff" }
		, { L"fbe.image_import.filter_avif", L"AVIF", L"*.avif" }
		, { L"fbe.image_import.filter_heif", L"HEIC / HEIF", L"*.heic;*.heif" }
	};
	CString result;
	for (size_t index = 0; index < _countof(filters); ++index) {
		CString caption = ImageMessage(filters[index].key, filters[index].fallback);
		result += caption + L" (" + filters[index].pattern + L")";
		result.AppendChar(L'\0');
		result += filters[index].pattern;
		result.AppendChar(L'\0');
	}
	result.AppendChar(L'\0');
	return result;
}
