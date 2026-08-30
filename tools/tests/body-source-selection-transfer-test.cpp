#include <cstdlib>
#include <iostream>
#include <string>

#include "BodySourceSelectionTransfer.h"

using FBEBodySourceTransfer::FindVisibleXmlTextRange;
using FBEBodySourceTransfer::FindXmlNodeTextPosition;
using FBEBodySourceTransfer::FindEnclosingXmlElementRange;
using FBEBodySourceTransfer::XmlTextRange;

static void Need(bool value, const char* message)
{
	if (!value) { std::cerr << message << std::endl; std::exit(1); }
}

static int At(const std::wstring& text, const std::wstring& needle)
{
	const std::wstring::size_type position = text.find(needle);
	Need(position != std::wstring::npos, "fixture position");
	return static_cast<int>(position);
}

static XmlTextRange Match(const std::wstring& xml, const std::wstring& text,
	const std::wstring& scopeOpen, const std::wstring& scopeClose, int expected)
{
	const int start = At(xml, scopeOpen);
	const std::wstring::size_type close = xml.find(scopeClose, static_cast<size_t>(start));
	Need(close != std::wstring::npos, "fixture scope end");
	const int end = static_cast<int>(close + scopeClose.size());
	XmlTextRange result = { -1, -1 };
	Need(FindVisibleXmlTextRange(xml, text, start, end, expected, result), "positioned match");
	return result;
}

