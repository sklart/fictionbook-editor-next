#include "stdafx.h"
#include "HtmlPasteSanitizer.h"

#include <algorithm>
#include <string>
#include <vector>
#include <atlbase.h>
#include <new>

namespace FbeClipboard {
namespace {

constexpr SIZE_T kMaxFragmentBytes = 2 * 1024 * 1024;
constexpr int kMaxDepth = 128;
constexpr size_t kMaxNodes = 100000;
constexpr wchar_t kInternalClipboardMarker[] = L"FBE Next internal HTML fragment v2";
constexpr wchar_t kInternalClipboardFormat[] = L"FBE Next Internal HTML v2";
struct InternalMarkerPayload {
  wchar_t signature[_countof(kInternalClipboardMarker)];
  GUID origin;
};
InternalMarkerPayload MarkerPayload(const GUID& origin) {
  InternalMarkerPayload payload = {};
  memcpy(payload.signature, kInternalClipboardMarker, sizeof(kInternalClipboardMarker));
  payload.origin = origin;
  return payload;
}

// MSHTML owns its clipboard through OLE. Replacing that IDataObject preserves
// delayed rendering and every original format while adding our marker.
class MarkedDataObject final : public IDataObject {
public:
  MarkedDataObject(IDataObject* source, CLIPFORMAT marker, const GUID& origin) : source_(source), marker_(marker), origin_(origin) {}
  STDMETHOD(QueryInterface)(REFIID iid, void** value) override {
    if (!value) return E_POINTER;
    *value = nullptr;
    if (iid == IID_IUnknown || iid == IID_IDataObject) { *value = static_cast<IDataObject*>(this); AddRef(); return S_OK; }
    return E_NOINTERFACE;
  }
  STDMETHOD_(ULONG, AddRef)() override { return static_cast<ULONG>(::InterlockedIncrement(&refs_)); }
  STDMETHOD_(ULONG, Release)() override { const ULONG refs = static_cast<ULONG>(::InterlockedDecrement(&refs_)); if (!refs) delete this; return refs; }
  STDMETHOD(GetData)(FORMATETC* format, STGMEDIUM* medium) override {
    if (!format || !medium) return E_POINTER;
    if (format->cfFormat != marker_) return source_->GetData(format, medium);
    if (format->dwAspect != DVASPECT_CONTENT || format->lindex != -1 || !(format->tymed & TYMED_HGLOBAL)) return DV_E_FORMATETC;
    HGLOBAL data = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(InternalMarkerPayload));
    if (!data) return E_OUTOFMEMORY;
    void* bytes = ::GlobalLock(data);
    if (!bytes) { ::GlobalFree(data); return E_OUTOFMEMORY; }
    const InternalMarkerPayload payload = MarkerPayload(origin_);
    memcpy(bytes, &payload, sizeof(payload));
    ::GlobalUnlock(data);
    medium->tymed = TYMED_HGLOBAL; medium->hGlobal = data; medium->pUnkForRelease = nullptr;
    return S_OK;
  }
  STDMETHOD(GetDataHere)(FORMATETC* format, STGMEDIUM* medium) override { return source_->GetDataHere(format, medium); }
  STDMETHOD(QueryGetData)(FORMATETC* format) override {
    if (!format) return E_POINTER;
    return format->cfFormat == marker_ ?
      (format->dwAspect == DVASPECT_CONTENT && format->lindex == -1 && (format->tymed & TYMED_HGLOBAL) ? S_OK : DV_E_FORMATETC) :
      source_->QueryGetData(format);
  }
  STDMETHOD(GetCanonicalFormatEtc)(FORMATETC* format, FORMATETC* result) override { return source_->GetCanonicalFormatEtc(format, result); }
  STDMETHOD(SetData)(FORMATETC* format, STGMEDIUM* medium, BOOL release) override { return source_->SetData(format, medium, release); }
  STDMETHOD(EnumFormatEtc)(DWORD direction, IEnumFORMATETC** result) override {
    if (!result) return E_POINTER;
    *result = nullptr;
    if (direction != DATADIR_GET) return source_->EnumFormatEtc(direction, result);
    CComPtr<IEnumFORMATETC> sourceEnum;
    HRESULT hr = source_->EnumFormatEtc(direction, &sourceEnum);
    if (FAILED(hr)) return hr;
    auto enumerator = new (std::nothrow) MarkedFormatEnum(sourceEnum, marker_);
    if (!enumerator) return E_OUTOFMEMORY;
    *result = enumerator;
    return S_OK;
  }
  STDMETHOD(DAdvise)(FORMATETC* format, DWORD flags, IAdviseSink* sink, DWORD* connection) override { return source_->DAdvise(format, flags, sink, connection); }
  STDMETHOD(DUnadvise)(DWORD connection) override { return source_->DUnadvise(connection); }
  STDMETHOD(EnumDAdvise)(IEnumSTATDATA** result) override { return source_->EnumDAdvise(result); }
private:
  class MarkedFormatEnum final : public IEnumFORMATETC {
  public:
    MarkedFormatEnum(IEnumFORMATETC* source, CLIPFORMAT marker) : source_(source), marker_(marker) {}
    STDMETHOD(QueryInterface)(REFIID iid, void** value) override {
      if (!value) return E_POINTER;
      *value = nullptr;
      if (iid == IID_IUnknown || iid == IID_IEnumFORMATETC) { *value = static_cast<IEnumFORMATETC*>(this); AddRef(); return S_OK; }
      return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return static_cast<ULONG>(::InterlockedIncrement(&refs_)); }
    STDMETHOD_(ULONG, Release)() override { const ULONG refs = static_cast<ULONG>(::InterlockedDecrement(&refs_)); if (!refs) delete this; return refs; }
    STDMETHOD(Next)(ULONG requested, FORMATETC* formats, ULONG* fetched) override {
      if (!formats || (requested != 1 && !fetched)) return E_POINTER;
      ULONG count = 0;
      if (!marked_ && requested) {
        formats[count++] = {marker_, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        marked_ = true;
      }
      ULONG fromSource = 0;
      if (count < requested) source_->Next(requested - count, formats + count, &fromSource);
      count += fromSource;
      if (fetched) *fetched = count;
      return count == requested ? S_OK : S_FALSE;
    }
    STDMETHOD(Skip)(ULONG count) override { FORMATETC format; while (count--) if (Next(1, &format, nullptr) != S_OK) return S_FALSE; return S_OK; }
    STDMETHOD(Reset)() override { marked_ = false; return source_->Reset(); }
    STDMETHOD(Clone)(IEnumFORMATETC** result) override {
      if (!result) return E_POINTER;
      CComPtr<IEnumFORMATETC> sourceClone;
      HRESULT hr = source_->Clone(&sourceClone);
      if (FAILED(hr)) return hr;
      auto clone = new (std::nothrow) MarkedFormatEnum(sourceClone, marker_);
      if (!clone) return E_OUTOFMEMORY;
      clone->marked_ = marked_;
      *result = clone;
      return S_OK;
    }
  private:
    volatile LONG refs_ = 1;
    CComPtr<IEnumFORMATETC> source_;
    CLIPFORMAT marker_;
    bool marked_ = false;
  };
  volatile LONG refs_ = 1;
  CComPtr<IDataObject> source_;
  CLIPFORMAT marker_;
  GUID origin_;
};

class ClipboardScope {
public:
  explicit ClipboardScope(HWND owner) : open_(false) {
    for (int attempt = 0; attempt < 8 && !open_; ++attempt) {
      open_ = ::OpenClipboard(owner) != FALSE;
      if (!open_) ::Sleep(2);
    }
  }
  ~ClipboardScope() { if (open_) ::CloseClipboard(); }
  bool Open() const { return open_; }
private:
  bool open_;
};

struct Node {
  CString tag;
  CString text;
  CString href;
  CString src;
  CString id;
  CString cssClass;
  unsigned formatOn = 0;
  unsigned formatOff = 0;
  std::vector<size_t> children;
};

enum Format : unsigned { Strong = 1, Em = 2, Strike = 4, Sup = 8, Sub = 16 };

bool Space(wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; }
bool NameChar(wchar_t c) {
  return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
         (c >= L'0' && c <= L'9') || c == L'-' || c == L':' || c == L'_';
}
CString Lower(CString s) { s.MakeLower(); return s; }

bool Decimal(const char* bytes, size_t length, const char* key, size_t& value, size_t maximum = 0) {
  const size_t keyLength = strlen(key);
  for (size_t i = 0; i + keyLength < length; ++i) {
    if (_strnicmp(bytes + i, key, keyLength) != 0) continue;
    size_t p = i + keyLength;
    while (p < length && bytes[p] == ' ') ++p;
    if (p == length || bytes[p] < '0' || bytes[p] > '9') return false;
    size_t result = 0;
    while (p < length && bytes[p] >= '0' && bytes[p] <= '9') {
      const unsigned digit = bytes[p++] - '0';
      if (result > ((maximum ? maximum : length) - digit) / 10) return false;
      result = result * 10 + digit;
    }
    value = result;
    return true;
  }
  return false;
}

bool AppendCodepoint(unsigned codepoint, CString& out) {
  if (!codepoint || codepoint > 0x10ffff ||
      (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
  if (codepoint < 0x10000) out.AppendChar(static_cast<wchar_t>(codepoint));
  else {
    codepoint -= 0x10000;
    out.AppendChar(static_cast<wchar_t>(0xd800 + (codepoint >> 10)));
    out.AppendChar(static_cast<wchar_t>(0xdc00 + (codepoint & 0x3ff)));
  }
  return true;
}

bool Decode(const CString& input, CString& output) {
  static const struct { const wchar_t* name; unsigned codepoint; } named[] = {
    {L"amp", 38}, {L"lt", 60}, {L"gt", 62}, {L"quot", 34}, {L"apos", 39},
    {L"nbsp", 0xa0}, {L"laquo", 0xab}, {L"raquo", 0xbb}, {L"copy", 0xa9},
    {L"reg", 0xae}, {L"trade", 0x2122}, {L"euro", 0x20ac}, {L"cent", 0xa2},
    {L"pound", 0xa3}, {L"yen", 0xa5}, {L"sect", 0xa7}, {L"para", 0xb6},
    {L"middot", 0xb7}, {L"bull", 0x2022}, {L"hellip", 0x2026},
    {L"ndash", 0x2013}, {L"mdash", 0x2014}, {L"lsquo", 0x2018},
    {L"rsquo", 0x2019}, {L"ldquo", 0x201c}, {L"rdquo", 0x201d},
    {L"sbquo", 0x201a}, {L"bdquo", 0x201e}, {L"lsaquo", 0x2039},
    {L"rsaquo", 0x203a}, {L"permil", 0x2030}, {L"prime", 0x2032},
    {L"Prime", 0x2033}, {L"minus", 0x2212}, {L"plusmn", 0xb1},
    {L"times", 0xd7}, {L"divide", 0xf7}, {L"deg", 0xb0}, {L"micro", 0xb5},
    {L"frac14", 0xbc}, {L"frac12", 0xbd}, {L"frac34", 0xbe},
    {L"sup1", 0xb9}, {L"sup2", 0xb2}, {L"sup3", 0xb3},
    {L"larr", 0x2190}, {L"rarr", 0x2192},
    {L"uarr", 0x2191}, {L"darr", 0x2193}, {L"harr", 0x2194},
    {L"acute", 0xb4}, {L"cedil", 0xb8},
    {L"iexcl", 0xa1}, {L"iquest", 0xbf}, {L"not", 0xac},
    {L"shy", 0xad}, {L"brvbar", 0xa6}, {L"curren", 0xa4}
  };
  for (int i = 0; i < input.GetLength(); ++i) {
    if (input[i] != L'&') { output.AppendChar(input[i]); continue; }
    const int end = input.Find(L';', i + 1);
    if (end < 0 || end - i > 32) { output.AppendChar(L'&'); continue; }
    const CString entity = input.Mid(i + 1, end - i - 1);
    unsigned codepoint = 0;
    for (const auto& item : named) if (entity == item.name) { codepoint = item.codepoint; break; }
    bool valid = codepoint != 0;
    if (!valid && entity.GetLength() > 1 && entity[0] == L'#') {
      const bool hex = entity.GetLength() > 2 && (entity[1] == L'x' || entity[1] == L'X');
      const int start = hex ? 2 : 1;
      valid = start < entity.GetLength();
      for (int j = start; j < entity.GetLength(); ++j) {
        const wchar_t c = entity[j];
        const int digit = c >= L'0' && c <= L'9' ? c - L'0' :
          hex && c >= L'a' && c <= L'f' ? c - L'a' + 10 :
          hex && c >= L'A' && c <= L'F' ? c - L'A' + 10 : -1;
        if (digit < 0 || codepoint > 0x10ffff / (hex ? 16u : 10u)) { valid = false; break; }
        codepoint = codepoint * (hex ? 16u : 10u) + digit;
      }
    }
    if (valid && AppendCodepoint(codepoint, output)) i = end;
    else output.AppendChar(L'&'); // Keep malformed/unknown entities as text.
  }
  return true;
}

CString Escape(const CString& input, const CString& nbspReplacement) {
  CString out;
  for (int i = 0; i < input.GetLength(); ++i) {
    const wchar_t c = input[i];
    if (c == L'&') out += L"&amp;";
    else if (c == L'<') out += L"&lt;";
    else if (c == L'>') out += L"&gt;";
    else if (c == L'"') out += L"&quot;";
    else if (c == 0xa0 && nbspReplacement != L"\u00a0") out += Escape(nbspReplacement, L"\u00a0");
    else out.AppendChar(c);
  }
  return out;
}

bool Block(const CString& tag) {
  return tag == L"p" || tag == L"div" || tag == L"blockquote" ||
    tag == L"h1" || tag == L"h2" || tag == L"h3" ||
    tag == L"h4" || tag == L"h5" || tag == L"h6";
}
bool Void(const CString& tag) {
  return tag == L"br" || tag == L"hr" || tag == L"meta" ||
         tag == L"link" || tag == L"input" || tag == L"w:br";
}
bool Complex(const CString& tag) {
  return tag == L"table" || tag == L"img" ||
    tag == L"ol" || tag == L"ul" || tag == L"li" ||
    tag == L"math" || tag == L"svg" || tag == L"object" ||
    tag == L"video" || tag == L"audio" || tag == L"iframe";
}
bool Discard(const CString& tag) {
  return tag == L"script" || tag == L"style" || tag == L"head" ||
    tag == L"template" || tag == L"noscript" || tag == L"iframe" ||
    tag == L"object" || tag == L"embed" || tag == L"svg" ||
    tag == L"video" || tag == L"audio";
}

void StyleFormat(const CString& style, unsigned& on, unsigned& off) {
  int start = 0;
  while (start < style.GetLength()) {
    const int separator = style.Find(L';', start);
    const CString item = style.Mid(start, (separator < 0 ? style.GetLength() : separator) - start);
    const int colon = item.Find(L':');
    if (colon > 0) {
      CString key = Lower(item.Left(colon)); key.Trim();
      CString value = Lower(item.Mid(colon + 1)); value.Trim();
      auto set = [&](unsigned bit, bool enabled) {
        if (enabled) { on |= bit; off &= ~bit; }
        else { off |= bit; on &= ~bit; }
      };
      if (key == L"font-weight") {
        if (value == L"bold" || value == L"bolder" ||
            (value.GetLength() == 3 && value[0] >= L'6' && value[0] <= L'9' && value.Mid(1) == L"00")) set(Strong, true);
        else if (value == L"normal" || value == L"lighter" ||
            (value.GetLength() == 3 && value[0] >= L'1' && value[0] <= L'5' && value.Mid(1) == L"00")) set(Strong, false);
      }
      if (key == L"font-style") {
        if (value == L"italic" || value == L"oblique") set(Em, true);
        else if (value == L"normal") set(Em, false);
      }
      if (key == L"text-decoration" || key == L"text-decoration-line") {
        if (value.Find(L"line-through") >= 0) set(Strike, true);
        else if (value == L"none") set(Strike, false);
      }
      if (key == L"vertical-align") {
        if (value == L"super") { set(Sup, true); set(Sub, false); }
        else if (value == L"sub") { set(Sub, true); set(Sup, false); }
        else if (value == L"baseline") { set(Sup, false); set(Sub, false); }
      }
    }
    if (separator < 0) break;
    start = separator + 1;
  }
}

bool SafeHref(const CString& value) {
  CString href = Lower(value); href.Trim();
  if (href.IsEmpty()) return false;
  for (int i = 0; i < href.GetLength(); ++i)
    if (href[i] < 0x20 || href[i] == 0x7f) return false;
  return href[0] == L'#' || href.Left(7) == L"http://" ||
    href.Left(8) == L"https://" || href.Left(7) == L"mailto:";
}

bool FbeStructureClass(const CString& value) {
  CString lower(Lower(value));
  int start = 0;
  while (start < lower.GetLength()) {
    while (start < lower.GetLength() && Space(lower[start])) ++start;
    int end = start;
    while (end < lower.GetLength() && !Space(lower[end])) ++end;
    const CString token(lower.Mid(start, end - start));
    if (token == L"section" || token == L"title" || token == L"subtitle" ||
        token == L"annotation" || token == L"image" || token == L"table" ||
        token == L"note" || token == L"code" || token == L"stanza" ||
        token == L"poem" || token == L"cite" || token == L"epigraph") return true;
    start = end;
  }
  return false;
}

bool HasClass(const CString& value, const wchar_t* wanted) {
  CString lower(Lower(value));
  int start = 0;
  while (start < lower.GetLength()) {
    while (start < lower.GetLength() && Space(lower[start])) ++start;
    int end = start;
    while (end < lower.GetLength() && !Space(lower[end])) ++end;
    if (lower.Mid(start, end - start) == wanted) return true;
    start = end;
  }
  return false;
}

bool InternalTag(const CString& tag) {
  return tag == L"div" || tag == L"p" || tag == L"span" || tag == L"a" ||
    tag == L"img" || tag == L"br" || tag == L"strong" || tag == L"em" ||
    tag == L"b" || tag == L"i" || tag == L"s" || tag == L"strike" ||
    tag == L"sub" || tag == L"sup" || tag == L"table" || tag == L"tbody" || tag == L"tr" ||
    tag == L"td" || tag == L"th" || tag == L"ul" || tag == L"ol" ||
    tag == L"li" || tag == L"hr";
}

bool SafeInternalAttribute(const CString& tag, const CString& attr, const CString& value) {
  if (attr.Left(2) == L"on")
    return attr == L"onresizestart" && (tag == L"div" || tag == L"span") && Lower(value) == L"return false";
  if (attr == L"contenteditable") return (tag == L"div" || tag == L"span") && Lower(value) == L"false";
  if (attr == L"src") return tag == L"img" && Lower(value).Left(13) == L"fbw-internal:";
  if (attr == L"href") return tag == L"a" ? SafeHref(value) :
    (tag == L"div" || tag == L"span") && !value.IsEmpty() && value[0] == L'#';
  if (attr == L"style") {
    const CString style(Lower(value));
    return style.Find(L"url") < 0 && style.Find(L"expression") < 0 &&
      style.Find(L"@import") < 0 && style.Find(L"behavior") < 0 &&
      style.Find(L"binding") < 0 && style.Find(L"filter") < 0 &&
      style.Find(L"progid") < 0;
  }
  return attr != L"srcset" && attr != L"background" && attr != L"action" &&
    attr != L"data" && attr != L"dynsrc" && attr != L"lowsrc" &&
    attr != L"poster" && attr != L"formaction";
}

HtmlPasteOutcome Parse(const CString& input, std::vector<Node>& nodes, bool internal = false) {
  nodes.emplace_back();
  std::vector<size_t> stack(1, 0);
  for (int i = 0; i < input.GetLength();) {
    if (nodes.size() >= kMaxNodes) return HtmlPasteOutcome::PlainText;
    if (input[i] != L'<') {
      int end = input.Find(L'<', i);
      if (end < 0) end = input.GetLength();
      Node node;
      if (!Decode(input.Mid(i, end - i), node.text)) return HtmlPasteOutcome::PlainText;
      nodes[stack.back()].children.push_back(nodes.size()); nodes.push_back(std::move(node));
      i = end; continue;
    }
    if (input.Mid(i, 4) == L"<!--") {
      const int end = input.Find(L"-->", i + 4);
      if (end < 0) return HtmlPasteOutcome::PlainText;
      i = end + 3; continue;
    }
    int end = i + 1;
    wchar_t quote = 0;
    for (; end < input.GetLength(); ++end) {
      const wchar_t c = input[end];
      if (quote) { if (c == quote) quote = 0; }
      else if (c == L'\'' || c == L'"') quote = c;
      else if (c == L'>') break;
    }
    if (end == input.GetLength()) return HtmlPasteOutcome::PlainText;
    CString content = input.Mid(i + 1, end - i - 1);
    i = end + 1;
    int p = 0; while (p < content.GetLength() && Space(content[p])) ++p;
    if (p == content.GetLength()) return HtmlPasteOutcome::PlainText;
    if (content[p] == L'!' || content[p] == L'?') continue;
    const bool closing = content[p] == L'/';
    if (closing) ++p;
    const int nameStart = p;
    while (p < content.GetLength() && NameChar(content[p])) ++p;
    if (p == nameStart) return HtmlPasteOutcome::PlainText;
    const CString tag = Lower(content.Mid(nameStart, p - nameStart));
    if (internal && !InternalTag(tag)) return HtmlPasteOutcome::PlainText;
    if (closing) {
      for (size_t s = stack.size(); s > 1; --s) {
        if (nodes[stack[s - 1]].tag == tag) { stack.resize(s - 1); break; }
      }
      continue;
    }
    if (!internal && Complex(tag)) return HtmlPasteOutcome::Legacy;
    const bool selfClosing = Void(tag) || (content.GetLength() && content[content.GetLength() - 1] == L'/');
    if (Discard(tag)) {
      if (internal) return HtmlPasteOutcome::PlainText;
      if (!selfClosing) {
        const CString ending = L"</" + tag;
        bool found = false;
        for (int scan = i; scan + ending.GetLength() < input.GetLength(); ++scan) {
          if (_wcsnicmp(static_cast<LPCWSTR>(input) + scan, ending, ending.GetLength()) != 0) continue;
          const wchar_t next = input[scan + ending.GetLength()];
          if (!Space(next) && next != L'>') continue;
          const int close = input.Find(L'>', scan + ending.GetLength());
          if (close < 0) return HtmlPasteOutcome::PlainText;
          i = close + 1; found = true; break;
        }
        if (!found) return HtmlPasteOutcome::PlainText;
      }
      continue;
    }
    Node node; node.tag = tag;
    if (tag == L"b" || tag == L"strong") node.formatOn |= Strong;
    if (tag == L"i" || tag == L"em") node.formatOn |= Em;
    if (tag == L"s" || tag == L"strike" || tag == L"del") node.formatOn |= Strike;
    if (tag == L"sup") { node.formatOn |= Sup; node.formatOff |= Sub; }
    if (tag == L"sub") { node.formatOn |= Sub; node.formatOff |= Sup; }
    while (p < content.GetLength()) {
      while (p < content.GetLength() && (Space(content[p]) || content[p] == L'/')) ++p;
      const int attrStart = p;
      while (p < content.GetLength() && NameChar(content[p])) ++p;
      if (p == attrStart) { if (p < content.GetLength()) ++p; continue; }
      const CString attr = Lower(content.Mid(attrStart, p - attrStart));
      while (p < content.GetLength() && Space(content[p])) ++p;
      CString value;
      if (p < content.GetLength() && content[p] == L'=') {
        ++p; while (p < content.GetLength() && Space(content[p])) ++p;
        wchar_t q = 0;
        if (p < content.GetLength() && (content[p] == L'\'' || content[p] == L'"')) q = content[p++];
        const int valueStart = p;
        while (p < content.GetLength() && (q ? content[p] != q : !Space(content[p]) && content[p] != L'/')) ++p;
        if (q && p == content.GetLength()) return HtmlPasteOutcome::PlainText;
        if (!Decode(content.Mid(valueStart, p - valueStart), value)) return HtmlPasteOutcome::PlainText;
        if (q) ++p;
      }
      if (attr == L"style") StyleFormat(value, node.formatOn, node.formatOff);
      else if (attr == L"href" && ((tag == L"a" && SafeHref(value)) ||
          (internal && (tag == L"div" || tag == L"span")))) node.href = value;
      if (internal && attr == L"src" && tag == L"img") node.src = value;
      if (internal && attr == L"id") node.id = value;
      if (internal && attr == L"class") node.cssClass = value;
      if (internal && !SafeInternalAttribute(tag, attr, value)) return HtmlPasteOutcome::PlainText;
      if (!internal && ((attr == L"class" && FbeStructureClass(value)) ||
          (attr == L"href" && !SafeHref(value)))) return HtmlPasteOutcome::Legacy;
    }
    nodes[stack.back()].children.push_back(nodes.size()); nodes.push_back(std::move(node));
    if (!selfClosing) {
      if (stack.size() >= kMaxDepth) return HtmlPasteOutcome::PlainText;
      stack.push_back(nodes.size() - 1);
    }
  }
  return HtmlPasteOutcome::Sanitized;
}

const wchar_t* FormatTag(unsigned bit) {
  switch (bit) { case Strong: return L"strong"; case Em: return L"em";
    case Strike: return L"strike"; case Sup: return L"sup"; default: return L"sub"; }
}

struct InlineToken { CString text; CString href; unsigned format; };
struct OpenTag { CString open; CString close; };

unsigned ApplyFormat(unsigned inherited, const Node& node) {
  return (inherited | node.formatOn) & ~node.formatOff;
}

void CollectInline(const std::vector<Node>& nodes, size_t index,
                   const CString& nbspReplacement, unsigned inherited,
                   const CString& inheritedHref, std::vector<InlineToken>& tokens) {
  const Node& node = nodes[index];
  if (node.tag.IsEmpty()) {
    tokens.push_back({Escape(node.text, nbspReplacement), inheritedHref, inherited});
    return;
  }
  const unsigned format = ApplyFormat(inherited, node);
  const CString href = node.href.IsEmpty() ? inheritedHref : node.href;
  if (node.tag == L"br" || node.tag == L"w:br") {
    tokens.push_back({L"<br>", href, format}); return;
  }
  if (node.tag == L"meta" || node.tag == L"link" || node.tag == L"hr" || node.tag == L"input") return;
  for (size_t child : node.children) CollectInline(nodes, child, nbspReplacement, format, href, tokens);
}

CString Inline(const std::vector<InlineToken>& tokens) {
  CString out;
  std::vector<OpenTag> active;
  const unsigned bits[] = {Strong, Em, Strike, Sup, Sub};
  for (const InlineToken& token : tokens) {
    if (token.text.IsEmpty()) continue;
    std::vector<OpenTag> wanted;
    for (unsigned bit : bits) if (token.format & bit) {
      const CString name(FormatTag(bit));
      wanted.push_back({L"<" + name + L">", L"</" + name + L">"});
    }
    if (!token.href.IsEmpty())
      wanted.push_back({L"<a href=\"" + Escape(token.href, L"\u00a0") + L"\">", L"</a>"});
    size_t common = 0;
    while (common < active.size() && common < wanted.size() && active[common].open == wanted[common].open) ++common;
    for (size_t i = active.size(); i > common; --i) out += active[i - 1].close;
    for (size_t i = common; i < wanted.size(); ++i) out += wanted[i].open;
    active.swap(wanted);
    out += token.text;
  }
  for (size_t i = active.size(); i > 0; --i) out += active[i - 1].close;
  return out;
}

bool ContainsBlock(const std::vector<Node>& nodes, size_t index) {
  if (Block(nodes[index].tag)) return true;
  for (size_t child : nodes[index].children) if (ContainsBlock(nodes, child)) return true;
  return false;
}

bool HasVisibleContent(const std::vector<InlineToken>& tokens) {
  for (const InlineToken& token : tokens) {
    if (token.text == L"<br>") return true;
    CString trimmed(token.text); trimmed.Trim();
    if (!trimmed.IsEmpty()) return true;
  }
  return false;
}

void Render(const std::vector<Node>& nodes, size_t index,
            const CString& nbspReplacement, unsigned inherited,
            const CString& inheritedHref, CString& out) {
  const Node& node = nodes[index];
  const unsigned format = ApplyFormat(inherited, node);
  const CString href = node.href.IsEmpty() ? inheritedHref : node.href;
  bool hasBlockDescendant = false;
  for (size_t child : node.children) if (ContainsBlock(nodes, child)) { hasBlockDescendant = true; break; }
  if (Block(node.tag) && !hasBlockDescendant) {
    std::vector<InlineToken> tokens;
    for (size_t child : node.children) CollectInline(nodes, child, nbspReplacement, format, href, tokens);
    out += L"<p>" + Inline(tokens) + L"</p>";
    return;
  }
  std::vector<InlineToken> inlineRun;
  for (size_t child : node.children) {
    if (ContainsBlock(nodes, child)) {
      if (HasVisibleContent(inlineRun)) out += L"<p>" + Inline(inlineRun) + L"</p>";
      inlineRun.clear();
      Render(nodes, child, nbspReplacement, format, href, out);
    } else CollectInline(nodes, child, nbspReplacement, format, href, inlineRun);
  }
  if (HasVisibleContent(inlineRun)) {
    if (index == 0 && out.IsEmpty()) out += Inline(inlineRun);
    else out += L"<p>" + Inline(inlineRun) + L"</p>";
  }
}

} // namespace

bool HtmlPasteSanitizer::MarkInternalCopy(HWND owner, const GUID& origin) {
  if (::IsEqualGUID(origin, GUID_NULL)) return false;
  const UINT html = ::RegisterClipboardFormat(L"HTML Format");
  const UINT marker = ::RegisterClipboardFormat(kInternalClipboardFormat);
  if (!html || !marker || !::IsClipboardFormatAvailable(html)) return false;
  const HWND clipboardOwner = ::GetClipboardOwner();
  DWORD ownerProcess = 0;
  if (clipboardOwner) ::GetWindowThreadProcessId(clipboardOwner, &ownerProcess);
  if (ownerProcess == ::GetCurrentProcessId()) {
    ClipboardScope clipboard(owner);
    if (!clipboard.Open() || ::GetClipboardOwner() != clipboardOwner) return false;
    HGLOBAL data = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(InternalMarkerPayload));
    if (!data) return false;
    void* bytes = ::GlobalLock(data);
    if (!bytes) { ::GlobalFree(data); return false; }
    const InternalMarkerPayload payload = MarkerPayload(origin);
    memcpy(bytes, &payload, sizeof(payload));
    ::GlobalUnlock(data);
    if (!::SetClipboardData(marker, data)) { ::GlobalFree(data); return false; }
    return true;
  }
  CString renderedFragment;
  if (!ReadClipboardFragment(owner, renderedFragment)) return false;
  CComPtr<IDataObject> original;
  if (FAILED(::OleGetClipboard(&original)) || !original) return false;
  const DWORD sequence = ::GetClipboardSequenceNumber();
  CComPtr<IDataObject> marked;
  marked.Attach(new (std::nothrow) MarkedDataObject(original, static_cast<CLIPFORMAT>(marker), origin));
  if (!marked || sequence != ::GetClipboardSequenceNumber()) return false;
  return SUCCEEDED(::OleSetClipboard(marked));
}

bool HtmlPasteSanitizer::GetInternalCopyOrigin(HWND owner, GUID& origin) {
  origin = GUID_NULL;
  const UINT marker = ::RegisterClipboardFormat(kInternalClipboardFormat);
  const UINT html = ::RegisterClipboardFormat(L"HTML Format");
  if (!marker || !html) return false;
  for (int attempt = 0; attempt < 4; ++attempt) {
    if (!::IsClipboardFormatAvailable(marker) || !::IsClipboardFormatAvailable(html)) return false;
    ClipboardScope clipboard(owner);
    if (clipboard.Open()) {
      HGLOBAL data = static_cast<HGLOBAL>(::GetClipboardData(marker));
      if (!data || ::GlobalSize(data) != sizeof(InternalMarkerPayload)) return false;
      const void* value = ::GlobalLock(data);
      if (!value) return false;
      const InternalMarkerPayload* payload = static_cast<const InternalMarkerPayload*>(value);
      const bool valid = memcmp(payload->signature, kInternalClipboardMarker, sizeof(kInternalClipboardMarker)) == 0 &&
        !::IsEqualGUID(payload->origin, GUID_NULL);
      if (valid) origin = payload->origin;
      ::GlobalUnlock(data);
      return valid;
    }
    ::Sleep(1);
  }
  return false;
}

bool HtmlPasteSanitizer::HasInternalCopyMarker(HWND owner) {
  GUID origin = GUID_NULL;
  return GetInternalCopyOrigin(owner, origin);
}

bool HtmlPasteSanitizer::IsSafeInternalFragment(const CString& html, bool sameDocument,
    const std::function<bool(const CString&, InternalReferenceKind)>& referenceExists,
    const std::function<bool(const CString&)>& idExists) {
  if (html.IsEmpty() || html.GetLength() > static_cast<int>(kMaxFragmentBytes)) return false;
  std::vector<Node> nodes;
  if (Parse(html, nodes, true) != HtmlPasteOutcome::Sanitized) return false;
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].id.IsEmpty()) continue;
    if (idExists && idExists(nodes[i].id)) return false;
    for (size_t j = 0; j < i; ++j)
      if (nodes[j].id == nodes[i].id) return false;
  }
  for (const Node& node : nodes) {
    if (!node.href.IsEmpty() && node.href[0] == L'#') {
      const CString id(node.href.Mid(1));
      const InternalReferenceKind kind = node.tag == L"a" ?
        (HasClass(node.cssClass, L"note") ? InternalReferenceKind::Note : InternalReferenceKind::Link) :
        InternalReferenceKind::Image;
      if (id.IsEmpty()) return false;
      const Node* localTarget = nullptr;
      for (const Node& target : nodes) if (target.id == id) { localTarget = &target; break; }
      if (localTarget && kind != InternalReferenceKind::Image) {
        // The fragment does not prove that a copied section belongs to a notes body.
        if (kind == InternalReferenceKind::Note) return false;
      } else if (!sameDocument || (referenceExists && !referenceExists(id, kind))) return false;
    }
    if (!node.src.IsEmpty()) {
      const CString prefix(L"fbw-internal:#");
      if (node.src.Left(prefix.GetLength()).CompareNoCase(prefix) != 0 ||
          !sameDocument || (referenceExists && !referenceExists(node.src.Mid(prefix.GetLength()), InternalReferenceKind::Image))) return false;
    }
  }
  return true;
}

