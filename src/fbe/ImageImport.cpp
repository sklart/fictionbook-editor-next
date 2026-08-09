#include "stdafx.h"
#include "ImageImport.h"

#include <webp/decode.h>
#define OPJ_STATIC
#include <openjpeg-2.5/openjpeg.h>

namespace {
enum class SourceFormat { Unknown, Jpeg, Png, Webp, Jp2, J2k, Tiff, Bmp, Gif };
const size_t kMaxImagePixels = 100000000; // 100 MP keeps 32-bit process allocations bounded.

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
	if (b.size() >= 4 && ((b[0]=='I' && b[1]=='I' && b[2]==42 && b[3]==0) || (b[0]=='M' && b[1]=='M' && b[2]==0 && b[3]==42))) return SourceFormat::Tiff;
	if (b.size() >= 2 && b[0]=='B' && b[1]=='M') return SourceFormat::Bmp;
	if (StartsWith(b,gif87,sizeof(gif87)) || StartsWith(b,gif89,sizeof(gif89))) return SourceFormat::Gif;
	return SourceFormat::Unknown;
}
bool ReadBytes(const CString& file, std::vector<BYTE>& data) {
	CAtlFile f; if (FAILED(f.Create(file, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING))) return false;
	ULONGLONG n=0; if (FAILED(f.GetSize(n)) || n > ULONG_MAX) return false;
	data.resize(static_cast<size_t>(n)); DWORD read=0; return n == 0 || SUCCEEDED(f.Read(data.data(), static_cast<DWORD>(n), read)) && read == n;
}
CString TargetName(const CString& file, bool png) { CString r(file); int p=r.ReverseFind(L'\\'); if(p>=0) r=r.Mid(p+1); int dot=r.ReverseFind(L'.'); if(dot>=0) r=r.Left(dot); return r + (png ? L".png" : L".jpg"); }
bool GetEncoder(const WCHAR* mime, CLSID& clsid) {
	UINT n=0, bytes=0; if (Gdiplus::GetImageEncodersSize(&n,&bytes)!=Gdiplus::Ok || !bytes) return false;
	std::vector<BYTE> p(bytes); auto e=reinterpret_cast<Gdiplus::ImageCodecInfo*>(p.data()); if (Gdiplus::GetImageEncoders(n,bytes,e)!=Gdiplus::Ok) return false;
	for(UINT i=0;i<n;++i) if(wcscmp(e[i].MimeType,mime)==0) { clsid=e[i].Clsid; return true; } return false;
}
bool SaveBitmap(Gdiplus::Image& image, bool png, int quality, std::vector<BYTE>& out, bool flattenWhite = false) {
	CComPtr<IStream> stream; if(FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stream))) return false;
	CLSID clsid; if(!GetEncoder(png ? L"image/png" : L"image/jpeg",clsid)) return false;
	Gdiplus::EncoderParameters ep = {}; ep.Count=1; ULONG q=(ULONG)max(0,min(100,quality)); ep.Parameter[0].Guid=Gdiplus::EncoderQuality; ep.Parameter[0].Type=Gdiplus::EncoderParameterValueTypeLong; ep.Parameter[0].NumberOfValues=1; ep.Parameter[0].Value=&q;
	if (flattenWhite) {
		Gdiplus::Bitmap flattened(image.GetWidth(), image.GetHeight(), PixelFormat24bppRGB);
		Gdiplus::Graphics graphics(&flattened); graphics.Clear(Gdiplus::Color::White);
		if (graphics.DrawImage(&image, 0, 0, image.GetWidth(), image.GetHeight()) != Gdiplus::Ok || flattened.Save(stream, &clsid, &ep) != Gdiplus::Ok) return false;
	} else if(image.Save(stream, &clsid, png ? NULL : &ep) != Gdiplus::Ok) return false;
	HGLOBAL h=NULL; if(FAILED(GetHGlobalFromStream(stream,&h))) return false; SIZE_T n=GlobalSize(h); void* p=GlobalLock(h); if(!p || !n || n>ULONG_MAX) { if(p) GlobalUnlock(h); return false; } out.assign((BYTE*)p,(BYTE*)p+n); GlobalUnlock(h); return true;
}
bool BitmapHasAlpha(Gdiplus::Image& image) {
	const UINT w=image.GetWidth(), h=image.GetHeight(); if(!w||!h||size_t(w)*h>kMaxImagePixels) return false;
	Gdiplus::Bitmap b(w,h,PixelFormat32bppARGB); Gdiplus::Graphics g(&b); if(g.DrawImage(&image,0,0,w,h)!=Gdiplus::Ok) return false;
	Gdiplus::Rect r(0,0,w,h); Gdiplus::BitmapData d={}; if(b.LockBits(&r,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&d)!=Gdiplus::Ok) return false;
	bool alpha=false; for(UINT y=0;y<h&&!alpha;++y) { BYTE* row=(BYTE*)d.Scan0+y*d.Stride; for(UINT x=0;x<w;++x) if(row[x*4+3]!=255) { alpha=true; break; } } b.UnlockBits(&d); return alpha;
}
HRESULT DecodeGdi(const std::vector<BYTE>& data, SourceFormat type, const ImageImportOptions& o, ImageImportResult& r, CString& err) {
	HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,data.size()); if(!h) return E_OUTOFMEMORY; void* p=GlobalLock(h); memcpy(p,data.data(),data.size()); GlobalUnlock(h);
	CComPtr<IStream> s; HRESULT hr=CreateStreamOnHGlobal(h,TRUE,&s); if(FAILED(hr)) { GlobalFree(h); return hr; }
	Gdiplus::Image image(s); if(image.GetLastStatus()!=Gdiplus::Ok) { err=L"Повреждённое или неподдерживаемое изображение"; return E_FAIL; }
	if(image.GetWidth()==0||image.GetHeight()==0||size_t(image.GetWidth())*image.GetHeight()>kMaxImagePixels) { err=L"Слишком большое изображение"; return E_FAIL; }
	UINT dimensionCount=image.GetFrameDimensionsCount(); if(dimensionCount) { std::vector<GUID> d(dimensionCount); image.GetFrameDimensionsList(d.data(),dimensionCount); UINT frames=image.GetFrameCount(&d[0]); if(frames>1) { err=(type==SourceFormat::Gif ? L"Анимированные GIF пока не поддерживаются" : L"Многостраничные TIFF пока не поддерживаются"); return E_NOTIMPL; } }
	r.hasAlpha=BitmapHasAlpha(image); bool png=o.outputFormat==ImageOutputFormat::Png || (o.outputFormat==ImageOutputFormat::Auto && r.hasAlpha);
	if(o.outputFormat==ImageOutputFormat::Jpeg && r.hasAlpha && !o.flattenTransparentJpeg) { err=L"Изображение содержит прозрачность; требуется подтверждение JPEG на белом фоне"; return E_ABORT; }
	if(!SaveBitmap(image,png,o.jpegQuality,r.data,o.flattenTransparentJpeg && !png)) { err=L"Не удалось закодировать изображение"; return E_FAIL; }
	r.logicalFileName=TargetName(r.logicalFileName,png); r.mimeType=png?L"image/png":L"image/jpeg"; r.converted=true; return S_OK;
}
HRESULT DecodeWebp(const std::vector<BYTE>& data, const ImageImportOptions& o, ImageImportResult& r, CString& err) {
	WebPBitstreamFeatures f={}; VP8StatusCode st=WebPGetFeatures(data.data(),data.size(),&f); if(st!=VP8_STATUS_OK) { err=L"Повреждённый WebP"; return E_FAIL; }
	if(f.has_animation) { err=L"Анимированные WebP пока не поддерживаются"; return E_NOTIMPL; }
	if(f.width<=0||f.height<=0||size_t(f.width)*f.height>kMaxImagePixels) { err=L"Слишком большое изображение"; return E_FAIL; }
	r.hasAlpha=f.has_alpha!=0; bool png=o.outputFormat==ImageOutputFormat::Png || (o.outputFormat==ImageOutputFormat::Auto&&r.hasAlpha); if(o.outputFormat==ImageOutputFormat::Jpeg&&r.hasAlpha&&!o.flattenTransparentJpeg) { err=L"WebP содержит прозрачность; требуется подтверждение JPEG на белом фоне"; return E_ABORT; }
	std::vector<BYTE> pixels(size_t(f.width)*f.height*4); if(!WebPDecodeBGRAInto(data.data(),data.size(),pixels.data(),pixels.size(),f.width*4)) { err=L"Не удалось декодировать WebP"; return E_FAIL; }
	Gdiplus::Bitmap b(f.width,f.height,f.width*4,PixelFormat32bppARGB,pixels.data()); if(!SaveBitmap(b,png,o.jpegQuality,r.data,o.flattenTransparentJpeg && !png)) { err=L"Не удалось закодировать WebP"; return E_FAIL; }
	r.logicalFileName=TargetName(r.logicalFileName,png); r.mimeType=png?L"image/png":L"image/jpeg"; r.converted=true; return S_OK;
}
struct J2kMemory { const BYTE* data; size_t size; size_t pos; };
OPJ_SIZE_T J2kRead(void* buffer, OPJ_SIZE_T bytes, void* user) { J2kMemory* m=(J2kMemory*)user; size_t n=min((size_t)bytes,m->size-m->pos); if(!n) return (OPJ_SIZE_T)-1; memcpy(buffer,m->data+m->pos,n); m->pos+=n; return n; }
OPJ_OFF_T J2kSkip(OPJ_OFF_T bytes, void* user) { J2kMemory* m=(J2kMemory*)user; if(bytes<0 || (size_t)bytes>m->size-m->pos) return -1; m->pos+=(size_t)bytes; return bytes; }
OPJ_BOOL J2kSeek(OPJ_OFF_T pos, void* user) { J2kMemory* m=(J2kMemory*)user; if(pos<0 || (size_t)pos>m->size) return OPJ_FALSE; m->pos=(size_t)pos; return OPJ_TRUE; }
void J2kMessage(const char*, void*) {}
BYTE ComponentByte(const opj_image_comp_t& c, size_t offset) { int value=c.data[offset]; if(c.sgnd) value += 1 << (c.prec-1); if(c.prec>8) value >>= c.prec-8; else if(c.prec<8) value <<= 8-c.prec; return (BYTE)max(0,min(255,value)); }
HRESULT DecodeJ2k(const std::vector<BYTE>& data, SourceFormat type, const ImageImportOptions& o, ImageImportResult& r, CString& err) {
	J2kMemory memory={data.data(),data.size(),0}; opj_stream_t* stream=opj_stream_create(1024*1024,OPJ_TRUE); if(!stream) return E_OUTOFMEMORY;
	opj_stream_set_user_data(stream,&memory,NULL); opj_stream_set_user_data_length(stream,data.size()); opj_stream_set_read_function(stream,J2kRead); opj_stream_set_skip_function(stream,J2kSkip); opj_stream_set_seek_function(stream,J2kSeek);
	opj_dparameters_t params; opj_set_default_decoder_parameters(&params); opj_codec_t* codec=opj_create_decompress(type==SourceFormat::Jp2?OPJ_CODEC_JP2:OPJ_CODEC_J2K); opj_image_t* image=NULL;
	if(!codec) { opj_stream_destroy(stream); return E_OUTOFMEMORY; }
	opj_set_error_handler(codec,J2kMessage,NULL); opj_set_warning_handler(codec,J2kMessage,NULL); bool ok=opj_setup_decoder(codec,&params)&&opj_read_header(stream,codec,&image)&&opj_decode(codec,stream,image)&&opj_end_decompress(codec,stream);
	if(!ok||!image||image->numcomps<1||image->numcomps>4) { if(image) opj_image_destroy(image); opj_destroy_codec(codec); opj_stream_destroy(stream); err=L"Повреждённый или неподдерживаемый JPEG 2000"; return E_FAIL; }
	UINT w=image->comps[0].w,h=image->comps[0].h; if(!w||!h||size_t(w)*h>kMaxImagePixels) { opj_image_destroy(image); opj_destroy_codec(codec); opj_stream_destroy(stream); err=L"Слишком большое изображение"; return E_FAIL; }
	for(UINT i=1;i<image->numcomps;++i) if(image->comps[i].w!=w||image->comps[i].h!=h||image->comps[i].prec==0||image->comps[i].prec>16) { opj_image_destroy(image); opj_destroy_codec(codec); opj_stream_destroy(stream); err=L"Неподдерживаемая структура JPEG 2000"; return E_FAIL; }
	if ((image->numcomps >= 3 && image->color_space == OPJ_CLRSPC_SYCC) || image->color_space == OPJ_CLRSPC_CMYK) { opj_image_destroy(image); opj_destroy_codec(codec); opj_stream_destroy(stream); err=L"Цветовое пространство JPEG 2000 пока не поддерживается"; return E_NOTIMPL; }
	r.hasAlpha=image->numcomps==2 || image->numcomps==4; bool png=o.outputFormat==ImageOutputFormat::Png || (o.outputFormat==ImageOutputFormat::Auto&&r.hasAlpha); if(o.outputFormat==ImageOutputFormat::Jpeg&&r.hasAlpha&&!o.flattenTransparentJpeg) { opj_image_destroy(image); opj_destroy_codec(codec); opj_stream_destroy(stream); err=L"JPEG 2000 содержит прозрачность; требуется подтверждение JPEG на белом фоне"; return E_ABORT; }
	std::vector<BYTE> pixels(size_t(w)*h*4); for(size_t i=0;i<size_t(w)*h;++i) { BYTE v=ComponentByte(image->comps[0],i); BYTE red=image->numcomps>=3?ComponentByte(image->comps[0],i):v, green=image->numcomps>=3?ComponentByte(image->comps[1],i):v, blue=image->numcomps>=3?ComponentByte(image->comps[2],i):v; BYTE alpha=r.hasAlpha?ComponentByte(image->comps[image->numcomps==2 ? 1 : 3],i):255; pixels[i*4]=blue; pixels[i*4+1]=green; pixels[i*4+2]=red; pixels[i*4+3]=alpha; }
	opj_image_destroy(image); opj_destroy_codec(codec); opj_stream_destroy(stream);
	Gdiplus::Bitmap b(w,h,w*4,PixelFormat32bppARGB,pixels.data()); if(!SaveBitmap(b,png,o.jpegQuality,r.data,o.flattenTransparentJpeg && !png)) { err=L"Не удалось закодировать JPEG 2000"; return E_FAIL; }
	r.logicalFileName=TargetName(r.logicalFileName,png); r.mimeType=png?L"image/png":L"image/jpeg"; r.converted=true; return S_OK;
}
}

