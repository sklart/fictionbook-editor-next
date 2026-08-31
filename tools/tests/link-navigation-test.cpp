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
	Need(GetInternalTargetId(L"file:///book.fb2#n1", L"file:///book.fb2") == L"n1", "MSHTML current file target");
	Need(GetInternalTargetId(L"file:///other.fb2#n1", L"file:///book.fb2").empty(), "external file target rejected");
	Need(GetInternalTargetId(L"fbw-internal:#n1") == L"n1", "FBE internal target");
	Need(GetInternalTargetId(L"https://site/x#n1").empty(), "external URL is not an internal target");
	Need(IsExternalHttpUrl(L"https://site/x"), "https external URL");
	Need(IsExternalHttpUrl(L"HTTP://site/x"), "case-insensitive http external URL");
	Need(IsBlockedUrl(L"javascript:alert(1)") && IsBlockedUrl(L"data:text/plain,x"), "unsafe schemes blocked");
	return 0;
}
