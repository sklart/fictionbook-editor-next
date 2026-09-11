#pragma once
#include <mshtml.h>
#include <atlstr.h>
namespace FBELinkNavigation { MSHTML::IHTMLElementPtr FindNearestLinkElement(MSHTML::IHTMLElementPtr element, MSHTML::IHTMLElementPtr body); MSHTML::IHTMLElementPtr GetEditableBody(MSHTML::IHTMLDocument2Ptr document); CString GetInternalLinkTargetId(MSHTML::IHTMLDocument2Ptr document, MSHTML::IHTMLElementPtr link); long GetLinkTargetOrdinal(MSHTML::IHTMLDocument2Ptr document, MSHTML::IHTMLElementPtr link, const CString& targetId); }
