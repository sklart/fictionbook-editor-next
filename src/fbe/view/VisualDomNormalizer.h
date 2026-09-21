#pragma once

#include <mshtml.h>

namespace FbeVisualDom {
bool IsDirectSectionImage(MSHTML::IHTMLDOMNode *node);
void NormalizeStructure(MSHTML::IHTMLDocument2Ptr document,
                        MSHTML::IHTMLDOMNodePtr root);
void BubbleUp(MSHTML::IHTMLDOMNode *node, const wchar_t *name);
void FixupParagraphs(MSHTML::IHTMLElement2Ptr element);
void KillDivs(MSHTML::IHTMLElement2Ptr element);
void KillStyles(MSHTML::IHTMLElement2Ptr element);
void RelocateParagraphs(MSHTML::IHTMLDOMNode *node);
void PackText(MSHTML::IHTMLElement2Ptr element,
              MSHTML::IHTMLDocument2 *document);
void RemoveEmptyNodes(MSHTML::IHTMLDOMNode *node);
void SplitBRs(MSHTML::IHTMLElement2Ptr element);
} // namespace FbeVisualDom
