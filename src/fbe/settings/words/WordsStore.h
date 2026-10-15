#pragma once

#include "WordsItem.h"

namespace FbeSettings { namespace Words {
	void Load(std::vector<WordsItem>& words);
	bool Save(const std::vector<WordsItem>& words);
	bool SaveToFile(const std::vector<WordsItem>& words, const CString& path);
} }
