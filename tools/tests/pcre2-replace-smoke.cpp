#include <windows.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "atlstr.h"
#include "atlcoll.h"

#define PCRE2_CODE_UNIT_WIDTH 16
#define PCRE2_STATIC
#include "pcre2.h"
#include "RegexPcre2MatchLoop.h"
#include "ReplacementParser.h"

typedef CSimpleArray<CString> CStrings;

struct ISubMatches {
public:
	CString GetItem(long index) { return m_strs[index]; }
	long GetCount() { return m_strs.GetSize(); }
	void AddItem(CString item) { m_strs.Add(item); }
private:
	CStrings m_strs;
};

struct IMatch2 {
public:
	IMatch2(CString str, int index): m_str(str), m_index(index) {}
	CString GetValue() { return m_str; }
	long GetFirstIndex() { return m_index; }
	long GetLength() { return m_str.GetLength(); }
	ISubMatches* GetSubMatches() { return &m_submatches; }
	void AddSubMatch(CString item) { m_submatches.AddItem(item); }
private:
	CString m_str;
	int m_index;
	ISubMatches m_submatches;
};

struct IMatchCollection {
public:
	long GetCount() { return m_matches.GetSize(); }
	IMatch2* GetItem(long index)
	{
		const int count = static_cast<int>(GetCount());
		if (count == 0 || index >= count)
			return NULL;
		return &m_matches[index];
	}
	void AddItem(IMatch2* item) { m_matches.Add(*item); }
private:
	CSimpleArray<IMatch2> m_matches;
};

struct IRegExp2 {
public:
	CString Pattern;
	VARIANT_BOOL IgnoreCase = VARIANT_FALSE;
	VARIANT_BOOL Global = VARIANT_FALSE;
	VARIANT_BOOL Multiline = VARIANT_FALSE;

