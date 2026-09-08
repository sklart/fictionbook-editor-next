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
	{ const std::string text = "<section id=\"x\">text</section>"; XmlTagMatcher m(text); for (size_t position : { size_t(0), size_t(1), size_t(4), size_t(8), size_t(10), size_t(14), size_t(15), size_t(20), size_t(21), size_t(22), size_t(29) }) Check(m.ResultAt(position).state == XmlTagMatchState::Matched, "caret position inside matched tag"); }
	{ XmlTagMatcher m("<"); Check(m.ResultAt(0).state == XmlTagMatchState::Incomplete, "incomplete angle"); }
	{ XmlTagMatcher m("</"); Check(m.ResultAt(0).state == XmlTagMatchState::Incomplete, "incomplete close"); }
	{ XmlTagMatcher m("<section"); Check(m.ResultAt(1).state == XmlTagMatchState::Incomplete, "incomplete tag"); }
	{ XmlTagMatcher m("<section attr=\"x"); Check(m.ResultAt(1).state == XmlTagMatchState::Incomplete, "incomplete quoted attr"); }
	{ const std::string text = u8"Русский текст <section><p>Текст</p></section>"; XmlTagMatcher m(text); const XmlTagToken* t = First(m, XmlTagTokenType::OpeningTag); Check(t && t->fullRange.start == std::string(u8"Русский текст ").size(), "UTF-8 byte offset before tag"); }
	return failures ? 1 : 0;
}