int main()
{
	const std::wstring xml =
		L"<FictionBook><body name='main'><section id='one'><title><p>Repeated phrase</p></title>"
		L"<p>Repeated <emphasis>phrase</emphasis> A</p><poem><stanza><v>line one</v><v>line two</v></stanza></poem>"
		L"<table><tr><td><p>cell text</p></td></tr></table></section>"
		L"<section id='two'><p>Repeated <strong>phrase</strong> B</p><p>Exact duplicate</p>"
		L"<p>space&#160;hex&#xA0;shy&#173;zwsp&#x200B;narrow&#x202F;emoji &#x1F600;</p>"
		L"<p>first paragraph</p><p>Repeated <a xlink:href='#x'>phrase</a> C</p></section>"
		L"<section id='three'><p>Exact duplicate</p></section></body>"
		L"<body name='nested'><section id='outer'><section id='a'><p>Exact duplicate</p></section>"
		L"<section id='b'><p>Exact duplicate</p></section></section></body>"
		L"<body name='notes'><section id='note'><p>Repeated phrase A</p><p>Exact duplicate</p></section></body></FictionBook>";

	const int sectionOne = At(xml, L"<section id='one'>");
	const int sectionTwo = At(xml, L"<section id='two'>");
	const int notes = At(xml, L"<body name='notes'>");
	const int firstPhrase = At(xml, L"Repeated <emphasis>");
	const int secondPhrase = At(xml, L"Repeated <strong>");
	const int notePhrase = At(xml, L"Repeated phrase A</p><p>Exact duplicate");
	const int secondDuplicate = static_cast<int>(xml.find(L"Exact duplicate", sectionTwo));
	const int thirdDuplicate = static_cast<int>(xml.find(L"Exact duplicate", At(xml, L"<section id='three'>")));
	const int noteDuplicate = static_cast<int>(xml.find(L"Exact duplicate", notes));
	const int nestedOuter = At(xml, L"<section id='outer'>");
	const int nestedA = At(xml, L"<section id='a'>");
	const int nestedB = At(xml, L"<section id='b'>");
	const int nestedBDuplicate = static_cast<int>(xml.find(L"Exact duplicate", nestedB));
	const int nestedBEnd = static_cast<int>(xml.find(L"</section>", nestedB)) +
		static_cast<int>(std::wstring(L"</section>").size());
	Need(secondDuplicate >= 0 && thirdDuplicate >= 0 && noteDuplicate >= 0, "duplicate fixture positions");

	// Body -> Source: duplicate text is resolved by the DOM-derived expected
	// position inside the matching structural region, never by first global hit.
	XmlTextRange first = Match(xml, L"Repeated phrase A", L"<section id='one'>", L"</section>", firstPhrase);
	Need(first.start == firstPhrase, "section one structural round trip");
	XmlTextRange second = Match(xml, L"Repeated phrase B", L"<section id='two'>", L"</section>", secondPhrase);
	Need(second.start == secondPhrase, "section two inline structural round trip");
	XmlTextRange note = Match(xml, L"Repeated phrase A", L"<body name='notes'>", L"</body>", notePhrase);
	Need(note.start == notePhrase, "notes body is independent from main body");
	Need(note.start != first.start && second.start != first.start, "duplicate phrase positions stay distinct");
	XmlTextRange exactSecond = Match(xml, L"Exact duplicate", L"<body name='main'>", L"</body>", secondDuplicate);
	Need(exactSecond.start == secondDuplicate && exactSecond.start != thirdDuplicate,
		"second identical phrase selected by expected structural position");

	// Inline markup, title, poem/stanza/v, table and paragraph-spanning text all
	// retain source boundaries while tags themselves are not selectable text.
	Need(Match(xml, L"Repeated phrase A", L"<section id='one'>", L"</section>", firstPhrase).end > first.start,
		"inline emphasis selection");
	Need(Match(xml, L"Repeated phrase", L"<title>", L"</title>", At(xml, L"<title>")).start == At(xml, L"Repeated phrase</p></title>"), "title selection");
	Need(Match(xml, L"line one", L"<poem>", L"</poem>", At(xml, L"line one")).start == At(xml, L"line one"), "poem selection");
	Need(Match(xml, L"cell text", L"<table>", L"</table>", At(xml, L"cell text")).start == At(xml, L"cell text"), "table selection");
	Need(Match(xml, L"first paragraph Repeated phrase C", L"<section id='two'>", L"</section>", At(xml, L"first paragraph")).start == At(xml, L"first paragraph"), "multi paragraph forward range");

	const std::wstring decoded = L"space\xA0" L"hex\xA0" L"shy\xAD" L"zwsp\x200B" L"narrow\x202F" L"emoji \xD83D\xDE00";
	Need(Match(xml, decoded, L"<section id='two'>", L"</section>", At(xml, L"space&#160;")).start == At(xml, L"space&#160;"), "named and numeric entities plus unicode");

	XmlTextRange ambiguous = { -1, -1 };
	Need(!FindVisibleXmlTextRange(xml, L"Repeated phrase A", 0, static_cast<int>(xml.size()), -1, ambiguous),
		"unscoped duplicate safely refuses transfer");
	Need(!FindVisibleXmlTextRange(xml, L"emphasis", sectionOne, sectionTwo, firstPhrase, ambiguous),
		"markup-only source selection safely refuses transfer");
	Need(!FindVisibleXmlTextRange(xml, L"", sectionOne, sectionTwo, firstPhrase, ambiguous), "empty selection remains caret-only");
	// Caret-only fallback has no selected text.  Its XML-node text lookup is
	// constrained to the DOM-selected section (and then body), so neither the
	// next main-body section nor an identical note can win.
	XmlTextRange caretMain = Match(xml, L"Exact duplicate", L"<section id='two'>", L"</section>", secondDuplicate);
	XmlTextRange caretNotes = Match(xml, L"Exact duplicate", L"<body name='notes'>", L"</body>", noteDuplicate);
	Need(caretMain.start == secondDuplicate && caretNotes.start == noteDuplicate,
		"caret-only body scopes keep identical text in its own body");

	// This calls the same scoped helper used by the production caret fallback.
	// The closest enclosing nested section must be b, not outer or a.
	XmlTextRange nestedScope = { -1, -1 };
	Need(FindEnclosingXmlElementRange(xml, nestedBDuplicate, L"section", nestedScope),
		"nested section scope");
	Need(nestedScope.start == nestedB && nestedScope.end == nestedBEnd,
		"deepest nested section is selected");
	Need(FindXmlNodeTextPosition(xml, L"Exact duplicate", 0,
		nestedScope.start, nestedScope.end) == nestedBDuplicate,
		"caret fallback maps nested section b");
	Need(FindXmlNodeTextPosition(xml, L"Exact duplicate", 0,
		nestedOuter, nestedScope.end) == -1,
		"ambiguous caret fallback safely refuses outer nested section");
	return 0;
}
