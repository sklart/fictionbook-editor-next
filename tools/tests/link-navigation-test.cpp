#include <cstdlib>
#include <iostream>
#include <string>

#include "LinkNavigation.h"

static void Need(bool value, const char* message)
{
	if(!value) { std::cerr << message << std::endl; std::exit(1); }
}

int main()
{
	using namespace FBELinkNavigation;
	Need(GetInternalTargetId(L"#n1") == L"n1", "fragment target");
	Need(GetInternalTargetId(L"").empty() && GetInternalTargetId(L"  ").empty(), "empty href");
	Need(GetInternalTargetId(L"file:///book.fb2#n1", L"file:///book.fb2") == L"n1", "MSHTML current file target");
	Need(GetInternalTargetId(L"file:///other.fb2#n1", L"file:///book.fb2").empty(), "external file target rejected");
	Need(GetInternalTargetId(L"fbw-internal:#n1") == L"n1", "FBE internal target");
	Need(GetInternalTargetId(L"https://site/x#n1").empty(), "external URL is not an internal target");
	Need(IsExternalHttpUrl(L"https://site/x"), "https external URL");
	Need(IsExternalHttpUrl(L"HTTP://site/x"), "case-insensitive http external URL");
	Need(IsBlockedUrl(L"javascript:alert(1)") && IsBlockedUrl(L"data:text/plain,x"), "unsafe schemes blocked");
	Need(IsBlockedUrl(L"JaVaScRiPt:alert(1)") && IsBlockedUrl(L"DATA:text/plain,x"), "mixed-case unsafe schemes");
	Need(DecideLinkActivation(L"#n1", L"", true, false, false) == LinkActivation::Internal, "ctrl internal");
	Need(DecideLinkActivation(L"#n1", L"", false, true, false) == LinkActivation::Internal, "alt internal");
	Need(DecideLinkActivation(L"http://site/x", L"", true, false, false) == LinkActivation::ExternalHttp, "ctrl http");
	Need(DecideLinkActivation(L"https://site/x", L"", true, false, false) == LinkActivation::ExternalHttp, "ctrl https");
	Need(DecideLinkActivation(L"http://site/x", L"", false, true, false) == LinkActivation::Ignore, "alt http ignored");
	Need(DecideLinkActivation(L"https://site/x", L"", false, true, false) == LinkActivation::Ignore, "alt https ignored");
	Need(DecideLinkActivation(L"javascript:alert(1)", L"", true, false, false) == LinkActivation::Blocked, "ctrl javascript blocked");
	Need(DecideLinkActivation(L"data:text/plain,x", L"", true, false, false) == LinkActivation::Blocked, "ctrl data blocked");
	Need(DecideLinkActivation(L"javascript:alert(1)", L"", false, true, false) == LinkActivation::Blocked, "alt javascript blocked");
	Need(DecideLinkActivation(L"data:text/plain,x", L"", false, true, false) == LinkActivation::Blocked, "alt data blocked");
	Need(DecideLinkActivation(L"#n1", L"", true, false, true) == LinkActivation::Ignore, "shift ctrl ignored");
	Need(DecideLinkActivation(L"#n1", L"", false, true, true) == LinkActivation::Ignore, "shift alt ignored");
	Need(DecideLinkActivation(L"#", L"", true, false, false) == LinkActivation::Ignore, "fragment without target");
	return 0;
}