	IMatchCollection* Execute(CString sourceString)
	{
		uint32_t options = IgnoreCase ? PCRE2_CASELESS : 0;
		options |= PCRE2_UTF;
		if (Multiline)
			options |= PCRE2_MULTILINE;

		IMatchCollection* matches = new IMatchCollection();
		int errorNumber = 0;
		PCRE2_SIZE errorOffset = 0;
		pcre2_code* re = pcre2_compile(
			reinterpret_cast<PCRE2_SPTR>(static_cast<LPCWSTR>(Pattern)),
			static_cast<PCRE2_SIZE>(Pattern.GetLength()),
			options,
			&errorNumber,
			&errorOffset,
			NULL);
		if (!re)
			return matches;

		pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(re, NULL);
		if (!matchData)
		{
			pcre2_code_free(re);
			return matches;
		}

		AU::RegexPcre2::ForEachMatch(
			re,
			reinterpret_cast<PCRE2_SPTR>(static_cast<LPCWSTR>(sourceString)),
			static_cast<PCRE2_SIZE>(sourceString.GetLength()),
			Global == VARIANT_TRUE,
			matchData,
			NULL,
			[&matches, &sourceString](int rc, PCRE2_SIZE* ovector)
			{
				CString str(static_cast<LPCWSTR>(sourceString) + ovector[0],
					static_cast<int>(ovector[1] - ovector[0]));
				IMatch2 item(str, static_cast<int>(ovector[0]));
				for (int i = 1; i < rc; i++)
				{
					const PCRE2_SIZE groupStart = ovector[i * 2];
					const PCRE2_SIZE groupEnd = ovector[i * 2 + 1];
					if (groupStart != PCRE2_UNSET && groupEnd != PCRE2_UNSET)
						item.AddSubMatch(CString(static_cast<LPCWSTR>(sourceString) + groupStart,
							static_cast<int>(groupEnd - groupStart)));
				}
				matches->AddItem(&item);
			});

		pcre2_match_data_free(matchData);
		pcre2_code_free(re);
		return matches;
	}
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

static bool ParseBool(const char* text)
{
	return std::strcmp(text, "1") == 0;
}

static CString Utf8ToCString(const std::string& text)
{
	return CString(CA2T(CStringA(text.c_str()), CP_UTF8));
}

static std::string CStringToUtf8(const CString& text)
{
	return std::string(CT2A(text, CP_UTF8));
}

static CString ApplyReplace(
	const CString& source,
	const CString& replacement,
	IMatchCollection* matches,
	bool global)
{
	CString result(source);
	int applied = 0;

	for (long i = matches->GetCount() - 1; i >= 0; --i)
	{
		if (!global && i > 0)
			continue;

		IMatch2* match = matches->GetItem(i);
		if (!match)
			continue;

		AU::Search::ReplacementMatch replacementMatch;
		replacementMatch.Value = match->GetValue();
		ISubMatches* subMatches = match->GetSubMatches();
		if (subMatches != NULL)
			for (long subMatch = 0; subMatch < subMatches->GetCount(); ++subMatch)
				replacementMatch.SubMatches.push_back(subMatches->GetItem(subMatch));
		std::vector<AU::Search::ReplacementFormattingRun> formatting;
		CString repl = AU::Search::ExpandRegexReplacement(replacement, replacementMatch, formatting);
		result.Delete(match->GetFirstIndex(), match->GetLength());
		result.Insert(match->GetFirstIndex(), repl);
		applied++;

		if (!global && applied > 0)
			break;
	}

	return result;
}

static int VerifyReplacementFormatting()
{
	AU::Search::ReplacementMatch match;
	match.Value = L"ivan";
	match.SubMatches.push_back(L"ivan");
	struct Case { LPCWSTR replacement; LPCWSTR expected; int flags; int length; };
	const Case cases[] = {
		{ L"\\Uabc\\Q", L"ABC", 0, 0 },
		{ L"\\U$1-test\\Q", L"IVAN-TEST", 0, 0 },
		{ L"\\T$1\\Q", L"Ivan", 0, 0 },
		{ L"\\U$1\\Q-$1", L"IVAN-ivan", 0, 0 },
		{ L"\\Sabc def\\Q", L"abc def", AU::Search::ReplacementFormatStrong, 7 },
		{ L"\\Eabc def\\Q", L"abc def", AU::Search::ReplacementFormatEmphasis, 7 }
	};
	for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index)
	{
		std::vector<AU::Search::ReplacementFormattingRun> formatting;
		const CString result = AU::Search::ExpandRegexReplacement(cases[index].replacement, match, formatting);
		if (result != cases[index].expected) return 40 + static_cast<int>(index * 2);
		if (cases[index].flags == 0)
		{
			if (!formatting.empty()) return 41 + static_cast<int>(index * 2);
		}
		else if (formatting.size() != 1 || formatting[0].Flags != cases[index].flags ||
			formatting[0].Start != 0 || formatting[0].Length != cases[index].length)
			return 41 + static_cast<int>(index * 2);
	}
	return 0;
}
int main(int argc, char* argv[])
{
	const int formattingResult = VerifyReplacementFormatting();
	if (formattingResult != 0) return formattingResult;
	if (argc != 8)
		return 30;

	const std::string subject = DecodeHex(argv[1]);
	const std::string pattern = DecodeHex(argv[2]);
	const std::string replacement = DecodeHex(argv[3]);
	const bool ignoreCase = ParseBool(argv[4]);
	const bool global = ParseBool(argv[5]);
	const bool multiline = ParseBool(argv[6]);
	const std::string expectedText = DecodeHex(argv[7]);

	if (subject.empty() || pattern.empty())
		return 31;

	IRegExp2 re;
	re.Pattern = Utf8ToCString(pattern);
	re.IgnoreCase = ignoreCase ? VARIANT_TRUE : VARIANT_FALSE;
	re.Global = global ? VARIANT_TRUE : VARIANT_FALSE;
	re.Multiline = multiline ? VARIANT_TRUE : VARIANT_FALSE;

	IMatchCollection* matches = re.Execute(Utf8ToCString(subject));
	CString result = ApplyReplace(Utf8ToCString(subject), Utf8ToCString(replacement), matches, global);
	const std::string actualText = CStringToUtf8(result);
	if (actualText != expectedText)
	{
		std::cerr << "Ожидалось: " << expectedText << "\n";
		std::cerr << "Получено: " << actualText << "\n";
		return 1;
	}

	return 0;
}
