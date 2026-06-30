#include <windows.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "pcre.h"

static bool MatchPattern(const char* pattern, const char* subject, int options)
{
	const char* error = NULL;
	int errorOffset = 0;
	pcre* re = pcre_compile(pattern, options, &error, &errorOffset, NULL);
	if (re == NULL)
		return false;

	int ovector[30] = {};
	const int rc = pcre_exec(re, NULL, subject,
		static_cast<int>(std::strlen(subject)), 0, 0, ovector, 30);
	pcre_free(re);
	return rc > 0;
}

static int ParseOptions(const std::string& optionsText)
{
	int options = 0;
	if (optionsText.find("utf8") != std::string::npos)
		options |= PCRE_UTF8;
	if (optionsText.find("multiline") != std::string::npos)
		options |= PCRE_MULTILINE;
	if (optionsText.find("caseless") != std::string::npos)
		options |= PCRE_CASELESS;
	return options;
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

int main(int argc, char* argv[])
{
	if (argc != 5)
	{
		std::cerr << "Использование: pcre-smoke.exe <pattern-hex> <subject-hex> <options> <expected-match>\n";
		return 10;
	}

	const std::string pattern = DecodeHex(argv[1]);
	const std::string subject = DecodeHex(argv[2]);
	if (pattern.empty() || subject.empty())
		return 11;
	const int options = ParseOptions(argv[3]);
	const bool expectedMatch = std::strcmp(argv[4], "1") == 0;
	const bool actualMatch = MatchPattern(pattern.c_str(), subject.c_str(), options);

	if (actualMatch != expectedMatch)
		return 1;

	return 0;
}
