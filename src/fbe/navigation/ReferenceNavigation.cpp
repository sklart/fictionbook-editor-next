#include "stdafx.h"
#include "ReferenceNavigation.h"
#include "LinkDomNavigation.h"
#include "../apputils.h"
#include "../utils/utils.h"

namespace FBEReferenceNavigation {
namespace {

MSHTML::IHTMLElementPtr FindAnchor(MSHTML::IHTMLElementPtr element)
{
  while (element)
  {
    if (U::scmp(element->tagName, L"A") == 0)
      return element;
    element = element->parentElement;
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr FindParentDiv(MSHTML::IHTMLElementPtr element)
{
  while (element && U::scmp(element->tagName, L"DIV") != 0)
    element = element->parentElement;
  return element;
}

CString NormalizeLegacyReferenceHref(CString href)
{
  if (href.Find(L"file") == 0)
  {
    const int fragment = href.ReverseFind(L'#');
    // A file URL without a fragment is never an in-document reference.  The
    // legacy code passed -1 to Mid(), which is invalid for CString and could
    // surface as a COM failure while command enablement was probing a range.
    href = fragment >= 0 ? href.Mid(fragment, 1024) : CString();
  }
  return href;
}

MSHTML::IHTMLTxtRangePtr CurrentTextRange(MSHTML::IHTMLDocument2Ptr document)
{
  try
  {
    return document && document->selection ? MSHTML::IHTMLTxtRangePtr(document->selection->createRange()) : MSHTML::IHTMLTxtRangePtr();
  }
  catch (const _com_error&)
  {
    return MSHTML::IHTMLTxtRangePtr();
  }
}

} // namespace

Resolution FindFootnoteTarget(MSHTML::IHTMLDocument2Ptr document, bool resolveTarget)
{
  Resolution result;
  MSHTML::IHTMLTxtRangePtr range(CurrentTextRange(document));
  if (!range) return result;

  try
  {
    CString href(AU::GetAttrCS(FindAnchor(range->parentElement()), L"href"));
    if (href.IsEmpty())
    {
      MSHTML::IHTMLTxtRangePtr next(range->duplicate());
      next->moveEnd(L"character", +1);
      href = AU::GetAttrCS(FindAnchor(next->parentElement()), L"href");
    }
    if (href.IsEmpty())
    {
      MSHTML::IHTMLTxtRangePtr previous(range->duplicate());
      previous->moveStart(L"character", -1);
      href = AU::GetAttrCS(FindAnchor(previous->parentElement()), L"href");
    }

    href = NormalizeLegacyReferenceHref(href);
    if (href.IsEmpty() || href[0] != _T('#')) return result;

    result.status = ResolutionStatus::Candidate;
    if (!resolveTarget) return result;

    const CString targetId(href.Mid(1));
    MSHTML::IHTMLElementPtr target(FBELinkNavigation::FindTargetElement(document, targetId));
    if (!target) return result;

    MSHTML::IHTMLDOMNodePtr destination(target);
    MSHTML::IHTMLDOMNodePtr child;
    if (destination && !U::scmp(destination->nodeName, L"DIV") && !U::scmp(target->className, L"section"))
    {
      child = destination->firstChild;
      while (child && !U::scmp(child->nodeName, L"DIV") &&
        (!U::scmp(MSHTML::IHTMLElementPtr(child)->className, L"image") || !U::scmp(MSHTML::IHTMLElementPtr(child)->className, L"title")))
        child = child->nextSibling;
    }
    result.element = child ? MSHTML::IHTMLElementPtr(child) : target;
    result.scrollElement = target;
    result.status = result.element ? ResolutionStatus::Found : ResolutionStatus::Candidate;
  }
  catch (const _com_error&)
  {
    return Resolution();
  }
  return result;
}

Resolution FindReferenceTarget(MSHTML::IHTMLDocument2Ptr document, bool resolveTarget)
{
  Resolution result;
  MSHTML::IHTMLTxtRangePtr range(CurrentTextRange(document));
  if (!range) return result;

  try
  {
    if (range->compareEndPoints(L"StartToEnd", range) != 0) return result;
    MSHTML::IHTMLElementPtr section(FindParentDiv(range->parentElement()));
    while (section && (U::scmp(section->tagName, L"DIV") != 0 || U::scmp(section->className, L"section") != 0))
      section = section->parentElement;
    if (!section) return result;

    MSHTML::IHTMLElementPtr body(section->parentElement);
    while (body && (U::scmp(body->tagName, L"DIV") != 0 || U::scmp(body->className, L"body") != 0))
      body = body->parentElement;
    if (!body) return result;

    CString selectionId(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(range->parentElement())->id));
    const CString bodyName(AU::GetAttrCS(body, L"fbname"));
    if (selectionId.IsEmpty() && bodyName.CompareNoCase(L"notes") != 0 && bodyName.CompareNoCase(L"comments") != 0)
      return result;

    result.status = ResolutionStatus::Candidate;
    if (!resolveTarget) return result;

    MSHTML::IHTMLElementPtr root(document->body);
    MSHTML::IHTMLElementCollectionPtr links(root ? MSHTML::IHTMLElement2Ptr(root)->getElementsByTagName(L"A") : MSHTML::IHTMLElementCollectionPtr());
    if (!links || links->length == 0)
    {
      result.status = ResolutionStatus::NoReferences;
      return result;
    }

    const CString selectedHref = L"#" + selectionId;
    const CString sectionHref = L"#" + CString(static_cast<LPCWSTR>(section->id));
    for (long index = 0; index < links->length; ++index)
    {
      MSHTML::IHTMLElementPtr link(links->item(index));
      CString href(NormalizeLegacyReferenceHref(AU::GetAttrCS(link, L"href")));
      if (href.Find(_T("://"), 0) != -1) continue;
      if (href == sectionHref || href == selectedHref)
      {
        result.element = link;
        result.status = ResolutionStatus::Found;
        return result;
      }
    }
  }
  catch (const _com_error&)
  {
    return Resolution();
  }
  return result;
}

} // namespace FBEReferenceNavigation
