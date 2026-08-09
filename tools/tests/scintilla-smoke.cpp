#include <windows.h>
#include <psapi.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "Scintilla.h"
#include "SciLexer.h"

typedef void* (__stdcall *CreateLexerFn)(const char* name);

struct SearchOptions
{
	bool ignoreCase = false;
};

static int HexValue(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	return -1;
}

static std::string DecodeHex(const char* text)
{
	std::string decoded;
	const size_t length = std::strlen(text);
	if ((length % 2) != 0)
		return decoded;

	decoded.reserve(length / 2);
	for (size_t i = 0; i < length; i += 2)
	{
		const int high = HexValue(text[i]);
		const int low = HexValue(text[i + 1]);
		if (high < 0 || low < 0)
		{
			decoded.clear();
			return decoded;
		}
		decoded.push_back(static_cast<char>((high << 4) | low));
	}
	return decoded;
}

static int BuildSearchFlags(const SearchOptions& options)
{
	int flags = SCFIND_REGEXP | SCFIND_CXX11REGEX;
	if (!options.ignoreCase)
		flags |= SCFIND_MATCHCASE;
	return flags;
}

static void SetRegexSearchFlags(HWND editor, const SearchOptions& options)
{
	SendMessage(editor, SCI_SETSEARCHFLAGS, BuildSearchFlags(options), 0);
}

static bool SearchFrom(HWND editor, int start, const char* pattern,
	const SearchOptions& options, bool expectedFound, int expectedStart, int expectedEnd)
{
	SendMessage(editor, SCI_SETTARGETSTART, start, 0);
	SendMessage(editor, SCI_SETTARGETEND,
		SendMessage(editor, SCI_GETLENGTH, 0, 0), 0);
	SetRegexSearchFlags(editor, options);

	const LRESULT position = SendMessage(editor, SCI_SEARCHINTARGET,
		std::strlen(pattern), reinterpret_cast<LPARAM>(pattern));
	if (!expectedFound)
		return position == -1;

	return position == expectedStart &&
		SendMessage(editor, SCI_GETTARGETSTART, 0, 0) == expectedStart &&
		SendMessage(editor, SCI_GETTARGETEND, 0, 0) == expectedEnd;
}

static bool ReplaceAndCompare(HWND editor, const std::string& replacement,
	const std::string& expectedText)
{
	SendMessage(editor, SCI_REPLACETARGETRE, std::strlen(replacement.c_str()),
		reinterpret_cast<LPARAM>(replacement.c_str()));

	std::string buffer(expectedText.size() + 8, '\0');
	SendMessage(editor, SCI_GETTEXT, static_cast<WPARAM>(buffer.size()),
		reinterpret_cast<LPARAM>(&buffer[0]));
	buffer.resize(std::strlen(buffer.c_str()));
	return buffer == expectedText;
}

struct TargetReplacementResult
{
	LRESULT replacementLength;
	LRESULT targetStart;
	LRESULT targetEnd;
	std::string text;
	bool undoRestored;
};

static TargetReplacementResult ReplaceTargetAndCapture(HWND editor, const std::string& subject,
	int targetStart, int targetEnd, const std::string& replacement, bool minimal)
{
	SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(subject.c_str()));
	SendMessage(editor, SCI_EMPTYUNDOBUFFER, 0, 0);
	SendMessage(editor, SCI_SETTARGETSTART, targetStart, 0);
	SendMessage(editor, SCI_SETTARGETEND, targetEnd, 0);
	TargetReplacementResult result = {};
	result.replacementLength = SendMessage(editor,
		minimal ? SCI_REPLACETARGETMINIMAL : SCI_REPLACETARGET,
		replacement.size(), reinterpret_cast<LPARAM>(replacement.c_str()));
	result.targetStart = SendMessage(editor, SCI_GETTARGETSTART, 0, 0);
	result.targetEnd = SendMessage(editor, SCI_GETTARGETEND, 0, 0);
	std::string buffer(subject.size() + replacement.size() + 8, '\0');
	SendMessage(editor, SCI_GETTEXT, buffer.size(), reinterpret_cast<LPARAM>(&buffer[0]));
	result.text.assign(buffer.c_str());
	SendMessage(editor, SCI_UNDO, 0, 0);
	std::fill(buffer.begin(), buffer.end(), '\0');
	SendMessage(editor, SCI_GETTEXT, buffer.size(), reinterpret_cast<LPARAM>(&buffer[0]));
	result.undoRestored = subject == buffer.c_str();
	return result;
}