CString HtmlPasteSanitizer::NormalizeInternalLinks(const CString& html, const CString& documentUrl) {
  CString base(documentUrl);
  const int hash = base.Find(L'#');
  if (hash >= 0) base = base.Left(hash);
  if (base.Left(8).CompareNoCase(L"file:///") != 0 ||
      base.Right(10).CompareNoCase(L"/main.html") != 0) return html;

  CString result;
  int copied = 0;
  for (int i = 0; i < html.GetLength();) {
    if (html[i] != L'<') { ++i; continue; }
    if (html.Mid(i, 4) == L"<!--") {
      const int end = html.Find(L"-->", i + 4);
      if (end < 0) break;
      i = end + 3;
      continue;
    }
    int p = i + 1;
    if (p >= html.GetLength() ||
        !((html[p] >= L'A' && html[p] <= L'Z') || (html[p] >= L'a' && html[p] <= L'z'))) { ++i; continue; }
    const int nameStart = p;
    while (p < html.GetLength() && NameChar(html[p])) ++p;
    const CString tag(Lower(html.Mid(nameStart, p - nameStart)));
    int end = p;
    wchar_t quote = 0;
    for (; end < html.GetLength(); ++end) {
      if (quote) { if (html[end] == quote) quote = 0; }
      else if (html[end] == L'\'' || html[end] == L'"') quote = html[end];
      else if (html[end] == L'>') break;
    }
    if (end >= html.GetLength()) break;
    if (tag == L"script" || tag == L"style" || tag == L"textarea") {
      const CString closing(L"</" + tag);
      const int close = Lower(html).Find(closing, end + 1);
      if (close < 0) break;
      i = close;
      continue;
    }
    while (p < end) {
      while (p < end && Space(html[p])) ++p;
      const int attrStart = p;
      while (p < end && NameChar(html[p])) ++p;
      if (p == attrStart) { ++p; continue; }
      const CString attr(Lower(html.Mid(attrStart, p - attrStart)));
      while (p < end && Space(html[p])) ++p;
      if (p >= end || html[p] != L'=') continue;
      ++p;
      while (p < end && Space(html[p])) ++p;
      const wchar_t valueQuote = p < end && (html[p] == L'\'' || html[p] == L'"') ? html[p++] : 0;
      const int valueStart = p;
      while (p < end && (valueQuote ? html[p] != valueQuote : !Space(html[p]))) ++p;
      const int valueEnd = p;
      if (valueQuote && p < end) ++p;
      if (tag != L"a" || attr != L"href") continue;
      const CString value(html.Mid(valueStart, valueEnd - valueStart));
      if (value.GetLength() <= base.GetLength() ||
          value.Left(base.GetLength()).CompareNoCase(base) != 0 ||
          value[base.GetLength()] != L'#') continue;
      const CString fragment(value.Mid(base.GetLength()));
      if (!SafeHref(fragment) || fragment.GetLength() < 2) continue;
      result += html.Mid(copied, valueStart - copied);
      result += fragment;
      copied = valueEnd;
    }
    i = end + 1;
  }
  result += html.Mid(copied);
  return result;
}
bool HtmlPasteSanitizer::ReadClipboardFragment(HWND owner, CString& html) {
  html.Empty();
  const UINT format = ::RegisterClipboardFormat(L"HTML Format");
  if (!format || !::IsClipboardFormatAvailable(format)) return false;
  ClipboardScope clipboard(owner);
  if (!clipboard.Open()) return false;
  HGLOBAL data = static_cast<HGLOBAL>(::GetClipboardData(format));
  return DecodeClipboardFragment(data, html);
}
bool HtmlPasteSanitizer::DecodeClipboardFragment(HGLOBAL data, CString& html, bool strict) {
  html.Empty();
  const SIZE_T size = data ? ::GlobalSize(data) : 0;
  if (!size || size > kMaxFragmentBytes) return false;
  const char* bytes = static_cast<const char*>(::GlobalLock(data));
  if (!bytes) return false;
  size_t start = 0, end = 0;
  bool offsets = Decimal(bytes, size, "StartFragment:", start) &&
                       Decimal(bytes, size, "EndFragment:", end) &&
                       (strict ? start <= end : start < end) && end <= size;
  if (strict && offsets) {
    size_t htmlStart = 0, htmlEnd = 0;
    offsets = Decimal(bytes, start, "StartHTML:", htmlStart, size) && Decimal(bytes, start, "EndHTML:", htmlEnd, size) &&
      htmlStart <= start && end <= htmlEnd && htmlEnd <= size && htmlStart > 0;
  }
  if (offsets) {
    const int count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
      bytes + start, static_cast<int>(end - start), NULL, 0);
    if (count) {
      wchar_t* target = html.GetBuffer(count);
      ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes + start,
                            static_cast<int>(end - start), target, count);
      html.ReleaseBuffer(count);
    }
  }
  ::GlobalUnlock(data);
  return offsets && (!html.IsEmpty() || (strict && start == end));
}

