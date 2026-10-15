#include "stdafx.h"
#include "HtmlPasteSanitizer.h"

#include <algorithm>
#include <string>
#include <vector>

namespace FbeClipboard {
namespace {

constexpr SIZE_T kMaxFragmentBytes = 2 * 1024 * 1024;
constexpr int kMaxDepth = 128;
constexpr size_t kMaxNodes = 100000;
constexpr wchar_t kInternalClipboardMarker[] = L"FBE Next internal HTML fragment v1";
constexpr wchar_t kInternalClipboardFormat[] = L"FBE Next Internal HTML v1";

class ClipboardScope {
public:
  explicit ClipboardScope(HWND owner) : open_(::OpenClipboard(owner) != FALSE) {}
  ~ClipboardScope() { if (open_) ::CloseClipboard(); }
  bool Open() const { return open_; }
private:
  bool open_;
};

struct Node {
  CString tag;
  CString text;
  CString href;
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

bool Decimal(const char* bytes, size_t length, const char* key, size_t& value) {
  const size_t keyLength = strlen(key);
  for (size_t i = 0; i + keyLength < length; ++i) {
    if (_strnicmp(bytes + i, key, keyLength) != 0) continue;
    size_t p = i + keyLength;
    while (p < length && bytes[p] == ' ') ++p;
    if (p == length || bytes[p] < '0' || bytes[p] > '9') return false;
    size_t result = 0;
    while (p < length && bytes[p] >= '0' && bytes[p] <= '9') {
      const unsigned digit = bytes[p++] - '0';
      if (result > (length - digit) / 10) return false;
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
    tag == L"ol" || tag == L"ul" || tag == L"li";
}
bool Discard(const CString& tag) {
  return tag == L"script" || tag == L"style" || tag == L"head" ||
    tag == L"template" || tag == L"noscript" || tag == L"iframe" ||
    tag == L"object" || tag == L"embed" || tag == L"svg" ||
    tag == L"math" || tag == L"video" || tag == L"audio";
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

bool InternalTag(const CString& tag) {
  return tag == L"div" || tag == L"p" || tag == L"span" || tag == L"a" ||
    tag == L"img" || tag == L"br" || tag == L"strong" || tag == L"em" ||
    tag == L"b" || tag == L"i" || tag == L"s" || tag == L"strike" ||
    tag == L"sub" || tag == L"sup" || tag == L"table" || tag == L"tr" ||
    tag == L"td" || tag == L"th" || tag == L"ul" || tag == L"ol" ||
    tag == L"li" || tag == L"hr";
}

bool SafeInternalAttribute(const CString& tag, const CString& attr, const CString& value) {
  if (attr.Left(2) == L"on")
    return attr == L"onresizestart" && tag == L"div" && Lower(value) == L"return false";
  if (attr == L"src") return tag == L"img" && Lower(value).Left(13) == L"fbw-internal:";
  if (attr == L"href") return tag == L"a" ? SafeHref(value) :
    tag == L"div" && !value.IsEmpty() && value[0] == L'#';
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
      else if (attr == L"href" && tag == L"a" && SafeHref(value)) node.href = value;
      if (internal && !SafeInternalAttribute(tag, attr, value)) return HtmlPasteOutcome::PlainText;
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

bool HtmlPasteSanitizer::MarkInternalCopy(HWND owner) {
  const UINT html = ::RegisterClipboardFormat(L"HTML Format");
  const UINT marker = ::RegisterClipboardFormat(kInternalClipboardFormat);
  if (!html || !marker || !::IsClipboardFormatAvailable(html)) return false;
  ClipboardScope clipboard(owner);
  if (!clipboard.Open()) return false;
  HGLOBAL data = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(kInternalClipboardMarker));
  if (!data) return false;
  void* target = ::GlobalLock(data);
  if (!target) { ::GlobalFree(data); return false; }
  memcpy(target, kInternalClipboardMarker, sizeof(kInternalClipboardMarker));
  ::GlobalUnlock(data);
  if (!::SetClipboardData(marker, data)) { ::GlobalFree(data); return false; }
  return true;
}

bool HtmlPasteSanitizer::HasInternalCopyMarker(HWND owner) {
  const UINT marker = ::RegisterClipboardFormat(kInternalClipboardFormat);
  const UINT html = ::RegisterClipboardFormat(L"HTML Format");
  if (!marker || !html || !::IsClipboardFormatAvailable(marker) || !::IsClipboardFormatAvailable(html)) return false;
  ClipboardScope clipboard(owner);
  if (!clipboard.Open()) return false;
  HGLOBAL data = static_cast<HGLOBAL>(::GetClipboardData(marker));
  if (!data || ::GlobalSize(data) != sizeof(kInternalClipboardMarker)) return false;
  const void* value = ::GlobalLock(data);
  if (!value) return false;
  const bool valid = memcmp(value, kInternalClipboardMarker, sizeof(kInternalClipboardMarker)) == 0;
  ::GlobalUnlock(data);
  return valid;
}

bool HtmlPasteSanitizer::IsSafeInternalFragment(const CString& html) {
  if (html.IsEmpty() || html.GetLength() > static_cast<int>(kMaxFragmentBytes)) return false;
  std::vector<Node> nodes;
  return Parse(html, nodes, true) == HtmlPasteOutcome::Sanitized;
}

bool HtmlPasteSanitizer::ReadClipboardFragment(HWND owner, CString& html) {
  html.Empty();
  const UINT format = ::RegisterClipboardFormat(L"HTML Format");
  if (!format || !::IsClipboardFormatAvailable(format)) return false;
  ClipboardScope clipboard(owner);
  if (!clipboard.Open()) return false;
  HGLOBAL data = static_cast<HGLOBAL>(::GetClipboardData(format));
  const SIZE_T size = data ? ::GlobalSize(data) : 0;
  if (!size || size > kMaxFragmentBytes) return false;
  const char* bytes = static_cast<const char*>(::GlobalLock(data));
  if (!bytes) return false;
  size_t start = 0, end = 0;
  const bool offsets = Decimal(bytes, size, "StartFragment:", start) &&
                       Decimal(bytes, size, "EndFragment:", end) &&
                       start < end && end <= size;
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
  return offsets && !html.IsEmpty();
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