static bool VerifyMinimalReplaceTarget(HWND editor)
{
	struct ReplaceCase
	{
		const char* subject;
		int targetStart;
		int targetEnd;
		const char* replacement;
	};
	static const ReplaceCase cases[] = {
		{ "<p>alpha</p>", 3, 8, "alpine" },
		{ "<p>\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82</p>", 3, 15, "\xD0\xBF\xD1\x80\xD0\xB8\xD1\x8E\xD1\x82" },
		{ "<p>a\xC2\xA0" "b</p>", 4, 6, " " },
		{ "<p/>", 2, 2, "node" },
		{ "<p>remove</p>", 3, 9, "" }
	};
	bool observedApiDifference = false;
	for (size_t i = 0; i < _countof(cases); ++i)
	{
		const std::string subject(cases[i].subject);
		const std::string replacement(cases[i].replacement);
		const TargetReplacementResult regular = ReplaceTargetAndCapture(editor, subject,
			cases[i].targetStart, cases[i].targetEnd, replacement, false);
		const TargetReplacementResult minimal = ReplaceTargetAndCapture(editor, subject,
			cases[i].targetStart, cases[i].targetEnd, replacement, true);
		if (!regular.undoRestored)
			return false;
		if (regular.text != minimal.text || !minimal.undoRestored)
			return false;
		observedApiDifference = observedApiDifference ||
			regular.replacementLength != minimal.replacementLength ||
			regular.targetStart != minimal.targetStart || regular.targetEnd != minimal.targetEnd;
	}
	return observedApiDifference;
}

static bool VerifyModernSourceFeatures(HWND editor)
{
	SendMessage(editor, SCI_SETCOMMANDEVENTS, FALSE, 0);
	SendMessage(editor, SCI_SETMODEVENTMASK, SC_MOD_CHANGEFOLD, 0);
	SendMessage(editor, SCI_SETUNDOSELECTIONHISTORY,
		SC_UNDO_SELECTION_HISTORY_ENABLED | SC_UNDO_SELECTION_HISTORY_SCROLL, 0);

	SciFnDirect directFunction = reinterpret_cast<SciFnDirect>(
		SendMessage(editor, SCI_GETDIRECTFUNCTION, 0, 0));
	const sptr_t directPointer = static_cast<sptr_t>(
		SendMessage(editor, SCI_GETDIRECTPOINTER, 0, 0));
	if (directFunction == NULL || directPointer == 0)
		return false;
	if (directFunction(directPointer, SCI_GETLENGTH, 0, 0) !=
		static_cast<sptr_t>(SendMessage(editor, SCI_GETLENGTH, 0, 0)))
		return false;

	const std::string nestedXml = "<section><section><p>text</p></section></section>";
	const char* closingSection = "</section>";
	SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(nestedXml.c_str()));
	SendMessage(editor, SCI_SETTARGETSTART, 0, 0);
	SendMessage(editor, SCI_SETTARGETEND, nestedXml.size(), 0);
	SendMessage(editor, SCI_SETSEARCHFLAGS, SCFIND_REGEXP | SCFIND_MATCHCASE | SCFIND_CXX11REGEX, 0);
	const LRESULT messageSearch = SendMessage(editor, SCI_SEARCHINTARGET, std::strlen(closingSection),
		reinterpret_cast<LPARAM>(closingSection));
	const LRESULT messageTargetStart = SendMessage(editor, SCI_GETTARGETSTART, 0, 0);
	const LRESULT messageTargetEnd = SendMessage(editor, SCI_GETTARGETEND, 0, 0);
	directFunction(directPointer, SCI_SETTARGETSTART, 0, 0);
	directFunction(directPointer, SCI_SETTARGETEND, nestedXml.size(), 0);
	directFunction(directPointer, SCI_SETSEARCHFLAGS, SCFIND_REGEXP | SCFIND_MATCHCASE | SCFIND_CXX11REGEX, 0);
	const sptr_t directSearch = directFunction(directPointer, SCI_SEARCHINTARGET, std::strlen(closingSection),
		reinterpret_cast<sptr_t>(closingSection));
	if (directSearch != messageSearch ||
		directFunction(directPointer, SCI_GETTARGETSTART, 0, 0) != messageTargetStart ||
		directFunction(directPointer, SCI_GETTARGETEND, 0, 0) != messageTargetEnd)
		return false;

	SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("abcdef"));
	SendMessage(editor, SCI_SETSEL, 4, 2);
	SendMessage(editor, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("XY"));
	SendMessage(editor, SCI_SETSEL, 0, 0);
	SendMessage(editor, SCI_UNDO, 0, 0);
	return SendMessage(editor, SCI_GETSELECTIONSTART, 0, 0) == 2 &&
		SendMessage(editor, SCI_GETSELECTIONEND, 0, 0) == 4;
}

