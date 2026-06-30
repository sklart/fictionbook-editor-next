#include <windows.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "atlstr.h"
#include "atlcoll.h"
#include "pcre.h"

#define UTF8_CHAR_LEN( byte ) (( 0xE5000000 >> (( byte >> 3 ) & 0x1e )) & 3 ) + 1

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
#define OVECCOUNT 300
		pcre* re;
		const char* error = NULL;
		int options = IgnoreCase ? PCRE_CASELESS : 0;
		options |= PCRE_UTF8;
		if (Multiline)
			options |= PCRE_MULTILINE;

		int erroffset = 0;
		int ovector[OVECCOUNT];
		int subject_length = 0;
		int rc = 0, offset = 0, char_offset = 0;
		IMatchCollection* matches = new IMatchCollection();

		CT2A pat(Pattern, CP_UTF8);
		re = pcre_compile(pat, options, &error, &erroffset, NULL);
		if (!re)
			return matches;

		CT2A subj(sourceString, CP_UTF8);
		subject_length = static_cast<int>(std::strlen(subj));

		do
		{
			rc = pcre_exec(re, NULL, subj, subject_length, offset, 0, ovector, OVECCOUNT);
			if (rc > 0)
			{
				char* substring_start = const_cast<char*>(subj.m_psz) + ovector[0];
				int substring_length = ovector[1] - ovector[0];
				if (substring_length >= 0)
				{
					CStringA utf8Match(substring_start, substring_length);
					while (offset < ovector[0])
					{
						offset += UTF8_CHAR_LEN(subj[offset]);
						char_offset++;
					}

					CString str = CString(CA2T(utf8Match, CP_UTF8));
					IMatch2* item = new IMatch2(str, char_offset);

					for (int i = 1; i < rc; i++)
					{
						substring_start = const_cast<char*>(subj.m_psz) + ovector[i * 2];
						substring_length = ovector[i * 2 + 1] - ovector[i * 2];
						if (substring_length >= 0)
						{
							CStringA utf8SubMatch(substring_start, substring_length);
							item->AddSubMatch(CString(CA2T(utf8SubMatch, CP_UTF8)));
						}
					}

					matches->AddItem(item);
					char_offset += str.GetLength();
					offset = ovector[1];
					if (ovector[0] == ovector[1])
					{
						offset++;
						char_offset++;
					}
				}

				if (!Global)
					break;
			}
		} while (rc > 0);

		pcre_free(re);
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

int main(int argc, char* argv[])
{
	if (argc != 9)
		return 30;

	const std::string subject = DecodeHex(argv[1]);
	const std::string pattern = DecodeHex(argv[2]);
	const bool ignoreCase = ParseBool(argv[3]);
	const bool global = ParseBool(argv[4]);
	const bool multiline = ParseBool(argv[5]);
	const int expectedCount = std::atoi(argv[6]);
	const std::string expectedFirstValue = DecodeHex(argv[7]);
	const int expectedFirstIndex = std::atoi(argv[8]);

	if (subject.empty() || pattern.empty())
		return 31;

	IRegExp2 re;
	re.Pattern = CString(CA2T(CStringA(pattern.c_str()), CP_UTF8));
	re.IgnoreCase = ignoreCase ? VARIANT_TRUE : VARIANT_FALSE;
	re.Global = global ? VARIANT_TRUE : VARIANT_FALSE;
	re.Multiline = multiline ? VARIANT_TRUE : VARIANT_FALSE;

	IMatchCollection* matches = re.Execute(CString(CA2T(CStringA(subject.c_str()), CP_UTF8)));
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
	}

	return 0;
}
