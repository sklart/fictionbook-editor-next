#include "../stdafx.h"
#include "BodySourceSelectionCoordinator.h"
#include "SourceDocumentTransfer.h"
#include "SourceViewDiagnostics.h"
#include "XmlSelectionPosition.h"
#include "ui/SourceEditorControl.h"
#include "../apputils.h"
#include "../utils/utils.h"
#include "../FBDoc.h"
#include "../view/EditorSelectionState.h"
#include "../StartupTrace.h"

namespace {
using FBEBodySourceTransfer::XmlSelectionPosition;
using HtmlNode = MSHTML::IHTMLDOMNodePtr;
using XmlNode = MSXML2::IXMLDOMNodePtr;

std::vector<HtmlNode> HtmlChildren(HtmlNode parent)
{
 std::vector<HtmlNode> result;
 for (HtmlNode child = parent ? parent->firstChild : HtmlNode(); child; child = child->nextSibling) {
  if (child->nodeType != 1) continue;
  if (_wcsicmp(child->nodeName, L"TBODY") == 0) {
   const auto rows = HtmlChildren(child); result.insert(result.end(), rows.begin(), rows.end());
  } else result.push_back(child);
 }
 return result;
}
std::vector<XmlNode> XmlChildren(XmlNode parent)
{
 std::vector<XmlNode> result;
 for (XmlNode child = parent ? parent->firstChild : XmlNode(); child; child = child->nextSibling)
  if (child->nodeType == MSXML2::NODE_ELEMENT) result.push_back(child);
 return result;
}
CString Name(XmlNode node) { return node ? CString(static_cast<LPCWSTR>(node->baseName)) : CString(); }
HtmlNode HtmlRoot(FB::Doc& document) {
 return MSHTML::IHTMLDocument3Ptr(document.m_body.Document())->getElementById(L"fbw_body");
}
XmlNode ScopeXml(XmlNode root, HtmlNode htmlScope, FB::Doc& document)
{
 const CString kind(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(htmlScope)->className));
 if (kind == L"annotation") return root->selectSingleNode(L"*[local-name()='description']/*[local-name()='title-info']/*[local-name()='annotation']");
 if (kind == L"history") return root->selectSingleNode(L"*[local-name()='description']/*[local-name()='document-info']/*[local-name()='history']");
 if (kind != L"body") return XmlNode();
 int ordinal = 0;
 for (const auto& child : HtmlChildren(HtmlRoot(document))) {
  if (child == htmlScope) break;
  if (CString(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(child)->className)) == L"body") ++ordinal;
 }
 for (const auto& child : XmlChildren(root)) if (Name(child) == L"body" && ordinal-- == 0) return child;
 return XmlNode();
}
bool Equivalent(HtmlNode html, XmlNode xml)
{
 if (!html || !xml) return false;
 const CString name(Name(xml));
 MSHTML::IHTMLElementPtr element(html);
 CString tag(static_cast<LPCWSTR>(element->tagName)); tag.MakeLower();
 const CString kind(static_cast<LPCWSTR>(element->className));
 if (tag == name) return true;
 if (tag == L"div") return kind == name;
 if (tag == L"p") return name == L"v" || name == L"empty-line" || kind == name || (name == L"date" && kind == L"text-author");
 if (tag == L"em") return name == L"emphasis";
 if (tag == L"strike") return name == L"strikethrough";
 if (tag == L"span") return name == L"style" || (name == L"code" && kind == L"code") || (name == L"image" && kind == L"image");
 return false;
}
XmlNode XmlForHtml(FB::Doc& document, XmlNode root, HtmlNode html)
{
 const HtmlNode body = HtmlRoot(document);
 std::vector<int> path;
 HtmlNode current(html);
 while (current && current->parentNode != body) {
  HtmlNode parent = current->parentNode;
  if (parent && _wcsicmp(parent->nodeName, L"TBODY") == 0) parent = parent->parentNode;
  const auto siblings = HtmlChildren(parent);
  const auto found = std::find(siblings.begin(), siblings.end(), current);
  if (found == siblings.end()) return XmlNode();
  path.push_back(static_cast<int>(found - siblings.begin())); current = parent;
 }
 if (!current) return XmlNode();
 XmlNode xml = ScopeXml(root, current, document);
 for (auto at = path.rbegin(); xml && at != path.rend(); ++at) {
  const auto children = XmlChildren(xml);
  if (*at >= static_cast<int>(children.size())) return XmlNode();
  xml = children[static_cast<size_t>(*at)];
 }
 return Equivalent(html, xml) ? xml : XmlNode();
}
std::vector<int> XmlPath(XmlNode root, XmlNode node)
{
 std::vector<int> path;
 while (node && node != root) {
  const XmlNode parent = node->parentNode;
  const auto siblings = XmlChildren(parent);
  const auto found = std::find(siblings.begin(), siblings.end(), node);
  if (found == siblings.end()) return {};
  path.push_back(static_cast<int>(found - siblings.begin())); node = parent;
 }
 std::reverse(path.begin(), path.end()); return path;
}
HtmlNode HtmlForXml(FB::Doc& document, XmlNode root, XmlNode node)
{
 XmlNode scope(node);
 while (scope && Name(scope) != L"body" && Name(scope) != L"annotation" && Name(scope) != L"history") scope = scope->parentNode;
 // A section annotation is below a body, so use the outermost mapped scope.
 while (scope && scope->parentNode && scope->parentNode != root && Name(scope->parentNode) != L"title-info" && Name(scope->parentNode) != L"document-info") {
  scope = scope->parentNode;
 }
 if (!scope) return HtmlNode();
 HtmlNode html;
 for (const auto& candidate : HtmlChildren(HtmlRoot(document))) if (ScopeXml(root, candidate, document) == scope) { html = candidate; break; }
 const auto path = XmlPath(scope, node);
 for (int child : path) {
  const auto children = HtmlChildren(html);
  if (child >= static_cast<int>(children.size())) return HtmlNode();
  html = children[static_cast<size_t>(child)];
 }
 return Equivalent(html, node) ? html : HtmlNode();
}
XmlNode ByPath(XmlNode node, const std::vector<int>& path) {
 for (int child : path) { const auto children = XmlChildren(node); if (child < 0 || child >= static_cast<int>(children.size())) return XmlNode(); node = children[static_cast<size_t>(child)]; }
 return node;
}
void Unavailable(LPCWSTR code) { StartupTrace::Warning(L"selection", code, L"result=0"); }
// Use a real endpoint range: legacy GetSelectionInfo excludes inline descendant
// characters and cannot be used as an offset into an element's complete text.
HtmlNode AfterSubtree(HtmlNode node, HtmlNode root)
{
 while(node && node!=root) { if(node->nextSibling) return node->nextSibling; node=node->parentNode; }
 return HtmlNode();
}
HtmlNode FirstParagraph(HtmlNode node, HtmlNode boundary)
{
 while(node) {
  if(node->nodeType==1 && _wcsicmp(node->nodeName,L"P")==0) return node;
  node=node->firstChild ? node->firstChild : AfterSubtree(node,boundary);
 }
 return HtmlNode();
}
MSHTML::IHTMLElementPtr EndpointElement(FB::Doc& document, XmlNode xmlRoot, MSHTML::IHTMLTxtRangePtr endpoint, bool caret)
{
 MSHTML::IHTMLElementPtr element(endpoint->parentElement());
 const HtmlNode root=HtmlRoot(document);
 // The owner of a real collapsed caret preserves end/start affinity. Only an
 // unmapped synthetic placeholder needs an equivalent XML-backed boundary.
 if(caret && XmlForHtml(document,xmlRoot,HtmlNode(element))) return element;
 // At the beginning of a paragraph MSHTML may report the preceding paragraph
 // as parentElement(). Resolve the actual boundary by comparing native ranges,
 // without searching book text or scanning all preceding paragraphs.
 const auto startsHere = [&](HtmlNode node) {
  if(!node) return false;
  MSHTML::IHTMLTxtRangePtr candidate(endpoint->duplicate());
  candidate->moveToElementText(MSHTML::IHTMLElementPtr(node));
  return candidate->compareEndPoints(L"StartToStart",endpoint)==0;
 };
 if(element && _wcsicmp(element->tagName,L"P")!=0) {
  const HtmlNode first=FirstParagraph(HtmlNode(element)->firstChild,HtmlNode(element));
  if(startsHere(first) && XmlForHtml(document,xmlRoot,first)) return MSHTML::IHTMLElementPtr(first);
 }
 for(HtmlNode next=FirstParagraph(AfterSubtree(HtmlNode(element),root),root); next && startsHere(next); next=FirstParagraph(AfterSubtree(next,root),root))
  if(XmlForHtml(document,xmlRoot,next)) return MSHTML::IHTMLElementPtr(next);
 return element;
}
bool BodyEndpoint(FB::Doc& document, XmlNode root, const XmlSelectionPosition& positions,
 MSHTML::IHTMLTxtRangePtr range, bool start, int& position)
{
 if (!range) return false;
 MSHTML::IHTMLTxtRangePtr endpoint(range->duplicate());
 // An existing caret already supplies both endpoints; preserve its affinity.
 const bool caret=range->compareEndPoints(L"StartToEnd",range)==0;
 if(!caret) endpoint->collapse(start ? VARIANT_TRUE : VARIANT_FALSE);
 MSHTML::IHTMLElementPtr element(EndpointElement(document,root,endpoint,caret));
 const XmlNode xml = XmlForHtml(document, root, HtmlNode(element)); if (!xml) return false;
 MSHTML::IHTMLTxtRangePtr prefix(range->duplicate()); prefix->moveToElementText(element);
 CString text;
 if(prefix->compareEndPoints(L"StartToStart",endpoint)<=0) {
  prefix->setEndPoint(L"EndToStart", endpoint); text=static_cast<LPCWSTR>(prefix->text);
 } else return false;
 const int index = positions.ByPath(XmlPath(root, xml)); if (index < 0) return false;
 position = positions.Position(index, text.GetLength()); return position >= 0;
}
bool SourceEndpoint(FB::Doc& document, XmlNode root, const XmlSelectionPosition& positions,
 int position, MSHTML::IHTMLTxtRangePtr& endpoint, bool& exact)
{
 int index = positions.AtPosition(position); if (index < 0) return false;
 bool approximate = false;
 for (;;) {
  const auto& element=positions.At(index);
  const std::wstring container=L"|"+std::wstring(element.name)+L"|";
  if (std::wstring(L"|body|section|annotation|history|title|poem|stanza|epigraph|cite|table|tr|").find(container)==std::wstring::npos || element.children.empty()) break;
  int nearest=-1, distance=INT_MAX;
  for(int child:element.children) {
   const auto& candidate=positions.At(child);
   const int candidateDistance=position<candidate.start ? candidate.start-position : (position>candidate.end ? position-candidate.end : 0);
   if(candidateDistance<distance) {distance=candidateDistance;nearest=child;}
  }
  index=nearest;approximate=true;
 }
 const XmlNode xml = ByPath(root, positions.Path(index));
 const HtmlNode html = HtmlForXml(document, root, xml); if (!html) return false;
 int offset = 0; if (!positions.TextOffset(index, position, offset, exact)) return false;
 exact = exact && !approximate;
 endpoint = MSHTML::IHTMLBodyElementPtr(document.m_body.Document()->body)->createTextRange();
 endpoint->moveToElementText(MSHTML::IHTMLElementPtr(html)); endpoint->collapse(VARIANT_TRUE);
 if (offset > 0 && endpoint->move(L"character", offset) != offset) return false;
 // Same MSHTML boundary-affinity workaround as CFBEView::GoTo: a collapsed
 // start may otherwise select the end of the preceding element after focus.
 if(offset==0 && MSHTML::IHTMLElementPtr(html)!=endpoint->parentElement() && endpoint->move(L"character",1)==1)
  endpoint->move(L"character",-1);
 return true;
}
}