static bool IsEmbeddedXmlStyle(int style)
{
	return style == SCE_H_ASP || style == SCE_H_ASPAT ||
		(style >= SCE_HJ_START && style <= SCE_HJ_TEMPLATELITERAL) ||
		(style >= SCE_HB_START && style <= SCE_HB_STRINGEOL) ||
		(style >= SCE_HP_START && style <= SCE_HP_IDENTIFIER) ||
		(style >= SCE_HPHP_DEFAULT && style <= SCE_HPHP_COMPLEX_VARIABLE);
}

static bool VerifyXmlEmbeddedLanguagesDisabled(HWND editor)
{
	SendMessage(editor, SCI_SETPROPERTY, reinterpret_cast<WPARAM>("lexer.xml.allow.asp"),
		reinterpret_cast<LPARAM>("0"));
	SendMessage(editor, SCI_SETPROPERTY, reinterpret_cast<WPARAM>("lexer.xml.allow.php"),
		reinterpret_cast<LPARAM>("0"));
	SendMessage(editor, SCI_SETPROPERTY, reinterpret_cast<WPARAM>("lexer.xml.allow.scripts"),
		reinterpret_cast<LPARAM>("0"));
	const std::string fixture = "<?xml version=\"1.0\"?>\n<?php $fake = 1; ?>\n<asp:test/>\n<script>var value = 1;</script>";
	SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(fixture.c_str()));
	SendMessage(editor, SCI_COLOURISE, 0, -1);
	for (int position = 0; position < static_cast<int>(fixture.size()); ++position)
	{
		if (IsEmbeddedXmlStyle(static_cast<int>(SendMessage(editor, SCI_GETSTYLEAT, position, 0))))
			return false;
	}
	return true;
}

static bool VerifyXmlSubstyles(HWND editor)
{
	const LRESULT tagSubstyles = SendMessage(editor, SCI_ALLOCATESUBSTYLES, SCE_H_TAG, 2);
	const LRESULT attributeSubstyles = SendMessage(editor, SCI_ALLOCATESUBSTYLES, SCE_H_ATTRIBUTE, 2);
	if (tagSubstyles < 0 || attributeSubstyles < 0)
		return false;
	SendMessage(editor, SCI_SETIDENTIFIERS, tagSubstyles, reinterpret_cast<LPARAM>("image"));
	SendMessage(editor, SCI_SETIDENTIFIERS, tagSubstyles + 1, reinterpret_cast<LPARAM>("section"));
	SendMessage(editor, SCI_SETIDENTIFIERS, attributeSubstyles, reinterpret_cast<LPARAM>("id"));
	SendMessage(editor, SCI_SETIDENTIFIERS, attributeSubstyles + 1, reinterpret_cast<LPARAM>("l:href"));

	const std::string fixture = "<section id=\"main\"><image l:href=\"#cover\"/><unknown/></section>";
	SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(fixture.c_str()));
	SendMessage(editor, SCI_COLOURISE, 0, -1);
	auto styleAt = [&](const char* token) -> int {
		const size_t position = fixture.find(token);
		return position == std::string::npos ? -1 : static_cast<unsigned char>(
			static_cast<char>(SendMessage(editor, SCI_GETSTYLEAT, position, 0)));
	};
	const bool valid = styleAt("section") == SCE_H_TAG &&
		styleAt("image") == SCE_H_TAG &&
		styleAt("id") == attributeSubstyles &&
		styleAt("l:href") == attributeSubstyles + 1 &&
		styleAt("unknown") == SCE_H_TAG;
	if (!valid)
	{
		std::cerr << "XML substyles expected tag=" << tagSubstyles << ", attr=" << attributeSubstyles
			<< "; actual section=" << styleAt("section") << ", image=" << styleAt("image")
			<< ", id=" << styleAt("id") << ", href=" << styleAt("l:href")
			<< ", unknown=" << styleAt("unknown") << std::endl;
	}
	SendMessage(editor, SCI_FREESUBSTYLES, 0, 0);
	return valid;
}

static bool VerifyEolAnnotations(HWND editor)
{
	SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("<FictionBook>\n</FictionBook>"));
	const char* diagnostic = "XML validation error";
	SendMessage(editor, SCI_EOLANNOTATIONSETTEXT, 1, reinterpret_cast<LPARAM>(diagnostic));
	SendMessage(editor, SCI_EOLANNOTATIONSETSTYLE, 1, STYLE_LINENUMBER);
	SendMessage(editor, SCI_EOLANNOTATIONSETVISIBLE, EOLANNOTATION_STANDARD, 0);
	const LRESULT diagnosticLength = SendMessage(editor, SCI_EOLANNOTATIONGETTEXT, 1, 0);
	std::string actual(static_cast<size_t>(diagnosticLength) + 1, '\0');
	SendMessage(editor, SCI_EOLANNOTATIONGETTEXT, 1, reinterpret_cast<LPARAM>(&actual[0]));
	if (std::strcmp(actual.c_str(), diagnostic) != 0 ||
		SendMessage(editor, SCI_EOLANNOTATIONGETSTYLE, 1, 0) != STYLE_LINENUMBER ||
		SendMessage(editor, SCI_EOLANNOTATIONGETVISIBLE, 0, 0) != EOLANNOTATION_STANDARD)
		return false;
	SendMessage(editor, SCI_EOLANNOTATIONCLEARALL, 0, 0);
	return SendMessage(editor, SCI_EOLANNOTATIONGETTEXT, 1, 0) == 0;
}

