#pragma once

#include "BodySourceSelectionTransfer.h"
#include <string_view>
#include <algorithm>

namespace FBEBodySourceTransfer {
// Transient element-only index into the existing serialized buffer. Neither
// document text nor a second DOM is retained by the view session.
class XmlSelectionPosition {
public:
 struct Element {
  int parent = -1;
  int start = 0, content = 0, close = 0, end = 0;
  std::wstring_view name;
  std::vector<int> children;
 };
 explicit XmlSelectionPosition(std::wstring_view text) : m_text(text) { Parse(); }
 bool Valid() const { return m_valid; }
 const Element& At(int index) const { return m_elements.at(static_cast<size_t>(index)); }
 int Root() const { return m_elements.empty() ? -1 : 0; }
 int ByPath(const std::vector<int>& path) const {
  int node = Root();
  for (int child : path) {
   if (node < 0 || child < 0 || child >= static_cast<int>(At(node).children.size())) return -1;
   node = At(node).children[static_cast<size_t>(child)];
  }
  return node;
 }
 std::vector<int> Path(int node) const {
  std::vector<int> path;
  while (node >= 0 && At(node).parent >= 0) {
   const int parent = At(node).parent;
   const auto& siblings = At(parent).children;
   path.push_back(static_cast<int>(std::find(siblings.begin(), siblings.end(), node) - siblings.begin()));
   node = parent;
  }
  std::reverse(path.begin(), path.end()); return path;
 }
 int AtPosition(int position) const {
  if (!m_valid || position < 0 || position > static_cast<int>(m_text.size())) return -1;
  int found = -1;
  for (int index = 0; index < static_cast<int>(m_elements.size()); ++index) {
   const auto& node = At(index);
   if (node.start <= position && position <= node.end) found = index;
   if (node.start > position) break;
  }
  return found;
 }
 // UTF-16 offsets include descendant inline text. Boundary points stay outside
 // entities; an offset inside a surrogate entity is deliberately unavailable.
 bool TextOffset(int node, int position, int& offset, bool& exact) const {
  const auto& element = At(node);
  exact = position >= element.content && position <= element.close;
  position = (std::max)(element.content, (std::min)(position, element.close));
  if (position > 0 && position < static_cast<int>(m_text.size()) && m_text[static_cast<size_t>(position-1)] >= 0xD800 && m_text[static_cast<size_t>(position-1)] <= 0xDBFF && m_text[static_cast<size_t>(position)] >= 0xDC00 && m_text[static_cast<size_t>(position)] <= 0xDFFF) { --position; exact=false; }
  offset = 0;
  bool boundary = position == element.content && element.content == element.close;
  const bool valid = WalkText(node, [&](int start, int end, int length) {
   if (position == start || position == end) boundary=true;
   if (end <= position) offset += length;
   else if (position > start) { exact = false; }
   return true;
  });
  exact = exact && boundary; return valid;
 }
 int Position(int node, int offset) const {
  if (offset < 0) return -1;
  int consumed = 0, result = -1;
  const bool valid = WalkText(node, [&](int start, int end, int length) {
   if (result >= 0) return true;
   if (offset == consumed) result = start;
   consumed += length;
   if (offset == consumed) result = end;
   return true;
  });
  if (!valid) return -1;
  if (result < 0 && offset == 0) return At(node).content;
  if (result > 0 && result < static_cast<int>(m_text.size()) && m_text[static_cast<size_t>(result-1)] >= 0xD800 && m_text[static_cast<size_t>(result-1)] <= 0xDBFF && m_text[static_cast<size_t>(result)] >= 0xDC00 && m_text[static_cast<size_t>(result)] <= 0xDFFF) return -1;
  return result;
 }
private:
 std::wstring_view m_text;
 std::vector<Element> m_elements;
 bool m_valid = false;
 int TagEnd(int start) const {
  wchar_t quote = 0;
  for (int at = start; at < static_cast<int>(m_text.size()); ++at) {
   const wchar_t ch = m_text[static_cast<size_t>(at)];
   if (quote) { if (ch == quote) quote = 0; }
   else if (ch == L'\'' || ch == L'"') quote = ch;
   else if (ch == L'>') return at + 1;
  }
  return -1;
 }
 void Parse() {
  std::vector<int> stack;
  for (size_t at = 0; (at = m_text.find(L'<', at)) != std::wstring_view::npos;) {
   const int start = static_cast<int>(at);
   if (m_text.substr(at, 4) == L"<!--" || m_text.substr(at, 9) == L"<![CDATA[" || m_text.substr(at, 2) == L"<?") {
    const bool comment = m_text.substr(at, 4) == L"<!--", cdata = m_text.substr(at, 9) == L"<![CDATA[";
    const std::wstring_view terminator = comment ? L"-->" : (cdata ? L"]]>" : L"?>");
    const size_t end = m_text.find(terminator, at + 2);
    if (end == std::wstring_view::npos) return;
    at = end + terminator.size(); continue;
   }
   if (m_text.substr(at, 2) == L"<!") return; // DTDs are not editor serialization.
   const int end = TagEnd(start + 1); if (end < 0) return;
   const bool closing = m_text[at + 1] == L'/';
   const size_t nameStart = at + (closing ? 2 : 1);
   const size_t nameEnd = m_text.find_first_of(L" \t\r\n/>", nameStart);
   if (nameEnd == std::wstring_view::npos || nameEnd == nameStart) return;
   const auto name = m_text.substr(nameStart, nameEnd - nameStart);
   if (closing) {
    if (stack.empty() || At(stack.back()).name != name) return;
    auto& element = m_elements[static_cast<size_t>(stack.back())];
    element.close = start; element.end = end; stack.pop_back();
   } else {
    Element element; element.parent = stack.empty() ? -1 : stack.back();
    element.start = start; element.content = end; element.close = element.end = end; element.name = name;
    const int index = static_cast<int>(m_elements.size());
    if (element.parent >= 0) m_elements[static_cast<size_t>(element.parent)].children.push_back(index);
    else if (!m_elements.empty()) return;
    m_elements.push_back(element);
    if (m_text[static_cast<size_t>(end - 2)] != L'/') stack.push_back(index);
   }
   at = static_cast<size_t>(end);
  }
  m_valid = !m_elements.empty() && stack.empty();
 }
 template<class Visitor> bool WalkText(int node, Visitor visitor) const {
  const auto& element = At(node);
  for (int at = element.content; at < element.close;) {
   if (m_text.substr(static_cast<size_t>(at), 9) == L"<![CDATA[") {
    const size_t end = m_text.find(L"]]>", static_cast<size_t>(at + 9));
    if (end == std::wstring_view::npos) return false;
    for (int c = at + 9; c < static_cast<int>(end); ++c) visitor(c, c + 1, 1);
    at = static_cast<int>(end) + 3;
   } else if (m_text.substr(static_cast<size_t>(at), 4) == L"<!--") {
    const size_t end = m_text.find(L"-->", static_cast<size_t>(at + 4));
    if (end == std::wstring_view::npos) return false;
    at = static_cast<int>(end) + 3;
   } else if (m_text[static_cast<size_t>(at)] == L'<') {
    at = TagEnd(at + 1); if (at < 0) return false;
   } else if (m_text[static_cast<size_t>(at)] == L'&') {
    const size_t end = m_text.find(L';', static_cast<size_t>(at + 1));
    if (end == std::wstring_view::npos || end >= static_cast<size_t>(element.close)) return false;
    std::wstring decoded;
    if (!DecodeXmlCharacterReference(std::wstring(m_text.substr(static_cast<size_t>(at), end + 1 - at)), decoded)) return false;
    visitor(at, static_cast<int>(end) + 1, static_cast<int>(decoded.size())); at = static_cast<int>(end) + 1;
   } else { visitor(at, at + 1, 1); ++at; }
  }
  return true;
 }
};
}
