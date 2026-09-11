#pragma once
#include <atlstr.h>
#include <mshtml.h>
namespace FBELinkNavigation {
MSHTML::IHTMLElementPtr FindNearestLinkElement(MSHTML::IHTMLElementPtr element,
                                               MSHTML::IHTMLElementPtr body);
MSHTML::IHTMLElementPtr GetEditableBody(MSHTML::IHTMLDocument2Ptr document);
CString GetInternalLinkTargetId(MSHTML::IHTMLDocument2Ptr document,
                                MSHTML::IHTMLElementPtr link);
MSHTML::IHTMLElementPtr FindTargetElement(MSHTML::IHTMLDocument2Ptr document,
                                          const CString &targetId);
long GetLinkTargetOrdinal(MSHTML::IHTMLDocument2Ptr document,
                          MSHTML::IHTMLElementPtr link,
                          const CString &targetId);
MSHTML::IHTMLElementPtr FindOriginLink(MSHTML::IHTMLDocument2Ptr document,
                                       const CString &targetId,
                                       long originOrdinal);
} // namespace FBELinkNavigation
