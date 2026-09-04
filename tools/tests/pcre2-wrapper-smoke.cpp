#include <windows.h>
#include <cstring>
#include <iostream>
#include <string>

#include "atlstr.h"
#include "atlcoll.h"

#define PCRE2_CODE_UNIT_WIDTH 16
#define PCRE2_STATIC
#include "pcre2.h"

typedef CSimpleArray<CString> CStrings;
static PCRE2_SIZE AdvanceUtf16CodePoint(const CString& subject, PCRE2_SIZE offset);

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

		int rc = 0;
		PCRE2_SIZE offset = 0;
		uint32_t globalOptions = 0;
		while (true)
		{
			rc = pcre2_match(
				re,
				reinterpret_cast<PCRE2_SPTR>(static_cast<LPCWSTR>(sourceString)),
				static_cast<PCRE2_SIZE>(sourceString.GetLength()),
				offset,
				globalOptions,
				matchData,
				NULL);
			if (rc == PCRE2_ERROR_NOMATCH) {
				if ((globalOptions & PCRE2_NOTEMPTY_ATSTART) != 0) {
					offset = AdvanceUtf16CodePoint(sourceString, offset);
					globalOptions = 0;
					if (offset <= static_cast<PCRE2_SIZE>(sourceString.GetLength()))
						continue;
				}
				break;
			}
			if (rc < 0)
				break;
			PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(matchData);
			const bool retryAtEmptyMatch = (globalOptions & PCRE2_NOTEMPTY_ATSTART) != 0 && offset == ovector[0];
			if (retryAtEmptyMatch) {
				offset = AdvanceUtf16CodePoint(sourceString, offset);
				globalOptions = 0;
				if (offset <= static_cast<PCRE2_SIZE>(sourceString.GetLength()))
					continue;
				break;
			}
			if (ovector[1] >= ovector[0])
			{
				CString str(static_cast<LPCWSTR>(sourceString) + ovector[0], static_cast<int>(ovector[1] - ovector[0]));
				IMatch2* item = new IMatch2(str, static_cast<int>(ovector[0]));

					for (int i = 1; i < rc; i++)
					{
						const PCRE2_SIZE groupStart = ovector[i * 2];
						const PCRE2_SIZE groupEnd = ovector[i * 2 + 1];
						if (groupStart != PCRE2_UNSET && groupEnd != PCRE2_UNSET)
						{
						item->AddSubMatch(CString(static_cast<LPCWSTR>(sourceString) + groupStart, static_cast<int>(groupEnd - groupStart)));
						}
					}

					matches->AddItem(item);

			}
			if (!Global || !pcre2_next_match(matchData, &offset, &globalOptions))
				break;
		}

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

static PCRE2_SIZE AdvanceUtf16CodePoint(const CString& subject, PCRE2_SIZE offset)
{
	const PCRE2_SIZE length = static_cast<PCRE2_SIZE>(subject.GetLength());
	if (offset >= length)
		return length + 1;
	const wchar_t first = static_cast<LPCWSTR>(subject)[offset++];
	if (first >= 0xd800 && first <= 0xdbff && offset < length) {
		const wchar_t second = static_cast<LPCWSTR>(subject)[offset];
		if (second >= 0xdc00 && second <= 0xdfff)
			++offset;
	}
	return offset;
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

static std::string CStringToUtf8(const CString& text)
{
	return std::string(CT2A(text, CP_UTF8));
}

static std::string EncodeHex(const std::string& text)
{
	static const char digits[] = "0123456789abcdef";
	std::string result;
	result.reserve(text.size() * 2);
	for (size_t i = 0; i < text.size(); ++i) {
		const unsigned char value = static_cast<unsigned char>(text[i]);
		result.push_back(digits[value >> 4]);
		result.push_back(digits[value & 15]);
	}
	return result;
}

static std::string SerializeMatches(IMatchCollection* matches)
{
	std::string result;
	for (long index = 0; matches != NULL && index < matches->GetCount(); ++index) {
		if (index != 0) result += ';';
		IMatch2* match = matches->GetItem(index);
		result += EncodeHex(CStringToUtf8(match->GetValue()));
		result += '@' + std::to_string(match->GetFirstIndex()) + '@';
		ISubMatches* groups = match->GetSubMatches();
		for (long group = 0; groups != NULL && group < groups->GetCount(); ++group) {
			if (group != 0) result += ',';
			result += EncodeHex(CStringToUtf8(groups->GetItem(group)));
		}
	}
	return result;
}

int main(int argc, char* argv[])
{
	if (argc != 13)
		return 30;

	const std::string subject = DecodeHex(argv[1]);
	const std::string pattern = DecodeHex(argv[2]);
	const bool ignoreCase = ParseBool(argv[3]);
	const bool global = ParseBool(argv[4]);
	const bool multiline = ParseBool(argv[5]);
	const int expectedCount = std::atoi(argv[6]);
	const std::string expectedFirstValue = DecodeHex(argv[7]);
	const int expectedFirstIndex = std::atoi(argv[8]);
	const bool expectedCompileError = ParseBool(argv[9]);
	const int expectedFirstSubMatchCount = std::atoi(argv[10]);
	const std::string expectedFirstSubMatchValue = DecodeHex(argv[11]);
	const std::string expectedCollection = argv[12];

	if (pattern.empty())
		return 31;

	IRegExp2 re;
	re.Pattern = CString(CA2T(CStringA(pattern.c_str()), CP_UTF8));
	re.IgnoreCase = ignoreCase ? VARIANT_TRUE : VARIANT_FALSE;
	re.Global = global ? VARIANT_TRUE : VARIANT_FALSE;
	re.Multiline = multiline ? VARIANT_TRUE : VARIANT_FALSE;

	IMatchCollection* matches = re.Execute(CString(CA2T(CStringA(subject.c_str()), CP_UTF8)));

	if (expectedCompileError)
	{
		if (matches->GetCount() != 0)
			return 5;

		return 0;
	}

	if (matches->GetCount() != expectedCount)
		return 1;

	if (expectedCount > 0)
	{
		IMatch2* first = matches->GetItem(0);
		if (!first)
			return 2;

		CStringA firstValueUtf8(CT2A(first->GetValue(), CP_UTF8));
		if (expectedFirstValue != firstValueUtf8.GetString())
			return 3;
		if (first->GetFirstIndex() != expectedFirstIndex)
			return 4;

		ISubMatches* subMatches = first->GetSubMatches();
		const long actualSubMatchCount = subMatches ? subMatches->GetCount() : 0;
		if (actualSubMatchCount != expectedFirstSubMatchCount)
			return 6;

		if (expectedFirstSubMatchCount > 0)
		{
			CStringA firstSubMatchUtf8(CT2A(subMatches->GetItem(0), CP_UTF8));
			if (expectedFirstSubMatchValue != firstSubMatchUtf8.GetString())
				return 7;
		}
	}

	if (!expectedCollection.empty() && SerializeMatches(matches) != expectedCollection)
		return 8;

	return 0;
}