static bool VerifySpecialCharacterRepresentations(HWND editor)
{
	struct SpecialCharacterRepresentation
	{
		const char* character;
		const char* label;
	};
	static const SpecialCharacterRepresentation representations[] = {
		{ "\xC2\xA0", "NBSP" }, { "\xC2\xAD", "SHY" },
		{ "\xE2\x80\x8B", "ZWSP" }, { "\xE2\x80\x8C", "ZWNJ" },
		{ "\xE2\x80\x8D", "ZWJ" }, { "\xE2\x80\xAF", "NNBSP" },
		{ "\xE2\x81\xA0", "WJ" }, { "\xEF\xBB\xBF", "BOM" }
	};
	std::string document("A");
	for (size_t i = 0; i < _countof(representations); ++i)
		document += representations[i].character;
	document += "Z";
	SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(document.c_str()));

	for (size_t i = 0; i < _countof(representations); ++i)
	{
		SendMessage(editor, SCI_SETREPRESENTATION,
			reinterpret_cast<WPARAM>(representations[i].character),
			reinterpret_cast<LPARAM>(representations[i].label));
		SendMessage(editor, SCI_SETREPRESENTATIONAPPEARANCE,
			reinterpret_cast<WPARAM>(representations[i].character), SC_REPRESENTATION_PLAIN);
		const LRESULT length = SendMessage(editor, SCI_GETREPRESENTATION,
			reinterpret_cast<WPARAM>(representations[i].character), 0);
		std::string actual(static_cast<size_t>(length) + 1, '\0');
		SendMessage(editor, SCI_GETREPRESENTATION,
			reinterpret_cast<WPARAM>(representations[i].character), reinterpret_cast<LPARAM>(&actual[0]));
		if (std::strcmp(actual.c_str(), representations[i].label) != 0 ||
			SendMessage(editor, SCI_GETREPRESENTATIONAPPEARANCE,
				reinterpret_cast<WPARAM>(representations[i].character), 0) != SC_REPRESENTATION_PLAIN)
			return false;
	}

	std::string actualDocument(document.size() + 1, '\0');
	SendMessage(editor, SCI_GETTEXT, actualDocument.size(), reinterpret_cast<LPARAM>(&actualDocument[0]));
	if (std::strcmp(actualDocument.c_str(), document.c_str()) != 0)
		return false;
	for (size_t i = 0; i < _countof(representations); ++i)
	{
		SendMessage(editor, SCI_CLEARREPRESENTATION,
			reinterpret_cast<WPARAM>(representations[i].character), 0);
		if (SendMessage(editor, SCI_GETREPRESENTATION,
			reinterpret_cast<WPARAM>(representations[i].character), 0) != 0)
			return false;
	}
	return true;
}

static bool VerifyTechnologyAndLayoutApi(HWND editor)
{
	const LRESULT initialTechnology = SendMessage(editor, SCI_GETTECHNOLOGY, 0, 0);
	SendMessage(editor, SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWRITE, 0);
	if (SendMessage(editor, SCI_GETTECHNOLOGY, 0, 0) != SC_TECHNOLOGY_DIRECTWRITE)
		return false;

	const bool supportsThreadSafeMeasurements = SendMessage(editor, SCI_SUPPORTSFEATURE,
		SC_SUPPORTS_THREAD_SAFE_MEASURE_WIDTHS, 0) != 0;
	if (supportsThreadSafeMeasurements)
	{
		SendMessage(editor, SCI_SETLAYOUTTHREADS, 2, 0);
		if (SendMessage(editor, SCI_GETLAYOUTTHREADS, 0, 0) < 1)
			return false;
		SendMessage(editor, SCI_SETLAYOUTTHREADS, 1, 0);
	}
	SendMessage(editor, SCI_SETTECHNOLOGY, initialTechnology, 0);
	return SendMessage(editor, SCI_GETTECHNOLOGY, 0, 0) == initialTechnology;
}

