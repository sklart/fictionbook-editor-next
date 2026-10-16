#include "stdafx.h"
#include "VisualDomNormalizer.h"
#include "../utils/utils.h"

namespace FbeVisualDom {
static bool IsNativeTableBlockName(const _bstr_t &name) {
  return U::scmp(name, L"TABLE") == 0;
}
static bool HasDivClass(MSHTML::IHTMLDOMNode *node, const wchar_t *className) {
  MSHTML::IHTMLElementPtr element(node);
  return element && U::scmp(element->tagName, L"DIV") == 0 &&
         U::scmp(element->className, className) == 0;
}
// MSHTML preserves the DIV.title wrapper when a complete title is copied and
// pasted into another title.  FB2 title containers cannot nest, so unwrap the
// inner container in place rather than deleting it (and its user content).
static void FlattenNestedTitles(MSHTML::IHTMLDOMNodePtr node) {
  if (!node || node->nodeType != 1)
    return;
  for (MSHTML::IHTMLDOMNodePtr current(node->firstChild); current;) {
    MSHTML::IHTMLDOMNodePtr next = current->nextSibling;
    FlattenNestedTitles(current);
    if (HasDivClass(node, L"title") && HasDivClass(current, L"title")) {
      while ((bool)current->firstChild)
        node->insertBefore(current->firstChild, current.GetInterfacePtr());
      current->removeNode(VARIANT_TRUE);
    }
    current = next;
  }
}
static MSHTML::IHTMLDOMNodePtr FindContainingSection(MSHTML::IHTMLDOMNodePtr node) {
  for (MSHTML::IHTMLDOMNodePtr current(node); current; current = current->parentNode)
    if (HasDivClass(current, L"section"))
      return current;
  return MSHTML::IHTMLDOMNodePtr();
}
// A paste at a title boundary can also make MSHTML produce two direct title
// children.  Keep the first title and append every later title's nodes, so the
// author-visible text and inline formatting remain intact.
static void MergeDirectSectionTitles(MSHTML::IHTMLDOMNodePtr section) {
  if (!HasDivClass(section, L"section"))
    return;
  MSHTML::IHTMLDOMNodePtr firstTitle;
  for (MSHTML::IHTMLDOMNodePtr current(section->firstChild); current;) {
    MSHTML::IHTMLDOMNodePtr next = current->nextSibling;
    if (HasDivClass(current, L"title")) {
      if (!firstTitle)
        firstTitle = current;
      else {
        while ((bool)current->firstChild)
          firstTitle->appendChild(current->firstChild);
        current->removeNode(VARIANT_TRUE);
      }
    }
    current = next;
  }
}
static bool IsEmptyNode(MSHTML::IHTMLDOMNode *node) {
  if (!node || node->nodeType != 1)
    return false;
  _bstr_t name(node->nodeName);
  // BR is converted to a paragraph boundary later in normalization; an empty
  // P is an author-visible empty line.  Neither is disposable whitespace.
  if (U::scmp(name, L"P") == 0 || U::scmp(name, L"BR") == 0)
    return false;
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
bool IsDirectSectionImage(MSHTML::IHTMLDOMNode *node) {
  MSHTML::IHTMLElementPtr image(node), parent(image ? image->parentElement : MSHTML::IHTMLElementPtr());
  return image && parent && U::scmp(image->tagName, L"DIV") == 0 && U::scmp(image->className, L"image") == 0 && U::scmp(parent->tagName, L"DIV") == 0 && U::scmp(parent->className, L"section") == 0;
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
  // Replacing an entire structural owner reparses unaffected paragraphs and
  // turns MSHTML's serialized inter-paragraph whitespace into new range steps.
  // Rewrite only paragraphs that actually contain a BR, retaining their owner
  // and all unaffected sibling nodes (and their live selection coordinates).
  MSHTML::IHTMLElementCollectionPtr collection(element->getElementsByTagName(L"P"));
  std::vector<MSHTML::IHTMLElementPtr> paragraphs;
  for (long i = 0; collection && i < collection->length; ++i) paragraphs.push_back(MSHTML::IHTMLElementPtr(collection->item(i)));
  MSHTML::IHTMLElementPtr root(element);
  if (root && U::scmp(root->tagName, L"P") == 0) paragraphs.push_back(root);
  for (const auto& paragraph : paragraphs) {
    CString html = paragraph->outerHTML;
    if (html.Replace(L"<BR>", L"</P><P>") > 0) paragraph->outerHTML = html.AllocSysString();
  }
}
void NormalizeStructure(MSHTML::IHTMLDocument2Ptr document,
                        MSHTML::IHTMLDOMNodePtr root) {
	FlattenNestedTitles(root);
	// A title is itself a valid normalization scope.  Still repair the one
	// enclosing section when needed, rather than escalating a title paste to
	// a BODY-wide pass.
	MergeDirectSectionTitles(FindContainingSection(root));
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
