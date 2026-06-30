#include <windows.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "atlstr.h"
#include "atlcoll.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include "pcre2.h"

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
		uint32_t options = IgnoreCase ? PCRE2_CASELESS : 0;
		options |= PCRE2_UTF;
		if (Multiline)
			options |= PCRE2_MULTILINE;

		IMatchCollection* matches = new IMatchCollection();
		CT2A pat(Pattern, CP_UTF8);
		int errorNumber = 0;
		PCRE2_SIZE errorOffset = 0;
		pcre2_code* re = pcre2_compile(
			reinterpret_cast<PCRE2_SPTR>(static_cast<LPCSTR>(pat)),
			PCRE2_ZERO_TERMINATED,
			options,
			&errorNumber,
			&errorOffset,
			NULL);
		if (!re)
			return matches;

		CT2A subj(sourceString, CP_UTF8);
		const int subjectLength = static_cast<int>(std::strlen(subj));
		pcre2_match_data* matchData = pcre2_match_data_create(OVECCOUNT / 3, NULL);
		if (!matchData)
		{
			pcre2_code_free(re);
			return matches;
		}

		int rc = 0, offset = 0, char_offset = 0;
		do
		{
			rc = pcre2_match(
				re,
				reinterpret_cast<PCRE2_SPTR>(static_cast<LPCSTR>(subj)),
				subjectLength,
				offset,
				0,
				matchData,
				NULL);
			if (rc > 0)
			{
				PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(matchData);
				char* substring_start = const_cast<char*>(static_cast<LPCSTR>(subj)) + ovector[0];
				int substring_length = static_cast<int>(ovector[1] - ovector[0]);
				if (substring_length >= 0)
				{
					CStringA utf8Match(substring_start, substring_length);
					while (offset < static_cast<int>(ovector[0]))
					{
						offset += UTF8_CHAR_LEN(subj[offset]);
						char_offset++;
					}

					CString str = CString(CA2T(utf8Match, CP_UTF8));
					IMatch2* item = new IMatch2(str, char_offset);

					for (int i = 1; i < rc; i++)
					{
						const PCRE2_SIZE groupStart = ovector[i * 2];
						const PCRE2_SIZE groupEnd = ovector[i * 2 + 1];
						if (groupStart != PCRE2_UNSET && groupEnd != PCRE2_UNSET)
						{
							substring_start = const_cast<char*>(static_cast<LPCSTR>(subj)) + groupStart;
							substring_length = static_cast<int>(groupEnd - groupStart);
							CStringA utf8SubMatch(substring_start, substring_length);
							item->AddSubMatch(CString(CA2T(utf8SubMatch, CP_UTF8)));
						}
					}

					matches->AddItem(item);
					char_offset += str.GetLength();
					offset = static_cast<int>(ovector[1]);
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

		pcre2_match_data_free(matchData);
		pcre2_code_free(re);
		return matches;
	}
};

struct RR
{
	enum
	{
		STRONG = 1,
		EMPHASIS = 2,
		UPPER = 4,
		LOWER = 8,
		TITLE = 16
	};

	int flags;
	int start;
	int len;
};

typedef CSimpleValArray<RR> RRList;

static void ApplyCaseMap(TCHAR* text, int start, int len, DWORD flags)
{
	if (!text || len <= 0)
		return;

	if (flags == LCMAP_UPPERCASE)
	{
		CharUpperBuff(text + start, len);
		return;
	}

	if (flags == LCMAP_LOWERCASE)
		CharLowerBuff(text + start, len);
}

static CString GetSM(ISubMatches* sm, int idx)
{
	if(!sm)
		return CString();

	if(idx < 0 || idx >= sm->GetCount())
		return CString();

	return sm->GetItem(idx);
}

static CString GetReplStr(const CString& rstr, IMatch2* rm, RRList& rl)
{
	CString rep;
	rep.GetBuffer(rstr.GetLength());
	rep.ReleaseBuffer(0);

	ISubMatches* rs = rm->GetSubMatches();

	RR cr;
	memset(&cr, 0, sizeof(cr));
	int flags = 0;

	CString rv;
	bool emptyParam = false;

	for (int i = 0; i < rstr.GetLength(); ++i)
	{
		if ((rstr[i] == L'$' && i < rstr.GetLength() - 1) ||
			(rstr[i] == L'\\' && i < rstr.GetLength() - 1))
		{
			switch (rstr[++i])
			{
			case L'0':
				rv = rm->GetValue();
				break;
			case L'+':
				rv = GetSM(rs, rs->GetCount() - 1);
				break;
			case L'1':
			case L'2':
			case L'3':
			case L'4':
			case L'5':
			case L'6':
			case L'7':
			case L'8':
			case L'9':
				rv = GetSM(rs, rstr[i] - L'0' - 1);
				if (rv.IsEmpty())
					emptyParam = true;
				break;
			case L'T':
				flags |= RR::TITLE;
				continue;
			case L'U':
				flags |= RR::UPPER;
				continue;
			case L'L':
				flags |= RR::LOWER;
				continue;
			case L'S':
				flags |= RR::STRONG;
				continue;
			case L'E':
				flags |= RR::EMPHASIS;
				continue;
			case L'Q':
				flags = 0;
				continue;
			default:
				continue;
			}
		}

		if (cr.flags != flags && cr.flags && cr.start < rep.GetLength())
		{
			cr.len = rep.GetLength() - cr.start;
			rl.Add(cr);
			cr.flags = 0;
		}

		if (flags)
		{
			cr.flags = flags;
			cr.start = rep.GetLength();
		}

		if (!emptyParam)
		{
			if (!rv.IsEmpty())
			{
				rep += rv;
				rv.Empty();
			}
			else
				rep += rstr[i];
		}
		else
			emptyParam = false;
	}

	if (cr.flags && cr.start < rep.GetLength())
	{
		cr.len = rep.GetLength() - cr.start;
		rl.Add(cr);
	}

	int tl = rep.GetLength();
	TCHAR* cp = rep.GetBuffer(tl);
	for (int j = 0; j < rl.GetSize();)
	{
		RR rr = rl[j];
		if (rr.flags & RR::UPPER)
			ApplyCaseMap(cp, rr.start, rr.len, LCMAP_UPPERCASE);
		else if (rr.flags & RR::LOWER)
			ApplyCaseMap(cp, rr.start, rr.len, LCMAP_LOWERCASE);
		else if (rr.flags & RR::TITLE && rr.len > 0)
		{
			ApplyCaseMap(cp, rr.start, 1, LCMAP_UPPERCASE);
			ApplyCaseMap(cp, rr.start + 1, rr.len - 1, LCMAP_LOWERCASE);
		}

		if((rr.flags &~ (RR::UPPER | RR::LOWER | RR::TITLE)) == 0)
			rl.RemoveAt(j);
		else
			++j;
	}
	rep.ReleaseBuffer(tl);
	return rep;
}

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

		RRList rl;
		CString repl = GetReplStr(replacement, match, rl);
		result.Delete(match->GetFirstIndex(), match->GetLength());
		result.Insert(match->GetFirstIndex(), repl);
		applied++;

		if (!global && applied > 0)
			break;
	}

	return result;
}

int main(int argc, char* argv[])
{
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