void BodySourceSelectionCoordinator::MapSourceSelectionToBody(FB::Doc& document,
 MSXML2::IXMLDOMDocumentPtr xml, const SourceDocumentText& source, EditorSelectionState& selection)
{
 selection.BodySource().sourceToBodyTransferred = false;
 selection.BodySource().sourceToBodyApproximate = false;
 const XmlSelectionPosition positions(std::wstring_view(source.text, source.text.GetLength()));
 MSHTML::IHTMLTxtRangePtr begin, end; bool beginExact = false, endExact = false;
 if (!positions.Valid() || !SourceEndpoint(document, xml->documentElement, positions, source.selectionStart, begin, beginExact) ||
  !SourceEndpoint(document, xml->documentElement, positions, source.selectionEnd, end, endExact)) { selection.BodyRange() = MSHTML::IHTMLTxtRangePtr(document.m_body.Document()->selection->createRange()); Unavailable(L"BS202"); return; }
 if(!source.caret) begin->setEndPoint(L"EndToStart", end);
 selection.BodyRange() = begin;
 selection.BodySource().sourceToBodyTransferred = true;
 selection.BodySource().sourceToBodyApproximate = !beginExact || !endExact;
 if (selection.BodySource().sourceToBodyApproximate) StartupTrace::Event(L"selection", L"BS203", L"result=1");
}

