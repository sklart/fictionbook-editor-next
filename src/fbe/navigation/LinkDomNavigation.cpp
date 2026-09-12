#include "stdafx.h"
#include "LinkDomNavigation.h"
#include "../LinkNavigation.h"
#include "../apputils.h"
namespace FBELinkNavigation {
static bool IsAnchor(MSHTML::IHTMLElementPtr element) {
  if (!element)
    return false;
  const _bstr_t tagName(element->tagName);
  const wchar_t *name = tagName;
  return name && _wcsicmp(name, L"A") == 0;
}

MSHTML::IHTMLElementPtr FindNearestLinkElement(MSHTML::IHTMLElementPtr e,
                                               MSHTML::IHTMLElementPtr b) {
  while (e && e != b) {
    if (IsAnchor(e))
      return e;
    e = e->parentElement;
  }
  return IsAnchor(b) ? b : MSHTML::IHTMLElementPtr();
}
MSHTML::IHTMLElementPtr GetEditableBody(MSHTML::IHTMLDocument2Ptr d) {
  return d ? MSHTML::IHTMLElementPtr(d->all->item(L"fbw_body"))
           : MSHTML::IHTMLElementPtr();
}
CString GetInternalLinkTargetId(MSHTML::IHTMLDocument2Ptr d,
                                MSHTML::IHTMLElementPtr l) {
  if (!d || !l)
    return CString();
  CString u;
  try {
    MSHTML::IHTMLDocument4Ptr d4(d);
    if (d4)
      u = (LPCWSTR)d4->URLUnencoded;
  } catch (const _com_error &) {
  }
  return CString(FBELinkNavigation::GetInternalTargetId(
                     (LPCWSTR)AU::GetAttrCS(l, L"href"), (LPCWSTR)u)
                     .c_str());
}
MSHTML::IHTMLElementPtr FindTargetElement(MSHTML::IHTMLDocument2Ptr d,
                                          const CString &id) {
  return d && !id.IsEmpty()
             ? MSHTML::IHTMLElementPtr(d->all->item((LPCWSTR)id))
             : MSHTML::IHTMLElementPtr();
}
long GetLinkUniqueNumber(MSHTML::IHTMLElementPtr link) {
  if (!link) return -1;
  try { return MSHTML::IHTMLUniqueNamePtr(link)->uniqueNumber; }
  catch (const _com_error &) { return -1; }
}
MSHTML::IHTMLElementPtr FindOriginLink(MSHTML::IHTMLDocument2Ptr d,
                                       const CString & /*id*/, long originUniqueNumber) {
  if (!d || originUniqueNumber < 0) return MSHTML::IHTMLElementPtr();
  MSHTML::IHTMLElement2Ptr body(GetEditableBody(d));
  MSHTML::IHTMLElementCollectionPtr links(body ? body->getElementsByTagName(L"A") : MSHTML::IHTMLElementCollectionPtr());
  for (long index = 0; links && index < links->length; ++index) {
    MSHTML::IHTMLElementPtr candidate(links->item(index));
    // The destination href can be normalized by MSHTML after an unrelated DOM
    // insertion.  uniqueNumber identifies the original link itself, which is
    // the only safe criterion for a history return.
    if (GetLinkUniqueNumber(candidate) == originUniqueNumber) return candidate;
  }
  return MSHTML::IHTMLElementPtr();
}
} // namespace FBELinkNavigation
