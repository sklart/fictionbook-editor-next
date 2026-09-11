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
long GetLinkTargetOrdinal(MSHTML::IHTMLDocument2Ptr d,
                          MSHTML::IHTMLElementPtr l, const CString &id) {
  MSHTML::IHTMLElement2Ptr b(GetEditableBody(d));
  MSHTML::IHTMLElementCollectionPtr a(b ? b->getElementsByTagName(L"A")
                                        : MSHTML::IHTMLElementCollectionPtr());
  long o = 0;
  for (long i = 0; a && i < a->length; ++i) {
    MSHTML::IHTMLElementPtr c(a->item(i));
    if (GetInternalLinkTargetId(d, c) != id)
      continue;
    if (c == l)
      return o;
    ++o;
  }
  return -1;
}
} // namespace FBELinkNavigation
