#include "../../src/fbe/XmlTagMatcher.h"
#include <iostream>

namespace {
int failures = 0;
void Check(bool value, const char* message) { if (!value) { std::cerr << "FAILED: " << message << "\n"; ++failures; } }
const XmlTagToken* First(const XmlTagMatcher& matcher, XmlTagTokenType type) { for (const XmlTagToken& token : matcher.Tokens()) if (token.type == type) return &token; return 0; }
}

int main()
{
	{ XmlTagMatcher m("<section><p>Text</p></section>"); const XmlTagMatchResult r = m.ResultAt(1); Check(m.Diagnostics().empty(), "valid nested XML"); Check(r.state == XmlTagMatchState::Matched, "opening pair"); Check(r.currentNameRange.start == 1 && r.currentNameRange.end == 8 && r.matchingNameRange.start == 22 && r.matchingNameRange.end == 29, "matching name ranges"); }
	{ XmlTagMatcher m("<section><section><p>Text</p></section></section>"); Check(m.Diagnostics().empty(), "same-name nesting"); }
	{ XmlTagMatcher m("<image l:href=\"#cover\"/>"); const XmlTagToken* t = First(m, XmlTagTokenType::SelfClosingTag); Check(t && t->name == "image", "namespace attribute and self close"); Check(t && t->attributeRanges.size() == 1 && t->attributeRanges[0].end - t->attributeRanges[0].start == 15, "full attribute range"); }
	{ XmlTagMatcher m("<fb:section><fb:p/></fb:section>"); Check(m.Diagnostics().empty(), "namespace element names"); }
	{ XmlTagMatcher m("<section\n id=\"chapter-1\"\n xml:lang=\"ru\">\n</section>"); Check(m.Diagnostics().empty(), "multiline tag"); }
	{ XmlTagMatcher m("<!-- <section></section> --><![CDATA[<p></p>]]><?xml-stylesheet value=\"<p>\"?><!DOCTYPE root [ <!ELEMENT root (#PCDATA)> ]>"); Check(m.Diagnostics().empty(), "non-element constructs ignored"); }
	{ XmlTagMatcher m("<element value=\"1 > 0\"/><element value='< >'/>"); Check(m.Diagnostics().empty(), "quoted angle brackets"); }
	{ XmlTagMatcher m("<section><p>Text</section></p>"); Check(!m.Diagnostics().empty(), "crossing nesting diagnostics"); Check(m.ResultAt(1).state != XmlTagMatchState::Matched, "crossing nesting has no false section pair"); Check(m.ResultAt(17).state == XmlTagMatchState::Mismatched, "mismatched closing state"); }
	{ XmlTagMatcher m("<section><p>Text</p>"); Check(m.ResultAt(1).state == XmlTagMatchState::MissingClosing, "missing closing"); }
	{ XmlTagMatcher m("</section>"); Check(m.ResultAt(1).state == XmlTagMatchState::MissingOpening, "missing opening"); }
	{ XmlTagMatcher m("<Section></section>"); Check(!m.Diagnostics().empty(), "case-sensitive names"); }
	{ XmlTagMatcher m("<fb:section></fb:section>"); Check(m.Diagnostics().empty(), "namespace names form a valid pair"); }
	{ XmlTagMatcher m("<fb:section></section>"); Check(!m.Diagnostics().empty(), "namespace names remain case- and prefix-sensitive"); }
	{
		const std::string text = "</missing><section><p>text</section></p>";
		XmlTagMatcher m(text); const std::vector<XmlTagMatchResult>& diagnostics = m.Diagnostics();
		Check(diagnostics.size() == 5, "mixed malformed XML produces every structural diagnostic");
		for (size_t i = 1; i < diagnostics.size(); ++i) Check(diagnostics[i - 1].currentTagRange.start <= diagnostics[i].currentTagRange.start, "diagnostics are sorted in document order");
		Check(diagnostics.size() == 5 && diagnostics[0].state == XmlTagMatchState::MissingOpening && diagnostics[0].currentTagRange.start == text.find("</missing>"), "first mixed diagnostic is the orphan close");
		Check(diagnostics.size() == 5 && diagnostics[1].state == XmlTagMatchState::MissingClosing && diagnostics[1].currentTagRange.start == text.find("<section>"), "mixed diagnostics retain the section opener");
		Check(diagnostics.size() == 5 && diagnostics[2].state == XmlTagMatchState::MissingClosing && diagnostics[2].currentTagRange.start == text.find("<p>"), "mixed diagnostics retain the paragraph opener");
		Check(diagnostics.size() == 5 && diagnostics[3].state == XmlTagMatchState::Mismatched && diagnostics[3].currentTagRange.start == text.find("</section>"), "mixed diagnostics include the first crossing close");
		Check(diagnostics.size() == 5 && diagnostics[4].state == XmlTagMatchState::Mismatched && diagnostics[4].currentTagRange.start == text.find("</p>"), "mixed diagnostics include the second crossing close");
	}
	{
		const std::string text = "<root><section><p>"; XmlTagMatcher m(text); const std::vector<XmlTagMatchResult>& diagnostics = m.Diagnostics();
		Check(diagnostics.size() == 3, "all missing closers are diagnosed");
		Check(diagnostics.size() == 3 && diagnostics[0].currentTagRange.start == text.find("<root>") && diagnostics[1].currentTagRange.start == text.find("<section>") && diagnostics[2].currentTagRange.start == text.find("<p>"), "missing closers are ordered by opening tag");
	}
	{
		const std::string text = "</p></section>"; XmlTagMatcher m(text); const std::vector<XmlTagMatchResult>& diagnostics = m.Diagnostics();
		Check(diagnostics.size() == 2 && diagnostics[0].state == XmlTagMatchState::MissingOpening && diagnostics[1].state == XmlTagMatchState::MissingOpening, "multiple orphan closers are diagnosed");
	}
	{ XmlTagMatcher m("<a><b></a></b>"); Check(m.ResultAt(1).state != XmlTagMatchState::Matched && m.ResultAt(4).state != XmlTagMatchState::Matched, "crossing nesting creates no false tag pairs"); }
	{ XmlTagMatcher m("<section id=test>"); Check(m.ResultAt(1).state == XmlTagMatchState::Invalid, "completed unquoted attribute value is invalid"); }
	{ XmlTagMatcher m("<section id="); Check(m.ResultAt(1).state == XmlTagMatchState::Incomplete, "unfinished attribute value remains incomplete"); }
	{ const std::string text = "<section id=\"x\">text</section>"; XmlTagMatcher m(text); for (size_t position : { size_t(0), size_t(1), size_t(4), size_t(8), size_t(10), size_t(14), size_t(15), size_t(20), size_t(21), size_t(22), size_t(29) }) Check(m.ResultAt(position).state == XmlTagMatchState::Matched, "caret position inside matched tag"); }
	{
		const std::string text = "<section>text</section>"; XmlTagMatcher m(text); const size_t openingEnd = text.find('>'); const size_t closingStart = text.find("</section>");
		Check(m.ResultAt(0).state == XmlTagMatchState::Matched && m.ResultAt(openingEnd).state == XmlTagMatchState::Matched, "opening tag boundaries are included");
		Check(m.ResultAt(openingEnd + 1).state == XmlTagMatchState::None && m.ResultAt(text.find("text")).state == XmlTagMatchState::None, "position after opening tag and ordinary text are outside tag ranges");
		Check(m.ResultAt(closingStart).state == XmlTagMatchState::Matched && m.ResultAt(text.size() - 1).state == XmlTagMatchState::Matched, "closing tag boundaries are included");
		Check(m.ResultAt(text.size()).state == XmlTagMatchState::None, "position after closing tag is outside the half-open range");
	}
	{ XmlTagMatcher m("<"); Check(m.ResultAt(0).state == XmlTagMatchState::Incomplete, "incomplete angle"); }
	{ XmlTagMatcher m("</"); Check(m.ResultAt(0).state == XmlTagMatchState::Incomplete, "incomplete close"); }
	{ XmlTagMatcher m("<section"); Check(m.ResultAt(1).state == XmlTagMatchState::Incomplete, "incomplete tag"); }
	{ XmlTagMatcher m("<section attr=\"x"); Check(m.ResultAt(1).state == XmlTagMatchState::Incomplete, "incomplete quoted attr"); }
	{ const std::string text = u8"Русский текст <section><p>Текст</p></section>"; XmlTagMatcher m(text); const XmlTagToken* t = First(m, XmlTagTokenType::OpeningTag); Check(t && t->fullRange.start == std::string(u8"Русский текст ").size(), "UTF-8 byte offset before tag"); }
	{
		std::string large;
		std::vector<size_t> offsets;
		offsets.reserve(12000);
		for (int i = 0; i < 12000; ++i) {
			offsets.push_back(large.size());
			large += "<item attr=\"quoted\"/>";
		}
		XmlTagMatcher m(large);
		Check(m.Tokens().size() == 12000, "large ResultAt token array");
		for (const size_t offset : offsets) {
			for (const size_t relative : { size_t(0), size_t(1), size_t(3), size_t(5), size_t(6), size_t(10), size_t(11), size_t(14), size_t(18), size_t(19), size_t(20) }) {
				Check(m.ResultAt(offset + relative).state == XmlTagMatchState::SelfClosing,
					"ResultAt handles tag, name, whitespace, attribute, quoted value, slash and closing bracket");
			}
		}
	}
	return failures ? 1 : 0;
}
