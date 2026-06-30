#include <windows.h>
#include <cstring>
#include <iostream>
#include <string>

#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include "pcre2.h"

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

static uint32_t ParseOptions(const std::string& optionsText)
{
	uint32_t options = 0;
	if (optionsText.find("utf8") != std::string::npos)
		options |= PCRE2_UTF;
	if (optionsText.find("multiline") != std::string::npos)
		options |= PCRE2_MULTILINE;
	if (optionsText.find("caseless") != std::string::npos)
		options |= PCRE2_CASELESS;
	return options;
}

static bool MatchPattern(const char* pattern, const char* subject, uint32_t options)
{
	int errorNumber = 0;
	PCRE2_SIZE errorOffset = 0;
	pcre2_code* re = pcre2_compile(
		reinterpret_cast<PCRE2_SPTR>(pattern),
		PCRE2_ZERO_TERMINATED,
		options,
		&errorNumber,
		&errorOffset,
		NULL);
	if (re == NULL)
		return false;

	pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(re, NULL);
	const int rc = pcre2_match(
		re,
		reinterpret_cast<PCRE2_SPTR>(subject),
		std::strlen(subject),
		0,
		0,
		matchData,
		NULL);

	pcre2_match_data_free(matchData);
	pcre2_code_free(re);
	return rc >= 0;
}

int main(int argc, char* argv[])
{
	if (argc != 6)
	{
		std::cerr << "Использование: pcre2-smoke.exe <pattern-hex> <subject-hex> <options> <expected-match> <expected-compile-error>\n";
		return 10;
	}

	const std::string pattern = DecodeHex(argv[1]);
	const std::string subject = DecodeHex(argv[2]);
	if (pattern.empty())
		return 11;

	const uint32_t options = ParseOptions(argv[3]);
	const bool expectedMatch = std::strcmp(argv[4], "1") == 0;
	const bool expectedCompileError = std::strcmp(argv[5], "1") == 0;

	int errorNumber = 0;
	PCRE2_SIZE errorOffset = 0;
	pcre2_code* re = pcre2_compile(
		reinterpret_cast<PCRE2_SPTR>(pattern.c_str()),
		PCRE2_ZERO_TERMINATED,
		options,
		&errorNumber,
		&errorOffset,
		NULL);

	if (expectedCompileError)
	{
		if (re != NULL)
		{
			pcre2_code_free(re);
			return 2;
		}

		return 0;
	}

	if (re == NULL)
		return 3;

	pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(re, NULL);
	const int rc = pcre2_match(
		re,
		reinterpret_cast<PCRE2_SPTR>(subject.c_str()),
		subject.length(),
		0,
		0,
		matchData,
		NULL);

	pcre2_match_data_free(matchData);
	pcre2_code_free(re);

	const bool actualMatch = rc >= 0;
	if (actualMatch != expectedMatch)
		return 1;

	return 0;
}