static bool VerifyChangeHistoryApi(HWND editor)
{
	SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("first\nsecond\n"));
	SendMessage(editor, SCI_EMPTYUNDOBUFFER, 0, 0);
	SendMessage(editor, SCI_SETSAVEPOINT, 0, 0);
	const int historyFlags = SC_CHANGE_HISTORY_ENABLED | SC_CHANGE_HISTORY_MARKERS;
	SendMessage(editor, SCI_SETCHANGEHISTORY, historyFlags, 0);
	if (SendMessage(editor, SCI_GETCHANGEHISTORY, 0, 0) != historyFlags)
		return false;
	SendMessage(editor, SCI_APPENDTEXT, std::strlen("changed"), reinterpret_cast<LPARAM>("changed"));
	const LRESULT historyMarker = SendMessage(editor, SCI_MARKERGET, 2, 0);
	SendMessage(editor, SCI_SETCHANGEHISTORY, SC_CHANGE_HISTORY_DISABLED, 0);
	return (historyMarker & (1 << SC_MARKNUM_HISTORY_MODIFIED)) != 0;
}

static LRESULT CallScintilla(HWND editor, SciFnDirect directFunction, sptr_t directPointer,
	bool direct, unsigned int message, WPARAM wParam = 0, LPARAM lParam = 0)
{
	if (direct)
		return static_cast<LRESULT>(directFunction(directPointer, message,
			static_cast<uptr_t>(wParam), static_cast<sptr_t>(lParam)));
	return SendMessage(editor, message, wParam, lParam);
}

static std::vector<double> MeasureSearchCallSeries(HWND editor, size_t textBytes, bool direct)
{
	std::string document;
	document.reserve(textBytes + 64);
	while (document.size() < textBytes)
		document += "<section><title><p>FBE benchmark text</p></title><p>content</p></section>\n";
	SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(document.c_str()));

	const SciFnDirect directFunction = reinterpret_cast<SciFnDirect>(
		SendMessage(editor, SCI_GETDIRECTFUNCTION, 0, 0));
	const sptr_t directPointer = static_cast<sptr_t>(SendMessage(editor, SCI_GETDIRECTPOINTER, 0, 0));
	const int length = static_cast<int>(document.size());
	const char* pattern = "<section[ >]";
	std::vector<double> samples;
	for (int iteration = -1; iteration < 7; ++iteration)
	{
		const auto start = std::chrono::steady_clock::now();
		for (int move = 0; move < 400; ++move)
		{
			const int position = (move * 7919) % length;
			CallScintilla(editor, directFunction, directPointer, direct, SCI_GETSTYLEAT, position);
			CallScintilla(editor, directFunction, directPointer, direct, SCI_SETTARGETSTART, position);
			CallScintilla(editor, directFunction, directPointer, direct, SCI_SETTARGETEND, length);
			CallScintilla(editor, directFunction, directPointer, direct, SCI_SETSEARCHFLAGS,
				SCFIND_REGEXP | SCFIND_MATCHCASE | SCFIND_CXX11REGEX);
			CallScintilla(editor, directFunction, directPointer, direct, SCI_SEARCHINTARGET,
				std::strlen(pattern), reinterpret_cast<LPARAM>(pattern));
			CallScintilla(editor, directFunction, directPointer, direct, SCI_GETTARGETSTART);
			CallScintilla(editor, directFunction, directPointer, direct, SCI_GETTARGETEND);
		}
		const auto finish = std::chrono::steady_clock::now();
		if (iteration >= 0)
			samples.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
	}
	return samples;
}

static void PrintBenchmark(const char* path, size_t textBytes, const std::vector<double>& samples)
{
	std::vector<double> sorted = samples;
	std::sort(sorted.begin(), sorted.end());
	std::cout << path << "\t" << textBytes << "\t" << sorted[sorted.size() / 2]
		<< "\t" << sorted.front() << "\t" << sorted.back() << std::endl;
}

static std::string MakeWrappedXml(size_t textBytes)
{
	std::string document;
	document.reserve(textBytes + 1024);
	const std::string paragraph(900, 'x');
	while (document.size() < textBytes)
		document += "<section><p>" + paragraph + "</p></section>\n";
	return document;
}

