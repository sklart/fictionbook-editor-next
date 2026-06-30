#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "Scintilla.h"

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
	else
	{
		exitCode = 26;
	}

	DestroyWindow(editor);
	FreeLibrary(lexilla);
	FreeLibrary(scintilla);
	return exitCode;
}
