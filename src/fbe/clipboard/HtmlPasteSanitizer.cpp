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
  unsigned format = 0;
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
  for (int i = 0; i < input.GetLength(); ++i) {
    if (input[i] != L'&') { output.AppendChar(input[i]); continue; }
    const int end = input.Find(L';', i + 1);
    if (end < 0 || end - i > 32) return false;
    const CString entity = input.Mid(i + 1, end - i - 1);
    unsigned codepoint = 0;
    if (entity == L"amp") codepoint = L'&';
    else if (entity == L"lt") codepoint = L'<';
    else if (entity == L"gt") codepoint = L'>';
    else if (entity == L"quot") codepoint = L'"';
    else if (entity == L"apos") codepoint = L'\'';
    else if (entity == L"nbsp") codepoint = 0xa0;
    else if (entity == L"ndash") codepoint = 0x2013;
    else if (entity == L"mdash") codepoint = 0x2014;
    else if (entity == L"lsquo") codepoint = 0x2018;
    else if (entity == L"rsquo") codepoint = 0x2019;
    else if (entity == L"ldquo") codepoint = 0x201c;
    else if (entity == L"rdquo") codepoint = 0x201d;
    else if (entity == L"hellip") codepoint = 0x2026;
    else if (entity == L"bull") codepoint = 0x2022;
    else if (entity.GetLength() > 1 && entity[0] == L'#') {
      const bool hex = entity.GetLength() > 2 && (entity[1] == L'x' || entity[1] == L'X');
      const int start = hex ? 2 : 1;
      if (start == entity.GetLength()) return false;
      for (int j = start; j < entity.GetLength(); ++j) {
        const wchar_t c = entity[j];
        const int digit = c >= L'0' && c <= L'9' ? c - L'0' :
          hex && c >= L'a' && c <= L'f' ? c - L'a' + 10 :
          hex && c >= L'A' && c <= L'F' ? c - L'A' + 10 : -1;
        if (digit < 0 || codepoint > 0x10ffff / (hex ? 16u : 10u)) return false;
        codepoint = codepoint * (hex ? 16u : 10u) + digit;
      }
    } else return false; // Unknown entities use the plain-text clipboard fallback.
    if (!AppendCodepoint(codepoint, output)) return false;
    i = end;
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

unsigned StyleFormat(const CString& style) {
  unsigned result = 0;
  int start = 0;
  while (start < style.GetLength()) {
    const int separator = style.Find(L';', start);
    const CString item = style.Mid(start, (separator < 0 ? style.GetLength() : separator) - start);
    const int colon = item.Find(L':');
    if (colon > 0) {
      CString key = Lower(item.Left(colon)); key.Trim();
      CString value = Lower(item.Mid(colon + 1)); value.Trim();
      if (key == L"font-weight" && (value == L"bold" || value == L"bolder" ||
          (value.GetLength() == 3 && value[0] >= L'6' && value[0] <= L'9' && value.Mid(1) == L"00"))) result |= Strong;
      if (key == L"font-style" && (value == L"italic" || value == L"oblique")) result |= Em;
      if (key == L"text-decoration" && value.Find(L"line-through") >= 0) result |= Strike;
      if (key == L"vertical-align" && value == L"super") result |= Sup;
      if (key == L"vertical-align" && value == L"sub") result |= Sub;
    }
    if (separator < 0) break;
    start = separator + 1;
  }
  return result;
}

bool SafeHref(const CString& value) {
  CString href = Lower(value); href.Trim();
  if (href.IsEmpty()) return false;
  for (int i = 0; i < href.GetLength(); ++i)
    if (href[i] < 0x20 || href[i] == 0x7f) return false;
  return href[0] == L'#' || href.Left(7) == L"http://" ||
    href.Left(8) == L"https://" || href.Left(7) == L"mailto:";
}

HtmlPasteOutcome Parse(const CString& input, std::vector<Node>& nodes) {
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
    if (closing) {
      for (size_t s = stack.size(); s > 1; --s) {
        if (nodes[stack[s - 1]].tag == tag) { stack.resize(s - 1); break; }
      }
      continue;
    }
    if (Complex(tag)) return HtmlPasteOutcome::Legacy;
    const bool selfClosing = Void(tag) || (content.GetLength() && content[content.GetLength() - 1] == L'/');
    if (Discard(tag)) {
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
    if (tag == L"b" || tag == L"strong") node.format |= Strong;
    if (tag == L"i" || tag == L"em") node.format |= Em;
    if (tag == L"s" || tag == L"strike" || tag == L"del") node.format |= Strike;
    if (tag == L"sup") node.format |= Sup;
    if (tag == L"sub") node.format |= Sub;
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
      if (attr == L"style") node.format |= StyleFormat(value);
      else if (attr == L"href" && tag == L"a" && SafeHref(value)) node.href = value;
      else if ((attr == L"id" && Lower(value) == L"fbw_body") ||
          (attr == L"class" && (Lower(value) == L"section" || Lower(value) == L"title" || Lower(value) == L"annotation" || Lower(value) == L"image")) ||
          (attr == L"href" && Lower(value).Find(L"fbw-internal:") >= 0)) return HtmlPasteOutcome::Legacy;
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

CString Inline(const std::vector<Node>& nodes, size_t index,
               const CString& nbspReplacement, unsigned active) {
  const Node& node = nodes[index];
  if (node.tag.IsEmpty()) return Escape(node.text, nbspReplacement);
  if (node.tag == L"br" || node.tag == L"w:br") return L"<br>";
  if (node.tag == L"meta" || node.tag == L"link" || node.tag == L"hr" || node.tag == L"input") return CString();
  CString body;
  unsigned own = node.format & ~active;
  if ((own & Sup) && (active & Sub)) own &= ~Sup;
  if ((own & Sub) && (active & Sup)) own &= ~Sub;
  for (size_t child : node.children) body += Inline(nodes, child, nbspReplacement, active | own);
  if (body.IsEmpty()) return body;
  if (node.tag == L"a" && !node.href.IsEmpty())
    body = L"<a href=\"" + Escape(node.href, L"\u00a0") + L"\">" + body + L"</a>";
  const unsigned bits[] = {Sub, Sup, Strike, Em, Strong};
  for (unsigned bit : bits) if (own & bit) body = CString(L"<") + FormatTag(bit) + L">" + body + L"</" + FormatTag(bit) + L">";
  return body;
}

bool HasBlockChild(const std::vector<Node>& nodes, size_t index) {
  for (size_t child : nodes[index].children) if (Block(nodes[child].tag)) return true;
  return false;
}

bool ContainsBlock(const std::vector<Node>& nodes, size_t index) {
  if (Block(nodes[index].tag)) return true;
  for (size_t child : nodes[index].children) if (ContainsBlock(nodes, child)) return true;
  return false;
}

bool HasVisibleContent(const CString& value) {
  CString trimmed(value); trimmed.Trim(); return !trimmed.IsEmpty();
}

void Render(const std::vector<Node>& nodes, size_t index,
            const CString& nbspReplacement, CString& out) {
  const Node& node = nodes[index];
  if (Block(node.tag) && !HasBlockChild(nodes, index)) {
    out += L"<p>" + Inline(nodes, index, nbspReplacement, 0) + L"</p>";
    return;
  }
  CString inlineRun;
  for (size_t child : node.children) {
    if (ContainsBlock(nodes, child)) {
      if (HasVisibleContent(inlineRun)) out += L"<p>" + inlineRun + L"</p>";
      inlineRun.Empty();
      Render(nodes, child, nbspReplacement, out);
    } else inlineRun += Inline(nodes, child, nbspReplacement, 0);
  }
  if (HasVisibleContent(inlineRun)) {
    if (index == 0 && out.IsEmpty()) out += inlineRun;
    else out += L"<p>" + inlineRun + L"</p>";
  }
}

} // namespace

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
  Render(nodes, 0, nbspReplacement, output);
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
                                            const CString& nbspReplacement) {
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