HtmlPasteOutcome HtmlPasteSanitizer::Sanitize(const CString& input,
                                  const CString& nbspReplacement, CString& output) {
  output.Empty();
  if (input.IsEmpty() || input.GetLength() > static_cast<int>(kMaxFragmentBytes)) return HtmlPasteOutcome::PlainText;
  std::vector<Node> nodes;
  const HtmlPasteOutcome parsed = Parse(input, nodes);
  if (parsed != HtmlPasteOutcome::Sanitized) return parsed;
  Render(nodes, 0, nbspReplacement, 0, CString(), output);
  return output.IsEmpty() ? HtmlPasteOutcome::PlainText : HtmlPasteOutcome::Sanitized;
}

bool HtmlPasteSanitizer::ReadPlainText(HWND owner, CString& text) {
  text.Empty();
  if (!::IsClipboardFormatAvailable(CF_UNICODETEXT)) return false;
  ClipboardScope clipboard(owner);
  if (!clipboard.Open()) return false;
  HGLOBAL data = static_cast<HGLOBAL>(::GetClipboardData(CF_UNICODETEXT));
  const SIZE_T size = data ? ::GlobalSize(data) : 0;
  if (!size || size > kMaxFragmentBytes * sizeof(wchar_t)) return false;
  const wchar_t* value = static_cast<const wchar_t*>(::GlobalLock(data));
  if (!value) return false;
  const size_t length = wcsnlen_s(value, size / sizeof(wchar_t));
  if (length < size / sizeof(wchar_t)) text.SetString(value, static_cast<int>(length));
  ::GlobalUnlock(data);
  return length < size / sizeof(wchar_t);
}

CString HtmlPasteSanitizer::PlainTextToHtml(const CString& text,
                                            const CString& nbspReplacement,
                                            bool insideParagraph) {
  if (insideParagraph && text.FindOneOf(L"\r\n") < 0)
    return Escape(text, nbspReplacement);
  CString html(L"<p>");
  for (int i = 0; i < text.GetLength(); ++i) {
    const wchar_t c = text[i];
    if (c == L'\r' || c == L'\n') {
      if (c == L'\r' && i + 1 < text.GetLength() && text[i + 1] == L'\n') ++i;
      html += L"</p><p>";
    } else html += Escape(text.Mid(i, 1), nbspReplacement);
  }
  html += L"</p>";
  return html;
}

} // namespace FbeClipboard
