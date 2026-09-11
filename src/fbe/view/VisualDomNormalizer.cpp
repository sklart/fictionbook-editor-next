#include "stdafx.h"
#include "VisualDomNormalizer.h"
#include "../utils/utils.h"

namespace FbeVisualDom {
static bool IsNativeTableBlockName(const _bstr_t &name) {
  return U::scmp(name, L"TABLE") == 0;
}
static bool IsEmptyNode(MSHTML::IHTMLDOMNode *node) {
  if (!node || node->nodeType != 1)
    return false;
  _bstr_t name(node->nodeName);
  if (U::scmp(name, L"DIV") == 0 &&
      U::scmp(MSHTML::IHTMLElementPtr(node)->className, L"image") == 0)
    return false;
  if (U::scmp(name, L"IMG") == 0)
    return false;
  if (node->hasChildNodes() == VARIANT_FALSE)
    return true;
  if (U::scmp(name, L"A") == 0 || (bool)node->firstChild->nextSibling ||
      node->firstChild->nodeType != 3)
    return false;
  return U::is_whitespace(node->firstChild->nodeValue.bstrVal);
}
void RemoveEmptyNodes(MSHTML::IHTMLDOMNode *node) {
  if (!node || node->nodeType != 1)
    return;
  _bstr_t name(node->nodeName);
  if (U::scmp(name, L"TABLE") == 0 || U::scmp(name, L"TBODY") == 0 ||
      U::scmp(name, L"TR") == 0 || U::scmp(name, L"TD") == 0 ||
      U::scmp(name, L"TH") == 0)
    return;
  for (MSHTML::IHTMLDOMNodePtr current(node->firstChild); current;) {
    MSHTML::IHTMLDOMNodePtr next;
    try {
      next = current->nextSibling;
    } catch (...) {
      return;
    }
    RemoveEmptyNodes(current);
    if (IsEmptyNode(current))
      current->removeNode(VARIANT_TRUE);
    current = next;
  }
}
static void MoveUp(bool copyFormatting, MSHTML::IHTMLDOMNodePtr &node) {
  MSHTML::IHTMLDOMNodePtr parent(node->parentNode);
  MSHTML::IHTMLElement2Ptr element(parent);
  if (copyFormatting) {
    MSHTML::IHTMLDOMNodePtr clone(parent->cloneNode(VARIANT_FALSE));
    while ((bool)node->firstChild)
      clone->appendChild(node->firstChild);
    node->appendChild(clone);
  }
  if ((bool)node->nextSibling) {
    MSHTML::IHTMLDOMNodePtr clone(parent->cloneNode(VARIANT_FALSE));
    while ((bool)node->nextSibling)
      clone->appendChild(node->nextSibling);
    element->insertAdjacentElement(L"afterEnd", MSHTML::IHTMLElementPtr(clone));
    if (U::scmp(parent->nodeName, L"P") == 0)
      MSHTML::IHTMLElement3Ptr(clone)->inflateBlock = VARIANT_TRUE;
  }
  node->removeNode(VARIANT_TRUE);
  node = element->insertAdjacentElement(L"afterEnd",
                                        MSHTML::IHTMLElementPtr(node));
}
void BubbleUp(MSHTML::IHTMLDOMNode *node, const wchar_t *name) {
  MSHTML::IHTMLElement2Ptr element(node);
  MSHTML::IHTMLElementCollectionPtr elements(
      element->getElementsByTagName(name));
  const long length = elements->length;
  for (long index = 0; index < length; ++index) {
    MSHTML::IHTMLDOMNodePtr child(elements->item(index));
    if (!child)
      break;
    for (int level = 0; child->parentNode != node && level < 30; ++level)
      MoveUp(true, child);
    MoveUp(false, child);
  }
}
void RelocateParagraphs(MSHTML::IHTMLDOMNode *node) {
  if (!node || node->nodeType != 1)
    return;
  _bstr_t name(node->nodeName);
  if (U::scmp(name, L"TABLE") == 0 || U::scmp(name, L"TBODY") == 0 ||
      U::scmp(name, L"TR") == 0 || U::scmp(name, L"TD") == 0 ||
      U::scmp(name, L"TH") == 0)
    return;
  for (MSHTML::IHTMLDOMNodePtr current(node->firstChild); current;
       current = current->nextSibling)
    if (current->nodeType == 1) {
      if (!U::scmp(current->nodeName, L"P")) {
        BubbleUp(current, L"P");
        BubbleUp(current, L"DIV");
      } else
        RelocateParagraphs(current);
    }
}
void PackText(MSHTML::IHTMLElement2Ptr element,
              MSHTML::IHTMLDocument2 *document) {
  MSHTML::IHTMLElementCollectionPtr elements(
      element->getElementsByTagName(L"DIV"));
  for (long index = 0; index < elements->length; ++index) {
    MSHTML::IHTMLDOMNodePtr div(elements->item(index));
    if (U::scmp(MSHTML::IHTMLElementPtr(div)->className, L"image") == 0)
      continue;
    for (MSHTML::IHTMLDOMNodePtr current(div->firstChild); current;) {
      _bstr_t name(current->nodeName);
      if (U::scmp(name, L"P") && U::scmp(name, L"DIV") &&
          !IsNativeTableBlockName(name)) {
        MSHTML::IHTMLElementPtr paragraph(document->createElement(L"P"));
        MSHTML::IHTMLDOMNodePtr paragraphNode(paragraph);
        current->replaceNode(paragraphNode);
        paragraphNode->appendChild(current);
        while ((bool)paragraphNode->nextSibling) {
          name = paragraphNode->nextSibling->nodeName;
          if (U::scmp(name, L"P") == 0 || U::scmp(name, L"DIV") == 0 ||
              IsNativeTableBlockName(name))
            break;
          paragraphNode->appendChild(paragraphNode->nextSibling);
        }
        current = paragraphNode->nextSibling;
      } else
        current = current->nextSibling;
    }
  }
}
void SplitBRs(MSHTML::IHTMLElement2Ptr element) {
  CString html = MSHTML::IHTMLElementPtr(element)->outerHTML;
  if (html.Replace(L"<BR>", L"</P><P>") > 0)
    MSHTML::IHTMLElementPtr(element)->outerHTML = html.AllocSysString();
}
void NormalizeStructure(MSHTML::IHTMLDocument2Ptr document,
                        MSHTML::IHTMLDOMNodePtr root) {
  RelocateParagraphs(root);
  RemoveEmptyNodes(root);
  PackText(MSHTML::IHTMLElement2Ptr(root), document);
  RelocateParagraphs(root);
  SplitBRs(MSHTML::IHTMLElement2Ptr(root));
  RemoveEmptyNodes(root);
}
void KillDivs(MSHTML::IHTMLElement2Ptr element) {
  MSHTML::IHTMLElementCollectionPtr divs(element->getElementsByTagName(L"DIV"));
  while (divs->length > 0)
    MSHTML::IHTMLDOMNodePtr(divs->item(0L))->removeNode(VARIANT_FALSE);
}
void KillStyles(MSHTML::IHTMLElement2Ptr element) {
  MSHTML::IHTMLElementCollectionPtr paragraphs(
      element->getElementsByTagName(L"P"));
  for (long index = 0; index < paragraphs->length; ++index)
    CheckError(
        MSHTML::IHTMLElementPtr(paragraphs->item(index))->put_className(NULL));
}
void FixupParagraphs(MSHTML::IHTMLElement2Ptr element) {
  MSHTML::IHTMLElementCollectionPtr paragraphs(
      element->getElementsByTagName(L"P"));
  for (long index = 0; index < paragraphs->length; ++index)
    MSHTML::IHTMLElement3Ptr(paragraphs->item(index))->inflateBlock =
        VARIANT_TRUE;
}
} // namespace FbeVisualDom