static std::vector<double> MeasureWrapLayout(HWND editor, size_t textBytes, int technology, int layoutThreads)
{
	SendMessage(editor, SCI_SETTECHNOLOGY, technology, 0);
	SendMessage(editor, SCI_SETLAYOUTTHREADS, layoutThreads, 0);
	SendMessage(editor, SCI_SETWRAPMODE, SC_WRAP_WORD, 0);
	const std::string document = MakeWrappedXml(textBytes);
	std::vector<double> samples;
	for (int iteration = -1; iteration < 7; ++iteration)
	{
		SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(document.c_str()));
		SetWindowPos(editor, nullptr, 0, 0, 640, 480, SWP_NOACTIVATE | SWP_NOZORDER);
		const auto start = std::chrono::steady_clock::now();
		volatile sptr_t wrapLines = 0;
		for (int line = 0; line < SendMessage(editor, SCI_GETLINECOUNT, 0, 0); ++line)
			wrapLines += SendMessage(editor, SCI_WRAPCOUNT, line, 0);
		SetWindowPos(editor, nullptr, 0, 0, 960, 480, SWP_NOACTIVATE | SWP_NOZORDER);
		for (int line = 0; line < SendMessage(editor, SCI_GETLINECOUNT, 0, 0); ++line)
			wrapLines += SendMessage(editor, SCI_WRAPCOUNT, line, 0);
		const auto finish = std::chrono::steady_clock::now();
		if (iteration >= 0)
			samples.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
	}
	return samples;
}

static void RunLayoutBenchmark(HWND editor, bool largeOnly)
{
	const bool supportsThreadSafeMeasurements = SendMessage(editor, SCI_SUPPORTSFEATURE,
		SC_SUPPORTS_THREAD_SAFE_MEASURE_WIDTHS, 0) != 0;
	std::cout << "path\tbytes\tmedian-ms\tmin-ms\tmax-ms" << std::endl;
	const std::vector<size_t> sizes = largeOnly ?
		std::vector<size_t> { size_t(20 * 1024 * 1024) } :
		std::vector<size_t> { size_t(1024 * 1024), size_t(5 * 1024 * 1024) };
	for (const size_t textBytes : sizes)
	{
		PrintBenchmark("default-layout-1", textBytes,
			MeasureWrapLayout(editor, textBytes, SC_TECHNOLOGY_DEFAULT, 1));
		PrintBenchmark("directwrite-layout-1", textBytes,
			MeasureWrapLayout(editor, textBytes, SC_TECHNOLOGY_DIRECTWRITE, 1));
		if (supportsThreadSafeMeasurements)
			PrintBenchmark("directwrite-layout-2", textBytes,
				MeasureWrapLayout(editor, textBytes, SC_TECHNOLOGY_DIRECTWRITE, 2));
	}
	SendMessage(editor, SCI_SETLAYOUTTHREADS, 1, 0);
	SendMessage(editor, SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DEFAULT, 0);
}

static std::string MakeLineDenseXml(size_t textBytes, int* lineCount)
{
	std::string document;
	document.reserve(textBytes + 128);
	int lines = 0;
	while (document.size() < textBytes)
	{
		document += "<p>FBE source line content</p>\n";
		++lines;
	}
	if (lineCount)
		*lineCount = lines + 1;
	return document;
}