HRESULT ImportImageForFb2(const CString& sourceFile, const ImageImportOptions& options, ImageImportResult& result, CString& errorMessage) {
	result=ImageImportResult(); errorMessage.Empty(); result.logicalFileName=sourceFile;
	std::vector<BYTE> source; if(!ReadBytes(sourceFile,source)) { errorMessage=L"Не удалось прочитать файл изображения"; return HRESULT_FROM_WIN32(ERROR_READ_FAULT); }
	SourceFormat type=Detect(source); if(type==SourceFormat::Unknown) { errorMessage=L"Формат изображения не поддерживается или файл повреждён"; return E_FAIL; }
	if(options.keepSupportedImages && (type==SourceFormat::Jpeg||type==SourceFormat::Png)) { result.data.swap(source); result.mimeType=type==SourceFormat::Jpeg?L"image/jpeg":L"image/png"; result.hasAlpha=type==SourceFormat::Png; int p=result.logicalFileName.ReverseFind(L'\\'); if(p>=0) result.logicalFileName=result.logicalFileName.Mid(p+1); return S_OK; }
	if(type==SourceFormat::Webp) return DecodeWebp(source,options,result,errorMessage);
	if(type==SourceFormat::Bmp||type==SourceFormat::Gif||type==SourceFormat::Tiff) return DecodeGdi(source,type,options,result,errorMessage);
	return DecodeJ2k(source,type,options,result,errorMessage);
}