void BodySourceSelectionCoordinator::MapBodySelectionToSource(FB::Doc& document,
 MSXML2::IXMLDOMDocumentPtr xml, SourceEditorControl& source, const CString& serializedSource,
 EditorSelectionState& selection, FbeSourceDiagnostics::SourceViewPhaseProfiler* profiler)
{
 selection.BodySource().bodyToSourceTransferred = false;
 const XmlSelectionPosition positions(std::wstring_view(serializedSource, serializedSource.GetLength()));
 int beginPosition = -1, endPosition = -1;
 if (!xml || !positions.Valid() || !BodyEndpoint(document, xml->documentElement, positions, selection.BodyRange(), true, beginPosition) ||
  !BodyEndpoint(document, xml->documentElement, positions, selection.BodyRange(), false, endPosition) || endPosition < beginPosition) { Unavailable(L"BS201"); return; }
 if (profiler) profiler->Mark("structural selection mapping");
 const int beginByte = ::WideCharToMultiByte(CP_UTF8, 0, serializedSource, beginPosition, NULL, 0, NULL, NULL);
 const int endByte = ::WideCharToMultiByte(CP_UTF8, 0, serializedSource, endPosition, NULL, 0, NULL, NULL);
 selection.BodySource().bodyToSourceTransferred = true;
 selection.BodySource().sourceStart = beginByte; selection.BodySource().sourceEnd = endByte;
 source.SendMessage(SCI_SETSELECTIONSTART, beginByte); source.SendMessage(SCI_SETSELECTIONEND, endByte);
 source.SendMessage(SCI_SCROLLCARET);
 if (profiler) profiler->Mark("selection restoration");
}