static std::vector<double> MeasureBulkAppendLoadSeries(CreateLexerFn createLexer,
	const std::string& document, int expectedLines, bool allocateLines)
{
	std::vector<double> samples;
	for (int iteration = -1; iteration < 7; ++iteration)
	{
		HWND editor = CreateWindowW(L"Scintilla", L"", WS_POPUP,
			0, 0, 100, 100, NULL, NULL, GetModuleHandle(NULL), NULL);
		if (editor == NULL)
			return std::vector<double>();
		void* xmlLexer = createLexer("xml");
		if (xmlLexer == NULL)
		{
			DestroyWindow(editor);
			return std::vector<double>();
		}
		SendMessage(editor, SCI_SETILEXER, 0, reinterpret_cast<LPARAM>(xmlLexer));
		SendMessage(editor, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
		const auto start = std::chrono::steady_clock::now();
		if (allocateLines)
			SendMessage(editor, SCI_ALLOCATELINES, expectedLines, 0);
		SendMessage(editor, SCI_APPENDTEXT, document.size(), reinterpret_cast<LPARAM>(document.c_str()));
		const auto finish = std::chrono::steady_clock::now();
		const bool correct = SendMessage(editor, SCI_GETLENGTH, 0, 0) == static_cast<LRESULT>(document.size()) &&
			SendMessage(editor, SCI_GETLINECOUNT, 0, 0) == expectedLines;
		DestroyWindow(editor);
		if (!correct)
			return std::vector<double>();
		if (iteration >= 0)
			samples.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
	}
	return samples;
}

static bool RunAllocateLinesBenchmark(CreateLexerFn createLexer)
{
	std::cout << "path\tbytes\tmedian-ms\tmin-ms\tmax-ms" << std::endl;
	for (const size_t textBytes : std::vector<size_t> { size_t(1024 * 1024), size_t(5 * 1024 * 1024), size_t(20 * 1024 * 1024) })
	{
		int expectedLines = 0;
		const std::string document = MakeLineDenseXml(textBytes, &expectedLines);
		const std::vector<double> baseline = MeasureBulkAppendLoadSeries(createLexer, document, expectedLines, false);
		const std::vector<double> allocated = MeasureBulkAppendLoadSeries(createLexer, document, expectedLines, true);
		if (baseline.empty() || allocated.empty())
			return false;
		PrintBenchmark("bulk-append", textBytes, baseline);
		PrintBenchmark("allocate-lines-bulk-append", textBytes, allocated);
	}
	return true;
}

static size_t GetPrivateBytes()
{
	PROCESS_MEMORY_COUNTERS_EX counters = {};
	counters.cb = sizeof(counters);
	if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
		return 0;
	return static_cast<size_t>(counters.PrivateUsage);
}

static void RunMemoryBenchmark(HWND editor, size_t textBytes)
{
	int expectedLines = 0;
	const std::string document = MakeLineDenseXml(textBytes, &expectedLines);
	SendMessage(editor, SCI_CLEARALL, 0, 0);
	const size_t privateBefore = GetPrivateBytes();
	SendMessage(editor, SCI_ALLOCATELINES, expectedLines, 0);
	SendMessage(editor, SCI_APPENDTEXT, document.size(), reinterpret_cast<LPARAM>(document.c_str()));
	const size_t privateAfterText = GetPrivateBytes();
	SendMessage(editor, SCI_COLOURISE, 0, -1);
	const size_t privateAfter = GetPrivateBytes();
	std::cout << "bytes\tprivate-before\tprivate-after-text\tprivate-after-style\ttext-delta\tstyle-delta" << std::endl;
	std::cout << document.size() << '\t' << privateBefore << '\t' << privateAfterText << '\t' << privateAfter << '\t'
		<< (privateAfterText >= privateBefore ? privateAfterText - privateBefore : 0) << '\t'
		<< (privateAfter >= privateAfterText ? privateAfter - privateAfterText : 0) << std::endl;
}

int main(int argc, char* argv[])
{
	if (argc < 2)
		return 20;

	HMODULE scintilla = LoadLibraryW(L"Scintilla.dll");
	HMODULE lexilla = LoadLibraryW(L"Lexilla.dll");
	if (scintilla == NULL || lexilla == NULL)
		return 1;

	CreateLexerFn createLexer = reinterpret_cast<CreateLexerFn>(
		GetProcAddress(lexilla, "CreateLexer"));
	if (createLexer == NULL)
		return 2;

	HWND editor = CreateWindowW(L"Scintilla", L"", WS_POPUP,
		0, 0, 100, 100, NULL, NULL, GetModuleHandle(NULL), NULL);
	if (editor == NULL)
		return 3;

	void* xmlLexer = createLexer("xml");
	if (xmlLexer == NULL)
		return 4;
	SendMessage(editor, SCI_SETILEXER, 0, reinterpret_cast<LPARAM>(xmlLexer));
	SendMessage(editor, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
	if (!VerifyModernSourceFeatures(editor))
	{
		DestroyWindow(editor);
		FreeLibrary(lexilla);
		FreeLibrary(scintilla);
		return 5;
	}
	if (!VerifyMinimalReplaceTarget(editor))
	{
		DestroyWindow(editor);
		FreeLibrary(lexilla);
		FreeLibrary(scintilla);
		return 11;
	}
	if (!VerifyXmlEmbeddedLanguagesDisabled(editor))
	{
		DestroyWindow(editor);
		FreeLibrary(lexilla);
		FreeLibrary(scintilla);
		return 6;
	}
	if (!VerifyXmlSubstyles(editor))
	{
		DestroyWindow(editor);
		FreeLibrary(lexilla);
		FreeLibrary(scintilla);
		return 12;
	}
	if (!VerifyEolAnnotations(editor))
	{
		DestroyWindow(editor);
		FreeLibrary(lexilla);
		FreeLibrary(scintilla);
		return 7;
	}
	if (!VerifySpecialCharacterRepresentations(editor))
	{
		DestroyWindow(editor);
		FreeLibrary(lexilla);
		FreeLibrary(scintilla);
		return 8;
	}
	if (!VerifyTechnologyAndLayoutApi(editor))
	{
		DestroyWindow(editor);
		FreeLibrary(lexilla);
		FreeLibrary(scintilla);
		return 9;
	}
	if (!VerifyChangeHistoryApi(editor))
	{
		DestroyWindow(editor);
		FreeLibrary(lexilla);
		FreeLibrary(scintilla);
		return 10;
	}

	const std::string mode(argv[1]);
	int exitCode = 0;

	if (mode == "search")
	{
		if (argc != 9)
			exitCode = 21;
		else
		{
			const std::string subject = DecodeHex(argv[2]);
			const std::string pattern = DecodeHex(argv[3]);
			if (subject.empty() || pattern.empty())
				exitCode = 22;
			else
			{
				SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(subject.c_str()));
				const int fromOffset = std::atoi(argv[4]);
				SearchOptions options;
				options.ignoreCase = std::atoi(argv[5]) != 0;
				const bool expectedFound = std::atoi(argv[6]) != 0;
				const int expectedStart = std::atoi(argv[7]);
				const int expectedEnd = std::atoi(argv[8]);
				if (!SearchFrom(editor, fromOffset, pattern.c_str(), options,
					expectedFound, expectedStart, expectedEnd))
					exitCode = 1;
			}
		}
	}
	else if (mode == "replace" || mode == "replace-gui")
	{
		if (argc != 8)
			exitCode = (mode == "replace") ? 23 : 27;
		else
		{
			const std::string subject = DecodeHex(argv[2]);
			const std::string pattern = DecodeHex(argv[3]);
			const std::string replacement = DecodeHex(argv[4]);
			const std::string expectedText = DecodeHex(argv[5]);
			if (subject.empty() || pattern.empty() || replacement.empty() || expectedText.empty())
				exitCode = (mode == "replace") ? 24 : 28;
			else
			{
				SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(subject.c_str()));
				const int fromOffset = std::atoi(argv[6]);
				SearchOptions options;
				options.ignoreCase = std::atoi(argv[7]) != 0;
				SendMessage(editor, SCI_SETTARGETSTART, fromOffset, 0);
				SendMessage(editor, SCI_SETTARGETEND,
					SendMessage(editor, SCI_GETLENGTH, 0, 0), 0);
				SetRegexSearchFlags(editor, options);
				const LRESULT position = SendMessage(editor, SCI_SEARCHINTARGET,
					std::strlen(pattern.c_str()), reinterpret_cast<LPARAM>(pattern.c_str()));
				if (position < 0)
					exitCode = (mode == "replace") ? 25 : 29;
				else if (mode == "replace")
				{
					if (!ReplaceAndCompare(editor, replacement, expectedText))
						exitCode = 1;
				}
				else
				{
					const LRESULT targetStart = SendMessage(editor, SCI_GETTARGETSTART, 0, 0);
					const LRESULT targetEnd = SendMessage(editor, SCI_GETTARGETEND, 0, 0);
					SendMessage(editor, SCI_SETSELECTIONSTART, targetStart, 0);
					SendMessage(editor, SCI_SETSELECTIONEND, targetEnd, 0);
					SendMessage(editor, SCI_TARGETFROMSELECTION, 0, 0);
					SetRegexSearchFlags(editor, options);
					const LRESULT refreshedPosition = SendMessage(editor, SCI_SEARCHINTARGET,
						std::strlen(pattern.c_str()), reinterpret_cast<LPARAM>(pattern.c_str()));
					if (refreshedPosition != targetStart ||
						SendMessage(editor, SCI_GETTARGETEND, 0, 0) != targetEnd)
						exitCode = 30;
					else if (!ReplaceAndCompare(editor, replacement, expectedText))
						exitCode = 1;
				}
			}
		}
	}
	else if (mode == "direct-benchmark")
	{
		if (argc != 2)
			exitCode = 31;
		else
		{
			std::cout << "path\tbytes\tmedian-ms\tmin-ms\tmax-ms" << std::endl;
			for (const size_t textBytes : { size_t(128 * 1024), size_t(2 * 1024 * 1024), size_t(16 * 1024 * 1024) })
			{
				PrintBenchmark("sendmessage", textBytes, MeasureSearchCallSeries(editor, textBytes, false));
				PrintBenchmark("direct", textBytes, MeasureSearchCallSeries(editor, textBytes, true));
			}
		}
	}
	else if (mode == "layout-benchmark" || mode == "layout-benchmark-large")
	{
		if (argc != 2)
			exitCode = 32;
		else
			RunLayoutBenchmark(editor, mode == "layout-benchmark-large");
	}
	else if (mode == "allocate-lines-benchmark")
	{
		if (argc != 2)
			exitCode = 33;
		else if (!RunAllocateLinesBenchmark(createLexer))
			exitCode = 34;
	}
	else if (mode == "memory-benchmark-1" || mode == "memory-benchmark-5" || mode == "memory-benchmark-20")
	{
		if (argc != 2)
			exitCode = 35;
		else if (mode == "memory-benchmark-1")
			RunMemoryBenchmark(editor, size_t(1024 * 1024));
		else if (mode == "memory-benchmark-5")
			RunMemoryBenchmark(editor, size_t(5 * 1024 * 1024));
		else
			RunMemoryBenchmark(editor, size_t(20 * 1024 * 1024));
	}
	else
	{
		exitCode = 26;
	}

	DestroyWindow(editor);
	FreeLibrary(lexilla);
	FreeLibrary(scintilla);
	return exitCode;
}
