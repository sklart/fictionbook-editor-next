#include "stdafx.h"

#include "ReplacementParser.h"

namespace AU {
namespace Search {
namespace {

void ApplyCaseMap(TCHAR* text, int start, int length, DWORD flags)
{
	if (text == NULL || length <= 0)
		return;
	if (flags == LCMAP_UPPERCASE)
		CharUpperBuff(text + start, length);
	else if (flags == LCMAP_LOWERCASE)
		CharLowerBuff(text + start, length);
}

void CloseRun(CString& result, int& activeFlags, int& activeStart,
	std::vector<ReplacementFormattingRun>& formatting)
{
	if (activeFlags != 0 && activeStart < result.GetLength())
	{
		ReplacementFormattingRun run;
		run.Flags = activeFlags;
		run.Start = activeStart;
		run.Length = result.GetLength() - activeStart;
		formatting.push_back(run);
	}
	activeFlags = 0;
}

void SetFlags(CString& result, int newFlags, int& activeFlags, int& activeStart,
	std::vector<ReplacementFormattingRun>& formatting)
{
	if (newFlags == activeFlags)
		return;
	CloseRun(result, activeFlags, activeStart, formatting);
	activeFlags = newFlags;
	activeStart = result.GetLength();
}

void AppendText(CString& result, const CString& text, int flags,
	int& activeFlags, int& activeStart,
	std::vector<ReplacementFormattingRun>& formatting)
{
	if (text.IsEmpty())
		return;
	if (flags != activeFlags)
		SetFlags(result, flags, activeFlags, activeStart, formatting);
	result += text;
}

}

CString ExpandRegexReplacement(const CString& replacementTemplate,
	const ReplacementMatch& match,
	std::vector<ReplacementFormattingRun>& formatting)
{
	formatting.clear();
	CString result;
	int flags = 0;
	int activeFlags = 0;
	int activeStart = 0;

	for (int index = 0; index < replacementTemplate.GetLength(); ++index)
	{
		const wchar_t character = replacementTemplate[index];
		if ((character == L'$' || character == L'\\') && index + 1 < replacementTemplate.GetLength())
		{
			const wchar_t marker = replacementTemplate[++index];
			CString expansion;
			bool isExpansion = true;
			switch (marker)
			{
			case L'0': expansion = match.Value; break;
			case L'+': if (!match.SubMatches.empty()) expansion = match.SubMatches.back(); break;
			case L'1': case L'2': case L'3': case L'4': case L'5':
			case L'6': case L'7': case L'8': case L'9':
			{
				const std::size_t capture = static_cast<std::size_t>(marker - L'1');
				if (capture < match.SubMatches.size()) expansion = match.SubMatches[capture];
				break;
			}
			case L'T': flags |= ReplacementFormatTitle; SetFlags(result, flags, activeFlags, activeStart, formatting); continue;
			case L'U': flags |= ReplacementFormatUpper; SetFlags(result, flags, activeFlags, activeStart, formatting); continue;
			case L'L': flags |= ReplacementFormatLower; SetFlags(result, flags, activeFlags, activeStart, formatting); continue;
			case L'S': flags |= ReplacementFormatStrong; SetFlags(result, flags, activeFlags, activeStart, formatting); continue;
			case L'E': flags |= ReplacementFormatEmphasis; SetFlags(result, flags, activeFlags, activeStart, formatting); continue;
			case L'Q': flags = 0; SetFlags(result, flags, activeFlags, activeStart, formatting); continue;
			default: isExpansion = false; break;
			}
			if (isExpansion)
			{
				// Empty and nonparticipating captures are intentional empty output,
				// including $0 for a zero-length match and $+ without a capture.
				AppendText(result, expansion, flags, activeFlags, activeStart, formatting);
				continue;
			}
			// Preserve legacy FBE grammar: unknown two-character escapes are ignored.
			continue;
		}

		CString literal;
		literal += character;
		AppendText(result, literal, flags, activeFlags, activeStart, formatting);
	}
	CloseRun(result, activeFlags, activeStart, formatting);

	const int length = result.GetLength();
	TCHAR* characters = result.GetBuffer(length);
	for (std::vector<ReplacementFormattingRun>::iterator run = formatting.begin(); run != formatting.end(); )
	{
		if (run->Flags & ReplacementFormatUpper)
			ApplyCaseMap(characters, run->Start, run->Length, LCMAP_UPPERCASE);
		else if (run->Flags & ReplacementFormatLower)
			ApplyCaseMap(characters, run->Start, run->Length, LCMAP_LOWERCASE);
		else if ((run->Flags & ReplacementFormatTitle) && run->Length > 0)
		{
			ApplyCaseMap(characters, run->Start, 1, LCMAP_UPPERCASE);
			ApplyCaseMap(characters, run->Start + 1, run->Length - 1, LCMAP_LOWERCASE);
		}

		if ((run->Flags & ~(ReplacementFormatUpper | ReplacementFormatLower | ReplacementFormatTitle)) == 0)
			run = formatting.erase(run);
		else
			++run;
	}
	result.ReleaseBuffer(length);
	return result;
}

}
}