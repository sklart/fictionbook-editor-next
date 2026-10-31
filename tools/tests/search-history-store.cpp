#include "stdafx.h"
#include "search/SearchHistoryStore.h"

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2 || argc > 3) return 1;
	CString path(argv[1]);
	std::vector<CString> initial = { L"кириллица \\d+", L"a=b & <x>", L"строка\nдва" };
	if (argc == 3) {
		std::vector<CString> restored;
		return FbeSearchHistory::Load(path, restored) && restored == initial ? 0 : 7;
	}
	if (!FbeSearchHistory::Save(path, initial)) return 2;
	std::vector<CString> restored;
	if (!FbeSearchHistory::Load(path, restored) || restored != initial) return 3;
	if (!::SetFileAttributesW(path, FILE_ATTRIBUTE_READONLY)) return 4;
	const bool wrote = FbeSearchHistory::Save(path, { L"replacement" });
	::SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
	if (wrote || !FbeSearchHistory::Load(path, restored) || restored != initial) return 5;
	if (FbeSearchHistory::Save(path, { L"" })) return 6;
	return 0;
}
